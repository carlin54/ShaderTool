#include "meshgeometry.h"

#include <QFile>
#include <QTextStream>
#include <QVector3D>
#include <QVector2D>

#include <cstring>

bool loadObjFile(const QString &path, QVector<float> *outInterleavedVertices, QVector<uint32_t> *outIndices)
{
    if (!outInterleavedVertices || !outIndices)
        return false;
    *outInterleavedVertices = {};
    *outIndices = {};

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QVector<QVector3D> positions;
    QVector<QVector3D> normals;
    QVector<QVector2D> texcoords;

    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList p = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (p.isEmpty())
            continue;
        if (p[0] == QStringLiteral("v") && p.size() >= 4) {
            positions.append(QVector3D(p[1].toFloat(), p[2].toFloat(), p[3].toFloat()));
        } else if (p[0] == QStringLiteral("vn") && p.size() >= 4) {
            normals.append(QVector3D(p[1].toFloat(), p[2].toFloat(), p[3].toFloat()));
        } else if (p[0] == QStringLiteral("vt") && p.size() >= 3) {
            texcoords.append(QVector2D(p[1].toFloat(), p[2].toFloat()));
        } else if (p[0] == QStringLiteral("f") && p.size() >= 4) {
            const int n = p.size() - 1;
            if (n < 3)
                continue;
            auto parseFaceVert = [&](const QString &s) {
                const QStringList parts = s.split(QLatin1Char('/'));
                int vi = parts.isEmpty() ? 0 : parts[0].toInt();
                int ti = parts.size() > 1 && !parts[1].isEmpty() ? parts[1].toInt() : 0;
                int ni = parts.size() > 2 && !parts[2].isEmpty() ? parts[2].toInt() : 0;
                if (vi < 0)
                    vi = positions.size() + vi + 1;
                if (ti < 0)
                    ti = texcoords.size() + ti + 1;
                if (ni < 0)
                    ni = normals.size() + ni + 1;
                QVector3D pos = (vi > 0 && vi <= positions.size()) ? positions[vi - 1] : QVector3D();
                QVector3D nrm = (ni > 0 && ni <= normals.size()) ? normals[ni - 1] : QVector3D(0, 1, 0);
                QVector2D uv = (ti > 0 && ti <= texcoords.size()) ? texcoords[ti - 1] : QVector2D();
                uint32_t idx = uint32_t(outInterleavedVertices->size() / 8);
                outInterleavedVertices->append(pos.x());
                outInterleavedVertices->append(pos.y());
                outInterleavedVertices->append(pos.z());
                outInterleavedVertices->append(nrm.x());
                outInterleavedVertices->append(nrm.y());
                outInterleavedVertices->append(nrm.z());
                outInterleavedVertices->append(uv.x());
                outInterleavedVertices->append(uv.y());
                return idx;
            };

            QVector<uint32_t> ids;
            ids.reserve(n);
            for (int i = 1; i <= n; ++i)
                ids.append(parseFaceVert(p[i]));
            for (int t = 1; t + 1 < ids.size(); ++t) {
                outIndices->append(ids[0]);
                outIndices->append(ids[t]);
                outIndices->append(ids[t + 1]);
            }
        }
    }

    return !outIndices->isEmpty();
}

static VkResult createBuffer(QVulkanWindow *window, QVulkanDeviceFunctions *df, VkDeviceSize size,
                             VkBufferUsageFlags usage, VkMemoryPropertyFlags, VkBuffer *outBuf,
                             VkDeviceMemory *outMem, void **mappedOut)
{
    VkDevice dev = window->device();
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = usage;
    VkResult err = df->vkCreateBuffer(dev, &bi, nullptr, outBuf);
    if (err != VK_SUCCESS)
        return err;

    VkMemoryRequirements req{};
    df->vkGetBufferMemoryRequirements(dev, *outBuf, &req);

    const uint32_t memIndex = window->hostVisibleMemoryIndex();
    if (!(req.memoryTypeBits & (1u << memIndex))) {
        df->vkDestroyBuffer(dev, *outBuf, nullptr);
        *outBuf = VK_NULL_HANDLE;
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = memIndex;
    err = df->vkAllocateMemory(dev, &alloc, nullptr, outMem);
    if (err != VK_SUCCESS) {
        df->vkDestroyBuffer(dev, *outBuf, nullptr);
        *outBuf = VK_NULL_HANDLE;
        return err;
    }
    err = df->vkBindBufferMemory(dev, *outBuf, *outMem, 0);
    if (err != VK_SUCCESS) {
        df->vkFreeMemory(dev, *outMem, nullptr);
        df->vkDestroyBuffer(dev, *outBuf, nullptr);
        *outBuf = VK_NULL_HANDLE;
        *outMem = VK_NULL_HANDLE;
        return err;
    }
    if (mappedOut) {
        err = df->vkMapMemory(dev, *outMem, 0, size, 0, mappedOut);
        if (err != VK_SUCCESS) {
            df->vkFreeMemory(dev, *outMem, nullptr);
            df->vkDestroyBuffer(dev, *outBuf, nullptr);
            *outBuf = VK_NULL_HANDLE;
            *outMem = VK_NULL_HANDLE;
            *mappedOut = nullptr;
            return err;
        }
    }
    return VK_SUCCESS;
}

VkResult createMeshGeometry(QVulkanWindow *window, QVulkanDeviceFunctions *df,
                            const QVector<float> &interleavedVertices, const QVector<uint32_t> &indices,
                            MeshGeometry *out)
{
    if (!window || !df || !out || interleavedVertices.isEmpty() || indices.isEmpty())
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkDeviceSize vbSize = VkDeviceSize(interleavedVertices.size() * sizeof(float));
    const VkDeviceSize ibSize = VkDeviceSize(indices.size() * sizeof(uint32_t));

    void *vm = nullptr;
    VkResult err = createBuffer(window, df, vbSize,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &out->vertexBuffer, &out->vertexMemory, &vm);
    if (err != VK_SUCCESS)
        return err;
    std::memcpy(vm, interleavedVertices.constData(), size_t(vbSize));
    df->vkUnmapMemory(window->device(), out->vertexMemory);

    void *im = nullptr;
    err = createBuffer(window, df, ibSize,
                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &out->indexBuffer, &out->indexMemory, &im);
    if (err != VK_SUCCESS) {
        destroyMeshGeometry(df, window->device(), out);
        return err;
    }
    std::memcpy(im, indices.constData(), size_t(ibSize));
    df->vkUnmapMemory(window->device(), out->indexMemory);

    out->indexCount = uint32_t(indices.size());
    return VK_SUCCESS;
}

void destroyMeshGeometry(QVulkanDeviceFunctions *df, VkDevice dev, MeshGeometry *mesh)
{
    if (!df || !mesh || dev == VK_NULL_HANDLE)
        return;
    if (mesh->vertexBuffer) {
        df->vkDestroyBuffer(dev, mesh->vertexBuffer, nullptr);
        mesh->vertexBuffer = VK_NULL_HANDLE;
    }
    if (mesh->vertexMemory) {
        df->vkFreeMemory(dev, mesh->vertexMemory, nullptr);
        mesh->vertexMemory = VK_NULL_HANDLE;
    }
    if (mesh->indexBuffer) {
        df->vkDestroyBuffer(dev, mesh->indexBuffer, nullptr);
        mesh->indexBuffer = VK_NULL_HANDLE;
    }
    if (mesh->indexMemory) {
        df->vkFreeMemory(dev, mesh->indexMemory, nullptr);
        mesh->indexMemory = VK_NULL_HANDLE;
    }
    mesh->indexCount = 0;
}
