#ifndef RENDERCOMMANDS_H
#define RENDERCOMMANDS_H

#include "Types.h"
#include "EngineMath.h"

enum render_entry_type
{
    RenderEntry_Mesh = 0,
    RenderEntry_LoadMesh,
    RenderEntry_LoadTexture,
};

struct render_entry_header
{
    uint32 Type;
};

struct render_entry_mesh
{
    render_entry_header Header;
    Matrix4 Transform;
    Vector4 Tint;
    uint32  MeshID;
    uint32  TextureID;
};

struct render_entry_load_mesh
{
    render_entry_header Header;
    uint32  MeshID;
    uint32  VertexCount;
    real32 *Vertices;
};

struct render_entry_load_texture
{
    render_entry_header Header;
    uint32 TextureID;
    uint32 Width;
    uint32 Height;
    uint32 SRGB;
    void  *Pixels;
};

inline uint32 RenderEntrySize(uint32 Type)
{
    switch (Type)
    {
        case RenderEntry_Mesh:        return (uint32)sizeof(render_entry_mesh);
        case RenderEntry_LoadMesh:    return (uint32)sizeof(render_entry_load_mesh);
        case RenderEntry_LoadTexture: return (uint32)sizeof(render_entry_load_texture);
    }
    return 0;
}

struct render_commands
{
    real32  ClearR;
    real32  ClearG;
    real32  ClearB;
    Matrix4 View;
    real32  FovY;

    uint32 LoadCount;

    uint8 *PushBufferBase;
    uint32 PushBufferSize;
    uint32 MaxPushBufferSize;
};

inline render_commands InitRenderCommands(void *Memory, uint32 Size)
{
    render_commands Result = {};
    Result.View = Mat4Identity();
    Result.FovY = 0.785398f;

    Result.PushBufferBase    = (uint8 *)Memory;
    Result.MaxPushBufferSize = Size;

    return Result;
}

inline void *PushRenderElement_(render_commands *Commands, uint32 Size, render_entry_type Type)
{
    void *Result = 0;
    if (Commands->PushBufferSize + Size <= Commands->MaxPushBufferSize)
    {
        render_entry_header *Header = (render_entry_header *)(Commands->PushBufferBase + Commands->PushBufferSize);
        Header->Type = (uint32)Type;
        Result = Header;
        Commands->PushBufferSize += Size;
    }
    return Result;
}

#define PushRenderElement(Commands, type, EntryType) (type *)PushRenderElement_(Commands, (uint32)sizeof(type), EntryType)

inline render_entry_header *NextRenderEntry(render_commands *Commands, uint32 *Offset)
{
    if (*Offset >= Commands->PushBufferSize)
    {
        return 0;
    }

    render_entry_header *Header = (render_entry_header *)(Commands->PushBufferBase + *Offset);
    uint32 Size = RenderEntrySize(Header->Type);
    if (!Size)
    {
        Assert(!"Unknown render entry type");
        return 0;
    }

    *Offset += Size;
    return Header;
}

inline void SetRenderClear(render_commands *Commands, real32 R, real32 G, real32 B)
{
    Commands->ClearR = R;
    Commands->ClearG = G;
    Commands->ClearB = B;
}

inline void SetRenderCamera(render_commands *Commands, Matrix4 View, real32 FovY)
{
    Commands->View = View;
    Commands->FovY = FovY;
}

inline void PushRenderMesh(render_commands *Commands, Matrix4 Transform, Vector4 Tint, uint32 MeshID, uint32 TextureID)
{
    render_entry_mesh *Entry = PushRenderElement(Commands, render_entry_mesh, RenderEntry_Mesh);
    if (Entry)
    {
        Entry->Transform = Transform;
        Entry->Tint      = Tint;
        Entry->MeshID    = MeshID;
        Entry->TextureID = TextureID;
    }
}

inline void PushRenderLoadMesh(render_commands *Commands, uint32 MeshID, uint32 VertexCount, real32 *Vertices)
{
    render_entry_load_mesh *Entry = PushRenderElement(Commands, render_entry_load_mesh, RenderEntry_LoadMesh);
    if (Entry)
    {
        Entry->MeshID      = MeshID;
        Entry->VertexCount = VertexCount;
        Entry->Vertices    = Vertices;
        Commands->LoadCount++;
    }
}

inline void PushRenderLoadTexture(render_commands *Commands, uint32 TextureID, uint32 Width, uint32 Height, uint32 SRGB, void *Pixels)
{
    render_entry_load_texture *Entry = PushRenderElement(Commands, render_entry_load_texture, RenderEntry_LoadTexture);
    if (Entry)
    {
        Entry->TextureID = TextureID;
        Entry->Width     = Width;
        Entry->Height    = Height;
        Entry->SRGB      = SRGB;
        Entry->Pixels    = Pixels;
        Commands->LoadCount++;
    }
}

#endif
