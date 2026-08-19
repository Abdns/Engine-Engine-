#include "Types.h"
#include "Memory.h"
#include "Strings.h"
#include "EngineMath.h"
#include "EngaFormat.h"

#define LAKE_MAX_MESHES      256
#define LAKE_MAX_TEXTURES    16
#define LAKE_MAX_CUBEMAPS    4
#define LAKE_MAX_FONTS       4
#define LAKE_MAX_VERTICES    (1u << 20)
#define LAKE_MAX_INDICES     (1u << 21)
#define LAKE_MAX_PIXEL_BYTES Megabytes(48)

struct entity;

struct transform_pool
{
    Vector3 *Position;
    Vector3 *Velocity;
    Matrix4 *Rotation;
    Vector3 *AngularVelocity;
    Vector4 *Tint;
    uint32   Count;
};

struct widget_pool
{
    Vector2 *RectMin;
    Vector2 *RectMax;
    real32  *Value;
    uint32   Count;
};

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
    Vector3 *MeshBoundsMin;
    Vector3 *MeshBoundsMax;
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

    char            *FontNames;
    asset_font_info *FontInfo;
    uint32          *FontTextureHandle;
    uint16          *FontMap;
    real32          *FontAdvance;
    uint32           FontCount, FontCapacity;

    entity *Entities;
    char   *EntityNames;
    uint32  EntityCount, EntityCapacity;
    uint32  EntityNextID;

    transform_pool Transforms;
    widget_pool    Widgets;
};

struct asset_pack
{
    uint8       *Data;
    uint32       Size;
    asset_descriptor *Entries;
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
    if (Header->Magic != ENGA_MAGIC || Header->Version != ENGA_VERSION || Header->AssetTableOffset + Header->AssetCount * sizeof(asset_descriptor) > Size)
    {
        DebugLog("Asset pack is corrupt\n");
        return Result;
    }

    Result.Data    = (uint8 *)Data;
    Result.Size    = Size;
    Result.Entries = (asset_descriptor *)((uint8 *)Data + Header->AssetTableOffset);
    Result.Count   = Header->AssetCount;

    return Result;
}

internal void *AssetData(asset_pack *Pack, asset_descriptor *Entry)
{
    if (Entry->Offset + Entry->Size > Pack->Size)
    {
        return 0;
    }

    return Pack->Data + Entry->Offset;
}

