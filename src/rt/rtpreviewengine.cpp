#include "rtpreviewengine.h"

#include <QVulkanFunctions>
#include <QVulkanInstance>

#include <algorithm>
#include <cstring>

#ifndef VK_SHADER_UNUSED_KHR
#define VK_SHADER_UNUSED_KHR (~0U)
#endif

static const RayTraceStageBinary *findStage(const QVector<RayTraceStageBinary> &stages, const QString &kind)
{
    for (const RayTraceStageBinary &s : stages) {
        if (s.kind == kind)
            return &s;
    }
    return nullptr;
}

static VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize a)
{
    return (v + a - 1) & ~(a - 1);
}

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
                             VkMemoryRequirements req, VkMemoryAllocateFlags allocFlags, VkDeviceMemory *outMem)
{
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = findMemoryTypeIndex(pd, vf, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateFlagsInfo mafi{};
    if (allocFlags != 0) {
        mafi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        mafi.flags = allocFlags;
        mai.pNext = &mafi;
    }

    return df->vkAllocateMemory(dev, &mai, nullptr, outMem) == VK_SUCCESS;
}

static bool allocHostVisibleCoherentBda(VkPhysicalDevice pd, QVulkanFunctions *vf, QVulkanDeviceFunctions *df,
                                        VkDevice dev, VkMemoryRequirements req, VkDeviceMemory *outMem)
{
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex =
        findMemoryTypeIndex(pd, vf, req.memoryTypeBits,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateFlagsInfo mafi{};
    mafi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    mafi.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
    mai.pNext = &mafi;

    return df->vkAllocateMemory(dev, &mai, nullptr, outMem) == VK_SUCCESS;
}

bool RtPreviewEngine::loadRayTracingProcs(QVulkanInstance *qInst, VkDevice dev)
{
    auto pfnGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        qInst->getInstanceProcAddr("vkGetDeviceProcAddr"));
    if (!pfnGetDeviceProcAddr)
        return false;
    auto load = [dev, pfnGetDeviceProcAddr](const char *name) {
        return pfnGetDeviceProcAddr(dev, name);
    };
    m_pfnCreateRayTracingPipelinesKHR =
        reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(load("vkCreateRayTracingPipelinesKHR"));
    m_pfnGetRayTracingShaderGroupHandlesKHR =
        reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(load("vkGetRayTracingShaderGroupHandlesKHR"));
    m_pfnCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(load("vkCmdTraceRaysKHR"));
    return m_pfnCreateRayTracingPipelinesKHR && m_pfnGetRayTracingShaderGroupHandlesKHR && m_pfnCmdTraceRaysKHR;
}

RtPreviewEngine::~RtPreviewEngine()
{
    destroy();
}

void RtPreviewEngine::destroy()
{
    destroyPipelineResources();
    m_dev = nullptr;
    m_df = nullptr;
    m_device = VK_NULL_HANDLE;
}

void RtPreviewEngine::destroyPipelineResources()
{
    if (!m_df || m_device == VK_NULL_HANDLE)
        return;

    if (m_rtPipeline) {
        m_df->vkDestroyPipeline(m_device, m_rtPipeline, nullptr);
        m_rtPipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineCache) {
        m_df->vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);
        m_pipelineCache = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout) {
        m_df->vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_descPool) {
        m_df->vkDestroyDescriptorPool(m_device, m_descPool, nullptr);
        m_descPool = VK_NULL_HANDLE;
    }
    m_descSet = VK_NULL_HANDLE;
    if (m_descLayout) {
        m_df->vkDestroyDescriptorSetLayout(m_device, m_descLayout, nullptr);
        m_descLayout = VK_NULL_HANDLE;
    }

    destroyAccelerationStructures();

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

    if (m_sbtBuffer) {
        m_df->vkDestroyBuffer(m_device, m_sbtBuffer, nullptr);
        m_sbtBuffer = VK_NULL_HANDLE;
    }
    if (m_sbtMem) {
        m_df->vkFreeMemory(m_device, m_sbtMem, nullptr);
        m_sbtMem = VK_NULL_HANDLE;
    }

    if (m_modRaygen) {
        m_df->vkDestroyShaderModule(m_device, m_modRaygen, nullptr);
        m_modRaygen = VK_NULL_HANDLE;
    }
    if (m_modMiss) {
        m_df->vkDestroyShaderModule(m_device, m_modMiss, nullptr);
        m_modMiss = VK_NULL_HANDLE;
    }
    if (m_modClosest) {
        m_df->vkDestroyShaderModule(m_device, m_modClosest, nullptr);
        m_modClosest = VK_NULL_HANDLE;
    }

    if (m_hostUniformMapped) {
        m_df->vkUnmapMemory(m_device, m_hostUniformMem);
        m_hostUniformMapped = nullptr;
    }
    if (m_hostUniformBuffer) {
        m_df->vkDestroyBuffer(m_device, m_hostUniformBuffer, nullptr);
        m_hostUniformBuffer = VK_NULL_HANDLE;
    }
    if (m_hostUniformMem) {
        m_df->vkFreeMemory(m_device, m_hostUniformMem, nullptr);
        m_hostUniformMem = VK_NULL_HANDLE;
    }

    m_sbtSize = 0;
    m_raygenRegion = {};
    m_missRegion = {};
    m_hitRegion = {};
    m_callableRegion = {};
}

