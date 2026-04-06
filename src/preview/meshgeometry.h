#ifndef MESHGEOMETRY_H
#define MESHGEOMETRY_H

#include <QString>
#include <QVector>
#include <QVulkanDeviceFunctions>
#include <QVulkanWindow>

#include <vulkan/vulkan.h>

// Interleaved vertex: float3 pos, float3 normal, float2 uv (32 bytes).
struct MeshGeometry {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
};

// Minimal OBJ (triangles, v/vn/vt/f). Returns false on failure.
bool loadObjFile(const QString &path, QVector<float> *outInterleavedVertices, QVector<uint32_t> *outIndices);

VkResult createMeshGeometry(QVulkanWindow *window, QVulkanDeviceFunctions *df,
                            const QVector<float> &interleavedVertices, const QVector<uint32_t> &indices,
                            MeshGeometry *out);

void destroyMeshGeometry(QVulkanDeviceFunctions *df, VkDevice dev, MeshGeometry *mesh);

#endif
