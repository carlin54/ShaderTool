#include "examples.h"

#include "shaderproject.h"
#include "enums.h"

#include <QFile>
#include <QIODevice>
#include <QJsonDocument>

static QByteArray readResource(const QString &path, const QByteArray &fallback)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return fallback;
    const QByteArray data = f.readAll();
    return data.isEmpty() ? fallback : data;
}

static ShaderProject makeBasicsProject(const QString &name, const QString &fragOverride = QString())
{
    ShaderProject p;
    p.name = name;
    p.hostUniformCatalogVersion = 1;
    p.pipelineKind = QStringLiteral("raster");

    ShaderStage v;
    v.kind = QStringLiteral("vertex");
    v.entry = QStringLiteral("VSMain");
    v.source = DEFAULT_VERTEX;

    ShaderStage f;
    f.kind = QStringLiteral("fragment");
    f.entry = QStringLiteral("PSMain");
    f.source = fragOverride.isEmpty() ? DEFAULT_FRAGMENT : fragOverride;

    p.stages = {v, f};
    return p;
}

QByteArray Examples::jsonBasicsHello()
{
    const ShaderProject p = makeBasicsProject(QStringLiteral("Basics / Hello"));
    const QByteArray fallback = QJsonDocument(p.toJson()).toJson(QJsonDocument::Indented);
    return readResource(QStringLiteral(":/examples/basics/hello.json"), fallback);
}

QByteArray Examples::jsonBasicsWarm()
{
    const QString warmFrag = R"(
cbuffer Host : register(b0)
{
    float time_sim_ms;
    float time_wall_ms;
    float delta_time_ms;
    uint  frame_index;
    float resolution_x;
    float resolution_y;
    float mouse_x;
    float mouse_y;
    uint  mouse_buttons;
    float scroll_delta_x;
    float scroll_delta_y;
};
struct VSOut { float4 pos : SV_POSITION; };
float4 PSMain(VSOut i) : SV_Target
{
    float2 p = i.pos.xy / float2(resolution_x, resolution_y);
    p.y = 1.0f - p.y;
    return float4(p.x, p.y * 0.7f + 0.2f, 0.15f, 1.0f);
}
)";
    const ShaderProject p = makeBasicsProject(QStringLiteral("Basics / Warm tint"), warmFrag);
    const QByteArray fallback = QJsonDocument(p.toJson()).toJson(QJsonDocument::Indented);
    return readResource(QStringLiteral(":/examples/basics/warm.json"), fallback);
}

QByteArray Examples::jsonBasicsMeshObj()
{
    QFile f(QStringLiteral(":/examples/basics/mesh_obj.json"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

QByteArray Examples::jsonRtMinimalCompileTest()
{
    QFile f(QStringLiteral(":/examples/rt/minimal.json"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}
