#include "ShaderInterop.h"

[[vk::binding(BINDING_TEXTURES, SET_GLOBAL)]] Texture2D    Tex[TEXTURE_HEAP_SIZE];
[[vk::binding(BINDING_SAMPLER,  SET_GLOBAL)]] SamplerState Samp;

[[vk::push_constant]] push_constants pc;

struct ps_input
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
    [[vk::location(1)]] nointerpolation uint Index : TEXCOORD1;
};

float4 main(ps_input input) : SV_Target
{
    rect_params_ptr params = rect_params_ptr(pc.Params + (uint64_t)input.Index * RECT_PARAMS_STRIDE);

    float4 Color = params.Get().Tint;

    if (params.Get().TextureSlot != TEXTURE_NONE)
    {
        Color *= Tex[params.Get().TextureSlot].Sample(Samp, input.UV);
    }

    return Color;
}
