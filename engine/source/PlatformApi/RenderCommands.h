#ifndef RENDERCOMMANDS_H
#define RENDERCOMMANDS_H

#include "Types.h"
#include "EngineMath.h"

#define RENDER_MAX_VERTICES (1u << 20)
#define RENDER_MAX_INDICES  (1u << 21)
#define RENDER_MAX_TEXTURES 16

enum command_type
{
    Render_Mesh = 0,
    Set_Pipeline,
    Set_RenderState,
    Render_Camera,
    Load_Mesh,
    Load_Texture,
};

struct mesh_handle
{
    uint32 FirstVertex;
    uint32 VertexCount;
    uint32 FirstIndex;
    uint32 IndexCount;
};

struct texture_handle
{
    uint32 Index;
};

enum cull_mode
{
    Cull_None = 0,
    Cull_Back,
    Cull_Front,
};

enum blend_mode
{
    Blend_Opaque = 0,
    Blend_Alpha,
};

enum pipeline_type
{
    Pipeline_Unlit = 0,
    Pipeline_Count,
};

struct command_render_mesh
{
    command_type   Type;
    Matrix4        Transform;
    Vector4        Tint;
    mesh_handle    Mesh;
    texture_handle Texture;
};

struct command_load_mesh
{
    command_type Type;
    mesh_handle  Where;
    real32      *Vertices;
    uint32      *Indices;
};

struct command_load_texture
{
    command_type   Type;
    texture_handle Where;
    void          *Pixels;
    uint32         Width;
    uint32         Height;
    uint32         SRGB;
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

struct command_set_render_state
{
    command_type Type;
    cull_mode    CullMode;
    blend_mode   BlendMode;
    bool32       DepthTest;
    bool32       DepthWrite;
};

inline uint32 CommandSize(command_type Type)
{
    switch (Type)
    {
        case Render_Mesh:        return (uint32)sizeof(command_render_mesh);
        case Render_Camera:      return (uint32)sizeof(command_render_camera);
        case Load_Mesh:          return (uint32)sizeof(command_load_mesh);
        case Load_Texture:       return (uint32)sizeof(command_load_texture);
        case Set_Pipeline:        return (uint32)sizeof(command_set_pipeline);
        case Set_RenderState:    return (uint32)sizeof(command_set_render_state);
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

inline void PushRenderState(render_commands *Commands, cull_mode CullMode, blend_mode BlendMode, bool32 DepthTest, bool32 DepthWrite)
{
    command_set_render_state *cmd = (command_set_render_state *)PushRenderCommand(Commands, Set_RenderState);
    if (cmd)
    {
        cmd->CullMode   = CullMode;
        cmd->BlendMode  = BlendMode;
        cmd->DepthTest  = DepthTest;
        cmd->DepthWrite = DepthWrite;
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

inline void PushLoadMesh(render_commands *Commands, mesh_handle Where, real32 *Vertices, uint32 *Indices)
{
    command_load_mesh *cmd = (command_load_mesh *)PushRenderCommand(Commands, Load_Mesh);
    if (cmd)
    {
        cmd->Where    = Where;
        cmd->Vertices = Vertices;
        cmd->Indices  = Indices;

        Commands->LoadCount++;
    }
}

inline void PushLoadTexture(render_commands *Commands, texture_handle Where, void *Pixels, uint32 Width, uint32 Height, uint32 SRGB)
{
    command_load_texture *cmd = (command_load_texture *)PushRenderCommand(Commands, Load_Texture);
    if (cmd)
    {
        cmd->Where  = Where;
        cmd->Pixels = Pixels;
        cmd->Width  = Width;
        cmd->Height = Height;
        cmd->SRGB   = SRGB;

        Commands->LoadCount++;
    }
}

inline void PushRenderMesh(render_commands *Commands, Matrix4 Transform, Vector4 Tint, mesh_handle Mesh, texture_handle Texture)
{
    command_render_mesh* cmd = (command_render_mesh *)PushRenderCommand(Commands, Render_Mesh);
    if (cmd)
    {
        cmd->Transform = Transform;
        cmd->Tint      = Tint;
        cmd->Mesh      = Mesh;
        cmd->Texture   = Texture;
    }
}

#endif
