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

static QByteArray readResourceOnly(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
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
    float drag_accum_x;
    float drag_accum_y;
    float scroll_accum_x;
    float scroll_accum_y;
    float pan_accum_x;
    float pan_accum_y;
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
    return readResourceOnly(QStringLiteral(":/examples/basics/mesh_obj.json"));
}

QByteArray Examples::jsonRtMinimalCompileTest()
{
    return readResourceOnly(QStringLiteral(":/examples/rt/minimal.json"));
}

// --- Effects ---
QByteArray Examples::jsonEffectsFire() { return readResourceOnly(QStringLiteral(":/examples/effects/fire.json")); }
QByteArray Examples::jsonEffectsPlasma() { return readResourceOnly(QStringLiteral(":/examples/effects/plasma.json")); }
QByteArray Examples::jsonEffectsWaterRipples() { return readResourceOnly(QStringLiteral(":/examples/effects/water_ripples.json")); }
QByteArray Examples::jsonEffectsStarfield() { return readResourceOnly(QStringLiteral(":/examples/effects/starfield.json")); }
QByteArray Examples::jsonEffectsAurora() { return readResourceOnly(QStringLiteral(":/examples/effects/aurora.json")); }
QByteArray Examples::jsonEffectsLavaLamp() { return readResourceOnly(QStringLiteral(":/examples/effects/lava_lamp.json")); }

// --- Lighting ---
QByteArray Examples::jsonLightingToon() { return readResourceOnly(QStringLiteral(":/examples/lighting/toon.json")); }
QByteArray Examples::jsonLightingPhong() { return readResourceOnly(QStringLiteral(":/examples/lighting/phong.json")); }
QByteArray Examples::jsonLightingRimLight() { return readResourceOnly(QStringLiteral(":/examples/lighting/rim_light.json")); }
QByteArray Examples::jsonLightingMatcap() { return readResourceOnly(QStringLiteral(":/examples/lighting/matcap.json")); }
QByteArray Examples::jsonLightingWireframe() { return readResourceOnly(QStringLiteral(":/examples/lighting/wireframe.json")); }
QByteArray Examples::jsonLightingNormals() { return readResourceOnly(QStringLiteral(":/examples/lighting/normals.json")); }
QByteArray Examples::jsonLightingDynamicPointLight() { return readResourceOnly(QStringLiteral(":/examples/lighting/dynamic_point_light.json")); }
QByteArray Examples::jsonLightingOrbitLight() { return readResourceOnly(QStringLiteral(":/examples/lighting/orbit_light.json")); }
QByteArray Examples::jsonLightingMultiLight() { return readResourceOnly(QStringLiteral(":/examples/lighting/multi_light.json")); }

// --- Textures ---
QByteArray Examples::jsonTexturesTexturedQuad() { return readResourceOnly(QStringLiteral(":/examples/textures/textured_quad.json")); }
QByteArray Examples::jsonTexturesDistortion() { return readResourceOnly(QStringLiteral(":/examples/textures/distortion.json")); }
QByteArray Examples::jsonTexturesNormalMap() { return readResourceOnly(QStringLiteral(":/examples/textures/normal_map.json")); }
QByteArray Examples::jsonTexturesMultiBlend() { return readResourceOnly(QStringLiteral(":/examples/textures/multi_blend.json")); }

// --- Post-processing ---
QByteArray Examples::jsonPostprocessCrt() { return readResourceOnly(QStringLiteral(":/examples/postprocess/crt.json")); }
QByteArray Examples::jsonPostprocessPixelation() { return readResourceOnly(QStringLiteral(":/examples/postprocess/pixelation.json")); }
QByteArray Examples::jsonPostprocessChromaticAberration() { return readResourceOnly(QStringLiteral(":/examples/postprocess/chromatic_aberration.json")); }
QByteArray Examples::jsonPostprocessVignette() { return readResourceOnly(QStringLiteral(":/examples/postprocess/vignette.json")); }
QByteArray Examples::jsonPostprocessEdgeDetection() { return readResourceOnly(QStringLiteral(":/examples/postprocess/edge_detection.json")); }
QByteArray Examples::jsonPostprocessGlitch() { return readResourceOnly(QStringLiteral(":/examples/postprocess/glitch.json")); }

