#include "computepreviewwindow.h"

#include <QEvent>
#include <QExposeEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QVulkanFunctions>
#include <QVulkanInstance>
#include <QWheelEvent>

#include <QDebug>

#include <algorithm>
#include <cstring>

ComputePreviewWindow::ComputePreviewWindow(QVulkanInstance *inst, QWindow *parent)
    : QWindow(parent)
    , m_qInst(inst)
{
    setSurfaceType(QWindow::VulkanSurface);
    setFlags(Qt::Window);
    m_simTime.start();
    m_wallTime.start();
    m_lastFrameTimer.start();
    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, &ComputePreviewWindow::onFrameTick);
}

ComputePreviewWindow::~ComputePreviewWindow()
{
    m_timer->stop();
    releaseGpu();
}

bool ComputePreviewWindow::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent *>(e);
        const QPointF pos = me->position();
        const QSize sz = size();
        if (pos.x() >= 0 && pos.y() >= 0 && pos.x() < sz.width() && pos.y() < sz.height()) {
            m_mouseInside = true;
            m_lastMouseInside = pos;
        } else {
            m_mouseInside = false;
        }
        m_buttons = 0;
        if (me->buttons() & Qt::LeftButton)
            m_buttons |= 1u;
        if (me->buttons() & Qt::RightButton)
            m_buttons |= 2u;
        if (me->buttons() & Qt::MiddleButton)
            m_buttons |= 4u;
        break;
    }
    case QEvent::Wheel: {
        auto *we = static_cast<QWheelEvent *>(e);
        const QPoint ad = we->angleDelta();
        m_scrollX += ad.x() / 120.f;
        m_scrollY += ad.y() / 120.f;
        break;
    }
    default:
        break;
    }
    return QWindow::event(e);
}

void ComputePreviewWindow::updateHostUniformsForFrame()
{
    if (m_swapExtent.width > 0 && m_swapExtent.height > 0) {
        m_host.resolution_x = float(m_swapExtent.width);
        m_host.resolution_y = float(m_swapExtent.height);
    } else {
        const qreal dpr = devicePixelRatio();
        m_host.resolution_x = float(width() * dpr);
        m_host.resolution_y = float(height() * dpr);
    }

    m_host.time_sim_ms = float(m_simTime.elapsed());
    m_host.time_wall_ms = float(m_wallTime.elapsed());
    m_host.delta_time_ms = float(m_lastFrameTimer.restart());
    m_host.frame_index = quint32(m_frameIndex++);
    m_host.mouse_x = float(m_lastMouseInside.x() * devicePixelRatio());
    m_host.mouse_y = float(m_lastMouseInside.y() * devicePixelRatio());
    m_host.mouse_buttons = m_buttons;
    m_host.scroll_delta_x = m_scrollX;
    m_host.scroll_delta_y = m_scrollY;
    m_scrollX = 0.f;
    m_scrollY = 0.f;
}

void ComputePreviewWindow::loadInstanceProcs()
{
    m_pfnGetSurfaceCapabilities = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        m_qInst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    m_pfnGetSurfaceFormats = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
        m_qInst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceFormatsKHR"));
    m_pfnGetPresentModes = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
        m_qInst->getInstanceProcAddr("vkGetPhysicalDeviceSurfacePresentModesKHR"));
}

void ComputePreviewWindow::loadDeviceProcs()
{
    VkDevice dev = m_device.device();
    auto pfnGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        m_qInst->getInstanceProcAddr("vkGetDeviceProcAddr"));
    if (!pfnGetDeviceProcAddr)
        return;
    m_pfnCreateSwapchainKHR =
        reinterpret_cast<PFN_vkCreateSwapchainKHR>(pfnGetDeviceProcAddr(dev, "vkCreateSwapchainKHR"));
    m_pfnDestroySwapchainKHR =
        reinterpret_cast<PFN_vkDestroySwapchainKHR>(pfnGetDeviceProcAddr(dev, "vkDestroySwapchainKHR"));
    m_pfnGetSwapchainImagesKHR =
        reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(pfnGetDeviceProcAddr(dev, "vkGetSwapchainImagesKHR"));
    m_pfnAcquireNextImageKHR =
        reinterpret_cast<PFN_vkAcquireNextImageKHR>(pfnGetDeviceProcAddr(dev, "vkAcquireNextImageKHR"));
    m_pfnQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(pfnGetDeviceProcAddr(dev, "vkQueuePresentKHR"));
}

