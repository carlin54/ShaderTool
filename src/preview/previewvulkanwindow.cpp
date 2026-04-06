#include "previewvulkanwindow.h"
#include "previewrenderer.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>

PreviewVulkanWindow::PreviewVulkanWindow()
{
    setTitle(QStringLiteral("Preview"));
    m_simTime.start();
    m_wallTime.start();
}

void PreviewVulkanWindow::setRasterStages(const QVector<RasterStageBinary> &stages)
{
    setRasterPayload(stages, {}, {});
}

void PreviewVulkanWindow::setRasterPayload(const QVector<RasterStageBinary> &stages,
                                           const QVector<QImage> &textures, const QString &meshPath)
{
    QMutexLocker lock(&m_shaderMutex);
    m_pendingStages = stages;
    m_pendingTextures = textures;
    m_pendingMeshPath = meshPath;
    m_hasPendingShader = true;
    requestUpdate();
}

bool PreviewVulkanWindow::takeRasterStages(QVector<RasterStageBinary> *out)
{
    RasterPreviewBuild b;
    if (!takeRasterPreviewBuild(&b))
        return false;
    *out = b.stages;
    return true;
}

bool PreviewVulkanWindow::takeRasterPreviewBuild(RasterPreviewBuild *out)
{
    QMutexLocker lock(&m_shaderMutex);
    if (!m_hasPendingShader)
        return false;
    out->stages = m_pendingStages;
    out->textures = m_pendingTextures;
    out->meshPath = m_pendingMeshPath;
    m_hasPendingShader = false;
    return true;
}

void PreviewVulkanWindow::setSimulationTimeOverrideMs(float ms)
{
    m_simTimeOverrideMs = ms;
}

void PreviewVulkanWindow::resetFrameScrollDeltas()
{
    m_scrollX = 0.f;
    m_scrollY = 0.f;
}

bool PreviewVulkanWindow::event(QEvent *e)
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
    case QEvent::Resize: {
        const QSize sz = size();
        const qreal dpr = devicePixelRatio();
        m_host.resolution_x = float(sz.width() * dpr);
        m_host.resolution_y = float(sz.height() * dpr);
        break;
    }
    default:
        break;
    }
    return QVulkanWindow::event(e);
}

QVulkanWindowRenderer *PreviewVulkanWindow::createRenderer()
{
    return new PreviewRenderer(this);
}
