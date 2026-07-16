#include "ShaderInterop.h"

[[vk::combinedImageSampler]] [[vk::binding(0, SET_PER_MATERIAL)]] Texture2D    Tex;
[[vk::combinedImageSampler]] [[vk::binding(0, SET_PER_MATERIAL)]] SamplerState Samp;

[[vk::binding(0, SET_PER_OBJECT)]] ConstantBuffer<object_uniforms> obj;

struct ps_input
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 Color : COLOR0;
    [[vk::location(1)]] float2 UV    : TEXCOORD0;
};

float4 main(ps_input input) : SV_Target
{
    return Tex.Sample(Samp, input.UV) * obj.Tint;
}
