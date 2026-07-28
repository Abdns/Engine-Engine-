#ifndef RENDERCOMMANDS_H
#define RENDERCOMMANDS_H

#include "Types.h"
#include "EngineMath.h"

enum command_type
{
    Render_Mesh = 0,
    Set_Pipline,
    Set_RenderState,
    Render_Camera,
};

struct mesh_handle
{
    uint32 FirstVertex;
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
        case Set_Pipline:        return (uint32)sizeof(command_set_pipeline);
        case Set_RenderState:    return (uint32)sizeof(command_set_render_state);
    }
    return 0;
}

struct render_commands
{
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

inline void PushRenderPipline(render_commands* Commands, pipeline_type type)
{
    command_set_pipeline* cmd = (command_set_pipeline*)PushRenderCommand(Commands, Set_Pipline);
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
