#ifndef RENDERCOMMANDS_H
#define RENDERCOMMANDS_H

#include "Types.h"
#include "EngineMath.h"

enum command_type
{
    Render_Mesh = 0,
    Render_Camera,
    Render_Skybox,
    Render_Rect,
    Set_Pipeline,
    Load_Mesh,
    Load_Texture,
    Load_Cubemap,
    Load_Material,
};

enum cull_mode
{
    Cull_None = 0,
    Cull_Back,
    Cull_Front,
};

enum texture_format
{
    TextureFormat_RGBA8 = 0,
    TextureFormat_RGBA16F,
};

enum blend_mode
{
    Blend_Opaque = 0,
    Blend_Alpha,
};

enum pipeline_type
{
    Pipeline_Unlit = 0,
    Pipeline_Skybox,
    Pipeline_Post,
    Pipeline_UI,
    Pipeline_UIRect,
    Pipeline_Count,

    Pipeline_MeshCount = Pipeline_Skybox,
};

struct command_render_mesh
{
    command_type   Type;
    Matrix4        Transform;
    Vector4        Tint;
    uint32         MeshHandle;
    uint32         MaterialHandle;
};

struct command_render_skybox
{
    command_type   Type;
    uint32 CubemapHandle;
};

struct command_render_rect
{
    command_type Type;
    Vector2      Min;
    Vector2      Max;
    Vector4      Color;
    Vector4      UV;
    uint32       TextureSlot;
};

struct command_load_mesh
{
    command_type Type;
    uint32       MeshHandle;
    void        *Vertices;
    uint32       VertexCount;
    uint32      *Indices;
    uint32       IndexCount;
};

struct command_load_texture
{
    command_type   Type;
    uint32         TextureHandle;
    void          *Pixels;
    uint32         Width;
    uint32         Height;
    uint32         SRGB;
    texture_format Format;
};

struct command_load_cubemap
{
    command_type   Type;
    uint32         CubemapHandle;
    void          *Pixels;
    uint32         FaceSize;
    texture_format Format;
};

struct command_load_material
{
    command_type  Type;
    uint32        MaterialHandle;

    pipeline_type Pipeline;
    cull_mode     CullMode;
    blend_mode    BlendMode;
    bool32        DepthTest;
    bool32        DepthWrite;

    Vector4       BaseColor;
    uint32        TextureHandle;
};

struct command_render_camera
{
    command_type Type;
    Matrix4 View;
    real32  FovY;
};

struct command_set_pipeline
{
    command_type Type;
    pipeline_type PipelineType;
};

inline uint32 CommandSize(command_type Type)
{
    switch (Type)
    {
        case Render_Mesh:        return (uint32)sizeof(command_render_mesh);
        case Render_Skybox:      return (uint32)sizeof(command_render_skybox);
        case Render_Rect:        return (uint32)sizeof(command_render_rect);
        case Render_Camera:      return (uint32)sizeof(command_render_camera);
        case Load_Mesh:          return (uint32)sizeof(command_load_mesh);
        case Load_Texture:       return (uint32)sizeof(command_load_texture);
        case Load_Cubemap:       return (uint32)sizeof(command_load_cubemap);
        case Load_Material:      return (uint32)sizeof(command_load_material);
        case Set_Pipeline:       return (uint32)sizeof(command_set_pipeline);
    }
    return 0;
}

struct render_commands
{
    uint32 LoadCount;

    uint8 *PushBufferBase;
    uint32 PushBufferSize;
    uint32 MaxPushBufferSize;
};

inline render_commands InitRenderCommands(void *Memory, uint32 Size)
{
    render_commands Result = {};
    Result.PushBufferBase    = (uint8 *)Memory;
    Result.MaxPushBufferSize = Size;

    return Result;
}

inline void *PushRenderCommand(render_commands *Commands, command_type Type)
{
    uint32 Size = CommandSize(Type);

    void *Base = 0;
    if (Commands->PushBufferSize + Size <= Commands->MaxPushBufferSize)
    {
        command_type *CmdBase = (command_type *)(Commands->PushBufferBase + Commands->PushBufferSize);
        *CmdBase = Type;
        Base = CmdBase;
        Commands->PushBufferSize += Size;
    }
    return Base;
}

inline command_type *NextRenderCommand(render_commands *Commands, uint32 *Offset)
{
    if (*Offset >= Commands->PushBufferSize)
    {
        return 0;
    }

    command_type *CmdBase = (command_type *)(Commands->PushBufferBase + *Offset);
    uint32 Size = CommandSize(*CmdBase);
    if (!Size)
    {
        Assert(!"Unknown render cmd");
        return 0;
    }

    *Offset += Size;
    return CmdBase;
}

