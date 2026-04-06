#include "rtpreviewengine.h"

#include <QVulkanFunctions>
#include <QVulkanInstance>

#include <algorithm>
#include <cstring>

#ifndef VK_SHADER_UNUSED_KHR
#define VK_SHADER_UNUSED_KHR (~0U)
#endif

static uint32_t findMemoryTypeIndex(VkPhysicalDevice pd, QVulkanFunctions *vf, uint32_t typeFilter,
                                  VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vf->vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return 0;
}

static bool allocDeviceLocal(VkPhysicalDevice pd, QVulkanFunctions *vf, QVulkanDeviceFunctions *df, VkDevice dev,
                             VkMemoryRequirements req, bool deviceAddress, VkDeviceMemory *outMem)
{
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = findMemoryTypeIndex(pd, vf, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateFlagsInfo mafi{};
    if (deviceAddress) {
        mafi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        mafi.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        mai.pNext = &mafi;
    }

    return df->vkAllocateMemory(dev, &mai, nullptr, outMem) == VK_SUCCESS;
}

static bool allocHostVisible(VkPhysicalDevice pd, QVulkanFunctions *vf, QVulkanDeviceFunctions *df, VkDevice dev,
                             VkMemoryRequirements req, VkDeviceMemory *outMem)
{
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex =
        findMemoryTypeIndex(pd, vf, req.memoryTypeBits,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    return df->vkAllocateMemory(dev, &mai, nullptr, outMem) == VK_SUCCESS;
}

bool RtPreviewEngine::loadAccelerationStructureProcs(QVulkanInstance *qInst, VkDevice dev)
{
    auto pfnGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        qInst->getInstanceProcAddr("vkGetDeviceProcAddr"));
    if (!pfnGetDeviceProcAddr)
        return false;
    auto load = [dev, pfnGetDeviceProcAddr](const char *name) {
        return pfnGetDeviceProcAddr(dev, name);
    };
    m_pfnCreateAccelerationStructureKHR =
        reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(load("vkCreateAccelerationStructureKHR"));
    m_pfnDestroyAccelerationStructureKHR =
        reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(load("vkDestroyAccelerationStructureKHR"));
    m_pfnGetAccelerationStructureBuildSizesKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(load("vkGetAccelerationStructureBuildSizesKHR"));
    m_pfnCmdBuildAccelerationStructuresKHR =
        reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(load("vkCmdBuildAccelerationStructuresKHR"));
    m_pfnGetAccelerationStructureDeviceAddressKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(load("vkGetAccelerationStructureDeviceAddressKHR"));
    return m_pfnCreateAccelerationStructureKHR && m_pfnDestroyAccelerationStructureKHR
        && m_pfnGetAccelerationStructureBuildSizesKHR && m_pfnCmdBuildAccelerationStructuresKHR
        && m_pfnGetAccelerationStructureDeviceAddressKHR;
}

void RtPreviewEngine::destroyAccelerationStructures()
{
    if (!m_df || m_device == VK_NULL_HANDLE)
        return;

    if (m_tlas && m_pfnDestroyAccelerationStructureKHR) {
        m_pfnDestroyAccelerationStructureKHR(m_device, m_tlas, nullptr);
        m_tlas = VK_NULL_HANDLE;
    }
    if (m_blas && m_pfnDestroyAccelerationStructureKHR) {
        m_pfnDestroyAccelerationStructureKHR(m_device, m_blas, nullptr);
        m_blas = VK_NULL_HANDLE;
    }

    if (m_tlasStorageBuffer) {
        m_df->vkDestroyBuffer(m_device, m_tlasStorageBuffer, nullptr);
        m_tlasStorageBuffer = VK_NULL_HANDLE;
    }
    if (m_tlasStorageMem) {
        m_df->vkFreeMemory(m_device, m_tlasStorageMem, nullptr);
        m_tlasStorageMem = VK_NULL_HANDLE;
    }
    if (m_blasStorageBuffer) {
        m_df->vkDestroyBuffer(m_device, m_blasStorageBuffer, nullptr);
        m_blasStorageBuffer = VK_NULL_HANDLE;
    }
    if (m_blasStorageMem) {
        m_df->vkFreeMemory(m_device, m_blasStorageMem, nullptr);
        m_blasStorageMem = VK_NULL_HANDLE;
    }

    if (m_vertexBuffer) {
        m_df->vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_indexBuffer) {
        m_df->vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
        m_indexBuffer = VK_NULL_HANDLE;
    }
    if (m_scratchBlas) {
        m_df->vkDestroyBuffer(m_device, m_scratchBlas, nullptr);
        m_scratchBlas = VK_NULL_HANDLE;
    }
    if (m_scratchTlas) {
        m_df->vkDestroyBuffer(m_device, m_scratchTlas, nullptr);
        m_scratchTlas = VK_NULL_HANDLE;
    }
    if (m_instanceBuffer) {
        m_df->vkDestroyBuffer(m_device, m_instanceBuffer, nullptr);
        m_instanceBuffer = VK_NULL_HANDLE;
    }

    if (m_vertexMem) {
        m_df->vkFreeMemory(m_device, m_vertexMem, nullptr);
        m_vertexMem = VK_NULL_HANDLE;
    }
    if (m_indexMem) {
        m_df->vkFreeMemory(m_device, m_indexMem, nullptr);
        m_indexMem = VK_NULL_HANDLE;
    }
    if (m_scratchBlasMem) {
        m_df->vkFreeMemory(m_device, m_scratchBlasMem, nullptr);
        m_scratchBlasMem = VK_NULL_HANDLE;
    }
    if (m_scratchTlasMem) {
        m_df->vkFreeMemory(m_device, m_scratchTlasMem, nullptr);
        m_scratchTlasMem = VK_NULL_HANDLE;
    }
    if (m_instanceMem) {
        m_df->vkFreeMemory(m_device, m_instanceMem, nullptr);
        m_instanceMem = VK_NULL_HANDLE;
    }
    m_blasDeviceAddress = 0;
}

bool RtPreviewEngine::buildAccelerationStructures(QString *errorOut)
{
    destroyAccelerationStructures();

    VkPhysicalDevice pd = m_dev->physicalDevice();
    QVulkanFunctions *vf = m_dev->qVulkanInstance()->functions();

    bool cmdBufferEnded = false;

    static const float verts[9] = {-1.f, -1.f, 0.f, 1.f, -1.f, 0.f, 0.f, 1.f, 0.f};
    static const uint32_t idx[3] = {0, 1, 2};

    const VkDeviceSize vSize = sizeof(verts);
    const VkDeviceSize iSize = sizeof(idx);

    VkBufferCreateInfo vb{};
    vb.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vb.size = vSize;
    vb.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    vb.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &vb, nullptr, &m_vertexBuffer) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateBuffer (RT vertex) failed.");
        destroyAccelerationStructures();
        return false;
    }

    VkBufferCreateInfo ib{};
    ib.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ib.size = iSize;
    ib.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    ib.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &ib, nullptr, &m_indexBuffer) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateBuffer (RT index) failed.");
        destroyAccelerationStructures();
        return false;
    }

    VkMemoryRequirements vReq{}, iReq{};
    m_df->vkGetBufferMemoryRequirements(m_device, m_vertexBuffer, &vReq);
    m_df->vkGetBufferMemoryRequirements(m_device, m_indexBuffer, &iReq);
    if (!allocDeviceLocal(pd, vf, m_df, m_device, vReq, true, &m_vertexMem)
        || !allocDeviceLocal(pd, vf, m_df, m_device, iReq, true, &m_indexMem)) {
        if (errorOut)
            *errorOut = QStringLiteral("vkAllocateMemory (RT geom) failed.");
        destroyAccelerationStructures();
        return false;
    }
    if (m_df->vkBindBufferMemory(m_device, m_vertexBuffer, m_vertexMem, 0) != VK_SUCCESS
        || m_df->vkBindBufferMemory(m_device, m_indexBuffer, m_indexMem, 0) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkBindBufferMemory (RT geom) failed.");
        destroyAccelerationStructures();
        return false;
    }

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkBuffer instStaging = VK_NULL_HANDLE;
    VkDeviceMemory instStagingMem = VK_NULL_HANDLE;
    VkBufferCreateInfo sb{};
    sb.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    sb.size = vSize + iSize;
    sb.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    sb.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &sb, nullptr, &staging) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateBuffer (RT staging) failed.");
        return false;
    }
    VkMemoryRequirements sReq{};
    m_df->vkGetBufferMemoryRequirements(m_device, staging, &sReq);
    if (!allocHostVisible(pd, vf, m_df, m_device, sReq, &stagingMem)) {
        if (errorOut)
            *errorOut = QStringLiteral("vkAllocateMemory (RT staging) failed.");
        m_df->vkDestroyBuffer(m_device, staging, nullptr);
        destroyAccelerationStructures();
        return false;
    }
    if (m_df->vkBindBufferMemory(m_device, staging, stagingMem, 0) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkBindBufferMemory (RT staging) failed.");
        m_df->vkDestroyBuffer(m_device, staging, nullptr);
        m_df->vkFreeMemory(m_device, stagingMem, nullptr);
        destroyAccelerationStructures();
        return false;
    }
    void *map = nullptr;
    if (m_df->vkMapMemory(m_device, stagingMem, 0, VK_WHOLE_SIZE, 0, &map) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkMapMemory (RT staging) failed.");
        m_df->vkDestroyBuffer(m_device, staging, nullptr);
        m_df->vkFreeMemory(m_device, stagingMem, nullptr);
        destroyAccelerationStructures();
        return false;
    }
    std::memcpy(map, verts, sizeof(verts));
    std::memcpy(static_cast<char *>(map) + vSize, idx, sizeof(idx));
    m_df->vkUnmapMemory(m_device, stagingMem);

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    cpci.queueFamilyIndex = m_dev->graphicsQueueFamilyIndex();
    if (m_df->vkCreateCommandPool(m_device, &cpci, nullptr, &cmdPool) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateCommandPool (RT AS) failed.");
        m_df->vkDestroyBuffer(m_device, staging, nullptr);
        m_df->vkFreeMemory(m_device, stagingMem, nullptr);
        destroyAccelerationStructures();
        return false;
    }

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    if (m_df->vkAllocateCommandBuffers(m_device, &cbai, &cmd) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkAllocateCommandBuffers (RT AS) failed.");
        m_df->vkDestroyCommandPool(m_device, cmdPool, nullptr);
        m_df->vkDestroyBuffer(m_device, staging, nullptr);
        m_df->vkFreeMemory(m_device, stagingMem, nullptr);
        destroyAccelerationStructures();
        return false;
    }

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    m_df->vkBeginCommandBuffer(cmd, &bi);

    auto buildFail = [&](const QString &msg) -> bool {
        if (errorOut)
            *errorOut = msg;
        if (!cmdBufferEnded)
            m_df->vkEndCommandBuffer(cmd);
        m_df->vkFreeCommandBuffers(m_device, cmdPool, 1, &cmd);
        m_df->vkDestroyCommandPool(m_device, cmdPool, nullptr);
        m_df->vkDestroyBuffer(m_device, staging, nullptr);
        m_df->vkFreeMemory(m_device, stagingMem, nullptr);
        if (instStaging) {
            m_df->vkDestroyBuffer(m_device, instStaging, nullptr);
            m_df->vkFreeMemory(m_device, instStagingMem, nullptr);
        }
        destroyAccelerationStructures();
        return false;
    };

    VkBufferCopy copyV{};
    copyV.size = vSize;
    m_df->vkCmdCopyBuffer(cmd, staging, m_vertexBuffer, 1, &copyV);
    VkBufferCopy copyI{};
    copyI.srcOffset = vSize;
    copyI.dstOffset = 0;
    copyI.size = iSize;
    m_df->vkCmdCopyBuffer(cmd, staging, m_indexBuffer, 1, &copyI);

    VkMemoryBarrier geomCopyBarrier{};
    geomCopyBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    geomCopyBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    geomCopyBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                               0, 1, &geomCopyBarrier, 0, nullptr, 0, nullptr);

    VkBufferDeviceAddressInfo vAddrInfo{};
    vAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    vAddrInfo.buffer = m_vertexBuffer;
    const VkDeviceAddress vertAddr = m_df->vkGetBufferDeviceAddress(m_device, &vAddrInfo);
    VkBufferDeviceAddressInfo iAddrInfo{};
    iAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    iAddrInfo.buffer = m_indexBuffer;
    const VkDeviceAddress indexAddr = m_df->vkGetBufferDeviceAddress(m_device, &iAddrInfo);

    VkAccelerationStructureGeometryTrianglesDataKHR tri{};
    tri.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    tri.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    tri.vertexData.deviceAddress = vertAddr;
    tri.vertexStride = 12;
    tri.maxVertex = 2;
    tri.indexType = VK_INDEX_TYPE_UINT32;
    tri.indexData.deviceAddress = indexAddr;

    VkAccelerationStructureGeometryKHR geom{};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.geometry.triangles = tri;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR blasBuild{};
    blasBuild.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    blasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasBuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    blasBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    blasBuild.srcAccelerationStructure = VK_NULL_HANDLE;
    blasBuild.dstAccelerationStructure = VK_NULL_HANDLE;
    blasBuild.geometryCount = 1;
    blasBuild.pGeometries = &geom;

    uint32_t maxPrim = 1;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    m_pfnGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &blasBuild,
                                               &maxPrim, &sizeInfo);

    VkBufferCreateInfo blasBuf{};
    blasBuf.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    blasBuf.size = sizeInfo.accelerationStructureSize;
    blasBuf.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    blasBuf.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &blasBuf, nullptr, &m_blasStorageBuffer) != VK_SUCCESS) {
        return buildFail(QStringLiteral("vkCreateBuffer (BLAS storage) failed."));
    }
    VkMemoryRequirements blasBufReq{};
    m_df->vkGetBufferMemoryRequirements(m_device, m_blasStorageBuffer, &blasBufReq);
    if (!allocDeviceLocal(pd, vf, m_df, m_device, blasBufReq, false, &m_blasStorageMem)) {
        m_df->vkDestroyBuffer(m_device, m_blasStorageBuffer, nullptr);
        m_blasStorageBuffer = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkAllocateMemory (BLAS storage) failed."));
    }
    if (m_df->vkBindBufferMemory(m_device, m_blasStorageBuffer, m_blasStorageMem, 0) != VK_SUCCESS) {
        m_df->vkDestroyBuffer(m_device, m_blasStorageBuffer, nullptr);
        m_df->vkFreeMemory(m_device, m_blasStorageMem, nullptr);
        m_blasStorageBuffer = VK_NULL_HANDLE;
        m_blasStorageMem = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkBindBufferMemory (BLAS storage) failed."));
    }

    VkAccelerationStructureCreateInfoKHR blasCreate{};
    blasCreate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasCreate.buffer = m_blasStorageBuffer;
    blasCreate.offset = 0;
    blasCreate.size = sizeInfo.accelerationStructureSize;
    blasCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    if (m_pfnCreateAccelerationStructureKHR(m_device, &blasCreate, nullptr, &m_blas) != VK_SUCCESS) {
        m_df->vkDestroyBuffer(m_device, m_blasStorageBuffer, nullptr);
        m_df->vkFreeMemory(m_device, m_blasStorageMem, nullptr);
        m_blasStorageBuffer = VK_NULL_HANDLE;
        m_blasStorageMem = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkCreateAccelerationStructureKHR (BLAS) failed."));
    }

    VkBufferCreateInfo scratchInfo{};
    scratchInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    scratchInfo.size = sizeInfo.buildScratchSize;
    scratchInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    scratchInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &scratchInfo, nullptr, &m_scratchBlas) != VK_SUCCESS) {
        return buildFail(QStringLiteral("vkCreateBuffer (BLAS scratch) failed."));
    }
    VkMemoryRequirements scratchReq{};
    m_df->vkGetBufferMemoryRequirements(m_device, m_scratchBlas, &scratchReq);
    if (!allocDeviceLocal(pd, vf, m_df, m_device, scratchReq, true, &m_scratchBlasMem)) {
        m_df->vkDestroyBuffer(m_device, m_scratchBlas, nullptr);
        m_scratchBlas = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkAllocateMemory (BLAS scratch) failed."));
    }
    if (m_df->vkBindBufferMemory(m_device, m_scratchBlas, m_scratchBlasMem, 0) != VK_SUCCESS) {
        m_df->vkDestroyBuffer(m_device, m_scratchBlas, nullptr);
        m_df->vkFreeMemory(m_device, m_scratchBlasMem, nullptr);
        m_scratchBlas = VK_NULL_HANDLE;
        m_scratchBlasMem = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkBindBufferMemory (BLAS scratch) failed."));
    }

    VkBufferDeviceAddressInfo scratchAddrInfo{};
    scratchAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchAddrInfo.buffer = m_scratchBlas;
    const VkDeviceAddress scratchAddr = m_df->vkGetBufferDeviceAddress(m_device, &scratchAddrInfo);

    blasBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    blasBuild.dstAccelerationStructure = m_blas;
    blasBuild.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = 1;
    range.primitiveOffset = 0;
    range.firstVertex = 0;
    range.transformOffset = 0;
    const VkAccelerationStructureBuildRangeInfoKHR *pRange = &range;
    m_pfnCmdBuildAccelerationStructuresKHR(cmd, 1, &blasBuild, &pRange);

    VkMemoryBarrier blasBarrier{};
    blasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    blasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    blasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                               VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &blasBarrier, 0, nullptr, 0,
                               nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR blasAddrInfo{};
    blasAddrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    blasAddrInfo.accelerationStructure = m_blas;
    m_blasDeviceAddress = m_pfnGetAccelerationStructureDeviceAddressKHR(m_device, &blasAddrInfo);

    VkAccelerationStructureInstanceKHR inst{};
    std::memset(&inst, 0, sizeof(inst));
    inst.transform.matrix[0][0] = 1.f;
    inst.transform.matrix[1][1] = 1.f;
    inst.transform.matrix[2][2] = 1.f;
    inst.instanceCustomIndex = 0;
    inst.mask = 0xFF;
    inst.instanceShaderBindingTableRecordOffset = 0;
    inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    inst.accelerationStructureReference = m_blasDeviceAddress;

    VkBufferCreateInfo instBufInfo{};
    instBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    instBufInfo.size = sizeof(VkAccelerationStructureInstanceKHR);
    instBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    instBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &instBufInfo, nullptr, &m_instanceBuffer) != VK_SUCCESS) {
        return buildFail(QStringLiteral("vkCreateBuffer (instance) failed."));
    }
    VkMemoryRequirements instReq{};
    m_df->vkGetBufferMemoryRequirements(m_device, m_instanceBuffer, &instReq);
    if (!allocDeviceLocal(pd, vf, m_df, m_device, instReq, true, &m_instanceMem)) {
        m_df->vkDestroyBuffer(m_device, m_instanceBuffer, nullptr);
        m_instanceBuffer = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkAllocateMemory (instance) failed."));
    }
    if (m_df->vkBindBufferMemory(m_device, m_instanceBuffer, m_instanceMem, 0) != VK_SUCCESS) {
        m_df->vkDestroyBuffer(m_device, m_instanceBuffer, nullptr);
        m_df->vkFreeMemory(m_device, m_instanceMem, nullptr);
        m_instanceBuffer = VK_NULL_HANDLE;
        m_instanceMem = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkBindBufferMemory (instance) failed."));
    }

    VkBufferCreateInfo isb{};
    isb.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    isb.size = sizeof(inst);
    isb.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    isb.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &isb, nullptr, &instStaging) != VK_SUCCESS) {
        return buildFail(QStringLiteral("vkCreateBuffer (instance staging) failed."));
    }
    VkMemoryRequirements isReq{};
    m_df->vkGetBufferMemoryRequirements(m_device, instStaging, &isReq);
    if (!allocHostVisible(pd, vf, m_df, m_device, isReq, &instStagingMem)) {
        m_df->vkDestroyBuffer(m_device, instStaging, nullptr);
        instStaging = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkAllocateMemory (instance staging) failed."));
    }
    if (m_df->vkBindBufferMemory(m_device, instStaging, instStagingMem, 0) != VK_SUCCESS) {
        m_df->vkDestroyBuffer(m_device, instStaging, nullptr);
        m_df->vkFreeMemory(m_device, instStagingMem, nullptr);
        instStaging = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkBindBufferMemory (instance staging) failed."));
    }
    void *im = nullptr;
    m_df->vkMapMemory(m_device, instStagingMem, 0, VK_WHOLE_SIZE, 0, &im);
    std::memcpy(im, &inst, sizeof(inst));
    m_df->vkUnmapMemory(m_device, instStagingMem);

    VkBufferCopy icopy{};
    icopy.size = sizeof(inst);
    m_df->vkCmdCopyBuffer(cmd, instStaging, m_instanceBuffer, 1, &icopy);

    VkBufferDeviceAddressInfo instAddrInfo{};
    instAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    instAddrInfo.buffer = m_instanceBuffer;
    const VkDeviceAddress instBufAddr = m_df->vkGetBufferDeviceAddress(m_device, &instAddrInfo);

    VkAccelerationStructureGeometryInstancesDataKHR instData{};
    instData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instData.arrayOfPointers = VK_FALSE;
    instData.data.deviceAddress = instBufAddr;

    VkAccelerationStructureGeometryKHR igeom{};
    igeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    igeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    igeom.geometry.instances = instData;

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuild{};
    tlasBuild.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    tlasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlasBuild.srcAccelerationStructure = VK_NULL_HANDLE;
    tlasBuild.dstAccelerationStructure = VK_NULL_HANDLE;
    tlasBuild.geometryCount = 1;
    tlasBuild.pGeometries = &igeom;

    uint32_t maxInst = 1;
    VkAccelerationStructureBuildSizesInfoKHR tlasSizes{};
    tlasSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    m_pfnGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuild,
                                               &maxInst, &tlasSizes);

    VkBufferCreateInfo tlasBuf{};
    tlasBuf.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    tlasBuf.size = tlasSizes.accelerationStructureSize;
    tlasBuf.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    tlasBuf.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &tlasBuf, nullptr, &m_tlasStorageBuffer) != VK_SUCCESS) {
        return buildFail(QStringLiteral("vkCreateBuffer (TLAS storage) failed."));
    }
    VkMemoryRequirements tlasBufReq{};
    m_df->vkGetBufferMemoryRequirements(m_device, m_tlasStorageBuffer, &tlasBufReq);
    if (!allocDeviceLocal(pd, vf, m_df, m_device, tlasBufReq, false, &m_tlasStorageMem)) {
        m_df->vkDestroyBuffer(m_device, m_tlasStorageBuffer, nullptr);
        m_tlasStorageBuffer = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkAllocateMemory (TLAS storage) failed."));
    }
    if (m_df->vkBindBufferMemory(m_device, m_tlasStorageBuffer, m_tlasStorageMem, 0) != VK_SUCCESS) {
        m_df->vkDestroyBuffer(m_device, m_tlasStorageBuffer, nullptr);
        m_df->vkFreeMemory(m_device, m_tlasStorageMem, nullptr);
        m_tlasStorageBuffer = VK_NULL_HANDLE;
        m_tlasStorageMem = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkBindBufferMemory (TLAS storage) failed."));
    }

    VkAccelerationStructureCreateInfoKHR tlasCreate{};
    tlasCreate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasCreate.buffer = m_tlasStorageBuffer;
    tlasCreate.offset = 0;
    tlasCreate.size = tlasSizes.accelerationStructureSize;
    tlasCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    if (m_pfnCreateAccelerationStructureKHR(m_device, &tlasCreate, nullptr, &m_tlas) != VK_SUCCESS) {
        m_df->vkDestroyBuffer(m_device, m_tlasStorageBuffer, nullptr);
        m_df->vkFreeMemory(m_device, m_tlasStorageMem, nullptr);
        m_tlasStorageBuffer = VK_NULL_HANDLE;
        m_tlasStorageMem = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkCreateAccelerationStructureKHR (TLAS) failed."));
    }

    VkBufferCreateInfo tscratchInfo{};
    tscratchInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    tscratchInfo.size = tlasSizes.buildScratchSize;
    tscratchInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    tscratchInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &tscratchInfo, nullptr, &m_scratchTlas) != VK_SUCCESS) {
        return buildFail(QStringLiteral("vkCreateBuffer (TLAS scratch) failed."));
    }
    VkMemoryRequirements tscratchReq{};
    m_df->vkGetBufferMemoryRequirements(m_device, m_scratchTlas, &tscratchReq);
    if (!allocDeviceLocal(pd, vf, m_df, m_device, tscratchReq, true, &m_scratchTlasMem)) {
        m_df->vkDestroyBuffer(m_device, m_scratchTlas, nullptr);
        m_scratchTlas = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkAllocateMemory (TLAS scratch) failed."));
    }
    if (m_df->vkBindBufferMemory(m_device, m_scratchTlas, m_scratchTlasMem, 0) != VK_SUCCESS) {
        m_df->vkDestroyBuffer(m_device, m_scratchTlas, nullptr);
        m_df->vkFreeMemory(m_device, m_scratchTlasMem, nullptr);
        m_scratchTlas = VK_NULL_HANDLE;
        m_scratchTlasMem = VK_NULL_HANDLE;
        return buildFail(QStringLiteral("vkBindBufferMemory (TLAS scratch) failed."));
    }

    scratchAddrInfo.buffer = m_scratchTlas;
    const VkDeviceAddress tscratchAddr = m_df->vkGetBufferDeviceAddress(m_device, &scratchAddrInfo);

    VkMemoryBarrier instBarrier{};
    instBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    instBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    instBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                               0, 1, &instBarrier, 0, nullptr, 0, nullptr);

    tlasBuild.dstAccelerationStructure = m_tlas;
    tlasBuild.scratchData.deviceAddress = tscratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR trange{};
    trange.primitiveCount = 1;
    const VkAccelerationStructureBuildRangeInfoKHR *pTrange = &trange;
    m_pfnCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuild, &pTrange);

    m_df->vkEndCommandBuffer(cmd);
    cmdBufferEnded = true;

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    if (m_df->vkQueueSubmit(m_dev->graphicsQueue(), 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) {
        return buildFail(QStringLiteral("vkQueueSubmit (RT AS build) failed."));
    }
    m_df->vkDeviceWaitIdle(m_device);

    m_df->vkFreeCommandBuffers(m_device, cmdPool, 1, &cmd);
    m_df->vkDestroyCommandPool(m_device, cmdPool, nullptr);

    m_df->vkDestroyBuffer(m_device, staging, nullptr);
    m_df->vkFreeMemory(m_device, stagingMem, nullptr);
    m_df->vkDestroyBuffer(m_device, instStaging, nullptr);
    m_df->vkFreeMemory(m_device, instStagingMem, nullptr);

    return true;
}
