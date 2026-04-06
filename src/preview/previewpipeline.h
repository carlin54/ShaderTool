#ifndef PREVIEWPIPELINE_H
#define PREVIEWPIPELINE_H

#include <QVulkanWindow>
#include <QVulkanDeviceFunctions>
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QImage>
#include <vulkan/vulkan.h>

#include "hostuniforms.h"
#include "textureupload.h"

// One compiled stage for the raster graphics pipeline (order is sorted internally).
struct RasterStageBinary {
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
    QByteArray spirv;
    QString entry;
};

// Fullscreen triangle + fragment UBO (Host catalog). Supports VS + optional GS + FS.
// Optional: descriptor set 1 = combined image samplers (fragment), optional mesh vertex input.
class RasterPreviewPipeline
{
public:
    RasterPreviewPipeline() = default;
    ~RasterPreviewPipeline();

    void destroy(QVulkanDeviceFunctions *df, VkDevice dev);
    VkResult create(QVulkanWindow *window,
                    QVulkanDeviceFunctions *df,
                    const QVector<RasterStageBinary> &stagesIn,
                    const QVector<QImage> &textureImages = {},
                    bool useMeshVertexInput = false);

    VkPipeline pipeline() const { return m_pipeline; }
    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
    VkBuffer uniformBuffer() const { return m_buf; }
    VkDeviceMemory uniformBufferMemory() const { return m_bufMem; }
    const VkDescriptorBufferInfo *uniformBufferInfo() const { return m_uniformBufInfo; }

    int descriptorSetCount() const { return m_hasTextures ? 2 : 1; }
    VkDescriptorSet descriptorSetUniform(int frame) const { return m_descSetUniform[frame]; }
    VkDescriptorSet descriptorSetTextures(int frame) const
    {
        return m_hasTextures ? m_descSetTextures[frame] : VK_NULL_HANDLE;
    }

    bool hasTextures() const { return m_hasTextures; }
    bool useMeshVertexInput() const { return m_useMeshVertexInput; }

private:
    VkDeviceMemory m_bufMem = VK_NULL_HANDLE;
    VkBuffer m_buf = VK_NULL_HANDLE;
    VkDescriptorBufferInfo m_uniformBufInfo[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT]{};

    VkDescriptorPool m_descPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descLayoutUniform = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descLayoutTextures = VK_NULL_HANDLE;
    VkDescriptorSet m_descSetUniform[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT]{};
    VkDescriptorSet m_descSetTextures[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT]{};

    VkSampler m_sampler = VK_NULL_HANDLE;
    QVector<TextureUpload::GpuTexture2D> m_gpuTextures;
    bool m_hasTextures = false;
    bool m_useMeshVertexInput = false;

    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    QVector<VkShaderModule> m_shaderModules;

    static VkShaderModule createShaderModule(QVulkanDeviceFunctions *df, VkDevice dev, const QByteArray &spirv);
    static VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize byteAlign);
    static int stageSortOrder(VkShaderStageFlagBits s);
};

#endif
