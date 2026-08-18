#include "ShaderInterop.h"

[[vk::binding(BINDING_CUBEMAPS, SET_GLOBAL)]] TextureCube   Sky[MAX_CUBEMAPS];
[[vk::binding(BINDING_SAMPLER,  SET_GLOBAL)]] SamplerState  Samp;

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
    [[vk::location(0)]] float3 Direction : TEXCOORD0;
};

vs_output VSMain(uint vertexID : SV_VertexID)
{
    float2 NDC = Positions[vertexID];

    camera_uniforms cam = LoadCamera(LoadSkyboxParams(pc.ParamsPtr).Camera);

    vs_output output;
    output.Position  = float4(NDC, 1.0, 1.0);
    output.Direction = cam.SkyRight.xyz * NDC.x + cam.SkyUp.xyz * NDC.y + cam.SkyForward.xyz;
    return output;
}

float4 PSMain(vs_output input) : SV_Target
{
    skybox_params params = LoadSkyboxParams(pc.ParamsPtr);

    float3 Radiance = Sky[params.CubemapIndex].Sample(Samp, normalize(input.Direction)).rgb * params.Tint.rgb;

    return float4(Radiance, 1.0);
}
