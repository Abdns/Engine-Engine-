#include "ShaderInterop.h"

[[vk::binding(0, SET_GLOBAL)]] Texture2D    Tex[MAX_TEXTURES];
[[vk::binding(1, SET_GLOBAL)]] SamplerState Samp;

[[vk::push_constant]] primitive_push_constants pc;

struct ps_input
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 Color : COLOR0;
    [[vk::location(1)]] float2 UV    : TEXCOORD0;
};

float4 main(ps_input input) : SV_Target
{
    return Tex[pc.TextureIndex].Sample(Samp, input.UV) * pc.Tint;
}
