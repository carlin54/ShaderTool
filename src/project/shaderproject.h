#ifndef SHADERPROJECT_H
#define SHADERPROJECT_H

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <vulkan/vulkan.h>

struct ShaderStage {
    QString kind; // "vertex", "fragment", "geometry", "tessellation_control", "tessellation_evaluation"
    QString entry;
    QString source;
};

namespace ShaderStageHelpers {
VkShaderStageFlagBits vkStageFlag(const QString &kind);
QString dxcProfile(const QString &kind);
bool isSupportedInRasterPreview(const QString &kind);
}

struct ShaderProject {
    int formatVersion = 1;
    int hostUniformCatalogVersion = 1;
    QString name;
    QString pipelineKind = QStringLiteral("raster");
    QString meshPath; // optional: OBJ path relative to project file or absolute
    QVector<ShaderStage> stages;
    QJsonArray textures; // [{ "path": "...", "slot": 0 }, ...]

    // Ray tracing only: optional cap for VkRayTracingPipelineCreateInfoKHR::maxPipelineRayRecursionDepth.
    // 0 = let the preview engine choose (device-dependent default).
    quint32 maxPipelineRayRecursionDepth = 0;

    QJsonObject toJson() const;
    static ShaderProject fromJson(const QJsonObject &o, QString *errorOut = nullptr);

    // Returns empty string if OK; otherwise a user-visible validation message.
    static QString validationMessage(const ShaderProject &p);

    ShaderStage *findStage(const QString &kind);
    const ShaderStage *findStage(const QString &kind) const;
};

#endif
