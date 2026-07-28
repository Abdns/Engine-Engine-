#ifndef DATALAKE_H
#define DATALAKE_H

#include "Types.h"
#include "Memory.h"
#include "Strings.h"
#include "KBNFormat.h"
#include "AssetPack.h"
#include "PlatformAPI.h"

#define LAKE_MAX_MESHES 256
#define LAKE_MAX_PIXELS (1u << 22)

struct data_lake
{
    real32 *Vertices;
    uint32 *Indices;
    uint32 *Pixels;

    uint32 VertexCount, VertexCapacity;
    uint32 IndexCount,  IndexCapacity;
    uint32 PixelCount,  PixelCapacity;

    char        *MeshNames;
    mesh_handle *MeshHandle;
    uint32       MeshCount;

    char   *TextureNames;
    uint32 *TextureWidth;
    uint32 *TextureHeight;
    uint32 *TextureFirstPixel;
    uint32  TextureCount, TextureCapacity;
};

internal void LakeInit(data_lake *Lake, memory_arena *Arena)
{
    ZeroStruct(*Lake);

    Lake->VertexCapacity  = RENDER_MAX_VERTICES;
    Lake->IndexCapacity   = RENDER_MAX_INDICES;
    Lake->PixelCapacity   = LAKE_MAX_PIXELS;
    Lake->TextureCapacity = RENDER_MAX_TEXTURES;

    Lake->Vertices = PushArray(Arena, (memory_index)Lake->VertexCapacity * KBN_VERTEX_FLOATS, real32);
    Lake->Indices  = PushArray(Arena, Lake->IndexCapacity, uint32);
    Lake->Pixels   = PushArray(Arena, Lake->PixelCapacity, uint32);

    Lake->MeshNames  = PushArray(Arena, (memory_index)LAKE_MAX_MESHES * KBN_MAX_ASSET_NAME, char);
    Lake->MeshHandle = PushArray(Arena, LAKE_MAX_MESHES, mesh_handle);

    Lake->TextureNames      = PushArray(Arena, (memory_index)Lake->TextureCapacity * KBN_MAX_ASSET_NAME, char);
    Lake->TextureWidth      = PushArray(Arena, Lake->TextureCapacity, uint32);
    Lake->TextureHeight     = PushArray(Arena, Lake->TextureCapacity, uint32);
    Lake->TextureFirstPixel = PushArray(Arena, Lake->TextureCapacity, uint32);
}

internal real32 *LakeMeshVertices(data_lake *Lake, mesh_handle Mesh)
{
    return Lake->Vertices + (memory_index)Mesh.FirstVertex * KBN_VERTEX_FLOATS;
}

internal uint32 *LakeMeshIndices(data_lake *Lake, mesh_handle Mesh)
{
    return Lake->Indices + Mesh.FirstIndex;
}

internal mesh_handle LakeAddMesh(data_lake *Lake, render_commands *Commands, const char *Name, real32 *Vertices, uint32 VertexCount, uint32 *Indices, uint32 IndexCount)
{
    mesh_handle Result = {};

    if (Lake->MeshCount >= LAKE_MAX_MESHES)
    {
        DebugLog("Lake is full (%d meshes)\n", LAKE_MAX_MESHES);
        return Result;
    }

    if (!VertexCount || !IndexCount ||
        Lake->VertexCount + VertexCount > Lake->VertexCapacity ||
        Lake->IndexCount + IndexCount > Lake->IndexCapacity)
    {
        DebugLog("Lake out of geometry space for '%s' (%u vertices, %u indices)\n", Name, VertexCount, IndexCount);
        return Result;
    }

    Result.FirstVertex = Lake->VertexCount;
    Result.VertexCount = VertexCount;
    Result.FirstIndex  = Lake->IndexCount;
    Result.IndexCount  = IndexCount;

    CopySize((memory_index)VertexCount * KBN_VERTEX_FLOATS * sizeof(real32), Vertices, LakeMeshVertices(Lake, Result));
    CopySize((memory_index)IndexCount * sizeof(uint32), Indices, LakeMeshIndices(Lake, Result));

    PushLoadMesh(Commands, Result, LakeMeshVertices(Lake, Result), LakeMeshIndices(Lake, Result));

    Lake->VertexCount += VertexCount;
    Lake->IndexCount  += IndexCount;

    uint32 Slot = Lake->MeshCount++;
    AppendString(Lake->MeshNames + (memory_index)Slot * KBN_MAX_ASSET_NAME, KBN_MAX_ASSET_NAME, 0, Name);
    Lake->MeshHandle[Slot] = Result;

    return Result;
}

