#ifndef CUSTOMVULKANDEVICE_H
#define CUSTOMVULKANDEVICE_H

#include <QVulkanInstance>
#include <QVulkanDeviceFunctions>

#include <vulkan/vulkan.h>

#include <QString>

// Creates a VkDevice with ray tracing + buffer device address features (when supported).
class CustomVulkanDevice
{
public:
    CustomVulkanDevice() = default;
    ~CustomVulkanDevice();

    bool create(QVulkanInstance *qInst, QWindow *surfaceWindow, QString *errorOut,
                const QString &preferredGpuName = QString());
    void destroy();

    bool isValid() const { return m_device != VK_NULL_HANDLE; }

    VkInstance instance() const { return m_instance; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice device() const { return m_device; }
    VkSurfaceKHR surface() const { return m_surface; }
    uint32_t graphicsQueueFamilyIndex() const { return m_gfxQueueFamily; }
    VkQueue graphicsQueue() const { return m_graphicsQueue; }

    QVulkanDeviceFunctions *deviceFunctions() const { return m_df; }
    QVulkanInstance *qVulkanInstance() const { return m_qInst; }

    uint32_t hostVisibleMemoryTypeIndex() const { return m_hostVisibleMemIndex; }
    uint32_t deviceLocalMemoryTypeIndex() const { return m_deviceLocalMemIndex; }

private:
    QVulkanInstance *m_qInst = nullptr;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    uint32_t m_gfxQueueFamily = 0;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *m_df = nullptr;

    uint32_t m_hostVisibleMemIndex = 0;
    uint32_t m_deviceLocalMemIndex = 0;
};

#endif