VkFormat ComputePreviewWindow::pickStorageCompatibleSurfaceFormat(VkColorSpaceKHR *outColorSpace) const
{
    VkPhysicalDevice pd = m_device.physicalDevice();
    VkSurfaceKHR surf = m_device.surface();
    uint32_t n = 0;
    m_pfnGetSurfaceFormats(pd, surf, &n, nullptr);
    QVector<VkSurfaceFormatKHR> fmts;
    fmts.resize(int(n));
    if (n)
        m_pfnGetSurfaceFormats(pd, surf, &n, fmts.data());

    QVulkanFunctions *vf = m_qInst->functions();
    for (const VkSurfaceFormatKHR &sf : fmts) {
        VkFormatProperties fp{};
        vf->vkGetPhysicalDeviceFormatProperties(pd, sf.format, &fp);
        if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) {
            *outColorSpace = sf.colorSpace;
            return sf.format;
        }
    }
    if (!fmts.isEmpty()) {
        *outColorSpace = fmts[0].colorSpace;
        return fmts[0].format;
    }
    *outColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    return VK_FORMAT_UNDEFINED;
}

void ComputePreviewWindow::releaseGpu()
{
    if (!m_device.isValid() || !m_qInst)
        return;

    QVulkanDeviceFunctions *df = m_device.deviceFunctions();
    VkDevice dev = m_device.device();
    df->vkDeviceWaitIdle(dev);

    if (m_cmdPool) {
        df->vkDestroyCommandPool(dev, m_cmdPool, nullptr);
        m_cmdPool = VK_NULL_HANDLE;
        m_cmdBuf = VK_NULL_HANDLE;
    }
    if (m_imageAvailable) {
        df->vkDestroySemaphore(dev, m_imageAvailable, nullptr);
        m_imageAvailable = VK_NULL_HANDLE;
    }
    if (m_renderFinished) {
        df->vkDestroySemaphore(dev, m_renderFinished, nullptr);
        m_renderFinished = VK_NULL_HANDLE;
    }
    if (m_inFlight) {
        df->vkDestroyFence(dev, m_inFlight, nullptr);
        m_inFlight = VK_NULL_HANDLE;
    }

    if (m_swapchain) {
        if (m_pfnDestroySwapchainKHR)
            m_pfnDestroySwapchainKHR(dev, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    m_swapImages.clear();
    m_engine.destroy();
    m_device.destroy();
}

void ComputePreviewWindow::ensureGpu()
{
    if (m_device.isValid())
        return;

    loadInstanceProcs();
    QString err;
    if (!m_device.create(m_qInst, this, &err, m_preferredGpuName)) {
        qWarning() << "ComputePreviewWindow:" << err;
        if (!m_gpuErrorEmitted) {
            m_gpuErrorEmitted = true;
            emit gpuInitFailed(err);
        }
        return;
    }
    m_gpuErrorEmitted = false;
    loadDeviceProcs();

    QVulkanDeviceFunctions *df = m_device.deviceFunctions();
    VkDevice dev = m_device.device();

    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = m_device.graphicsQueueFamilyIndex();
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (df->vkCreateCommandPool(dev, &cpci, nullptr, &m_cmdPool) != VK_SUCCESS) {
        qWarning() << "ComputePreviewWindow: vkCreateCommandPool failed.";
        return;
    }

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = m_cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    if (df->vkAllocateCommandBuffers(dev, &cbai, &m_cmdBuf) != VK_SUCCESS) {
        qWarning() << "ComputePreviewWindow: vkAllocateCommandBuffers failed.";
        return;
    }

    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    df->vkCreateSemaphore(dev, &sci, nullptr, &m_imageAvailable);
    df->vkCreateSemaphore(dev, &sci, nullptr, &m_renderFinished);

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    df->vkCreateFence(dev, &fci, nullptr, &m_inFlight);

    m_swapchainNeedsRebuild = true;
}

void ComputePreviewWindow::recreateSwapchain()
{
    if (!m_device.isValid() || !m_pfnCreateSwapchainKHR)
        return;

    QVulkanDeviceFunctions *df = m_device.deviceFunctions();
    VkDevice dev = m_device.device();
    VkPhysicalDevice pd = m_device.physicalDevice();
    VkSurfaceKHR surf = m_device.surface();

    if (m_swapchain) {
        df->vkDeviceWaitIdle(dev);
        m_pfnDestroySwapchainKHR(dev, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
        m_swapImages.clear();
    }

    VkSurfaceCapabilitiesKHR caps{};
    m_pfnGetSurfaceCapabilities(pd, surf, &caps);

    const qreal dpr = devicePixelRatio();
    const uint32_t w = uint32_t(std::max(1, int(float(width()) * dpr)));
    const uint32_t h = uint32_t(std::max(1, int(float(height()) * dpr)));

    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width = std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        imageCount = std::min(imageCount, caps.maxImageCount);

    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    m_swapFormat = pickStorageCompatibleSurfaceFormat(&colorSpace);
    m_colorSpace = colorSpace;

    uint32_t presentModeCount = 0;
    m_pfnGetPresentModes(pd, surf, &presentModeCount, nullptr);
    QVector<VkPresentModeKHR> modes;
    modes.resize(int(presentModeCount));
    if (presentModeCount)
        m_pfnGetPresentModes(pd, surf, &presentModeCount, modes.data());
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = m;
            break;
        }
    }

    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = surf;
    sci.minImageCount = imageCount;
    sci.imageFormat = m_swapFormat;
    sci.imageColorSpace = m_colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = presentMode;
    sci.clipped = VK_TRUE;

    if (m_pfnCreateSwapchainKHR(dev, &sci, nullptr, &m_swapchain) != VK_SUCCESS) {
        qWarning() << "ComputePreviewWindow: vkCreateSwapchainKHR failed.";
        return;
    }

    uint32_t imgCount = 0;
    m_pfnGetSwapchainImagesKHR(dev, m_swapchain, &imgCount, nullptr);
    m_swapImages.resize(int(imgCount));
    m_pfnGetSwapchainImagesKHR(dev, m_swapchain, &imgCount, m_swapImages.data());

    m_swapExtent = extent;
    m_swapchainNeedsRebuild = false;
    m_payloadDirty = true;
}

void ComputePreviewWindow::resetGpuErrorAnnouncement()
{
    m_gpuErrorEmitted = false;
}

void ComputePreviewWindow::setPreferredGpuName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed == m_preferredGpuName)
        return;
    m_preferredGpuName = trimmed;
    releaseGpu();
    requestUpdate();
}

