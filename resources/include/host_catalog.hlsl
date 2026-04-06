// ShaderTool host uniform catalog (v1) — include prefix in your cbuffer so CPU offsets match.
// See .plan/PLAN.md appendix. Register must match the preview engine (b0, set 0).

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
