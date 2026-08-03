#include "ShaderInterop.h"

[[vk::binding(BINDING_PIPELINE_IMAGE, SET_PIPELINE)]] Texture2D SceneTexture;

[[vk::binding(BINDING_SAMPLER, SET_GLOBAL)]] SamplerState Samp;

struct ps_input
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
};

float4 main(ps_input input) : SV_Target
{
    float3 Color = SceneTexture.Sample(Samp, input.UV).rgb;

    Color = Color / (Color + 1.0);

    return float4(Color, 1.0);
}
