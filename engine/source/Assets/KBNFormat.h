#ifndef KBNFORMAT_H
#define KBNFORMAT_H

#include "Types.h"

#define KBN_MAGIC   (((uint32)'K') | ((uint32)'B' << 8) | ((uint32)'N' << 16) | ((uint32)'1' << 24))
#define KBN_VERSION 1

#define KBN_MAX_ASSET_NAME 32
#define KBN_VERTEX_FLOATS  8

enum asset_type
{
    Asset_None = 0,
    Asset_Image,
    Asset_Sound,
    Asset_Font,
    Asset_Mesh,
};

struct asset_file_header
{
    uint32 Magic;
    uint32 Version;
    uint32 AssetCount;
    uint32 AssetTableOffset;
};

struct asset_mesh_info
{
    uint32 VertexCount;
};

struct asset_image_info
{
    uint32 Width;
    uint32 Height;
    uint32 SRGB;
};

struct asset_entry
{
    uint32 Type;
    char   Name[KBN_MAX_ASSET_NAME];
    uint64 Offset;
    uint64 Size;
    union
    {
        asset_mesh_info  Mesh;
        asset_image_info Image;
    };
};

#endif