bool RtPreviewEngine::createStorageImage(VkFormat fmt, VkExtent2D ext, QString *errorOut)
{
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = fmt;
    ici.extent.width = ext.width;
    ici.extent.height = ext.height;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (m_df->vkCreateImage(m_device, &ici, nullptr, &m_storageImage) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateImage (RT output) failed.");
        return false;
    }

    VkMemoryRequirements req{};
    m_df->vkGetImageMemoryRequirements(m_device, m_storageImage, &req);

    VkPhysicalDevice pd = m_dev->physicalDevice();
    QVulkanFunctions *vf = m_dev->qVulkanInstance()->functions();
    if (!allocDeviceLocal(pd, vf, m_df, m_device, req, 0, &m_storageMem)) {
        if (errorOut)
            *errorOut = QStringLiteral("vkAllocateMemory (RT output image) failed.");
        return false;
    }
    if (m_df->vkBindImageMemory(m_device, m_storageImage, m_storageMem, 0) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkBindImageMemory (RT output) failed.");
        return false;
    }

    VkImageViewCreateInfo iv{};
    iv.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    iv.image = m_storageImage;
    iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
    iv.format = fmt;
    iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    iv.subresourceRange.levelCount = 1;
    iv.subresourceRange.layerCount = 1;

    if (m_df->vkCreateImageView(m_device, &iv, nullptr, &m_storageView) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateImageView (RT output) failed.");
        return false;
    }

    return true;
}

bool RtPreviewEngine::createHostUniformBuffer(QString *errorOut)
{
    VkPhysicalDevice pd = m_dev->physicalDevice();
    QVulkanFunctions *vf = m_dev->qVulkanInstance()->functions();
    VkPhysicalDeviceProperties props{};
    vf->vkGetPhysicalDeviceProperties(pd, &props);
    const VkDeviceSize align = props.limits.minUniformBufferOffsetAlignment;
    const VkDeviceSize sz =
        VkDeviceSize((sizeof(HostUniforms) + size_t(align) - 1) / size_t(align)) * VkDeviceSize(align);

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = sz;
    bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &bci, nullptr, &m_hostUniformBuffer) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateBuffer (RT host UBO) failed.");
        return false;
    }

    VkMemoryRequirements req{};
    m_df->vkGetBufferMemoryRequirements(m_device, m_hostUniformBuffer, &req);
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex =
        findMemoryTypeIndex(pd, vf, req.memoryTypeBits,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (m_df->vkAllocateMemory(m_device, &mai, nullptr, &m_hostUniformMem) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkAllocateMemory (RT host UBO) failed.");
        return false;
    }
    if (m_df->vkBindBufferMemory(m_device, m_hostUniformBuffer, m_hostUniformMem, 0) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkBindBufferMemory (RT host UBO) failed.");
        return false;
    }
    if (m_df->vkMapMemory(m_device, m_hostUniformMem, 0, VK_WHOLE_SIZE, 0, &m_hostUniformMapped) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkMapMemory (RT host UBO) failed.");
        return false;
    }
    return true;
}

