#include "ShaderInterop.h"

[[vk::binding(0, SET_PER_FRAME)]] ConstantBuffer<camera_uniforms> cam;

[[vk::push_constant]] primitive_push_constants pc;

struct vs_input
{
    [[vk::location(0)]] float3 Pos   : POSITION;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 UV    : TEXCOORD0;
};

struct vs_output
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 Color : COLOR0;
    [[vk::location(1)]] float2 UV    : TEXCOORD0;
};

vs_output main(vs_input input)
{
    vs_output output;
    output.Position = mul(cam.ViewProj, mul(pc.Model, float4(input.Pos, 1.0)));
    output.Color    = input.Color;
    output.UV       = input.UV;
    return output;
}
