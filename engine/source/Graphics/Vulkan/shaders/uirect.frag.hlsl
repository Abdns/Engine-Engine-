#include "ShaderInterop.h"

[[vk::push_constant]] draw_push_constants pc;

float4 main() : SV_Target
{
    return pc.Tint;
}
