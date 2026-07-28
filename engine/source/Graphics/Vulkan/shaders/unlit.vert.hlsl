#include "ShaderInterop.h"

[[vk::binding(BINDING_VERTICES, SET_GLOBAL)]] StructuredBuffer<vertex> Vertices;

[[vk::binding(BINDING_CAMERA, SET_GLOBAL)]] ConstantBuffer<camera_uniforms> cam;

[[vk::push_constant]] draw_push_constants pc;

struct vs_output
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 Color : COLOR0;
    [[vk::location(1)]] float2 UV    : TEXCOORD0;
};

vs_output main(uint vertexID : SV_VertexID)
{
    vertex v = Vertices[vertexID];

    vs_output output;
    output.Position = mul(cam.ViewProj, mul(pc.Model, float4(v.Pos, 1.0)));
    output.Color    = v.Color;
    output.UV       = v.UV;
    return output;
}
