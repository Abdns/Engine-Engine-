#include "ShaderInterop.h"

[[vk::push_constant]] push_constants pc;

struct vs_output
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 Color : COLOR0;
    [[vk::location(1)]] float2 UV    : TEXCOORD0;
};

vs_output main(uint vertexID : SV_VertexID)
{
    draw_params_ptr params = draw_params_ptr(pc.Params);

    vertex v = vk::RawBufferLoad<vertex>(params.Get().Vertices + (uint64_t)vertexID * VERTEX_STRIDE, 4);

    camera_ptr cam = camera_ptr(params.Get().Camera);

    vs_output output;
    output.Position = mul(cam.Get().ViewProj, mul(params.Get().Model, float4(v.Pos, 1.0)));
    output.Color    = v.Color;
    output.UV       = v.UV;
    return output;
}
