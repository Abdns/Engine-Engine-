#ifndef DATALAKE_H
#define DATALAKE_H

#include "Types.h"
#include "Memory.h"
#include "Strings.h"
#include "EngaFormat.h"
#include "AssetPack.h"

#define LAKE_MAX_MESHES      256
#define LAKE_MAX_TEXTURES    16
#define LAKE_MAX_CUBEMAPS    4
#define LAKE_MAX_VERTICES    (1u << 20)
#define LAKE_MAX_INDICES     (1u << 21)
#define LAKE_MAX_PIXEL_BYTES Megabytes(48)

struct data_lake
{
    enga_vertex *Vertices;
    uint32      *Indices;
    uint8       *Pixels;

    uint32 VertexUsed, VertexCapacity;
    uint32 IndexUsed,  IndexCapacity;
    uint64 PixelByteCount, PixelByteCapacity;

    char   *MeshNames;
    uint32 *MeshFirstVertex;
    uint32 *MeshVertexCount;
    uint32 *MeshFirstIndex;
    uint32 *MeshIndexCount;
    uint32  MeshCount, MeshCapacity;

    char   *TextureNames;
    uint64 *TextureFirstByte;
    uint32 *TextureWidth;
    uint32 *TextureHeight;
    uint32 *TextureSRGB;
    uint32 *TextureFormat;
    uint32  TextureCount, TextureCapacity;

    char   *CubemapNames;
    uint64 *CubemapFirstByte;
    uint32 *CubemapFaceSize;
    uint32 *CubemapFormat;
    uint32  CubemapCount, CubemapCapacity;
};

internal void LakeInit(data_lake *Lake, memory_arena *Arena)
{
    ZeroStruct(*Lake);

    Lake->VertexCapacity    = LAKE_MAX_VERTICES;
    Lake->IndexCapacity     = LAKE_MAX_INDICES;
    Lake->PixelByteCapacity = LAKE_MAX_PIXEL_BYTES;
    Lake->MeshCapacity      = LAKE_MAX_MESHES;
    Lake->TextureCapacity   = LAKE_MAX_TEXTURES;
    Lake->CubemapCapacity   = LAKE_MAX_CUBEMAPS;

    Lake->Vertices = PushArray(Arena, Lake->VertexCapacity, enga_vertex);
    Lake->Indices  = PushArray(Arena, Lake->IndexCapacity, uint32);
    Lake->Pixels   = (uint8 *)PushSize(Arena, Lake->PixelByteCapacity);

    Lake->MeshNames       = PushArray(Arena, (memory_index)Lake->MeshCapacity * ENGA_MAX_ASSET_NAME, char);
    Lake->MeshFirstVertex = PushArray(Arena, Lake->MeshCapacity, uint32);
    Lake->MeshVertexCount = PushArray(Arena, Lake->MeshCapacity, uint32);
    Lake->MeshFirstIndex  = PushArray(Arena, Lake->MeshCapacity, uint32);
    Lake->MeshIndexCount  = PushArray(Arena, Lake->MeshCapacity, uint32);

    Lake->TextureNames     = PushArray(Arena, (memory_index)Lake->TextureCapacity * ENGA_MAX_ASSET_NAME, char);
    Lake->TextureFirstByte = PushArray(Arena, Lake->TextureCapacity, uint64);
    Lake->TextureWidth     = PushArray(Arena, Lake->TextureCapacity, uint32);
    Lake->TextureHeight    = PushArray(Arena, Lake->TextureCapacity, uint32);
    Lake->TextureSRGB      = PushArray(Arena, Lake->TextureCapacity, uint32);
    Lake->TextureFormat    = PushArray(Arena, Lake->TextureCapacity, uint32);

    Lake->CubemapNames     = PushArray(Arena, (memory_index)Lake->CubemapCapacity * ENGA_MAX_ASSET_NAME, char);
    Lake->CubemapFirstByte = PushArray(Arena, Lake->CubemapCapacity, uint64);
    Lake->CubemapFaceSize  = PushArray(Arena, Lake->CubemapCapacity, uint32);
    Lake->CubemapFormat    = PushArray(Arena, Lake->CubemapCapacity, uint32);
}

