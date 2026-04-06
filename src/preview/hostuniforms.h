#ifndef HOSTUNIFORMS_H
#define HOSTUNIFORMS_H

#include <cstdint>

// Matches HLSL cbuffer Host : register(b0) { ... } — std140-style scalar packing (see .plan/PLAN.md appendix).
struct HostUniforms {
    float time_sim_ms = 0.f;
    float time_wall_ms = 0.f;
    float delta_time_ms = 0.f;
    std::uint32_t frame_index = 0;
    float resolution_x = 1.f;
    float resolution_y = 1.f;
    float mouse_x = 0.f;
    float mouse_y = 0.f;
    std::uint32_t mouse_buttons = 0;
    float scroll_delta_x = 0.f;
    float scroll_delta_y = 0.f;
};

static constexpr std::size_t kHostUniformsSize = sizeof(HostUniforms);

#endif
