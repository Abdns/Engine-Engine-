#ifndef SHADERINTEROP_H
#define SHADERINTEROP_H

#ifdef __cplusplus
    #define float4x4 Matrix4
    #define float4   Vector4
    #define float3   Vector3
    #define float2   Vector2
    #define uint     uint32
#endif

#define SET_GLOBAL       0

#define BINDING_TEXTURES 0
#define BINDING_SAMPLER  1
#define BINDING_VERTICES 2
#define BINDING_CAMERA   3

#define MAX_TEXTURES 16

struct vertex
{
    float3 Pos;
    float3 Color;
    float2 UV;
};

struct camera_uniforms
{
    float4x4 ViewProj;
};

struct draw_push_constants
{
    float4x4 Model;
    float4   Tint;
    uint     TextureIndex;
};

#ifdef __cplusplus
    #undef float4x4
    #undef float4
    #undef float3
    #undef float2
    #undef uint
#endif

#endif
