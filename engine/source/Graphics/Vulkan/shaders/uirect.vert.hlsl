#include "ShaderInterop.h"

[[vk::push_constant]] draw_push_constants pc;

static const float2 Corners[6] =
{
    float2(0.0, 0.0),
    float2(1.0, 0.0),
    float2(0.0, 1.0),
    float2(1.0, 0.0),
    float2(1.0, 1.0),
    float2(0.0, 1.0),
};

struct vs_output
{
    float4 Position : SV_Position;
};

vs_output main(uint vertexID : SV_VertexID)
{
    float2 NDC = lerp(pc.Rect.xy, pc.Rect.zw, Corners[vertexID]);

    vs_output output;
    output.Position = float4(NDC, 0.0, 1.0);
    return output;
}
