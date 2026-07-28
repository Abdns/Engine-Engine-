#ifndef DATALAKE_H
#define DATALAKE_H

#include "Types.h"
#include "Memory.h"
#include "Strings.h"
#include "KBNFormat.h"
#include "AssetPack.h"
#include "PlatformAPI.h"

#define LAKE_MAX_MESHES 256

struct data_lake
{
    game_memory *Platform;

    real32 *Vertices;
    uint32 *Indices;

    uint32 VertexCount, VertexCapacity;
    uint32 IndexCount,  IndexCapacity;

    char        *MeshNames;
    mesh_handle *MeshHandle;
    uint32       MeshCount;

    char           *TextureNames;
    uint32         *TextureWidth;
    uint32         *TextureHeight;
    uint32          TextureCount, TextureCapacity;
};

internal void LakeInit(data_lake *Lake, memory_arena *Arena, game_memory *Platform)
{
    ZeroStruct(*Lake);

    Lake->Platform = Platform;

    gpu_limits Limits = Platform->PlatformGetGpuLimits();

    Lake->VertexCapacity  = Limits.MaxVertices;
    Lake->IndexCapacity   = Limits.MaxIndices;
    Lake->TextureCapacity = Limits.MaxTextures;

    Lake->Vertices = PushArray(Arena, (memory_index)Lake->VertexCapacity * KBN_VERTEX_FLOATS, real32);
    Lake->Indices  = PushArray(Arena, Lake->IndexCapacity, uint32);

    Lake->MeshNames  = PushArray(Arena, (memory_index)LAKE_MAX_MESHES * KBN_MAX_ASSET_NAME, char);
    Lake->MeshHandle = PushArray(Arena, LAKE_MAX_MESHES, mesh_handle);

    Lake->TextureNames  = PushArray(Arena, (memory_index)Lake->TextureCapacity * KBN_MAX_ASSET_NAME, char);
    Lake->TextureWidth  = PushArray(Arena, Lake->TextureCapacity, uint32);
    Lake->TextureHeight = PushArray(Arena, Lake->TextureCapacity, uint32);
}

internal real32 *LakeVertices(data_lake *Lake, mesh_handle Mesh)
{
    return Lake->Vertices + (memory_index)Mesh.FirstVertex * KBN_VERTEX_FLOATS;
}

internal uint32 *LakeIndices(data_lake *Lake, mesh_handle Mesh)
{
    return Lake->Indices + Mesh.FirstIndex;
}

internal mesh_handle LakeAddMesh(data_lake *Lake, const char *Name, real32 *Vertices, uint32 VertexCount, uint32 *Indices, uint32 IndexCount)
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
    Result.FirstIndex  = Lake->IndexCount;
    Result.IndexCount  = IndexCount;

    if (!Lake->Platform->PlatformWriteVertices(Result.FirstVertex, Vertices, VertexCount) ||
        !Lake->Platform->PlatformWriteIndices(Result.FirstIndex, Indices, IndexCount))
    {
        DebugLog("GPU refused mesh '%s'\n", Name);

        mesh_handle Empty = {};
        return Empty;
    }

    CopySize((memory_index)VertexCount * KBN_VERTEX_FLOATS * sizeof(real32), Vertices, LakeVertices(Lake, Result));
    CopySize((memory_index)IndexCount * sizeof(uint32), Indices, LakeIndices(Lake, Result));

    Lake->VertexCount += VertexCount;
    Lake->IndexCount  += IndexCount;

    uint32 Slot = Lake->MeshCount++;
    AppendString(Lake->MeshNames + (memory_index)Slot * KBN_MAX_ASSET_NAME, KBN_MAX_ASSET_NAME, 0, Name);
    Lake->MeshHandle[Slot] = Result;

    return Result;
}

internal texture_handle LakeAddTexture(data_lake *Lake, const char *Name, void *Pixels, uint32 Width, uint32 Height, uint32 SRGB)
{
    texture_handle Result = {};

    if (Lake->TextureCount >= Lake->TextureCapacity)
    {
        DebugLog("Lake is full (%u textures)\n", Lake->TextureCapacity);
        return Result;
    }

    uint32 Slot = Lake->TextureCount;

    if (!Lake->Platform->PlatformWriteTexture(Slot, Pixels, Width, Height, SRGB))
    {
        DebugLog("GPU refused texture '%s'\n", Name);
        return Result;
    }

    AppendString(Lake->TextureNames + (memory_index)Slot * KBN_MAX_ASSET_NAME, KBN_MAX_ASSET_NAME, 0, Name);
    Lake->TextureWidth[Slot]  = Width;
    Lake->TextureHeight[Slot] = Height;
    Lake->TextureCount++;

    Result.Index = Slot;
    return Result;
}

internal void LakeLoadPack(data_lake *Lake, void *PackData, uint32 PackSize)
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

            LakeAddMesh(Lake, Entry->Name,
                        (real32 *)Data, Entry->Mesh.VertexCount,
                        (uint32 *)((uint8 *)Data + VertexBytes), Entry->Mesh.IndexCount);
        }
        else if (Entry->Type == (uint32)Asset_Image)
        {
            LakeAddTexture(Lake, Entry->Name, Data, Entry->Image.Width, Entry->Image.Height, Entry->Image.SRGB);
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
