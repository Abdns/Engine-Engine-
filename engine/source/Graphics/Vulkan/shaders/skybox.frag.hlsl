#include "ShaderInterop.h"

[[vk::binding(BINDING_CUBEMAPS, SET_GLOBAL)]] TextureCube   Sky[MAX_CUBEMAPS];
[[vk::binding(BINDING_SAMPLER,  SET_GLOBAL)]] SamplerState  Samp;

[[vk::push_constant]] draw_push_constants pc;

struct ps_input
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 Direction : TEXCOORD0;
};

float4 main(ps_input input) : SV_Target
{
    float3 Radiance = Sky[pc.CubemapIndex].Sample(Samp, normalize(input.Direction)).rgb * pc.Tint.rgb;

    return float4(Radiance, 1.0);
}
