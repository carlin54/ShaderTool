#include "customvulkandevice.h"

#include <QWindow>

#include <cstring>

CustomVulkanDevice::~CustomVulkanDevice()
{
    destroy();
}

void CustomVulkanDevice::destroy()
{
    if (m_device != VK_NULL_HANDLE && m_qInst) {
        if (m_df)
            m_df->vkDestroyDevice(m_device, nullptr);
        else {
            auto vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
                m_qInst->getInstanceProcAddr("vkDestroyDevice"));
            if (vkDestroyDevice)
                vkDestroyDevice(m_device, nullptr);
        }
        m_qInst->resetDeviceFunctions(m_device);
        m_df = nullptr;
        m_device = VK_NULL_HANDLE;
    }
    if (m_surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        auto destroySurf = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
            m_qInst->getInstanceProcAddr("vkDestroySurfaceKHR"));
        if (destroySurf)
            destroySurf(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    m_physicalDevice = VK_NULL_HANDLE;
    m_instance = VK_NULL_HANDLE;
    m_qInst = nullptr;
}

static bool extensionSupported(const QVector<VkExtensionProperties> &props, const char *name)
{
    for (const VkExtensionProperties &p : props) {
        if (std::strcmp(p.extensionName, name) == 0)
            return true;
    }
    return false;
}

