#include "shaderproject.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

static bool isRayTracingKind(const QString &kind)
{
    return kind == QStringLiteral("raygen") || kind == QStringLiteral("miss")
        || kind == QStringLiteral("closest_hit") || kind == QStringLiteral("any_hit")
        || kind == QStringLiteral("intersection") || kind == QStringLiteral("callable");
}

VkShaderStageFlagBits ShaderStageHelpers::vkStageFlag(const QString &kind)
{
    if (kind == QStringLiteral("vertex"))
        return VK_SHADER_STAGE_VERTEX_BIT;
    if (kind == QStringLiteral("fragment"))
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    if (kind == QStringLiteral("geometry"))
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    if (kind == QStringLiteral("tessellation_control"))
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (kind == QStringLiteral("tessellation_evaluation"))
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    return VK_SHADER_STAGE_VERTEX_BIT;
}

QString ShaderStageHelpers::dxcProfile(const QString &kind)
{
    if (kind == QStringLiteral("vertex"))
        return QStringLiteral("vs_6_0");
    if (kind == QStringLiteral("fragment"))
        return QStringLiteral("ps_6_0");
    if (kind == QStringLiteral("geometry"))
        return QStringLiteral("gs_6_0");
    if (kind == QStringLiteral("tessellation_control"))
        return QStringLiteral("hs_6_0");
    if (kind == QStringLiteral("tessellation_evaluation"))
        return QStringLiteral("ds_6_0");
    if (isRayTracingKind(kind))
        return QStringLiteral("lib_6_3");
    return QStringLiteral("vs_6_0");
}

bool ShaderStageHelpers::isSupportedInRasterPreview(const QString &kind)
{
    return kind == QStringLiteral("vertex") || kind == QStringLiteral("fragment")
        || kind == QStringLiteral("geometry");
}

QJsonObject ShaderProject::toJson() const
{
    QJsonObject root;
    root[QStringLiteral("formatVersion")] = formatVersion;
    root[QStringLiteral("hostUniformCatalogVersion")] = hostUniformCatalogVersion;
    root[QStringLiteral("name")] = name;
    root[QStringLiteral("pipelineKind")] = pipelineKind;
    if (!meshPath.isEmpty())
        root[QStringLiteral("meshPath")] = meshPath;
    QJsonArray arr;
    for (const ShaderStage &s : stages) {
        QJsonObject o;
        o[QStringLiteral("stage")] = s.kind;
        o[QStringLiteral("entryPoint")] = s.entry;
        o[QStringLiteral("source")] = s.source;
        arr.append(o);
    }
    root[QStringLiteral("stages")] = arr;
    root[QStringLiteral("language")] = QStringLiteral("hlsl");
    if (!textures.isEmpty())
        root[QStringLiteral("textures")] = textures;
    if (pipelineKind == QStringLiteral("raytrace") && maxPipelineRayRecursionDepth > 0)
        root[QStringLiteral("maxPipelineRayRecursionDepth")] = qint64(maxPipelineRayRecursionDepth);
    return root;
}

ShaderProject ShaderProject::fromJson(const QJsonObject &o, QString *errorOut)
{
    ShaderProject p;
    if (!o.contains(QStringLiteral("formatVersion"))) {
        if (errorOut)
            *errorOut = QStringLiteral("Missing formatVersion.");
        return p;
    }
    p.formatVersion = o.value(QStringLiteral("formatVersion")).toInt();
    p.hostUniformCatalogVersion = o.value(QStringLiteral("hostUniformCatalogVersion")).toInt(1);
    p.name = o.value(QStringLiteral("name")).toString();
    p.pipelineKind = o.value(QStringLiteral("pipelineKind")).toString(QStringLiteral("raster"));
    p.meshPath = o.value(QStringLiteral("meshPath")).toString();
    p.textures = o.value(QStringLiteral("textures")).toArray();
    p.maxPipelineRayRecursionDepth = quint32(
        std::max(0, std::min(31, o.value(QStringLiteral("maxPipelineRayRecursionDepth")).toInt(0))));

    const QJsonArray stages = o.value(QStringLiteral("stages")).toArray();
    for (const QJsonValue &v : stages) {
        const QJsonObject s = v.toObject();
        ShaderStage st;
        st.kind = s.value(QStringLiteral("stage")).toString();
        st.entry = s.value(QStringLiteral("entryPoint")).toString();
        st.source = s.value(QStringLiteral("source")).toString();
        if (!st.kind.isEmpty())
            p.stages.append(st);
    }
    return p;
}

QString ShaderProject::validationMessage(const ShaderProject &p)
{
    if (p.formatVersion < 1)
        return QStringLiteral("formatVersion must be >= 1.");
    if (p.pipelineKind != QStringLiteral("raster") && p.pipelineKind != QStringLiteral("raytrace"))
        return QStringLiteral("pipelineKind must be \"raster\" or \"raytrace\".");
    if (p.stages.isEmpty())
        return QStringLiteral("Project has no shader stages.");

    if (p.pipelineKind == QStringLiteral("raytrace")) {
        int nRaygen = 0;
        int nMiss = 0;
        int nClosest = 0;
        for (const ShaderStage &s : p.stages) {
            if (s.kind == QStringLiteral("raygen"))
                ++nRaygen;
            else if (s.kind == QStringLiteral("miss"))
                ++nMiss;
            else if (s.kind == QStringLiteral("closest_hit"))
                ++nClosest;
            else if (s.kind != QStringLiteral("any_hit") && s.kind != QStringLiteral("intersection")
                     && s.kind != QStringLiteral("callable"))
                return QStringLiteral("Unknown ray-trace stage kind \"%1\".").arg(s.kind);
            if (s.entry.trimmed().isEmpty())
                return QStringLiteral("Ray-trace stage \"%1\" has an empty entry point.").arg(s.kind);
            if (s.source.trimmed().isEmpty())
                return QStringLiteral("Ray-trace stage \"%1\" has empty source.").arg(s.kind);
        }
        if (nRaygen != 1)
            return QStringLiteral(
                "GPU ray-tracing preview requires exactly one \"raygen\" stage (found %1).").arg(nRaygen);
        if (nMiss < 1)
            return QStringLiteral(
                "GPU ray-tracing preview requires at least one \"miss\" stage (found %1).").arg(nMiss);
        if (nClosest < 1)
            return QStringLiteral(
                "GPU ray-tracing preview requires at least one \"closest_hit\" stage (found %1).").arg(nClosest);
    }

    return {};
}

ShaderStage *ShaderProject::findStage(const QString &kind)
{
    for (int i = 0; i < stages.size(); ++i) {
        if (stages[i].kind == kind)
            return &stages[i];
    }
    return nullptr;
}

const ShaderStage *ShaderProject::findStage(const QString &kind) const
{
    for (int i = 0; i < stages.size(); ++i) {
        if (stages[i].kind == kind)
            return &stages[i];
    }
    return nullptr;
}
