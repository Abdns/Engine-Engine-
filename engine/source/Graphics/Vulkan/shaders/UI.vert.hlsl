#include "ShaderInterop.h"

struct vs_output
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
};

vs_output main(uint vertexID : SV_VertexID)
{
    float2 NDC = float2((vertexID << 1) & 2, vertexID & 2) * 2.0 - 1.0;

    vs_output output;
    output.Position = float4(NDC, 0.0, 1.0);
    output.UV       = NDC * 0.5 + 0.5;
    return output;
}