internal void LakeInit(data_lake *Lake, memory_arena *Arena)
{
    ZeroStruct(*Lake);

    Lake->VertexCapacity    = LAKE_MAX_VERTICES;
    Lake->IndexCapacity     = LAKE_MAX_INDICES;
    Lake->PixelByteCapacity = LAKE_MAX_PIXEL_BYTES;
    Lake->MeshCapacity      = LAKE_MAX_MESHES;
    Lake->TextureCapacity   = LAKE_MAX_TEXTURES;
    Lake->CubemapCapacity   = LAKE_MAX_CUBEMAPS;
    Lake->FontCapacity      = LAKE_MAX_FONTS;

    Lake->Vertices = PushArray(Arena, Lake->VertexCapacity, enga_vertex);
    Lake->Indices  = PushArray(Arena, Lake->IndexCapacity, uint32);
    Lake->Pixels   = (uint8 *)PushSize(Arena, Lake->PixelByteCapacity);

    Lake->MeshNames       = PushArray(Arena, (memory_size)Lake->MeshCapacity * ENGA_MAX_ASSET_NAME, char);
    Lake->MeshFirstVertex = PushArray(Arena, Lake->MeshCapacity, uint32);
    Lake->MeshVertexCount = PushArray(Arena, Lake->MeshCapacity, uint32);
    Lake->MeshFirstIndex  = PushArray(Arena, Lake->MeshCapacity, uint32);
    Lake->MeshIndexCount  = PushArray(Arena, Lake->MeshCapacity, uint32);
    Lake->MeshBoundsMin   = PushArray(Arena, Lake->MeshCapacity, Vector3);
    Lake->MeshBoundsMax   = PushArray(Arena, Lake->MeshCapacity, Vector3);

    Lake->TextureNames     = PushArray(Arena, (memory_size)Lake->TextureCapacity * ENGA_MAX_ASSET_NAME, char);
    Lake->TextureFirstByte = PushArray(Arena, Lake->TextureCapacity, uint64);
    Lake->TextureWidth     = PushArray(Arena, Lake->TextureCapacity, uint32);
    Lake->TextureHeight    = PushArray(Arena, Lake->TextureCapacity, uint32);
    Lake->TextureSRGB      = PushArray(Arena, Lake->TextureCapacity, uint32);
    Lake->TextureFormat    = PushArray(Arena, Lake->TextureCapacity, uint32);

    Lake->CubemapNames     = PushArray(Arena, (memory_size)Lake->CubemapCapacity * ENGA_MAX_ASSET_NAME, char);
    Lake->CubemapFirstByte = PushArray(Arena, Lake->CubemapCapacity, uint64);
    Lake->CubemapFaceSize  = PushArray(Arena, Lake->CubemapCapacity, uint32);
    Lake->CubemapFormat    = PushArray(Arena, Lake->CubemapCapacity, uint32);

    Lake->FontNames         = PushArray(Arena, (memory_size)Lake->FontCapacity * ENGA_MAX_ASSET_NAME, char);
    Lake->FontInfo          = PushArray(Arena, Lake->FontCapacity, asset_font_info);
    Lake->FontTextureHandle = PushArray(Arena, Lake->FontCapacity, uint32);
    Lake->FontMap           = PushArray(Arena, (memory_size)Lake->FontCapacity * ENGA_MAX_CODEPOINT, uint16);
    Lake->FontAdvance       = PushArray(Arena, (memory_size)Lake->FontCapacity * ENGA_MAX_CODEPOINT, real32);
}

inline Vector3 EngaVertexPosition(enga_vertex *Vertex)
{
    return Vector3(Vertex->Pos[0], Vertex->Pos[1], Vertex->Pos[2]);
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

internal uint16 *LakeFontMap(data_lake *Lake, uint32 Slot)
{
    return Lake->FontMap + (memory_size)Slot * ENGA_MAX_CODEPOINT;
}

internal real32 *LakeFontAdvance(data_lake *Lake, uint32 Slot)
{
    return Lake->FontAdvance + (memory_size)Slot * ENGA_MAX_CODEPOINT;
}

internal uint32 LakeAddMesh(data_lake *Lake, const char *Name, enga_vertex *Vertices, uint32 VertexCount, uint32 *Indices, uint32 IndexCount)
{
    uint32 Handle = {};

    Assert(Lake->MeshCount < Lake->MeshCapacity);
    Assert(VertexCount && IndexCount);
    Assert(VertexCount <= Lake->VertexCapacity - Lake->VertexUsed);
    Assert(IndexCount <= Lake->IndexCapacity - Lake->IndexUsed);

    uint32 Slot = Lake->MeshCount;

    Lake->MeshFirstVertex[Slot] = Lake->VertexUsed;
    Lake->MeshVertexCount[Slot] = VertexCount;
    Lake->MeshFirstIndex[Slot]  = Lake->IndexUsed;
    Lake->MeshIndexCount[Slot]  = IndexCount;

    CopySize((memory_size)VertexCount * sizeof(enga_vertex), Vertices, LakeMeshVertices(Lake, Slot));
    CopySize((memory_size)IndexCount * sizeof(uint32), Indices, LakeMeshIndices(Lake, Slot));

    Vector3 BoundsMin = Vector3( REAL32_LARGE,  REAL32_LARGE,  REAL32_LARGE);
    Vector3 BoundsMax = Vector3(-REAL32_LARGE, -REAL32_LARGE, -REAL32_LARGE);
    for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
    {
        Vector3 P = EngaVertexPosition(Vertices + VertexIndex);
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            BoundsMin.Elements[Axis] = Minimum(BoundsMin.Elements[Axis], P.Elements[Axis]);
            BoundsMax.Elements[Axis] = Maximum(BoundsMax.Elements[Axis], P.Elements[Axis]);
        }
    }
    Lake->MeshBoundsMin[Slot] = BoundsMin;
    Lake->MeshBoundsMax[Slot] = BoundsMax;

    AppendString(Lake->MeshNames + (memory_size)Slot * ENGA_MAX_ASSET_NAME, ENGA_MAX_ASSET_NAME, 0, Name);

    Lake->VertexUsed += VertexCount;
    Lake->IndexUsed  += IndexCount;
    Lake->MeshCount++;

    Handle = Slot;

    return Handle;
}

