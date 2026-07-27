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

#define MAX_TEXTURES 16

struct camera_uniforms
{
    float4x4 ViewProj;
};

// Per-draw data rides in push constants: 84 bytes, comfortably under the 128 every
// implementation guarantees. No buffer, no descriptor set, no per-draw binding
struct primitive_push_constants
{
    float4x4 Model;
    float4   Tint;
    uint     TextureIndex;
};

#ifdef __cplusplus
    #undef float4x4
    #undef float4
    #undef uint
#endif

#endif
