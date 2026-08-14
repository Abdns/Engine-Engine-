#include "ShaderInterop.h"

[[vk::binding(BINDING_TEXTURES, SET_GLOBAL)]] Texture2D    Tex[TEXTURE_HEAP_SIZE];
[[vk::binding(BINDING_SAMPLER,  SET_GLOBAL)]] SamplerState Samp;

[[vk::push_constant]] push_constants pc;

struct ps_input
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 Color : COLOR0;
    [[vk::location(1)]] float2 UV    : TEXCOORD0;
};

float4 main(ps_input input) : SV_Target
{
    draw_params_ptr params = draw_params_ptr(pc.Params);

    gpu_material Material = vk::RawBufferLoad<gpu_material>(params.Get().Materials + (uint64_t)params.Get().MaterialSlot * MATERIAL_STRIDE, 16);

    return Tex[Material.TextureSlot].Sample(Samp, input.UV) * Material.BaseColor * params.Get().Tint;
}