bool RtPreviewEngine::createRayTracingPipelineAndSbt(const QVector<RayTraceStageBinary> &stages, QString *errorOut)
{
    auto makeModule = [&](const QByteArray &spirv, VkShaderModule *out) {
        VkShaderModuleCreateInfo sm{};
        sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        sm.codeSize = spirv.size();
        sm.pCode = reinterpret_cast<const uint32_t *>(spirv.constData());
        return m_df->vkCreateShaderModule(m_device, &sm, nullptr, out) == VK_SUCCESS;
    };

    auto kindToStageFlag = [](const QString &k) -> VkShaderStageFlagBits {
        if (k == QStringLiteral("raygen"))
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        if (k == QStringLiteral("miss"))
            return VK_SHADER_STAGE_MISS_BIT_KHR;
        if (k == QStringLiteral("closest_hit"))
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        if (k == QStringLiteral("any_hit"))
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        if (k == QStringLiteral("intersection"))
            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        if (k == QStringLiteral("callable"))
            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    };

    QVector<VkShaderModule> modules;
    QVector<QByteArray> entryNames;
    QVector<VkPipelineShaderStageCreateInfo> stageInfos;
    QVector<uint32_t> raygenStages;
    QVector<uint32_t> missStages;
    QVector<uint32_t> closestStages;
    QVector<uint32_t> anyHitStages;
    QVector<uint32_t> intersectionStages;
    QVector<uint32_t> callableStages;

    auto failAndDestroyModules = [&](const QString &msg) {
        for (VkShaderModule m : modules)
            m_df->vkDestroyShaderModule(m_device, m, nullptr);
        if (errorOut)
            *errorOut = msg;
        return false;
    };

    for (const RayTraceStageBinary &s : stages) {
        const VkShaderStageFlagBits stageFlag = kindToStageFlag(s.kind);
        if (stageFlag == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM)
            continue;
        if (s.spirv.isEmpty())
            return failAndDestroyModules(QStringLiteral("RT stage \"%1\" has empty SPIR-V.").arg(s.kind));

        VkShaderModule module = VK_NULL_HANDLE;
        if (!makeModule(s.spirv, &module))
            return failAndDestroyModules(QStringLiteral("vkCreateShaderModule (RT stage \"%1\") failed.").arg(s.kind));

        const uint32_t stageIndex = uint32_t(stageInfos.size());
        modules.append(module);
        entryNames.append(s.entry.toUtf8());

        VkPipelineShaderStageCreateInfo st{};
        st.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        st.stage = stageFlag;
        st.module = module;
        st.pName = entryNames.back().constData();
        stageInfos.append(st);

        if (s.kind == QStringLiteral("raygen"))
            raygenStages.append(stageIndex);
        else if (s.kind == QStringLiteral("miss"))
            missStages.append(stageIndex);
        else if (s.kind == QStringLiteral("closest_hit"))
            closestStages.append(stageIndex);
        else if (s.kind == QStringLiteral("any_hit"))
            anyHitStages.append(stageIndex);
        else if (s.kind == QStringLiteral("intersection"))
            intersectionStages.append(stageIndex);
        else if (s.kind == QStringLiteral("callable"))
            callableStages.append(stageIndex);
    }

    if (raygenStages.size() != 1 || missStages.isEmpty() || closestStages.isEmpty()) {
        return failAndDestroyModules(
            QStringLiteral("GPU RT preview requires exactly one raygen stage and at least one miss + closest_hit stage."));
    }

    QVector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    auto addGeneralGroup = [&](uint32_t generalShader) {
        VkRayTracingShaderGroupCreateInfoKHR g{};
        g.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        g.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        g.generalShader = generalShader;
        g.closestHitShader = VK_SHADER_UNUSED_KHR;
        g.anyHitShader = VK_SHADER_UNUSED_KHR;
        g.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.append(g);
    };
    auto addHitGroup = [&](uint32_t closestShader, uint32_t anyHitShader, uint32_t intersectionShader) {
        VkRayTracingShaderGroupCreateInfoKHR g{};
        g.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        g.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        g.generalShader = VK_SHADER_UNUSED_KHR;
        g.closestHitShader = closestShader;
        g.anyHitShader = anyHitShader;
        g.intersectionShader = intersectionShader;
        groups.append(g);
    };

    const uint32_t raygenGroupIndex = uint32_t(groups.size());
    addGeneralGroup(raygenStages[0]);
    const uint32_t missGroupBase = uint32_t(groups.size());
    for (uint32_t sidx : missStages)
        addGeneralGroup(sidx);

    const int hitGroupCount = std::max({closestStages.size(), anyHitStages.size(), intersectionStages.size()});
    const uint32_t hitGroupBase = uint32_t(groups.size());
    for (int i = 0; i < hitGroupCount; ++i) {
        const uint32_t c = i < closestStages.size() ? closestStages[i] : VK_SHADER_UNUSED_KHR;
        const uint32_t a = i < anyHitStages.size() ? anyHitStages[i] : VK_SHADER_UNUSED_KHR;
        const uint32_t is = i < intersectionStages.size() ? intersectionStages[i] : VK_SHADER_UNUSED_KHR;
        if (c == VK_SHADER_UNUSED_KHR && a == VK_SHADER_UNUSED_KHR && is == VK_SHADER_UNUSED_KHR)
            continue;
        addHitGroup(c, a, is);
    }
    const uint32_t callableGroupBase = uint32_t(groups.size());
    for (uint32_t sidx : callableStages)
        addGeneralGroup(sidx);

    VkDescriptorSetLayoutBinding bindings[3]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR
        | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
        | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo dsl{};
    dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl.bindingCount = 3;
    dsl.pBindings = bindings;
    if (m_df->vkCreateDescriptorSetLayout(m_device, &dsl, nullptr, &m_descLayout) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateDescriptorSetLayout (RT) failed.");
        return false;
    }

    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_descLayout;
    if (m_df->vkCreatePipelineLayout(m_device, &pl, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreatePipelineLayout (RT) failed.");
        return false;
    }

    VkDescriptorPoolSize poolSizes[3]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    poolSizes[2].descriptorCount = 1;
    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 1;
    dpci.poolSizeCount = 3;
    dpci.pPoolSizes = poolSizes;
    if (m_df->vkCreateDescriptorPool(m_device, &dpci, nullptr, &m_descPool) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateDescriptorPool (RT) failed.");
        return false;
    }

    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = m_descPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &m_descLayout;
    if (m_df->vkAllocateDescriptorSets(m_device, &dsai, &m_descSet) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkAllocateDescriptorSets (RT) failed.");
        return false;
    }

    VkDescriptorImageInfo dii{};
    dii.imageView = m_storageView;
    dii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorBufferInfo dbi{};
    dbi.buffer = m_hostUniformBuffer;
    dbi.offset = 0;
    dbi.range = sizeof(HostUniforms);

    VkWriteDescriptorSetAccelerationStructureKHR writeAS{};
    writeAS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    writeAS.accelerationStructureCount = 1;
    writeAS.pAccelerationStructures = &m_tlas;

    VkWriteDescriptorSet writes[3]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].pImageInfo = &dii;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &dbi;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].pNext = &writeAS;
    writes[2].dstSet = m_descSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    m_df->vkUpdateDescriptorSets(m_device, 3, writes, 0, nullptr);

    uint32_t maxRecursion = 0;
    if (m_rtProps.maxRayRecursionDepth == 0) {
        maxRecursion = 0;
    } else if (m_maxPipelineRayRecursionDepthOverride > 0) {
        maxRecursion = std::min(uint32_t(m_maxPipelineRayRecursionDepthOverride), m_rtProps.maxRayRecursionDepth);
        maxRecursion = std::max(1u, maxRecursion);
    } else {
        maxRecursion = std::min(2u, std::max(1u, m_rtProps.maxRayRecursionDepth));
    }

    VkRayTracingPipelineCreateInfoKHR rpci{};
    rpci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    rpci.stageCount = uint32_t(stageInfos.size());
    rpci.pStages = stageInfos.constData();
    rpci.groupCount = uint32_t(groups.size());
    rpci.pGroups = groups.constData();
    rpci.maxPipelineRayRecursionDepth = maxRecursion;
    rpci.layout = m_pipelineLayout;

    VkResult pr = m_pfnCreateRayTracingPipelinesKHR(m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rpci, nullptr,
                                                    &m_rtPipeline);
    for (VkShaderModule m : modules)
        m_df->vkDestroyShaderModule(m_device, m, nullptr);
    modules.clear();
    if (pr != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateRayTracingPipelinesKHR failed: %1").arg(int(pr));
        return false;
    }

    m_handleSize = m_rtProps.shaderGroupHandleSize;
    if (m_handleSize == 0) {
        if (errorOut)
            *errorOut = QStringLiteral("shaderGroupHandleSize is zero.");
        return false;
    }
    m_handleAlign = std::max(1u, m_rtProps.shaderGroupHandleAlignment);
    m_regionStride = uint32_t(aligned(m_handleSize, VkDeviceSize(m_handleAlign)));

    const uint32_t baseAlign = std::max(1u, m_rtProps.shaderGroupBaseAlignment);
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    const uint32_t groupCount = uint32_t(groups.size());
    bci.size = VkDeviceSize(baseAlign - 1) + VkDeviceSize(m_regionStride) * groupCount;
    m_sbtSize = bci.size;
    bci.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_device, &bci, nullptr, &m_sbtBuffer) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateBuffer (SBT) failed.");
        return false;
    }

    VkMemoryRequirements sbtReq{};
    m_df->vkGetBufferMemoryRequirements(m_device, m_sbtBuffer, &sbtReq);
    VkPhysicalDevice pd = m_dev->physicalDevice();
    QVulkanFunctions *vf = m_dev->qVulkanInstance()->functions();
    if (!allocHostVisibleCoherentBda(pd, vf, m_df, m_device, sbtReq, &m_sbtMem)) {
        if (errorOut)
            *errorOut = QStringLiteral("vkAllocateMemory (SBT host-visible BDA) failed.");
        return false;
    }
    if (m_df->vkBindBufferMemory(m_device, m_sbtBuffer, m_sbtMem, 0) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkBindBufferMemory (SBT) failed.");
        return false;
    }

    const size_t handleDataBytes = size_t(m_handleSize) * size_t(groupCount);
    QVector<uint8_t> handles;
    handles.resize(int(handleDataBytes));
    VkResult gh = m_pfnGetRayTracingShaderGroupHandlesKHR(m_device, m_rtPipeline, 0, groupCount, handleDataBytes,
                                                          handles.data());
    if (gh != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkGetRayTracingShaderGroupHandlesKHR failed: %1").arg(int(gh));
        return false;
    }

    void *mapped = nullptr;
    if (m_df->vkMapMemory(m_device, m_sbtMem, 0, m_sbtSize, 0, &mapped) != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkMapMemory (SBT) failed.");
        return false;
    }
    VkBufferDeviceAddressInfo bdai{};
    bdai.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bdai.buffer = m_sbtBuffer;
    const VkDeviceAddress baseAddr = m_df->vkGetBufferDeviceAddress(m_device, &bdai);
    VkDeviceSize pad = 0;
    if (baseAlign > 0) {
        const VkDeviceSize mod = VkDeviceSize(uint64_t(baseAddr) % uint64_t(baseAlign));
        if (mod != 0)
            pad = VkDeviceSize(baseAlign) - mod;
    }
    auto *p = static_cast<uint8_t *>(mapped) + pad;
    for (uint32_t i = 0; i < groupCount; ++i) {
        std::memcpy(p + i * m_regionStride, handles.constData() + i * int(m_handleSize), m_handleSize);
    }
    m_df->vkUnmapMemory(m_device, m_sbtMem);

    const VkDeviceAddress baseAligned = baseAddr + pad;

    m_raygenRegion.deviceAddress = baseAligned + VkDeviceSize(raygenGroupIndex) * m_regionStride;
    m_raygenRegion.stride = m_regionStride;
    m_raygenRegion.size = m_regionStride;

    m_missRegion = {};
    if (!missStages.isEmpty()) {
        m_missRegion.deviceAddress = baseAligned + VkDeviceSize(missGroupBase) * m_regionStride;
        m_missRegion.stride = m_regionStride;
        m_missRegion.size = VkDeviceSize(missStages.size()) * m_regionStride;
    }

    const uint32_t realHitGroupCount = uint32_t(groups.size()) - hitGroupBase - uint32_t(callableStages.size());
    m_hitRegion = {};
    if (realHitGroupCount > 0) {
        m_hitRegion.deviceAddress = baseAligned + VkDeviceSize(hitGroupBase) * m_regionStride;
        m_hitRegion.stride = m_regionStride;
        m_hitRegion.size = VkDeviceSize(realHitGroupCount) * m_regionStride;
    }

    m_callableRegion = {};
    if (!callableStages.isEmpty()) {
        m_callableRegion.deviceAddress = baseAligned + VkDeviceSize(callableGroupBase) * m_regionStride;
        m_callableRegion.stride = m_regionStride;
        m_callableRegion.size = VkDeviceSize(callableStages.size()) * m_regionStride;
    }

    return true;
}

