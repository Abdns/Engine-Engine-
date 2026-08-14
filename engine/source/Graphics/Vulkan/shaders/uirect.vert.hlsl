#include "ShaderInterop.h"

[[vk::push_constant]] push_constants pc;

static const float2 Corners[6] =
{
    float2(0.0, 0.0),
    float2(1.0, 0.0),
    float2(0.0, 1.0),
    float2(1.0, 0.0),
    float2(1.0, 1.0),
    float2(0.0, 1.0),
};

struct vs_output
{
    float4 Position : SV_Position;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
    [[vk::location(1)]] nointerpolation uint Index : TEXCOORD1;
};

vs_output main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    rect_params_ptr params = rect_params_ptr(pc.Params + (uint64_t)instanceID * RECT_PARAMS_STRIDE);

    float2 Corner = Corners[vertexID];

    vs_output output;
    output.Position = float4(lerp(params.Get().Rect.xy, params.Get().Rect.zw, Corner), 0.0, 1.0);
    output.UV       = lerp(params.Get().UVRect.xy, params.Get().UVRect.zw, Corner);
    output.Index    = instanceID;
    return output;
}
