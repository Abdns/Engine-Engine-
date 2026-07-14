#ifndef RENDERCOMMANDS_H
#define RENDERCOMMANDS_H

#include "Types.h"
#include "EngineMath.h"

enum mesh_id
{
    Mesh_Cube = 0,
    Mesh_Pyramid,
    Mesh_Count,
};

enum texture_id
{
    Texture_Photo = 0,
    Texture_Checker,
    Texture_Count,
};

struct render_entry_mesh
{
    Vector3 Position;
    real32  Rotation;
    uint32  MeshID;
    uint32  TextureID;
};

struct render_commands
{
    uint32 Width;
    uint32 Height;

    real32 ClearR;
    real32 ClearG;
    real32 ClearB;

    Matrix4 CameraView;
    real32  CameraFovY;

    render_entry_mesh *Meshes;
    uint32 MeshCount;
    uint32 MaxMeshes;
};

inline render_commands InitRenderCommands(void *Memory, uint32 Size, uint32 Width, uint32 Height)
{
    render_commands Result = {};
    Result.Width  = Width;
    Result.Height = Height;

    Result.CameraView = Mat4Identity();
    Result.CameraFovY = 0.785398f;

    Result.Meshes    = (render_entry_mesh *)Memory;
    Result.MeshCount = 0;
    Result.MaxMeshes = Size / (uint32)sizeof(render_entry_mesh);

    return Result;
}

inline void PushRenderClear(render_commands *Commands, real32 R, real32 G, real32 B)
{
    Commands->ClearR = R;
    Commands->ClearG = G;
    Commands->ClearB = B;
}

inline void PushRenderCamera(render_commands *Commands, Matrix4 View, real32 FovY)
{
    Commands->CameraView = View;
    Commands->CameraFovY = FovY;
}

inline void PushRenderMesh(render_commands *Commands, Vector3 Position, real32 Rotation, uint32 MeshID, uint32 TextureID)
{
    if (Commands->MeshCount < Commands->MaxMeshes)
    {
        render_entry_mesh *Mesh = &Commands->Meshes[Commands->MeshCount++];
        Mesh->Position = Position;
        Mesh->Rotation = Rotation;
        Mesh->MeshID = MeshID;
        Mesh->TextureID = TextureID;
    }
}

#endif
