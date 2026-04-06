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
};

class PreviewVulkanWindow : public QVulkanWindow
{
    friend class PreviewRenderer;
public:
    explicit PreviewVulkanWindow();

    void setRasterStages(const QVector<RasterStageBinary> &stages);
    void setRasterPayload(const QVector<RasterStageBinary> &stages, const QVector<QImage> &textures,
                          const QString &meshPath);

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
    bool m_hasPendingShader = false;

    QElapsedTimer m_simTime;
    QElapsedTimer m_wallTime;
    float m_simTimeOverrideMs = -1.f;
    quint64 m_frameIndex = 0;
    HostUniforms m_host;
    QPointF m_lastMouseInside;
    bool m_mouseInside = false;
    uint32_t m_buttons = 0;
    float m_scrollX = 0.f;
    float m_scrollY = 0.f;
};

#endif
