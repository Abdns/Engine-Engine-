#include "ShaderInterop.h"

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

vs_output main(uint vertexID : SV_VertexID)
{
    float2 NDC = Positions[vertexID];

    camera_ptr cam = camera_ptr(skybox_params_ptr(pc.Params).Get().Camera);

    vs_output output;
    output.Position  = float4(NDC, 1.0, 1.0);
    output.Direction = cam.Get().SkyRight.xyz * NDC.x + cam.Get().SkyUp.xyz * NDC.y + cam.Get().SkyForward.xyz;
    return output;
}
