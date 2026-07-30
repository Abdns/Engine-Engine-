#ifndef ASSETPACK_H
#define ASSETPACK_H

#include "Types.h"
#include "EngaFormat.h"

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
    if (Header->Magic != ENGA_MAGIC || Header->Version != ENGA_VERSION || Header->AssetTableOffset + Header->AssetCount * sizeof(asset_entry) > Size)
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

#endif
