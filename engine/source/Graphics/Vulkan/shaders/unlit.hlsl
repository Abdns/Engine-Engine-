#include "ShaderInterop.h"

[[vk::binding(BINDING_TEXTURES, SET_GLOBAL)]] Texture2D    Tex[TEXTURE_HEAP_SIZE];
[[vk::binding(BINDING_SAMPLER,  SET_GLOBAL)]] SamplerState Samp;

[[vk::push_constant]] push_constants pc;

struct vs_output
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 Color : COLOR0;
    [[vk::location(1)]] float2 UV    : TEXCOORD0;
};

vs_output VSMain(uint vertexID : SV_VertexID)
{
    draw_params params = LoadDrawParams(pc.ParamsPtr);

    vertex v = LoadVertex(params.Vertices, vertexID);

    camera_uniforms cam = LoadCamera(params.Camera);

    vs_output output;
    output.Position = mul(cam.ViewProj, mul(params.Model, float4(v.Pos, 1.0)));
    output.Color    = v.Color;
    output.UV       = v.UV;
    return output;
}

float4 PSMain(vs_output input) : SV_Target
{
    draw_params params = LoadDrawParams(pc.ParamsPtr);

    gpu_material Material = LoadMaterial(params.Materials, params.MaterialSlot);

    return Tex[Material.TextureSlot].Sample(Samp, input.UV) * Material.BaseColor * params.Tint;
}
