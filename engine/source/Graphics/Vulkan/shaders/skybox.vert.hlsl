#include "ShaderInterop.h"

[[vk::push_constant]] push_constants pc;

struct vs_output
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 Direction : TEXCOORD0;
};

vs_output main(uint vertexID : SV_VertexID)
{
    float2 NDC = float2((vertexID << 1) & 2, vertexID & 2) * 2.0 - 1.0;

    camera_ptr cam = camera_ptr(skybox_params_ptr(pc.Params).Get().Camera);

    vs_output output;
    output.Position  = float4(NDC, 1.0, 1.0);
    output.Direction = cam.Get().SkyRight.xyz * NDC.x + cam.Get().SkyUp.xyz * NDC.y + cam.Get().SkyForward.xyz;
    return output;
}