// --- Patterns ---
QByteArray Examples::jsonPatternsMandelbrot() { return readResourceOnly(QStringLiteral(":/examples/patterns/mandelbrot.json")); }
QByteArray Examples::jsonPatternsVoronoi() { return readResourceOnly(QStringLiteral(":/examples/patterns/voronoi.json")); }
QByteArray Examples::jsonPatternsTruchet() { return readResourceOnly(QStringLiteral(":/examples/patterns/truchet.json")); }
QByteArray Examples::jsonPatternsSdfRaymarch() { return readResourceOnly(QStringLiteral(":/examples/patterns/sdf_raymarch.json")); }
QByteArray Examples::jsonPatternsKaleidoscope() { return readResourceOnly(QStringLiteral(":/examples/patterns/kaleidoscope.json")); }
QByteArray Examples::jsonPatternsMoire() { return readResourceOnly(QStringLiteral(":/examples/patterns/moire.json")); }

// --- Multipass ---
QByteArray Examples::jsonMultipassBloom() { return readResourceOnly(QStringLiteral(":/examples/multipass/bloom.json")); }
QByteArray Examples::jsonMultipassDeferred() { return readResourceOnly(QStringLiteral(":/examples/multipass/deferred.json")); }
QByteArray Examples::jsonMultipassShadowMap() { return readResourceOnly(QStringLiteral(":/examples/multipass/shadow_map.json")); }
QByteArray Examples::jsonMultipassEdgeGlow() { return readResourceOnly(QStringLiteral(":/examples/multipass/edge_glow.json")); }

// --- Blending ---
QByteArray Examples::jsonBlendAdditiveGlow() { return readResourceOnly(QStringLiteral(":/examples/blend/additive_glow.json")); }
QByteArray Examples::jsonBlendTransparentLayers() { return readResourceOnly(QStringLiteral(":/examples/blend/transparent_layers.json")); }

// --- Compute ---
QByteArray Examples::jsonComputeGameOfLife() { return readResourceOnly(QStringLiteral(":/examples/compute/game_of_life.json")); }
QByteArray Examples::jsonComputeFluidSim() { return readResourceOnly(QStringLiteral(":/examples/compute/fluid_sim.json")); }
QByteArray Examples::jsonComputeImageBlur() { return readResourceOnly(QStringLiteral(":/examples/compute/image_blur.json")); }

// --- Tessellation ---
QByteArray Examples::jsonTessellationDisplacement() { return readResourceOnly(QStringLiteral(":/examples/tessellation/displacement.json")); }
QByteArray Examples::jsonTessellationPnTriangles() { return readResourceOnly(QStringLiteral(":/examples/tessellation/pn_triangles.json")); }
QByteArray Examples::jsonTessellationAdaptiveLod() { return readResourceOnly(QStringLiteral(":/examples/tessellation/adaptive_lod.json")); }

// --- Ray tracing ---
QByteArray Examples::jsonRtReflections() { return readResourceOnly(QStringLiteral(":/examples/rt/reflections.json")); }
QByteArray Examples::jsonRtShadows() { return readResourceOnly(QStringLiteral(":/examples/rt/shadows.json")); }
QByteArray Examples::jsonRtProceduralGeo() { return readResourceOnly(QStringLiteral(":/examples/rt/procedural_geo.json")); }
QByteArray Examples::jsonRtCallableMaterials() { return readResourceOnly(QStringLiteral(":/examples/rt/callable_materials.json")); }
QByteArray Examples::jsonRtAnyhitTransparency() { return readResourceOnly(QStringLiteral(":/examples/rt/anyhit_transparency.json")); }

// --- Subgroup / Wave ---
QByteArray Examples::jsonSubgroupWaveReduction() { return readResourceOnly(QStringLiteral(":/examples/subgroup/wave_reduction.json")); }
QByteArray Examples::jsonSubgroupWavePrefixScan() { return readResourceOnly(QStringLiteral(":/examples/subgroup/wave_prefix_scan.json")); }