internal enga_vertex *LakeMeshVertices(data_lake *Lake, uint32 Slot)
{
    return Lake->Vertices + Lake->MeshFirstVertex[Slot];
}

internal uint32 *LakeMeshIndices(data_lake *Lake, uint32 Slot)
{
    return Lake->Indices + Lake->MeshFirstIndex[Slot];
}

internal uint8 *LakeTexturePixels(data_lake *Lake, uint32 Slot)
{
    return Lake->Pixels + Lake->TextureFirstByte[Slot];
}

internal uint8 *LakeCubemapPixels(data_lake *Lake, uint32 Slot)
{
    return Lake->Pixels + Lake->CubemapFirstByte[Slot];
}

internal uint32 LakeAddMesh(data_lake *Lake, const char *Name, enga_vertex *Vertices, uint32 VertexCount, uint32 *Indices, uint32 IndexCount)
{
    uint32 Handle = {};

    if (Lake->MeshCount >= Lake->MeshCapacity)
    {
        DebugLog("Lake is full (%u meshes)\n", Lake->MeshCapacity);
        return Handle;
    }

    if (!VertexCount || !IndexCount || VertexCount > Lake->VertexCapacity - Lake->VertexUsed || IndexCount > Lake->IndexCapacity - Lake->IndexUsed)
    {
        DebugLog("Lake out of geometry space for '%s' (%u vertices, %u indices)\n", Name, VertexCount, IndexCount);
        return Handle;
    }

    uint32 Slot = Lake->MeshCount;

    Lake->MeshFirstVertex[Slot] = Lake->VertexUsed;
    Lake->MeshVertexCount[Slot] = VertexCount;
    Lake->MeshFirstIndex[Slot]  = Lake->IndexUsed;
    Lake->MeshIndexCount[Slot]  = IndexCount;

    CopySize((memory_index)VertexCount * sizeof(enga_vertex), Vertices, LakeMeshVertices(Lake, Slot));
    CopySize((memory_index)IndexCount * sizeof(uint32), Indices, LakeMeshIndices(Lake, Slot));

    AppendString(Lake->MeshNames + (memory_index)Slot * ENGA_MAX_ASSET_NAME, ENGA_MAX_ASSET_NAME, 0, Name);

    Lake->VertexUsed += VertexCount;
    Lake->IndexUsed  += IndexCount;
    Lake->MeshCount++;

    Handle = Slot + 1;

    return Handle;
}

internal uint32 LakeAddTexture(data_lake *Lake, const char *Name, void *Pixels, uint32 Width, uint32 Height, uint32 SRGB, asset_image_format Format)
{
    uint32 Handle = {};

    if (Lake->TextureCount >= Lake->TextureCapacity)
    {
        DebugLog("Lake is full (%u textures)\n", Lake->TextureCapacity);
        return Handle;
    }

    uint64 ByteSize = (uint64)Width * Height * AssetImageFormatBytes(Format);
    if (Lake->PixelByteCount + ByteSize > Lake->PixelByteCapacity)
    {
        DebugLog("Lake out of pixel space for '%s' (%llu bytes)\n", Name, ByteSize);
        return Handle;
    }

    uint32 Slot = Lake->TextureCount;

    Lake->TextureFirstByte[Slot] = Lake->PixelByteCount;
    CopySize(ByteSize, Pixels, LakeTexturePixels(Lake, Slot));

    AppendString(Lake->TextureNames + (memory_index)Slot * ENGA_MAX_ASSET_NAME, ENGA_MAX_ASSET_NAME, 0, Name);
    Lake->TextureWidth[Slot]  = Width;
    Lake->TextureHeight[Slot] = Height;
    Lake->TextureSRGB[Slot]   = SRGB;
    Lake->TextureFormat[Slot] = (uint32)Format;
    Lake->TextureCount++;
    Lake->PixelByteCount += ByteSize;

    Handle = Slot + 1;

    return Handle;
}