bool CustomVulkanDevice::create(QVulkanInstance *qInst, QWindow *surfaceWindow, QString *errorOut,
                                const QString &preferredGpuName)
{
    destroy();
    m_qInst = qInst;
    m_instance = qInst->vkInstance();

    m_surface = QVulkanInstance::surfaceForWindow(surfaceWindow);
    if (m_surface == VK_NULL_HANDLE) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not create Vulkan surface for window.");
        return false;
    }

    auto vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        qInst->getInstanceProcAddr("vkEnumeratePhysicalDevices"));
    auto vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<
        PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        qInst->getInstanceProcAddr("vkGetPhysicalDeviceQueueFamilyProperties"));
    auto vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        qInst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceSupportKHR"));
    auto vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
        qInst->getInstanceProcAddr("vkEnumerateDeviceExtensionProperties"));
    auto vkGetPhysicalDeviceFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
        qInst->getInstanceProcAddr("vkGetPhysicalDeviceFeatures2"));
    auto vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        qInst->getInstanceProcAddr("vkGetPhysicalDeviceMemoryProperties"));
    auto vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        qInst->getInstanceProcAddr("vkGetPhysicalDeviceProperties"));

    if (!vkEnumeratePhysicalDevices || !vkGetPhysicalDeviceQueueFamilyProperties
        || !vkGetPhysicalDeviceSurfaceSupportKHR || !vkEnumerateDeviceExtensionProperties
        || !vkGetPhysicalDeviceFeatures2 || !vkGetPhysicalDeviceMemoryProperties || !vkGetPhysicalDeviceProperties) {
        if (errorOut)
            *errorOut = QStringLiteral("Missing Vulkan instance functions.");
        return false;
    }

    uint32_t pdCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &pdCount, nullptr);
    QVector<VkPhysicalDevice> devices;
    devices.resize(int(pdCount));
    vkEnumeratePhysicalDevices(m_instance, &pdCount, devices.data());

    const char *requiredDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    };

    bool foundDevice = false;
    uint32_t chosenFamily = 0;

    const QString wantedGpu = preferredGpuName.trimmed();
    for (VkPhysicalDevice pd : devices) {
        if (!wantedGpu.isEmpty()) {
            VkPhysicalDeviceProperties pp{};
            vkGetPhysicalDeviceProperties(pd, &pp);
            const QString devName = QString::fromUtf8(pp.deviceName);
            if (!devName.contains(wantedGpu, Qt::CaseInsensitive))
                continue;
        }
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
        QVector<VkQueueFamilyProperties> qProps;
        qProps.resize(int(qCount));
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr);
        QVector<VkExtensionProperties> extProps;
        extProps.resize(int(extCount));
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, extProps.data());

        bool extOk = true;
        for (const char *req : requiredDeviceExtensions) {
            if (!extensionSupported(extProps, req)) {
                extOk = false;
                break;
            }
        }
        if (!extOk)
            continue;

        for (uint32_t i = 0; i < qCount; ++i) {
            if (!(qProps[int(i)].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                continue;
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, m_surface, &present);
            if (!present || !qInst->supportsPresent(pd, i, surfaceWindow))
                continue;

            VkPhysicalDeviceVulkan12Features vk12{};
            vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{};
            rt.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            VkPhysicalDeviceAccelerationStructureFeaturesKHR as{};
            as.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &vk12;
            vk12.pNext = &rt;
            rt.pNext = &as;

            vkGetPhysicalDeviceFeatures2(pd, &features2);
            if (!vk12.bufferDeviceAddress || !rt.rayTracingPipeline || !as.accelerationStructure)
                continue;

            chosenFamily = i;
            m_physicalDevice = pd;
            foundDevice = true;
            break;
        }
        if (foundDevice)
            break;
    }

    if (!foundDevice) {
        if (errorOut)
            *errorOut = QStringLiteral("%1%2")
                            .arg(wantedGpu.isEmpty()
                                     ? QString()
                                     : QStringLiteral("No compatible Vulkan RT device matched \"%1\". ").arg(wantedGpu))
                            .arg(QStringLiteral(
                "No Vulkan physical device on this system supports GPU ray-tracing preview. "
                "Required: queue with graphics+present, extensions "
                "VK_KHR_swapchain, VK_KHR_deferred_host_operations, VK_KHR_acceleration_structure, "
                "VK_KHR_ray_tracing_pipeline, VK_KHR_spirv_1_4, VK_KHR_buffer_device_address, "
                "and device features: Vulkan 1.2 bufferDeviceAddress, rayTracingPipeline, accelerationStructure. "
                "Update the GPU driver or use a discrete GPU with up-to-date Vulkan RT support."));
        return false;
    }

    m_gfxQueueFamily = chosenFamily;

    VkPhysicalDeviceVulkan12Features vk12{};
    vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12.bufferDeviceAddress = VK_TRUE;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{};
    rt.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rt.rayTracingPipeline = VK_TRUE;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR as{};
    as.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    as.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &vk12;
    vk12.pNext = &rt;
    rt.pNext = &as;

    float priority = 1.f;
    VkDeviceQueueCreateInfo dqci{};
    dqci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    dqci.queueFamilyIndex = m_gfxQueueFamily;
    dqci.queueCount = 1;
    dqci.pQueuePriorities = &priority;

    QVector<const char *> extNames;
    for (const char *e : requiredDeviceExtensions)
        extNames.append(e);

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &dqci;
    dci.enabledExtensionCount = uint32_t(extNames.size());
    dci.ppEnabledExtensionNames = extNames.constData();
    dci.pNext = &features2;

    auto vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(qInst->getInstanceProcAddr("vkCreateDevice"));
    if (!vkCreateDevice) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateDevice not found.");
        return false;
    }

    VkResult err = vkCreateDevice(m_physicalDevice, &dci, nullptr, &m_device);
    if (err != VK_SUCCESS) {
        if (errorOut)
            *errorOut = QStringLiteral("vkCreateDevice failed: %1").arg(int(err));
        return false;
    }

    m_df = qInst->deviceFunctions(m_device);
    if (!m_df) {
        auto vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(qInst->getInstanceProcAddr("vkDestroyDevice"));
        if (vkDestroyDevice)
            vkDestroyDevice(m_device, nullptr);
        qInst->resetDeviceFunctions(m_device);
        m_device = VK_NULL_HANDLE;
        if (errorOut)
            *errorOut = QStringLiteral("QVulkanDeviceFunctions for custom device failed.");
        return false;
    }

    m_df->vkGetDeviceQueue(m_device, m_gfxQueueFamily, 0, &m_graphicsQueue);

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    m_hostVisibleMemIndex = 0;
    m_deviceLocalMemIndex = 0;
    bool foundHost = false, foundLocal = false;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const VkMemoryType &t = memProps.memoryTypes[i];
        if ((t.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            && (t.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            m_hostVisibleMemIndex = i;
            foundHost = true;
        }
        if (t.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            m_deviceLocalMemIndex = i;
            foundLocal = true;
        }
    }
    if (!foundHost)
        m_hostVisibleMemIndex = 0;
    if (!foundLocal)
        m_deviceLocalMemIndex = 0;

    return true;
}
