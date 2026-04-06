#ifndef PREVIEWRENDERER_H
#define PREVIEWRENDERER_H

#include <QVulkanWindowRenderer>
#include <QElapsedTimer>

#include "meshgeometry.h"
#include "previewpipeline.h"

class PreviewVulkanWindow;

class PreviewRenderer : public QVulkanWindowRenderer
{
public:
    explicit PreviewRenderer(PreviewVulkanWindow *w);
    ~PreviewRenderer() override;

    void initResources() override;
    void initSwapChainResources() override;
    void releaseSwapChainResources() override;
    void releaseResources() override;
    void startNextFrame() override;

private:
    void tryRebuildPipeline();
    void updateHostUniformsForFrame(int frameIndex);
    void reloadMeshIfNeeded(const QString &meshPath);

    PreviewVulkanWindow *m_window = nullptr;
    QVulkanDeviceFunctions *m_df = nullptr;
    RasterPreviewPipeline m_pipeline;
    bool m_pipelineValid = false;

    MeshGeometry m_mesh;
    QString m_meshPathLoaded;

    QElapsedTimer m_lastFrameTimer;
};

#endif
