#include "enums.h"

// Map of Vulkan shader stage flag bits to GLSL shader stage strings
const QMap<VkShaderStageFlagBits, QString> mapShaderStageToGLSLString = {
    { VK_SHADER_STAGE_VERTEX_BIT, "vert" },
    { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tesc" },
    { VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tese" },
    { VK_SHADER_STAGE_GEOMETRY_BIT, "geom" },
    { VK_SHADER_STAGE_FRAGMENT_BIT, "frag" },
    { VK_SHADER_STAGE_COMPUTE_BIT, "comp" },
    { VK_SHADER_STAGE_RAYGEN_BIT_KHR, "rgen" },
    { VK_SHADER_STAGE_ANY_HIT_BIT_KHR, "rahit" },
    { VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "rchit" },
    { VK_SHADER_STAGE_MISS_BIT_KHR, "rmiss" },
    { VK_SHADER_STAGE_INTERSECTION_BIT_KHR, "rint" },
    { VK_SHADER_STAGE_CALLABLE_BIT_KHR, "rcall" }
};

// Map of Vulkan shader stage flag bits to GLSL shader stage strings
const QVector<VkShaderStageFlagBits> shaderStageOrder = {
    VK_SHADER_STAGE_VERTEX_BIT,
    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    VK_SHADER_STAGE_GEOMETRY_BIT,
    VK_SHADER_STAGE_FRAGMENT_BIT,
    VK_SHADER_STAGE_COMPUTE_BIT,
    VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
    VK_SHADER_STAGE_MISS_BIT_KHR,
    VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
    VK_SHADER_STAGE_CALLABLE_BIT_KHR
};

const QMap<QString, VkShaderStageFlagBits> mapTabTitleToShaderStage = {
    { "Vertex", VK_SHADER_STAGE_VERTEX_BIT },
    { "Tesselation Control", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT },
    { "Tesselation Evaluation", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT },
    { "Geometry", VK_SHADER_STAGE_GEOMETRY_BIT },
    { "Fragment", VK_SHADER_STAGE_FRAGMENT_BIT },
    { "Compute", VK_SHADER_STAGE_COMPUTE_BIT },
    { "Ray Gen", VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    { "Ray Any Hit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR },
    { "Ray Closest Hit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
    { "Ray Miss", VK_SHADER_STAGE_MISS_BIT_KHR },
    { "Ray Intersection", VK_SHADER_STAGE_INTERSECTION_BIT_KHR },
    { "Ray Callable", VK_SHADER_STAGE_CALLABLE_BIT_KHR }
};

const QVector<QString> tabOrder = {
    "Vertex", "Tesselation Control", "Tesselation Evaluation", "Geometry", "Fragment",
    "Compute", "Ray Gen", "Ray Any Hit", "Ray Closest Hit",  "Ray Miss", "Ray Intersection", "Ray Callable"
};


QMap<ShaderType, QVector<VkShaderStageFlagBits>> mapShaderTypes = {
    {ShaderType::Rasterisation, {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        VK_SHADER_STAGE_GEOMETRY_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT
    }},
    {ShaderType::RayTracing, {
        VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        VK_SHADER_STAGE_MISS_BIT_KHR,
        VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
        VK_SHADER_STAGE_CALLABLE_BIT_KHR
    }}
};

QMap<ShaderType, QString> mapShaderTypeToString = {
    {ShaderType::Rasterisation, "Rasterisation"},
    {ShaderType::RayTracing, "Ray Tracing"}
};

QMap<VkShaderStageFlagBits, QString> createReverseLookupMap(const QMap<QString, VkShaderStageFlagBits>& originalMap) {
    QMap<VkShaderStageFlagBits, QString> reverseMap;
    for (auto it = originalMap.begin(); it != originalMap.end(); ++it) {
        reverseMap.insert(it.value(), it.key());
    }
    return reverseMap;
}

const QMap<VkShaderStageFlagBits, QString> mapShaderStageToTabTitle = createReverseLookupMap(mapTabTitleToShaderStage);


QSet<VkShaderStageFlagBits> MANDATORY_SHADERS = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};

const QString DEFAULT_GEOMETRY = QStringLiteral("");

const QString DEFAULT_VERTEX = R"(
struct VSOut {
    float4 pos : SV_POSITION;
};

VSOut VSMain(uint vid : SV_VertexID)
{
    VSOut o;
    float2 tri[3] = {
        float2(0.0f, -0.5f),
        float2(0.5f, 0.5f),
        float2(-0.5f, 0.5f)
    };
    o.pos = float4(tri[vid], 0.0f, 1.0f);
    return o;
}
)";

const QString DEFAULT_FRAGMENT = R"(
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

struct VSOut {
    float4 pos : SV_POSITION;
};

float4 PSMain(VSOut i) : SV_Target
{
    float2 p = i.pos.xy / float2(resolution_x, resolution_y);
    p.y = 1.0f - p.y;
    float t = time_sim_ms * 0.001f;
    return float4(p.x * (0.5f + 0.5f * sin(t)),
                  p.y * (0.5f + 0.5f * cos(t)),
                  0.3f + 0.2f * sin(t + p.x * 6.28f),
                  1.0f);
}
)";

const QMap<VkShaderStageFlagBits, QString> DEFAULT_RASTERIZER = {
    {VK_SHADER_STAGE_VERTEX_BIT, DEFAULT_VERTEX},
    {VK_SHADER_STAGE_FRAGMENT_BIT, DEFAULT_FRAGMENT},
};

