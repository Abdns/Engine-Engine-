#include "ShaderInterop.h"

[[vk::binding(BINDING_TEXTURES, SET_GLOBAL)]] Texture2D    Tex[MAX_TEXTURES];
[[vk::binding(BINDING_SAMPLER,  SET_GLOBAL)]] SamplerState Samp;

[[vk::push_constant]] draw_push_constants pc;

struct ps_input
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
};

float4 main(ps_input input) : SV_Target
{
    float4 Color = pc.Tint;

    if (pc.TextureSlot)
    {
        Color *= Tex[pc.TextureSlot - 1].Sample(Samp, input.UV);
    }

    return Color;
}
