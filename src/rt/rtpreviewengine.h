#ifndef RTPREVIEWENGINE_H
#define RTPREVIEWENGINE_H

#include "customvulkandevice.h"
#include "hostuniforms.h"
#include "raytracestages.h"

#include <QSize>
#include <QString>
#include <QVector>

#include <vulkan/vulkan.h>

// Ray-tracing preview: RT pipeline + storage image + SBT; records trace + blit to swapchain image.
class QVulkanInstance;

class RtPreviewEngine
{
public:
    RtPreviewEngine() = default;
    ~RtPreviewEngine();

    void destroy();

    bool rebuild(CustomVulkanDevice *dev, const QVector<RayTraceStageBinary> &stages, QSize size,
                 VkFormat surfaceFormat, QString *errorOut,
                 quint32 maxPipelineRayRecursionDepthOverride = 0);

    bool isValid() const { return m_rtPipeline != VK_NULL_HANDLE; }

    void setHostUniforms(const HostUniforms &host);

    void recordFrame(VkCommandBuffer cmd, VkImage swapchainImage, VkImageLayout swapchainLayoutBefore,
                     VkExtent2D swapExtent);

private:
    bool loadRayTracingProcs(QVulkanInstance *qInst, VkDevice dev);
    void destroyPipelineResources();
    bool createStorageImage(VkFormat fmt, VkExtent2D ext, QString *errorOut);
    bool createHostUniformBuffer(QString *errorOut);
    bool loadAccelerationStructureProcs(QVulkanInstance *qInst, VkDevice dev);
    void destroyAccelerationStructures();
    bool buildAccelerationStructures(QString *errorOut);
    bool createRayTracingPipelineAndSbt(const QVector<RayTraceStageBinary> &stages, QString *errorOut);

    CustomVulkanDevice *m_dev = nullptr;
    QVulkanDeviceFunctions *m_df = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;

    PFN_vkCreateRayTracingPipelinesKHR m_pfnCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR m_pfnGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCmdTraceRaysKHR m_pfnCmdTraceRaysKHR = nullptr;

    PFN_vkCreateAccelerationStructureKHR m_pfnCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR m_pfnDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR m_pfnGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR m_pfnCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR m_pfnGetAccelerationStructureDeviceAddressKHR = nullptr;

    VkAccelerationStructureKHR m_blas = VK_NULL_HANDLE;
    VkAccelerationStructureKHR m_tlas = VK_NULL_HANDLE;
    VkBuffer m_blasStorageBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_blasStorageMem = VK_NULL_HANDLE;
    VkBuffer m_tlasStorageBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_tlasStorageMem = VK_NULL_HANDLE;
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkBuffer m_scratchBlas = VK_NULL_HANDLE;
    VkBuffer m_scratchTlas = VK_NULL_HANDLE;
    VkBuffer m_instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexMem = VK_NULL_HANDLE;
    VkDeviceMemory m_indexMem = VK_NULL_HANDLE;
    VkDeviceMemory m_scratchBlasMem = VK_NULL_HANDLE;
    VkDeviceMemory m_scratchTlasMem = VK_NULL_HANDLE;
    VkDeviceMemory m_instanceMem = VK_NULL_HANDLE;
    VkDeviceAddress m_blasDeviceAddress = 0;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProps{};

    VkExtent2D m_extent{};
    VkFormat m_storageFormat = VK_FORMAT_UNDEFINED;

    VkDescriptorSetLayout m_descLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descSet = VK_NULL_HANDLE;

    VkBuffer m_hostUniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_hostUniformMem = VK_NULL_HANDLE;
    void *m_hostUniformMapped = nullptr;

    VkPipeline m_rtPipeline = VK_NULL_HANDLE;
    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;

    VkShaderModule m_modRaygen = VK_NULL_HANDLE;
    VkShaderModule m_modMiss = VK_NULL_HANDLE;
    VkShaderModule m_modClosest = VK_NULL_HANDLE;

    VkImage m_storageImage = VK_NULL_HANDLE;
    VkDeviceMemory m_storageMem = VK_NULL_HANDLE;
    VkImageView m_storageView = VK_NULL_HANDLE;

    VkBuffer m_sbtBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_sbtMem = VK_NULL_HANDLE;
    VkDeviceSize m_sbtSize = 0;

    uint32_t m_handleSize = 0;
    uint32_t m_handleAlign = 0;
    uint32_t m_regionStride = 0;

    VkStridedDeviceAddressRegionKHR m_raygenRegion{};
    VkStridedDeviceAddressRegionKHR m_missRegion{};
    VkStridedDeviceAddressRegionKHR m_hitRegion{};
    VkStridedDeviceAddressRegionKHR m_callableRegion{};

    quint32 m_maxPipelineRayRecursionDepthOverride = 0;
};

#endif