internal uint32 LakeAddTexture(data_lake *Lake, const char *Name, void *Pixels, uint32 Width, uint32 Height, bool32 SRGB, asset_image_format Format)
{
    uint32 Handle = {};

    Assert(Lake->TextureCount < Lake->TextureCapacity);

    uint64 ByteSize = (uint64)Width * Height * AssetImageFormatBytes(Format);
    Assert(Lake->PixelByteCount + ByteSize <= Lake->PixelByteCapacity);

    uint32 Slot = Lake->TextureCount;

    Lake->TextureFirstByte[Slot] = Lake->PixelByteCount;
    CopySize(ByteSize, Pixels, LakeTexturePixels(Lake, Slot));

    AppendString(Lake->TextureNames + (memory_size)Slot * ENGA_MAX_ASSET_NAME, ENGA_MAX_ASSET_NAME, 0, Name);
    Lake->TextureWidth[Slot]  = Width;
    Lake->TextureHeight[Slot] = Height;
    Lake->TextureSRGB[Slot]   = SRGB;
    Lake->TextureFormat[Slot] = (uint32)Format;
    Lake->TextureCount++;
    Lake->PixelByteCount += ByteSize;

    Handle = Slot;

    return Handle;
}

internal uint32 LakeAddCubemap(data_lake *Lake, const char *Name, void *Pixels, uint32 FaceSize, asset_image_format Format)
{
    uint32 Handle = {};

    Assert(Lake->CubemapCount < Lake->CubemapCapacity);

    uint64 ByteSize = (uint64)FaceSize * FaceSize * 6 * AssetImageFormatBytes(Format);
    Assert(Lake->PixelByteCount + ByteSize <= Lake->PixelByteCapacity);

    uint32 Slot = Lake->CubemapCount;

    Lake->CubemapFirstByte[Slot] = Lake->PixelByteCount;
    CopySize(ByteSize, Pixels, LakeCubemapPixels(Lake, Slot));

    AppendString(Lake->CubemapNames + (memory_size)Slot * ENGA_MAX_ASSET_NAME, ENGA_MAX_ASSET_NAME, 0, Name);
    Lake->CubemapFaceSize[Slot] = FaceSize;
    Lake->CubemapFormat[Slot]   = (uint32)Format;
    Lake->CubemapCount++;
    Lake->PixelByteCount += ByteSize;

    Handle = Slot;

    return Handle;
}

internal uint32 LakeAddFont(data_lake *Lake, const char *Name, asset_font_info *Info, uint16 *Map, real32 *Advances, void *AtlasPixels)
{
    uint32 Handle = {};

    Assert(Lake->FontCount < Lake->FontCapacity);

    uint32 TextureHandle = LakeAddTexture(Lake, Name, AtlasPixels, Info->AtlasSize, Info->AtlasSize, false, ImageFormat_RGBA8);

    uint32 Slot = Lake->FontCount;

    CopySize(ENGA_MAX_CODEPOINT * sizeof(uint16), Map, LakeFontMap(Lake, Slot));
    CopySize(ENGA_MAX_CODEPOINT * sizeof(real32), Advances, LakeFontAdvance(Lake, Slot));

    AppendString(Lake->FontNames + (memory_size)Slot * ENGA_MAX_ASSET_NAME, ENGA_MAX_ASSET_NAME, 0, Name);
    Lake->FontInfo[Slot]          = *Info;
    Lake->FontTextureHandle[Slot] = TextureHandle;

    Lake->FontCount++;

    Handle = Slot;

    return Handle;
}

