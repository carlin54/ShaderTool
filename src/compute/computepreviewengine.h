#ifndef COMPUTEPREVIEWENGINE_H
#define COMPUTEPREVIEWENGINE_H

#include "customvulkandevice.h"
#include "hostuniforms.h"

#include <QByteArray>
#include <QSize>
#include <QString>

#include <vulkan/vulkan.h>

struct ComputeStageBinary {
    QByteArray spirv;
    QString entry;
};

class ComputePreviewEngine
{
public:
    ComputePreviewEngine() = default;
    ~ComputePreviewEngine();

    void destroy();

    bool rebuild(CustomVulkanDevice *dev, const ComputeStageBinary &stage, QSize size,
                 VkFormat surfaceFormat, QString *errorOut);

    bool isValid() const { return m_pipeline != VK_NULL_HANDLE; }

    void setHostUniforms(const HostUniforms &host);

    void recordFrame(VkCommandBuffer cmd, VkImage swapchainImage, VkImageLayout swapchainLayoutBefore,
                     VkExtent2D swapExtent);

private:
    bool createStorageImage(VkFormat fmt, VkExtent2D ext, QString *errorOut);
    bool createHostUniformBuffer(QString *errorOut);
    bool createComputePipeline(const ComputeStageBinary &stage, QString *errorOut);

    CustomVulkanDevice *m_dev = nullptr;
    QVulkanDeviceFunctions *m_df = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;

    VkExtent2D m_extent{};
    VkFormat m_storageFormat = VK_FORMAT_UNDEFINED;

    VkDescriptorSetLayout m_descLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descSet = VK_NULL_HANDLE;

    VkBuffer m_hostUniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_hostUniformMem = VK_NULL_HANDLE;
    void *m_hostUniformMapped = nullptr;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;

    VkShaderModule m_shaderModule = VK_NULL_HANDLE;

    VkImage m_storageImage = VK_NULL_HANDLE;
    VkDeviceMemory m_storageMem = VK_NULL_HANDLE;
    VkImageView m_storageView = VK_NULL_HANDLE;
    VkImageLayout m_storageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

#endif
