#include "computepreviewengine.h"

#include <QDebug>
#include <QVulkanFunctions>
#include <QVulkanInstance>
#include <cstring>

ComputePreviewEngine::~ComputePreviewEngine()
{
    destroy();
}

void ComputePreviewEngine::destroy()
{
    if (!m_df || m_device == VK_NULL_HANDLE)
        return;

    m_df->vkDeviceWaitIdle(m_device);

    if (m_pipeline) {
        m_df->vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineCache) {
        m_df->vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);
        m_pipelineCache = VK_NULL_HANDLE;
    }
    if (m_shaderModule) {
        m_df->vkDestroyShaderModule(m_device, m_shaderModule, nullptr);
        m_shaderModule = VK_NULL_HANDLE;
    }
    if (m_descPool) {
        m_df->vkDestroyDescriptorPool(m_device, m_descPool, nullptr);
        m_descPool = VK_NULL_HANDLE;
        m_descSet = VK_NULL_HANDLE;
    }
    if (m_descLayout) {
        m_df->vkDestroyDescriptorSetLayout(m_device, m_descLayout, nullptr);
        m_descLayout = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout) {
        m_df->vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_storageView) {
        m_df->vkDestroyImageView(m_device, m_storageView, nullptr);
        m_storageView = VK_NULL_HANDLE;
    }
    if (m_storageImage) {
        m_df->vkDestroyImage(m_device, m_storageImage, nullptr);
        m_storageImage = VK_NULL_HANDLE;
    }
    if (m_storageMem) {
        m_df->vkFreeMemory(m_device, m_storageMem, nullptr);
        m_storageMem = VK_NULL_HANDLE;
    }
    if (m_hostUniformBuffer) {
        m_df->vkDestroyBuffer(m_device, m_hostUniformBuffer, nullptr);
        m_hostUniformBuffer = VK_NULL_HANDLE;
    }
    if (m_hostUniformMem) {
        if (m_hostUniformMapped) {
            m_df->vkUnmapMemory(m_device, m_hostUniformMem);
            m_hostUniformMapped = nullptr;
        }
        m_df->vkFreeMemory(m_device, m_hostUniformMem, nullptr);
        m_hostUniformMem = VK_NULL_HANDLE;
    }

    m_storageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_dev = nullptr;
    m_df = nullptr;
    m_device = VK_NULL_HANDLE;
}

bool ComputePreviewEngine::rebuild(CustomVulkanDevice *dev, const ComputeStageBinary &stage,
                                   QSize size, VkFormat surfaceFormat, QString *errorOut)
{
    destroy();

    m_dev = dev;
    m_df = dev->deviceFunctions();
    m_device = dev->device();

    VkExtent2D ext{uint32_t(size.width()), uint32_t(size.height())};
    if (ext.width == 0 || ext.height == 0) {
        ext.width = 800;
        ext.height = 600;
    }

    if (!createStorageImage(surfaceFormat, ext, errorOut))
        return false;
    if (!createHostUniformBuffer(errorOut))
        return false;
    if (!createComputePipeline(stage, errorOut))
        return false;

    return true;
}

bool ComputePreviewEngine::createStorageImage(VkFormat fmt, VkExtent2D ext, QString *errorOut)
{
    m_extent = ext;
    m_storageFormat = fmt;

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = fmt;
    ici.extent = {ext.width, ext.height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult err = m_df->vkCreateImage(m_device, &ici, nullptr, &m_storageImage);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to create compute storage image.");
        return false;
    }

    VkMemoryRequirements memReq{};
    m_df->vkGetImageMemoryRequirements(m_device, m_storageImage, &memReq);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = m_dev->deviceLocalMemoryTypeIndex();

    err = m_df->vkAllocateMemory(m_device, &mai, nullptr, &m_storageMem);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to allocate compute storage memory.");
        return false;
    }
    m_df->vkBindImageMemory(m_device, m_storageImage, m_storageMem, 0);

    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = m_storageImage;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = fmt;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = 1;

    err = m_df->vkCreateImageView(m_device, &vci, nullptr, &m_storageView);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to create compute storage image view.");
        return false;
    }

    return true;
}

bool ComputePreviewEngine::createHostUniformBuffer(QString *errorOut)
{
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = sizeof(HostUniforms);
    bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VkResult err = m_df->vkCreateBuffer(m_device, &bci, nullptr, &m_hostUniformBuffer);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to create compute UBO.");
        return false;
    }

    VkMemoryRequirements memReq{};
    m_df->vkGetBufferMemoryRequirements(m_device, m_hostUniformBuffer, &memReq);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = m_dev->hostVisibleMemoryTypeIndex();

    err = m_df->vkAllocateMemory(m_device, &mai, nullptr, &m_hostUniformMem);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to allocate compute UBO memory.");
        return false;
    }
    m_df->vkBindBufferMemory(m_device, m_hostUniformBuffer, m_hostUniformMem, 0);
    err = m_df->vkMapMemory(m_device, m_hostUniformMem, 0, sizeof(HostUniforms), 0, &m_hostUniformMapped);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to map compute UBO.");
        return false;
    }

    HostUniforms initial{};
    initial.resolution_x = float(m_extent.width);
    initial.resolution_y = float(m_extent.height);
    std::memcpy(m_hostUniformMapped, &initial, sizeof(HostUniforms));

    return true;
}

