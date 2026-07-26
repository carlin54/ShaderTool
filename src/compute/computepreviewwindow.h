#ifndef COMPUTEPREVIEWWINDOW_H
#define COMPUTEPREVIEWWINDOW_H

#include <QElapsedTimer>
#include <QWindow>
#include <QMutex>
#include <QPointF>

#include "customvulkandevice.h"
#include "computepreviewengine.h"
#include "hostuniforms.h"

class QVulkanInstance;
class QTimer;

class ComputePreviewWindow : public QWindow
{
    Q_OBJECT
public:
    explicit ComputePreviewWindow(QVulkanInstance *inst, QWindow *parent = nullptr);
    ~ComputePreviewWindow() override;

    void setVulkanInstance(QVulkanInstance *inst)
    {
        m_qInst = inst;
        QWindow::setVulkanInstance(inst);
    }
    void setComputePayload(const ComputeStageBinary &stage);
    void setPreferredGpuName(const QString &name);
    void resetGpuErrorAnnouncement();

signals:
    void gpuInitFailed(const QString &message);

protected:
    bool event(QEvent *e) override;
    void exposeEvent(QExposeEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void onFrameTick();

private:
    void ensureGpu();
    void updateHostUniformsForFrame();
    void releaseGpu();
    void recreateSwapchain();
    void renderFrame();
    void loadInstanceProcs();
    void loadDeviceProcs();
    VkFormat pickStorageCompatibleSurfaceFormat(VkColorSpaceKHR *outColorSpace) const;

    QVulkanInstance *m_qInst = nullptr;

    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR m_pfnGetSurfaceCapabilities = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR m_pfnGetSurfaceFormats = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR m_pfnGetPresentModes = nullptr;

    PFN_vkCreateSwapchainKHR m_pfnCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR m_pfnDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR m_pfnGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR m_pfnAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR m_pfnQueuePresentKHR = nullptr;

    CustomVulkanDevice m_device;
    ComputePreviewEngine m_engine;

    QMutex m_payloadMutex;
    ComputeStageBinary m_pendingStage;
    bool m_payloadDirty = false;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    QVector<VkImage> m_swapImages;
    VkFormat m_swapFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR m_colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D m_swapExtent{};

    VkSemaphore m_imageAvailable = VK_NULL_HANDLE;
    VkSemaphore m_renderFinished = VK_NULL_HANDLE;
    VkFence m_inFlight = VK_NULL_HANDLE;

    VkCommandPool m_cmdPool = VK_NULL_HANDLE;
    VkCommandBuffer m_cmdBuf = VK_NULL_HANDLE;

    QTimer *m_timer = nullptr;
    bool m_swapchainNeedsRebuild = false;
    bool m_gpuErrorEmitted = false;
    QString m_preferredGpuName;

    QElapsedTimer m_simTime;
    QElapsedTimer m_wallTime;
    QElapsedTimer m_lastFrameTimer;
    quint64 m_frameIndex = 0;
    HostUniforms m_host{};
    QPointF m_lastMouseInside;
    bool m_mouseInside = false;
    uint32_t m_buttons = 0;
    float m_scrollX = 0.f;
    float m_scrollY = 0.f;
};

#endif
