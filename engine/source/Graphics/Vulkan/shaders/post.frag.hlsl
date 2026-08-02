#include "ShaderInterop.h"

[[vk::binding(BINDING_TEXTURES, SET_GLOBAL)]] Texture2D    Tex[MAX_TEXTURES];
[[vk::binding(BINDING_SAMPLER,  SET_GLOBAL)]] SamplerState Samp;

struct ps_input
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
};

float4 main(ps_input input) : SV_Target
{
    float3 Color = Tex[TEXTURE_SLOT_SCENE].Sample(Samp, input.UV).rgb;

    Color = Color / (Color + 1.0);

    return float4(Color, 1.0);
}
