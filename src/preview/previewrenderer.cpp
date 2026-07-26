#include "previewrenderer.h"
#include "previewvulkanwindow.h"
#include "meshgeometry.h"

#include <QDebug>
#include <QFileInfo>
#include <QVulkanWindow>
#include <cstring>

PreviewRenderer::PreviewRenderer(PreviewVulkanWindow *w)
    : m_window(w)
{
    m_lastFrameTimer.start();
}

PreviewRenderer::~PreviewRenderer() = default;

void PreviewRenderer::reloadMeshIfNeeded(const QString &meshPath)
{
    VkDevice dev = m_window->device();
    destroyMeshGeometry(m_df, dev, &m_mesh);
    m_meshPathLoaded.clear();

    if (meshPath.trimmed().isEmpty())
        return;
    if (!QFileInfo::exists(meshPath))
        return;

    QVector<float> vtx;
    QVector<uint32_t> idx;
    if (!loadObjFile(meshPath, &vtx, &idx))
        return;

    if (createMeshGeometry(m_window, m_df, vtx, idx, &m_mesh) == VK_SUCCESS)
        m_meshPathLoaded = meshPath;
}

void PreviewRenderer::tryRebuildPipeline()
{
    RasterPreviewBuild build;
    if (!m_window->takeRasterPreviewBuild(&build))
        return;

    VkDevice dev = m_window->device();
    m_df = m_window->vulkanInstance()->deviceFunctions(dev);
    m_pipeline.destroy(m_df, dev);

    reloadMeshIfNeeded(build.meshPath);

    const bool meshDraw = m_mesh.indexCount > 0;
    const VkResult err = m_pipeline.create(m_window, m_df, build.stages, build.textures, meshDraw, build.blendMode);
    if (err != VK_SUCCESS) {
        qWarning("Pipeline create failed: %d", int(err));
        m_pipelineValid = false;
        return;
    }
    m_pipelineValid = true;
}

void PreviewRenderer::initResources()
{
    m_df = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    tryRebuildPipeline();
}

void PreviewRenderer::initSwapChainResources()
{
    const QSize sz = m_window->swapChainImageSize();
    const qreal dpr = m_window->devicePixelRatio();
    m_window->hostState().resolution_x = float(sz.width() * dpr);
    m_window->hostState().resolution_y = float(sz.height() * dpr);
}

void PreviewRenderer::releaseSwapChainResources() {}

void PreviewRenderer::releaseResources()
{
    if (m_window && m_df) {
        destroyMeshGeometry(m_df, m_window->device(), &m_mesh);
        m_meshPathLoaded.clear();
        m_pipeline.destroy(m_df, m_window->device());
    }
    m_pipelineValid = false;
}

void PreviewRenderer::updateHostUniformsForFrame(int frameIndex)
{
    PreviewVulkanWindow *w = m_window;
    HostUniforms &h = w->hostState();

    if (w->simulationTimeOverrideMs() >= 0.f)
        h.time_sim_ms = w->simulationTimeOverrideMs();
    else
        h.time_sim_ms = float(w->m_simTime.elapsed());

    h.time_wall_ms = float(w->m_wallTime.elapsed());
    h.delta_time_ms = float(m_lastFrameTimer.restart());
    h.frame_index = quint32(w->m_frameIndex);
    ++w->m_frameIndex;
    h.mouse_x = float(w->m_lastMouseInside.x() * w->devicePixelRatio());
    h.mouse_y = float(w->m_lastMouseInside.y() * w->devicePixelRatio());
    h.mouse_buttons = w->m_buttons;
    h.scroll_delta_x = w->m_scrollX;
    h.scroll_delta_y = w->m_scrollY;
    h.drag_accum_x = w->m_dragAccumX;
    h.drag_accum_y = w->m_dragAccumY;
    h.scroll_accum_x = w->m_scrollAccumX;
    h.scroll_accum_y = w->m_scrollAccumY;
    h.pan_accum_x = w->m_panAccumX;
    h.pan_accum_y = w->m_panAccumY;
    w->resetFrameScrollDeltas();

    VkDevice dev = w->device();
    const VkDeviceSize offset = m_pipeline.uniformBufferInfo()[frameIndex].offset;
    quint8 *p = nullptr;
    VkResult err = m_df->vkMapMemory(dev, m_pipeline.uniformBufferMemory(), offset,
                                     sizeof(HostUniforms), 0, reinterpret_cast<void **>(&p));
    if (err == VK_SUCCESS) {
        std::memcpy(p, &h, sizeof(HostUniforms));
        m_df->vkUnmapMemory(dev, m_pipeline.uniformBufferMemory());
    }
}

void PreviewRenderer::startNextFrame()
{
    tryRebuildPipeline();

    VkDevice dev = m_window->device();
    VkCommandBuffer cb = m_window->currentCommandBuffer();
    const QSize sz = m_window->swapChainImageSize();
    const int frameIndex = m_window->currentFrame();

    if (!m_pipelineValid || m_pipeline.pipeline() == VK_NULL_HANDLE) {
        m_window->frameReady();
        m_window->requestUpdate();
        return;
    }

    updateHostUniformsForFrame(frameIndex);

    VkClearColorValue clearColor{{0.05f, 0.05f, 0.08f, 1.0f}};
    VkClearDepthStencilValue clearDS{1.0f, 0};
    VkClearValue clearValues[3]{};
    clearValues[0].color = clearValues[2].color = clearColor;
    clearValues[1].depthStencil = clearDS;

    VkRenderPassBeginInfo rpBeginInfo{};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = m_window->defaultRenderPass();
    rpBeginInfo.framebuffer = m_window->currentFramebuffer();
    rpBeginInfo.renderArea.extent.width = uint32_t(sz.width());
    rpBeginInfo.renderArea.extent.height = uint32_t(sz.height());
    rpBeginInfo.clearValueCount = m_window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3u : 2u;
    rpBeginInfo.pClearValues = clearValues;

    m_df->vkCmdBeginRenderPass(cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    m_df->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.pipeline());

    VkDescriptorSet sets[2];
    sets[0] = m_pipeline.descriptorSetUniform(frameIndex);
    uint32_t setCount = 1;
    if (m_pipeline.hasTextures()) {
        sets[1] = m_pipeline.descriptorSetTextures(frameIndex);
        setCount = 2;
    }
    m_df->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.pipelineLayout(), 0,
                                  setCount, sets, 0, nullptr);

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = float(sz.width());
    viewport.height = float(sz.height());
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    m_df->vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent.width = uint32_t(sz.width());
    scissor.extent.height = uint32_t(sz.height());
    m_df->vkCmdSetScissor(cb, 0, 1, &scissor);

    if (m_mesh.indexCount > 0) {
        VkBuffer vb = m_mesh.vertexBuffer;
        VkDeviceSize offs = 0;
        m_df->vkCmdBindVertexBuffers(cb, 0, 1, &vb, &offs);
        m_df->vkCmdBindIndexBuffer(cb, m_mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        m_df->vkCmdDrawIndexed(cb, m_mesh.indexCount, 1, 0, 0, 0);
    } else {
        m_df->vkCmdDraw(cb, 3, 1, 0, 0);
    }

    m_df->vkCmdEndRenderPass(cb);

    m_window->frameReady();
    m_window->requestUpdate();
}