internal void LakeLoadENGA(data_lake *Lake, void *PackData, uint32 PackSize)
{
    asset_pack Pack = AssetPackFromMemory(PackData, PackSize);

    for (uint32 Index = 0; Index < Pack.Count; ++Index)
    {
        asset_descriptor *Entry = Pack.Entries + Index;
        void *Data = AssetData(&Pack, Entry);
        Assert(Data);

        switch ((asset_type)Entry->Type)
        {
            case Asset_Mesh:
            {
                memory_size VertexBytes = (memory_size)Entry->Mesh.VertexCount * sizeof(enga_vertex);

                LakeAddMesh(Lake, Entry->Name, (enga_vertex *)Data, Entry->Mesh.VertexCount, (uint32 *)((uint8 *)Data + VertexBytes), Entry->Mesh.IndexCount);
            } break;

            case Asset_Image:
            {
                asset_image_format Format = (asset_image_format)Entry->Image.Format;

                if (Entry->Image.Layers == 6)
                {
                    LakeAddCubemap(Lake, Entry->Name, Data, Entry->Image.Width, Format);
                }
                else
                {
                    LakeAddTexture(Lake, Entry->Name, Data, Entry->Image.Width, Entry->Image.Height, Entry->Image.IsSRGB, Format);
                }
            } break;

            case Asset_Font:
            {
                memory_size MapBytes     = ENGA_MAX_CODEPOINT * sizeof(uint16);
                memory_size AdvanceBytes = ENGA_MAX_CODEPOINT * sizeof(real32);

                LakeAddFont(Lake, Entry->Name, &Entry->Font, (uint16 *)Data, (real32 *)((uint8 *)Data + MapBytes), (uint8 *)Data + MapBytes + AdvanceBytes);
            } break;

            default:
            {
                DebugLog("Lake: asset '%s' of type %u has no loader\n", Entry->Name, Entry->Type);
            } break;
        }
    }

    DebugLog("Lake loaded %u meshes (%u vertices, %u indices), %u textures, %u cubemaps, %u fonts\n",
             Lake->MeshCount, Lake->VertexUsed, Lake->IndexUsed, Lake->TextureCount, Lake->CubemapCount, Lake->FontCount);
}

internal uint32 LakeGetMeshHandle(data_lake *Lake, const char *Name)
{
    for (uint32 Index = 0; Index < Lake->MeshCount; ++Index)
    {
        if (StringsAreEqual(Lake->MeshNames + (memory_size)Index * ENGA_MAX_ASSET_NAME, Name))
        {
            return Index;
        }
    }

    Assert(!"Mesh not found");

    return 0;
}

internal uint32 LakeGetTextureHandle(data_lake *Lake, const char *Name)
{
    for (uint32 Index = 0; Index < Lake->TextureCount; ++Index)
    {
        if (StringsAreEqual(Lake->TextureNames + (memory_size)Index * ENGA_MAX_ASSET_NAME, Name))
        {
            return Index;
        }
    }

    Assert(!"Texture not found");

    return 0;
}

internal uint32 LakeGetCubemapHandle(data_lake *Lake, const char *Name)
{
    for (uint32 Index = 0; Index < Lake->CubemapCount; ++Index)
    {
        if (StringsAreEqual(Lake->CubemapNames + (memory_size)Index * ENGA_MAX_ASSET_NAME, Name))
        {
            return Index;
        }
    }

    Assert(!"Cubemap not found");

    return 0;
}

internal uint32 LakeGetFontHandle(data_lake *Lake, const char *Name)
{
    for (uint32 Index = 0; Index < Lake->FontCount; ++Index)
    {
        if (StringsAreEqual(Lake->FontNames + (memory_size)Index * ENGA_MAX_ASSET_NAME, Name))
        {
            return Index;
        }
    }

    Assert(!"Font not found");

    return 0;
}
