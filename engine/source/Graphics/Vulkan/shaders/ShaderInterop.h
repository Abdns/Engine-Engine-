#ifndef SHADERINTEROP_H
#define SHADERINTEROP_H

#ifdef __cplusplus
    #define float4x4 Matrix4
    #define float4   Vector4
    #define uint     uint32
#endif

#define SET_GLOBAL       0
#define SET_PER_FRAME    1
#define SET_PER_MATERIAL 2
#define SET_PER_OBJECT   3

#define MAX_TEXTURES 16

struct camera_uniforms
{
    float4x4 ViewProj;
};

struct object_uniforms
{
    float4 Tint;
    uint   TextureIndex;
};

struct primitive_push_constants
{
    float4x4 Model;
};

#ifdef __cplusplus
    #undef float4x4
    #undef float4
    #undef uint
#endif

#endif