void ComputePreviewWindow::setComputePayload(const ComputeStageBinary &stage)
{
    QMutexLocker lock(&m_payloadMutex);
    m_pendingStage = stage;
    m_payloadDirty = true;
    if (!m_timer->isActive())
        m_timer->start(16);
    requestUpdate();
}

void ComputePreviewWindow::onFrameTick()
{
    renderFrame();
}

void ComputePreviewWindow::exposeEvent(QExposeEvent *e)
{
    QWindow::exposeEvent(e);
    if (isExposed())
        renderFrame();
}

void ComputePreviewWindow::resizeEvent(QResizeEvent *e)
{
    QWindow::resizeEvent(e);
    m_swapchainNeedsRebuild = true;
    requestUpdate();
}

void ComputePreviewWindow::renderFrame()
{
    if (!isExposed() || width() <= 0 || height() <= 0)
        return;

    ensureGpu();
    if (!m_device.isValid())
        return;

    if (m_swapchainNeedsRebuild || m_swapchain == VK_NULL_HANDLE)
        recreateSwapchain();

    if (m_swapchain == VK_NULL_HANDLE || m_swapFormat == VK_FORMAT_UNDEFINED)
        return;

    ComputeStageBinary stageCopy;
    bool dirty = false;
    {
        QMutexLocker lock(&m_payloadMutex);
        stageCopy = m_pendingStage;
        dirty = m_payloadDirty;
    }

    QString err;
    if (dirty && !stageCopy.spirv.isEmpty()) {
        const QSize sz(int(m_swapExtent.width), int(m_swapExtent.height));
        if (!m_engine.rebuild(&m_device, stageCopy, sz, m_swapFormat, &err)) {
            if (!err.isEmpty())
                qWarning() << "ComputePreviewEngine:" << err;
        }
        QMutexLocker lock(&m_payloadMutex);
        m_payloadDirty = false;
    }

    if (!m_engine.isValid())
        return;

    QVulkanDeviceFunctions *df = m_device.deviceFunctions();
    VkDevice dev = m_device.device();
    VkQueue queue = m_device.graphicsQueue();

    VkResult fenceStatus = df->vkWaitForFences(dev, 1, &m_inFlight, VK_TRUE, 0);
    if (fenceStatus == VK_TIMEOUT)
        return;
    df->vkResetFences(dev, 1, &m_inFlight);

    uint32_t imageIndex = 0;
    VkResult ac = m_pfnAcquireNextImageKHR(dev, m_swapchain, UINT64_MAX, m_imageAvailable, VK_NULL_HANDLE,
                                           &imageIndex);
    if (ac != VK_SUCCESS && ac != VK_SUBOPTIMAL_KHR) {
        m_swapchainNeedsRebuild = true;
        return;
    }

    df->vkResetCommandBuffer(m_cmdBuf, 0);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    df->vkBeginCommandBuffer(m_cmdBuf, &bi);

    updateHostUniformsForFrame();
    m_engine.setHostUniforms(m_host);
    m_engine.recordFrame(m_cmdBuf, m_swapImages[int(imageIndex)], VK_IMAGE_LAYOUT_UNDEFINED, m_swapExtent);

    df->vkEndCommandBuffer(m_cmdBuf);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &m_imageAvailable;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &m_cmdBuf;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &m_renderFinished;

    if (df->vkQueueSubmit(queue, 1, &si, m_inFlight) != VK_SUCCESS)
        return;

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &m_renderFinished;
    pi.swapchainCount = 1;
    pi.pSwapchains = &m_swapchain;
    pi.pImageIndices = &imageIndex;

    VkResult pr = m_pfnQueuePresentKHR(queue, &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
        m_swapchainNeedsRebuild = true;
}