internal texture_handle LakeAddTexture(data_lake *Lake, render_commands *Commands, const char *Name, void *Pixels, uint32 Width, uint32 Height, uint32 SRGB)
{
    texture_handle Result = {};

    if (Lake->TextureCount >= Lake->TextureCapacity)
    {
        DebugLog("Lake is full (%u textures)\n", Lake->TextureCapacity);
        return Result;
    }

    uint32 PixelCount = Width * Height;
    if (Lake->PixelCount + PixelCount > Lake->PixelCapacity)
    {
        DebugLog("Lake out of pixel space for '%s' (%u pixels)\n", Name, PixelCount);
        return Result;
    }

    uint32 Slot = Lake->TextureCount;
    Result.Index = Slot;

    uint32 *Destination = Lake->Pixels + Lake->PixelCount;
    CopySize((memory_index)PixelCount * sizeof(uint32), Pixels, Destination);

    PushLoadTexture(Commands, Result, Destination, Width, Height, SRGB);

    AppendString(Lake->TextureNames + (memory_index)Slot * KBN_MAX_ASSET_NAME, KBN_MAX_ASSET_NAME, 0, Name);
    Lake->TextureWidth[Slot]      = Width;
    Lake->TextureHeight[Slot]     = Height;
    Lake->TextureFirstPixel[Slot] = Lake->PixelCount;
    Lake->TextureCount++;
    Lake->PixelCount += PixelCount;

    return Result;
}

internal void LakeLoadPack(data_lake *Lake, render_commands *Commands, void *PackData, uint32 PackSize)
{
    asset_pack Pack = AssetPackFromMemory(PackData, PackSize);

    for (uint32 Index = 0; Index < Pack.Count; ++Index)
    {
        asset_entry *Entry = Pack.Entries + Index;
        void *Data = AssetData(&Pack, Entry);
        if (!Data)
        {
            continue;
        }

        if (Entry->Type == (uint32)Asset_Mesh)
        {
            memory_index VertexBytes = (memory_index)Entry->Mesh.VertexCount * KBN_VERTEX_FLOATS * sizeof(real32);

            LakeAddMesh(Lake, Commands, Entry->Name,
                        (real32 *)Data, Entry->Mesh.VertexCount,
                        (uint32 *)((uint8 *)Data + VertexBytes), Entry->Mesh.IndexCount);
        }
        else if (Entry->Type == (uint32)Asset_Image)
        {
            LakeAddTexture(Lake, Commands, Entry->Name, Data, Entry->Image.Width, Entry->Image.Height, Entry->Image.SRGB);
        }
    }

    DebugLog("Lake loaded %u meshes (%u vertices, %u indices) and %u textures\n",
             Lake->MeshCount, Lake->VertexCount, Lake->IndexCount, Lake->TextureCount);
}

internal mesh_handle LakeMesh(data_lake *Lake, const char *Name)
{
    for (uint32 Index = 0; Index < Lake->MeshCount; ++Index)
    {
        if (StringsAreEqual(Lake->MeshNames + (memory_index)Index * KBN_MAX_ASSET_NAME, Name))
        {
            return Lake->MeshHandle[Index];
        }
    }

    DebugLog("Mesh '%s' not found\n", Name);

    mesh_handle Result = {};
    return Result;
}

internal texture_handle LakeTexture(data_lake *Lake, const char *Name)
{
    for (uint32 Index = 0; Index < Lake->TextureCount; ++Index)
    {
        if (StringsAreEqual(Lake->TextureNames + (memory_index)Index * KBN_MAX_ASSET_NAME, Name))
        {
            texture_handle Result;
            Result.Index = Index;
            return Result;
        }
    }

    DebugLog("Texture '%s' not found\n", Name);

    texture_handle Result = {};
    return Result;
}

#endif
