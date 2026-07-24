#ifndef RENDERCOMMANDS_H
#define RENDERCOMMANDS_H

#include "Types.h"
#include "EngineMath.h"

enum command_type
{
    Render_Mesh = 0,
    Set_Pipline,
    Load_Mesh,
    LoadTexture,
    Render_Camera,
};

struct command_render_mesh
{
    command_type Type;
    Matrix4 Transform;
    Vector4 Tint;
    uint32  MeshID;
    uint32  TextureID;
};

struct command_load_mesh
{
    command_type Type;
    uint32  MeshID;
    uint32  VertexCount;
    real32 *Vertices;
};

struct command_load_texture
{
    command_type Type;
    uint32 TextureID;
    uint32 Width; 
    uint32 Height;
    uint32 SRGB;
    void  *Pixels;
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
    int32        PipelineId;
};

inline uint32 CommandSize(command_type Type)
{
    switch (Type)
    {
        case Render_Mesh:        return (uint32)sizeof(command_render_mesh);
        case Load_Mesh:          return (uint32)sizeof(command_load_mesh);
        case LoadTexture:        return (uint32)sizeof(command_load_texture);
        case Render_Camera:      return (uint32)sizeof(command_render_camera);
        case Set_Pipline:        return (uint32)sizeof(command_set_pipeline);
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

inline void PushRenderPipline(render_commands* Commands, int32 index)
{
    command_set_pipeline* cmd = (command_set_pipeline*)PushRenderCommand(Commands, Set_Pipline);
    if (cmd)
    {
        cmd->PipelineId = index;
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

inline void PushRenderMesh(render_commands *Commands, Matrix4 Transform, Vector4 Tint, uint32 MeshID, uint32 TextureID)
{
    command_render_mesh* cmd = (command_render_mesh *)PushRenderCommand(Commands, Render_Mesh);
    if (cmd)
    {
        cmd->Transform = Transform;
        cmd->Tint      = Tint;
        cmd->MeshID    = MeshID;
        cmd->TextureID = TextureID;
    }
}

inline void PushRenderLoadMesh(render_commands *Commands, uint32 MeshID, uint32 VertexCount, real32 *Vertices)
{
    command_load_mesh* cmd = (command_load_mesh *)PushRenderCommand(Commands, Load_Mesh);
    if (cmd)
    {
        cmd->MeshID      = MeshID;
        cmd->VertexCount = VertexCount;
        cmd->Vertices    = Vertices;

        Commands->LoadCount++;
    }
}

inline void PushRenderLoadTexture(render_commands *Commands, uint32 TextureID, uint32 Width, uint32 Height, uint32 SRGB, void *Pixels)
{
    command_load_texture* cmd = (command_load_texture *)PushRenderCommand(Commands, LoadTexture);
    if (cmd)
    {
        cmd->TextureID = TextureID;
        cmd->Width     = Width;
        cmd->Height    = Height;
        cmd->SRGB      = SRGB;
        cmd->Pixels    = Pixels;

        Commands->LoadCount++;
    }
}

#endif