inline void PushRenderPipeline(render_commands* Commands, pipeline_type type)
{
    command_set_pipeline* cmd = (command_set_pipeline*)PushRenderCommand(Commands, Set_Pipeline);
    if (cmd)
    {
        cmd->PipelineType = type;
    }
}

inline void PushRenderCamera(render_commands *Commands, Matrix4 View, real32 FovY)
{
    command_render_camera *cmd = (command_render_camera *)PushRenderCommand(Commands, Render_Camera);
    if (cmd)
    {
        cmd->View = View;
        cmd->FovY = FovY;
    }
}

inline void PushLoadMesh(render_commands *Commands, uint32 MeshHandle, void *Vertices, uint32 VertexCount, uint32 *Indices, uint32 IndexCount)
{
    command_load_mesh *cmd = (command_load_mesh *)PushRenderCommand(Commands, Load_Mesh);
    if (cmd)
    {
        cmd->MeshHandle       = MeshHandle;
        cmd->Vertices    = Vertices;
        cmd->VertexCount = VertexCount;
        cmd->Indices     = Indices;
        cmd->IndexCount  = IndexCount;

        Commands->LoadCount++;
    }
}

inline uint32 TextureFormatBytes(texture_format Format)
{
    return (Format == TextureFormat_RGBA16F) ? 8u : 4u;
}

inline void PushLoadTexture(render_commands *Commands, uint32 TextureHandle, void *Pixels, uint32 Width, uint32 Height, uint32 SRGB, texture_format Format)
{
    command_load_texture *cmd = (command_load_texture *)PushRenderCommand(Commands, Load_Texture);
    if (cmd)
    {
        cmd->TextureHandle  = TextureHandle;
        cmd->Pixels = Pixels;
        cmd->Width  = Width;
        cmd->Height = Height;
        cmd->SRGB   = SRGB;
        cmd->Format = Format;

        Commands->LoadCount++;
    }
}

inline void PushLoadCubemap(render_commands *Commands, uint32 CubemapHandle, void *Pixels, uint32 FaceSize, texture_format Format)
{
    command_load_cubemap *cmd = (command_load_cubemap *)PushRenderCommand(Commands, Load_Cubemap);
    if (cmd)
    {
        cmd->CubemapHandle    = CubemapHandle;
        cmd->Pixels   = Pixels;
        cmd->FaceSize = FaceSize;
        cmd->Format   = Format;

        Commands->LoadCount++;
    }
}

inline void PushLoadMaterial(render_commands *Commands, uint32 MaterialHandle, pipeline_type Pipeline, cull_mode CullMode, blend_mode BlendMode, bool32 DepthTest, bool32 DepthWrite, Vector4 BaseColor, uint32 TextureHandle)
{
    command_load_material *cmd = (command_load_material *)PushRenderCommand(Commands, Load_Material);
    if (cmd)
    {
        cmd->MaterialHandle = MaterialHandle;
        cmd->Pipeline       = Pipeline;
        cmd->CullMode       = CullMode;
        cmd->BlendMode      = BlendMode;
        cmd->DepthTest      = DepthTest;
        cmd->DepthWrite     = DepthWrite;
        cmd->BaseColor      = BaseColor;
        cmd->TextureHandle  = TextureHandle;

        Commands->LoadCount++;
    }
}

inline void PushRenderTexturedRect(render_commands *Commands, Vector2 Min, Vector2 Max, Vector4 Color, Vector4 UV, uint32 TextureSlot)
{
    command_render_rect *cmd = (command_render_rect *)PushRenderCommand(Commands, Render_Rect);
    if (cmd)
    {
        cmd->Min         = Min;
        cmd->Max         = Max;
        cmd->Color       = Color;
        cmd->UV          = UV;
        cmd->TextureSlot = TextureSlot;
    }
}

inline void PushRenderRect(render_commands *Commands, Vector2 Min, Vector2 Max, Vector4 Color)
{
    PushRenderTexturedRect(Commands, Min, Max, Color, Vector4(0.0f, 0.0f, 0.0f, 0.0f), 0);
}

inline void PushRenderSkybox(render_commands *Commands, uint32 Cubemap)
{
    command_render_skybox *cmd = (command_render_skybox *)PushRenderCommand(Commands, Render_Skybox);
    if (cmd)
    {
        cmd->CubemapHandle = Cubemap;
    }
}

inline void PushRenderMesh(render_commands *Commands, Matrix4 Transform, Vector4 Tint, uint32 Mesh, uint32 Material)
{
    command_render_mesh* cmd = (command_render_mesh *)PushRenderCommand(Commands, Render_Mesh);
    if (cmd)
    {
        cmd->Transform      = Transform;
        cmd->Tint           = Tint;
        cmd->MeshHandle     = Mesh;
        cmd->MaterialHandle = Material;
    }
}

#endif