bool RtPreviewEngine::rebuild(CustomVulkanDevice *dev, const QVector<RayTraceStageBinary> &stages, QSize size,
                              VkFormat surfaceFormat, QString *errorOut,
                              quint32 maxPipelineRayRecursionDepthOverride)
{
    destroyPipelineResources();
    m_maxPipelineRayRecursionDepthOverride = maxPipelineRayRecursionDepthOverride;

    if (!dev || !dev->isValid() || stages.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Invalid device or empty ray-tracing stages.");
        return false;
    }

    m_dev = dev;
    m_df = dev->deviceFunctions();
    m_device = dev->device();
    if (!m_df || m_device == VK_NULL_HANDLE) {
        if (errorOut)
            *errorOut = QStringLiteral("No device functions.");
        return false;
    }

    if (!loadRayTracingProcs(dev->qVulkanInstance(), m_device)) {
        if (errorOut)
            *errorOut = QStringLiteral("Ray tracing device functions not available.");
        return false;
    }
    if (!loadAccelerationStructureProcs(dev->qVulkanInstance(), m_device)) {
        if (errorOut)
            *errorOut = QStringLiteral("Acceleration structure device functions not available.");
        return false;
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtp{};
    rtp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtp;
    dev->qVulkanInstance()->functions()->vkGetPhysicalDeviceProperties2(dev->physicalDevice(), &props2);
    m_rtProps = rtp;

    m_extent.width = uint32_t(std::max(1, size.width()));
    m_extent.height = uint32_t(std::max(1, size.height()));
    m_storageFormat = surfaceFormat;

    if (!createStorageImage(surfaceFormat, m_extent, errorOut)) {
        destroyPipelineResources();
        return false;
    }

    if (!createHostUniformBuffer(errorOut)) {
        destroyPipelineResources();
        return false;
    }

    if (!buildAccelerationStructures(errorOut)) {
        destroyPipelineResources();
        return false;
    }

    if (!createRayTracingPipelineAndSbt(stages, errorOut)) {
        destroyPipelineResources();
        return false;
    }

    return true;
}

void RtPreviewEngine::setHostUniforms(const HostUniforms &host)
{
    if (m_hostUniformMapped)
        std::memcpy(m_hostUniformMapped, &host, sizeof(HostUniforms));
}

void RtPreviewEngine::recordFrame(VkCommandBuffer cmd, VkImage swapchainImage, VkImageLayout swapchainLayoutBefore,
                                  VkExtent2D swapExtent)
{
    if (!m_rtPipeline || !m_df)
        return;

    VkImageMemoryBarrier storageBarrier{};
    storageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    storageBarrier.srcAccessMask = 0;
    storageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    storageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    storageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageBarrier.image = m_storageImage;
    storageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    storageBarrier.subresourceRange.levelCount = 1;
    storageBarrier.subresourceRange.layerCount = 1;

    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1,
                               &storageBarrier);

    VkImageMemoryBarrier swapBarrier{};
    swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapBarrier.srcAccessMask = 0;
    swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapBarrier.oldLayout = swapchainLayoutBefore;
    swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapBarrier.image = swapchainImage;
    swapBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapBarrier.subresourceRange.levelCount = 1;
    swapBarrier.subresourceRange.layerCount = 1;

    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                               0, nullptr, 1, &swapBarrier);

    m_df->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline);
    m_df->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipelineLayout, 0, 1, &m_descSet, 0,
                                  nullptr);

    m_pfnCmdTraceRaysKHR(cmd, &m_raygenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, m_extent.width,
                         m_extent.height, 1);

    VkImageMemoryBarrier afterTrace{};
    afterTrace.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    afterTrace.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    afterTrace.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    afterTrace.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    afterTrace.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    afterTrace.image = m_storageImage;
    afterTrace.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    afterTrace.subresourceRange.levelCount = 1;
    afterTrace.subresourceRange.layerCount = 1;

    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                               0, nullptr, 0, nullptr, 1, &afterTrace);

    VkImageBlit blit{};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {int32_t(m_extent.width), int32_t(m_extent.height), 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = 0;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {int32_t(swapExtent.width), int32_t(swapExtent.height), 1};

    m_df->vkCmdBlitImage(cmd, m_storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImage,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    VkImageMemoryBarrier present{};
    present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    present.dstAccessMask = 0;
    present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    present.image = swapchainImage;
    present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    present.subresourceRange.levelCount = 1;
    present.subresourceRange.layerCount = 1;

    m_df->vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                               nullptr, 0, nullptr, 1, &present);
}
