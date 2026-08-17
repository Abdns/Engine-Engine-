#include "ShaderInterop.h"

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

vs_output main(uint vertexID : SV_VertexID)
{
    float2 NDC = Positions[vertexID];

    vs_output output;
    output.Position = float4(NDC, 0.0, 1.0);
    output.UV       = NDC * 0.5 + 0.5;
    return output;
}
