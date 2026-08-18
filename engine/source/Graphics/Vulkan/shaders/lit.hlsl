#include "ShaderInterop.h"

[[vk::push_constant]] push_constants pc;

struct vs_output
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float3 WorldPos : TEXCOORD0;
};

vs_output VSMain(uint vertexID : SV_VertexID)
{
    draw_params params = LoadDrawParams(pc.ParamsPtr);

    vertex v = LoadVertex(params.Vertices, vertexID);

    frame_globals globals = LoadGlobals(pc.GlobalsPtr);

    float4 WorldPos = mul(params.Model, float4(v.Position, 1.0));

    vs_output output;
    output.Position = mul(globals.ViewProj, WorldPos);
    output.WorldPos = WorldPos.xyz;
    return output;
}

float4 PSMain(vs_output input) : SV_Target
{
    draw_params params = LoadDrawParams(pc.ParamsPtr);

    frame_globals globals = LoadGlobals(pc.GlobalsPtr);

    gpu_material Material = LoadMaterial(params.Materials, params.MaterialSlot);

    float3 Normal   = normalize(cross(ddy(input.WorldPos), ddx(input.WorldPos)));
    float3 LightDir = normalize(globals.LightDir);

    float Diffuse = saturate(dot(Normal, LightDir));

    float3 Color = Material.BaseColor.rgb * params.Tint.rgb * (0.1 + 0.9 * Diffuse);

    return float4(Color, Material.BaseColor.a * params.Tint.a);
}
