#include "ShaderInterop.h"

[[vk::binding(BINDING_CAMERA, SET_GLOBAL)]] ConstantBuffer<camera_uniforms> cam;

struct vs_output
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 Direction : TEXCOORD0;
};

vs_output main(uint vertexID : SV_VertexID)
{
    float2 NDC = float2((vertexID << 1) & 2, vertexID & 2) * 2.0 - 1.0;

    vs_output output;
    output.Position  = float4(NDC, 1.0, 1.0);
    output.Direction = cam.SkyRight.xyz * NDC.x + cam.SkyUp.xyz * NDC.y + cam.SkyForward.xyz;
    return output;
}
