#include "ShaderInterop.h"

[[vk::binding(BINDING_TEXTURES, SET_GLOBAL)]] Texture2D    Tex[TEXTURE_HEAP_SIZE];
[[vk::binding(BINDING_SAMPLER,  SET_GLOBAL)]] SamplerState Samp;

[[vk::push_constant]] push_constants pc;

static const float2 Positions[3] =
{
    float2(-1.0, -1.0),
    float2( 3.0, -1.0),
    float2(-1.0,  3.0),
};

struct vs_output
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
};

vs_output VSMain(uint vertexID : SV_VertexID)
{
    float2 NDC = Positions[vertexID];

    vs_output output;
    output.Position = float4(NDC, 0.0, 1.0);
    output.UV       = NDC * 0.5 + 0.5;
    return output;
}

float4 PSMain(vs_output input) : SV_Target
{
    image_params params = LoadImageParams(pc.ParamsPtr);

    float3 Color = Tex[params.TextureSlot].Sample(Samp, input.UV).rgb;

    Color = Color / (Color + 1.0);

    return float4(Color, 1.0);
}
