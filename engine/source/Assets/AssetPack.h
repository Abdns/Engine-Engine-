#ifndef ASSETPACK_H
#define ASSETPACK_H

#include "Types.h"
#include "Memory.h"
#include "Strings.h"
#include "KBNFormat.h"

struct asset_pack
{
    uint8       *Data;
    uint32       Size;
    asset_entry *Entries;
    uint32       Count;
};

internal asset_pack AssetPackFromMemory(void *Data, uint32 Size)
{
    asset_pack Result = {};

    if (!Data || Size < sizeof(asset_file_header))
    {
        DebugLog("Asset pack is missing or too small\n");
        return Result;
    }

    asset_file_header *Header = (asset_file_header *)Data;
    if (Header->Magic != KBN_MAGIC || Header->Version != KBN_VERSION || Header->AssetTableOffset + Header->AssetCount * sizeof(asset_entry) > Size)
    {
        DebugLog("Asset pack is corrupt\n");
        return Result;
    }

    Result.Data    = (uint8 *)Data;
    Result.Size    = Size;
    Result.Entries = (asset_entry *)((uint8 *)Data + Header->AssetTableOffset);
    Result.Count   = Header->AssetCount;

    return Result;
}

internal void *AssetData(asset_pack *Pack, asset_entry *Entry)
{
    if (Entry->Offset + Entry->Size > Pack->Size)
    {
        return 0;
    }

    return Pack->Data + Entry->Offset;
}

struct game_assets
{
    uint32 MeshCount;
    uint32 TextureCount;

    char   *MeshNames;
    uint32 *MeshVertexCount;
    uint32 *MeshFirstVertex;

    char   *TextureNames;
    uint32 *TextureWidth;
    uint32 *TextureHeight;
    uint32 *TextureSRGB;
    uint32 *TextureFirstPixel;

    real32 *Vertices;
    uint32 *Pixels;
};

internal real32 *MeshVertices(game_assets *Assets, uint32 MeshID)
{
    return Assets->Vertices + (memory_index)Assets->MeshFirstVertex[MeshID] * KBN_VERTEX_FLOATS;
}

internal uint32 *TexturePixels(game_assets *Assets, uint32 TextureID)
{
    return Assets->Pixels + Assets->TextureFirstPixel[TextureID];
}

internal void LoadGameAssets(game_assets *Assets, memory_arena *Arena, void *PackData, uint32 PackSize)
{
    ZeroStruct(*Assets);

    asset_pack Pack = AssetPackFromMemory(PackData, PackSize);

    uint32 MeshCount     = 0;
    uint32 TextureCount  = 0;
    uint32 TotalVertices = 0;
    uint32 TotalPixels   = 0;

    for (uint32 Index = 0; Index < Pack.Count; ++Index)
    {
        asset_entry *Entry = Pack.Entries + Index;
        if (!AssetData(&Pack, Entry))
        {
            continue;
        }

        if (Entry->Type == (uint32)Asset_Mesh)
        {
            MeshCount++;
            TotalVertices += Entry->Mesh.VertexCount;
        }
        else if (Entry->Type == (uint32)Asset_Image)
        {
            TextureCount++;
            TotalPixels += Entry->Image.Width * Entry->Image.Height;
        }
    }

    Assets->MeshNames         = PushArray(Arena, (memory_index)MeshCount * KBN_MAX_ASSET_NAME, char);
    Assets->MeshVertexCount   = PushArray(Arena, MeshCount, uint32);
    Assets->MeshFirstVertex   = PushArray(Arena, MeshCount, uint32);
    Assets->TextureNames      = PushArray(Arena, (memory_index)TextureCount * KBN_MAX_ASSET_NAME, char);
    Assets->TextureWidth      = PushArray(Arena, TextureCount, uint32);
    Assets->TextureHeight     = PushArray(Arena, TextureCount, uint32);
    Assets->TextureSRGB       = PushArray(Arena, TextureCount, uint32);
    Assets->TextureFirstPixel = PushArray(Arena, TextureCount, uint32);
    Assets->Vertices          = PushArray(Arena, (memory_index)TotalVertices * KBN_VERTEX_FLOATS, real32);
    Assets->Pixels            = PushArray(Arena, TotalPixels, uint32);

    uint32 VertexAt = 0;
    uint32 PixelAt  = 0;
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
            uint32 MeshIndex = Assets->MeshCount++;
            AppendString(Assets->MeshNames + MeshIndex * KBN_MAX_ASSET_NAME, KBN_MAX_ASSET_NAME, 0, Entry->Name);
            Assets->MeshVertexCount[MeshIndex] = Entry->Mesh.VertexCount;
            Assets->MeshFirstVertex[MeshIndex] = VertexAt;

            CopySize(Entry->Size, Data, MeshVertices(Assets, MeshIndex));
            VertexAt += Entry->Mesh.VertexCount;
        }
        else if (Entry->Type == (uint32)Asset_Image)
        {
            uint32 TextureIndex = Assets->TextureCount++;
            AppendString(Assets->TextureNames + TextureIndex * KBN_MAX_ASSET_NAME, KBN_MAX_ASSET_NAME, 0, Entry->Name);
            Assets->TextureWidth[TextureIndex]      = Entry->Image.Width;
            Assets->TextureHeight[TextureIndex]     = Entry->Image.Height;
            Assets->TextureSRGB[TextureIndex]       = Entry->Image.SRGB;
            Assets->TextureFirstPixel[TextureIndex] = PixelAt;

            CopySize(Entry->Size, Data, TexturePixels(Assets, TextureIndex));
            PixelAt += Entry->Image.Width * Entry->Image.Height;
        }
    }
}

internal uint32 GetMeshID(game_assets *Assets, const char *Name)
{
    for (uint32 Index = 0; Index < Assets->MeshCount; ++Index)
    {
        if (StringsAreEqual(Assets->MeshNames + Index * KBN_MAX_ASSET_NAME, Name))
        {
            return Index;
        }
    }

    DebugLog("Mesh '%s' not found\n", Name);
    return 0;
}

internal uint32 GetTextureID(game_assets *Assets, const char *Name)
{
    for (uint32 Index = 0; Index < Assets->TextureCount; ++Index)
    {
        if (StringsAreEqual(Assets->TextureNames + Index * KBN_MAX_ASSET_NAME, Name))
        {
            return Index;
        }
    }

    DebugLog("Texture '%s' not found\n", Name);
    return 0;
}

#endif