internal uint32 LakeAddCubemap(data_lake *Lake, const char *Name, void *Pixels, uint32 FaceSize, asset_image_format Format)
{
    uint32 Handle = {};

    if (Lake->CubemapCount >= Lake->CubemapCapacity)
    {
        DebugLog("Lake is full (%u cubemaps)\n", Lake->CubemapCapacity);
        return Handle;
    }

    uint64 ByteSize = (uint64)FaceSize * FaceSize * 6 * AssetImageFormatBytes(Format);
    if (Lake->PixelByteCount + ByteSize > Lake->PixelByteCapacity)
    {
        DebugLog("Lake out of pixel space for '%s' (%llu bytes)\n", Name, ByteSize);
        return Handle;
    }

    uint32 Slot = Lake->CubemapCount;

    Lake->CubemapFirstByte[Slot] = Lake->PixelByteCount;
    CopySize(ByteSize, Pixels, LakeCubemapPixels(Lake, Slot));

    AppendString(Lake->CubemapNames + (memory_index)Slot * ENGA_MAX_ASSET_NAME, ENGA_MAX_ASSET_NAME, 0, Name);
    Lake->CubemapFaceSize[Slot] = FaceSize;
    Lake->CubemapFormat[Slot]   = (uint32)Format;
    Lake->CubemapCount++;
    Lake->PixelByteCount += ByteSize;

    Handle = Slot + 1;

    return Handle;
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
            memory_index VertexBytes = (memory_index)Entry->Mesh.VertexCount * sizeof(enga_vertex);

            LakeAddMesh(Lake, Entry->Name, (enga_vertex *)Data, Entry->Mesh.VertexCount, (uint32 *)((uint8 *)Data + VertexBytes), Entry->Mesh.IndexCount);
        }
        else if (Entry->Type == (uint32)Asset_Image)
        {
            asset_image_format Format = (asset_image_format)Entry->Image.Format;

            if (Entry->Image.Layers == 6)
            {
                LakeAddCubemap(Lake, Entry->Name, Data, Entry->Image.Width, Format);
            }
            else
            {
                LakeAddTexture(Lake, Entry->Name, Data, Entry->Image.Width, Entry->Image.Height, Entry->Image.SRGB, Format);
            }
        }
    }

    DebugLog("Lake loaded %u meshes (%u vertices, %u indices), %u textures, %u cubemaps\n",
             Lake->MeshCount, Lake->VertexUsed, Lake->IndexUsed, Lake->TextureCount, Lake->CubemapCount);
}

internal uint32 LakeGetMeshHandle(data_lake *Lake, const char *Name)
{
    uint32 Handle = {};

    for (uint32 Index = 0; Index < Lake->MeshCount; ++Index)
    {
        if (StringsAreEqual(Lake->MeshNames + (memory_index)Index * ENGA_MAX_ASSET_NAME, Name))
        {
            Handle = Index + 1;
            return Handle;
        }
    }

    DebugLog("Mesh '%s' not found\n", Name);

    return Handle;
}

internal uint32 LakeGetTextureHandle(data_lake *Lake, const char *Name)
{
    uint32 Handle = {};

    for (uint32 Index = 0; Index < Lake->TextureCount; ++Index)
    {
        if (StringsAreEqual(Lake->TextureNames + (memory_index)Index * ENGA_MAX_ASSET_NAME, Name))
        {
            Handle = Index + 1;
            return Handle;
        }
    }

    DebugLog("Texture '%s' not found\n", Name);

    return Handle;
}

internal uint32 LakeGetCubemapHandle(data_lake *Lake, const char *Name)
{
    uint32 Handle = {};

    for (uint32 Index = 0; Index < Lake->CubemapCount; ++Index)
    {
        if (StringsAreEqual(Lake->CubemapNames + (memory_index)Index * ENGA_MAX_ASSET_NAME, Name))
        {
            Handle = Index + 1;

            return Handle;
        }
    }

    DebugLog("Cubemap '%s' not found\n", Name);

    return Handle;
}

#endif
