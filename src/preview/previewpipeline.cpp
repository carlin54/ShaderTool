#include "previewpipeline.h"

#include <QDebug>
#include <algorithm>
#include <cstring>

VkDeviceSize RasterPreviewPipeline::aligned(VkDeviceSize v, VkDeviceSize byteAlign)
{
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

int RasterPreviewPipeline::stageSortOrder(VkShaderStageFlagBits s)
{
    switch (s) {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return 0;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return 1;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return 2;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        return 3;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return 4;
    default:
        return 99;
    }
}

VkShaderModule RasterPreviewPipeline::createShaderModule(QVulkanDeviceFunctions *df, VkDevice dev,
                                                         const QByteArray &spirv)
{
    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = spirv.size();
    shaderInfo.pCode = reinterpret_cast<const uint32_t *>(spirv.constData());
    VkShaderModule mod = VK_NULL_HANDLE;
    VkResult err = df->vkCreateShaderModule(dev, &shaderInfo, nullptr, &mod);
    if (err != VK_SUCCESS) {
        qWarning("vkCreateShaderModule failed: %d", int(err));
        return VK_NULL_HANDLE;
    }
    return mod;
}

RasterPreviewPipeline::~RasterPreviewPipeline() = default;

void RasterPreviewPipeline::destroy(QVulkanDeviceFunctions *df, VkDevice dev)
{
    if (!df || dev == VK_NULL_HANDLE)
        return;

    if (m_pipeline) {
        df->vkDestroyPipeline(dev, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineCache) {
        df->vkDestroyPipelineCache(dev, m_pipelineCache, nullptr);
        m_pipelineCache = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout) {
        df->vkDestroyPipelineLayout(dev, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_descPool) {
        df->vkDestroyDescriptorPool(dev, m_descPool, nullptr);
        m_descPool = VK_NULL_HANDLE;
    }
    if (m_descLayoutUniform) {
        df->vkDestroyDescriptorSetLayout(dev, m_descLayoutUniform, nullptr);
        m_descLayoutUniform = VK_NULL_HANDLE;
    }
    if (m_descLayoutTextures) {
        df->vkDestroyDescriptorSetLayout(dev, m_descLayoutTextures, nullptr);
        m_descLayoutTextures = VK_NULL_HANDLE;
    }
    if (m_sampler) {
        df->vkDestroySampler(dev, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    for (TextureUpload::GpuTexture2D &t : m_gpuTextures)
        TextureUpload::destroy(df, dev, t);
    m_gpuTextures.clear();

    if (m_buf) {
        df->vkDestroyBuffer(dev, m_buf, nullptr);
        m_buf = VK_NULL_HANDLE;
    }
    if (m_bufMem) {
        df->vkFreeMemory(dev, m_bufMem, nullptr);
        m_bufMem = VK_NULL_HANDLE;
    }
    for (VkShaderModule m : m_shaderModules) {
        if (m)
            df->vkDestroyShaderModule(dev, m, nullptr);
    }
    m_shaderModules.clear();
    m_hasTextures = false;
    m_useMeshVertexInput = false;
}

VkResult RasterPreviewPipeline::create(QVulkanWindow *window, QVulkanDeviceFunctions *df,
                                       const QVector<RasterStageBinary> &stagesIn,
                                       const QVector<QImage> &textureImages,
                                       bool useMeshVertexInput)
{
    VkDevice dev = window->device();
    destroy(df, dev);

    if (stagesIn.isEmpty())
        return VK_ERROR_INITIALIZATION_FAILED;

    m_useMeshVertexInput = useMeshVertexInput;
    m_hasTextures = !textureImages.isEmpty();

    QVector<RasterStageBinary> stages = stagesIn;
    std::sort(stages.begin(), stages.end(), [](const RasterStageBinary &a, const RasterStageBinary &b) {
        return stageSortOrder(a.stage) < stageSortOrder(b.stage);
    });

    QVector<QByteArray> entryNames;
    entryNames.reserve(stages.size());

    for (const RasterStageBinary &s : stages) {
        VkShaderModule mod = createShaderModule(df, dev, s.spirv);
        if (!mod)
            return VK_ERROR_INITIALIZATION_FAILED;
        m_shaderModules.append(mod);
        entryNames.append(s.entry.toUtf8());
    }

    QVector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.reserve(stages.size());
    for (int i = 0; i < stages.size(); ++i) {
        VkPipelineShaderStageCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        si.stage = stages[i].stage;
        si.module = m_shaderModules[i];
        si.pName = entryNames[i].constData();
        shaderStages.append(si);
    }

    const int concurrentFrameCount = window->concurrentFrameCount();
    const VkPhysicalDeviceLimits *pdevLimits = &window->physicalDeviceProperties()->limits;
    const VkDeviceSize uniAlign = pdevLimits->minUniformBufferOffsetAlignment;
    const VkDeviceSize uniformAllocSize = aligned(static_cast<VkDeviceSize>(kHostUniformsSize), uniAlign);
    if (uniformAllocSize < uniAlign)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = uniformAllocSize * VkDeviceSize(concurrentFrameCount);
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VkResult err = df->vkCreateBuffer(dev, &bufInfo, nullptr, &m_buf);
    if (err != VK_SUCCESS)
        return err;

    VkMemoryRequirements memReq{};
    df->vkGetBufferMemoryRequirements(dev, m_buf, &memReq);

    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize = memReq.size;
    memAllocInfo.memoryTypeIndex = window->hostVisibleMemoryIndex();

    err = df->vkAllocateMemory(dev, &memAllocInfo, nullptr, &m_bufMem);
    if (err != VK_SUCCESS)
        return err;

    err = df->vkBindBufferMemory(dev, m_buf, m_bufMem, 0);
    if (err != VK_SUCCESS)
        return err;

    HostUniforms initial{};
    initial.resolution_x = 800.f;
    initial.resolution_y = 600.f;

    quint8 *p = nullptr;
    err = df->vkMapMemory(dev, m_bufMem, 0, memReq.size, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        return err;
    for (int i = 0; i < concurrentFrameCount; ++i) {
        const VkDeviceSize offset = VkDeviceSize(i) * uniformAllocSize;
        std::memcpy(p + offset, &initial, sizeof(HostUniforms));
        m_uniformBufInfo[i].buffer = m_buf;
        m_uniformBufInfo[i].offset = offset;
        m_uniformBufInfo[i].range = sizeof(HostUniforms);
    }
    df->vkUnmapMemory(dev, m_bufMem);

    VkDescriptorSetLayoutBinding layoutBindingUbo{};
    layoutBindingUbo.binding = 0;
    layoutBindingUbo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindingUbo.descriptorCount = 1;
    layoutBindingUbo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        | VK_SHADER_STAGE_GEOMETRY_BIT;

    VkDescriptorSetLayoutCreateInfo descLayoutUniformInfo{};
    descLayoutUniformInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutUniformInfo.bindingCount = 1;
    descLayoutUniformInfo.pBindings = &layoutBindingUbo;
    err = df->vkCreateDescriptorSetLayout(dev, &descLayoutUniformInfo, nullptr, &m_descLayoutUniform);
    if (err != VK_SUCCESS)
        return err;

    const uint32_t texCount = uint32_t(textureImages.size());
    QVector<VkDescriptorSetLayoutBinding> texBindings;
    if (m_hasTextures) {
        texBindings.resize(int(texCount));
        for (uint32_t i = 0; i < texCount; ++i) {
            texBindings[int(i)].binding = i;
            texBindings[int(i)].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            texBindings[int(i)].descriptorCount = 1;
            texBindings[int(i)].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo descLayoutTexInfo{};
        descLayoutTexInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descLayoutTexInfo.bindingCount = texCount;
        descLayoutTexInfo.pBindings = texBindings.constData();
        err = df->vkCreateDescriptorSetLayout(dev, &descLayoutTexInfo, nullptr, &m_descLayoutTextures);
        if (err != VK_SUCCESS)
            return err;

        VkSamplerCreateInfo sampInfo{};
        sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampInfo.magFilter = VK_FILTER_LINEAR;
        sampInfo.minFilter = VK_FILTER_LINEAR;
        sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        err = df->vkCreateSampler(dev, &sampInfo, nullptr, &m_sampler);
        if (err != VK_SUCCESS)
            return err;

        m_gpuTextures.resize(int(texCount));
        for (uint32_t i = 0; i < texCount; ++i) {
            err = TextureUpload::createFromImage(window, df, textureImages[int(i)], &m_gpuTextures[int(i)]);
            if (err != VK_SUCCESS)
                return err;
        }
    }

    VkDescriptorPoolSize poolSizes[2];
    int poolSizeCount = 1;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = uint32_t(concurrentFrameCount);
    if (m_hasTextures) {
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = concurrentFrameCount * texCount;
        poolSizeCount = 2;
    }

    VkDescriptorPoolCreateInfo descPoolInfo{};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.maxSets = uint32_t(concurrentFrameCount);
    if (m_hasTextures)
        descPoolInfo.maxSets = uint32_t(concurrentFrameCount * 2);
    descPoolInfo.poolSizeCount = uint32_t(poolSizeCount);
    descPoolInfo.pPoolSizes = poolSizes;
    err = df->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, &m_descPool);
    if (err != VK_SUCCESS)
        return err;

    for (int i = 0; i < concurrentFrameCount; ++i) {
        VkDescriptorSetAllocateInfo descSetAllocInfo{};
        descSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descSetAllocInfo.descriptorPool = m_descPool;
        descSetAllocInfo.descriptorSetCount = 1;
        descSetAllocInfo.pSetLayouts = &m_descLayoutUniform;
        err = df->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_descSetUniform[i]);
        if (err != VK_SUCCESS)
            return err;

        VkWriteDescriptorSet descWrite{};
        descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrite.dstSet = m_descSetUniform[i];
        descWrite.dstBinding = 0;
        descWrite.descriptorCount = 1;
        descWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descWrite.pBufferInfo = &m_uniformBufInfo[i];
        df->vkUpdateDescriptorSets(dev, 1, &descWrite, 0, nullptr);
    }

    if (m_hasTextures) {
        QVector<VkDescriptorImageInfo> imageInfos;
        imageInfos.resize(int(texCount));
        for (uint32_t i = 0; i < texCount; ++i) {
            imageInfos[int(i)].sampler = m_sampler;
            imageInfos[int(i)].imageView = m_gpuTextures[int(i)].view;
            imageInfos[int(i)].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        for (int i = 0; i < concurrentFrameCount; ++i) {
            VkDescriptorSetAllocateInfo descSetAllocInfo{};
            descSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descSetAllocInfo.descriptorPool = m_descPool;
            descSetAllocInfo.descriptorSetCount = 1;
            descSetAllocInfo.pSetLayouts = &m_descLayoutTextures;
            err = df->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_descSetTextures[i]);
            if (err != VK_SUCCESS)
                return err;

            QVector<VkWriteDescriptorSet> writes;
            writes.resize(int(texCount));
            for (uint32_t b = 0; b < texCount; ++b) {
                writes[int(b)].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[int(b)].dstSet = m_descSetTextures[i];
                writes[int(b)].dstBinding = b;
                writes[int(b)].descriptorCount = 1;
                writes[int(b)].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[int(b)].pImageInfo = &imageInfos[int(b)];
            }
            df->vkUpdateDescriptorSets(dev, texCount, writes.data(), 0, nullptr);
        }
    }

    VkPipelineCacheCreateInfo pipelineCacheInfo{};
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    err = df->vkCreatePipelineCache(dev, &pipelineCacheInfo, nullptr, &m_pipelineCache);
    if (err != VK_SUCCESS)
        return err;

    VkDescriptorSetLayout layouts[2] = {m_descLayoutUniform, m_descLayoutTextures};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (m_hasTextures) {
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;
    } else {
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descLayoutUniform;
    }
    err = df->vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    if (err != VK_SUCCESS)
        return err;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription bind{};
    VkVertexInputAttributeDescription attrs[3];

    if (m_useMeshVertexInput) {
        bind.binding = 0;
        bind.stride = 32;
        bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
        attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12};
        attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, 24};
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bind;
        vertexInputInfo.vertexAttributeDescriptionCount = 3;
        vertexInputInfo.pVertexAttributeDescriptions = attrs;
    }

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = window->sampleCountFlagBits();

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState att{};
    att.colorWriteMask = 0xF;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &att;

    VkDynamicState dynEnable[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = sizeof(dynEnable) / sizeof(VkDynamicState);
    dyn.pDynamicStates = dynEnable;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = uint32_t(shaderStages.size());
    pipelineInfo.pStages = shaderStages.constData();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &ia;
    pipelineInfo.pViewportState = &vp;
    pipelineInfo.pRasterizationState = &rs;
    pipelineInfo.pMultisampleState = &ms;
    pipelineInfo.pDepthStencilState = &ds;
    pipelineInfo.pColorBlendState = &cb;
    pipelineInfo.pDynamicState = &dyn;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = window->defaultRenderPass();

    err = df->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_pipeline);
    return err;
}
