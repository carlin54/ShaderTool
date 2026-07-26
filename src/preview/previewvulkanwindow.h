#ifndef PREVIEWVULKANWINDOW_H
#define PREVIEWVULKANWINDOW_H

#include <QVulkanWindow>
#include <QElapsedTimer>
#include <QByteArray>
#include <QString>
#include <QPointF>
#include <QMutex>
#include <QVector>
#include <QImage>

#include "hostuniforms.h"
#include "previewpipeline.h"

class PreviewRenderer;

struct RasterPreviewBuild {
    QVector<RasterStageBinary> stages;
    QVector<QImage> textures;
    QString meshPath;
    RasterPreviewPipeline::BlendMode blendMode = RasterPreviewPipeline::BlendOff;
};

class PreviewVulkanWindow : public QVulkanWindow
{
    friend class PreviewRenderer;
public:
    explicit PreviewVulkanWindow();

    void setRasterStages(const QVector<RasterStageBinary> &stages);
    void setRasterPayload(const QVector<RasterStageBinary> &stages, const QVector<QImage> &textures,
                          const QString &meshPath,
                          RasterPreviewPipeline::BlendMode blendMode = RasterPreviewPipeline::BlendOff);

    bool takeRasterStages(QVector<RasterStageBinary> *out);
    bool takeRasterPreviewBuild(RasterPreviewBuild *out);

    HostUniforms &hostState() { return m_host; }
    void resetFrameScrollDeltas();

    // Deterministic export: if < 0, use real elapsed timer; else override simulation time (ms).
    void setSimulationTimeOverrideMs(float ms);
    float simulationTimeOverrideMs() const { return m_simTimeOverrideMs; }

protected:
    bool event(QEvent *e) override;
    QVulkanWindowRenderer *createRenderer() override;

private:
    QMutex m_shaderMutex;
    QVector<RasterStageBinary> m_pendingStages;
    QVector<QImage> m_pendingTextures;
    QString m_pendingMeshPath;
    RasterPreviewPipeline::BlendMode m_pendingBlendMode = RasterPreviewPipeline::BlendOff;
    bool m_hasPendingShader = false;

    QElapsedTimer m_simTime;
    QElapsedTimer m_wallTime;
    float m_simTimeOverrideMs = -1.f;
    quint64 m_frameIndex = 0;
    HostUniforms m_host;
    QPointF m_lastMouseInside;
    QPointF m_lastDragPos;
    bool m_mouseInside = false;
    uint32_t m_buttons = 0;
    float m_scrollX = 0.f;
    float m_scrollY = 0.f;
    float m_dragAccumX = 0.f;
    float m_dragAccumY = 0.f;
    float m_scrollAccumX = 0.f;
    float m_scrollAccumY = 0.f;
    QPointF m_lastPanPos;
    float m_panAccumX = 0.f;
    float m_panAccumY = 0.f;
};

#endif
