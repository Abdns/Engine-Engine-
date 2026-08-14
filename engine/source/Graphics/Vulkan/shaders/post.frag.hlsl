#include "ShaderInterop.h"

[[vk::binding(BINDING_TEXTURES, SET_GLOBAL)]] Texture2D    Tex[TEXTURE_HEAP_SIZE];
[[vk::binding(BINDING_SAMPLER,  SET_GLOBAL)]] SamplerState Samp;

[[vk::push_constant]] push_constants pc;

struct ps_input
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
};

float4 main(ps_input input) : SV_Target
{
    image_params_ptr params = image_params_ptr(pc.Params);

    float3 Color = Tex[params.Get().TextureSlot].Sample(Samp, input.UV).rgb;

    Color = Color / (Color + 1.0);

    return float4(Color, 1.0);
}