bool ComputePreviewEngine::createComputePipeline(const ComputeStageBinary &stage, QString *errorOut)
{
    VkShaderModuleCreateInfo smci{};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = stage.spirv.size();
    smci.pCode = reinterpret_cast<const uint32_t *>(stage.spirv.constData());

    VkResult err = m_df->vkCreateShaderModule(m_device, &smci, nullptr, &m_shaderModule);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to create compute shader module.");
        return false;
    }

    // Descriptor layout: binding 0 = UBO, binding 1 = storage image
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 2;
    dslci.pBindings = bindings;
    err = m_df->vkCreateDescriptorSetLayout(m_device, &dslci, nullptr, &m_descLayout);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to create compute descriptor set layout.");
        return false;
    }

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_descLayout;
    err = m_df->vkCreatePipelineLayout(m_device, &plci, nullptr, &m_pipelineLayout);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to create compute pipeline layout.");
        return false;
    }

    // Descriptor pool + set
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 1;
    dpci.poolSizeCount = 2;
    dpci.pPoolSizes = poolSizes;
    err = m_df->vkCreateDescriptorPool(m_device, &dpci, nullptr, &m_descPool);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to create compute descriptor pool.");
        return false;
    }

    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = m_descPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &m_descLayout;
    err = m_df->vkAllocateDescriptorSets(m_device, &dsai, &m_descSet);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to allocate compute descriptor set.");
        return false;
    }

    // Update descriptors
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = m_hostUniformBuffer;
    bufInfo.offset = 0;
    bufInfo.range = sizeof(HostUniforms);

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageView = m_storageView;
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &bufInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &imgInfo;
    m_df->vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);

    // Pipeline cache
    VkPipelineCacheCreateInfo pcci{};
    pcci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    err = m_df->vkCreatePipelineCache(m_device, &pcci, nullptr, &m_pipelineCache);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to create compute pipeline cache.");
        return false;
    }

    // Compute pipeline
    VkComputePipelineCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = m_shaderModule;
    cpci.stage.pName = stage.entry.toUtf8().constData();
    cpci.layout = m_pipelineLayout;

    QByteArray entryUtf8 = stage.entry.toUtf8();
    cpci.stage.pName = entryUtf8.constData();

    err = m_df->vkCreateComputePipelines(m_device, m_pipelineCache, 1, &cpci, nullptr, &m_pipeline);
    if (err != VK_SUCCESS) {
        if (errorOut) *errorOut = QStringLiteral("Failed to create compute pipeline.");
        return false;
    }

    return true;
}

void ComputePreviewEngine::setHostUniforms(const HostUniforms &host)
{
    if (m_hostUniformMapped)
        std::memcpy(m_hostUniformMapped, &host, sizeof(HostUniforms));
}

void ComputePreviewEngine::recordFrame(VkCommandBuffer cmd, VkImage swapchainImage,
                                       VkImageLayout swapchainLayoutBefore, VkExtent2D swapExtent)
{
    if (!m_pipeline)
        return;

    // Transition storage image to GENERAL for compute read/write
    VkImageMemoryBarrier toGeneral{};
    toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toGeneral.srcAccessMask = (m_storageLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? VkAccessFlags(0) : VK_ACCESS_TRANSFER_READ_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    toGeneral.oldLayout = m_storageLayout;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toGeneral.image = m_storageImage;
    toGeneral.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               0, 0, nullptr, 0, nullptr, 1, &toGeneral);

    // Bind + dispatch
    m_df->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    m_df->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout,
                                  0, 1, &m_descSet, 0, nullptr);

    uint32_t groupsX = (m_extent.width + 7) / 8;
    uint32_t groupsY = (m_extent.height + 7) / 8;
    m_df->vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // Transition storage image to TRANSFER_SRC
    VkImageMemoryBarrier toSrc{};
    toSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toSrc.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSrc.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSrc.image = m_storageImage;
    toSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               0, 0, nullptr, 0, nullptr, 1, &toSrc);

    // Transition swapchain image to TRANSFER_DST
    VkImageMemoryBarrier swapToDst{};
    swapToDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapToDst.srcAccessMask = 0;
    swapToDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapToDst.oldLayout = swapchainLayoutBefore;
    swapToDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapToDst.image = swapchainImage;
    swapToDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               0, 0, nullptr, 0, nullptr, 1, &swapToDst);

    // Blit storage image to swapchain
    VkImageBlit region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffsets[0] = {0, 0, 0};
    region.srcOffsets[1] = {int32_t(m_extent.width), int32_t(m_extent.height), 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstOffsets[0] = {0, 0, 0};
    region.dstOffsets[1] = {int32_t(swapExtent.width), int32_t(swapExtent.height), 1};
    m_df->vkCmdBlitImage(cmd, m_storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1, &region, VK_FILTER_LINEAR);

    m_storageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    // Transition swapchain back to PRESENT_SRC
    VkImageMemoryBarrier swapToPresent{};
    swapToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapToPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapToPresent.dstAccessMask = 0;
    swapToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    swapToPresent.image = swapchainImage;
    swapToPresent.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                               0, 0, nullptr, 0, nullptr, 1, &swapToPresent);
}
