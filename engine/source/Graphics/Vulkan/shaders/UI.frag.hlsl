#include "ShaderInterop.h"

[[vk::binding(BINDING_PIPELINE_IMAGE, SET_PIPELINE)]] Texture2D FrameTexture;

[[vk::binding(BINDING_SAMPLER, SET_GLOBAL)]] SamplerState Samp;

struct ps_input
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
};

float4 main(ps_input input) : SV_Target
{
    float3 Color = FrameTexture.Sample(Samp, input.UV).rgb;

    float2 FromCenter = abs(input.UV - 0.5);
    bool CrosshairV = (FromCenter.x < 0.0015 && FromCenter.y < 0.012);
    bool CrosshairH = (FromCenter.y < 0.0025 && FromCenter.x < 0.007);
    if (CrosshairV || CrosshairH)
    {
        Color = 1.0 - Color;
    }

    return float4(Color, 1.0);
}
