#ifndef FONT_H
#define FONT_H

#include "Types.h"
#include "Memory.h"
#include "EngineMath.h"
#include "EngaFormat.h"

#pragma warning(push, 0)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#pragma warning(pop)

struct loaded_font
{
    void            *Blob;
    uint16          *Map;
    real32          *Advances;
    uint8           *Pixels;
    asset_font_info  Info;
};

internal loaded_font BakeFont(memory_arena *Arena, void *TTFData, real32 PixelHeight, int32 *Codepoints, uint32 CodepointCount, uint32 AtlasSize)
{
    loaded_font Result = {};

    Assert(TTFData && PixelHeight > 0.0f && Codepoints && CodepointCount && AtlasSize);

    uint8 *TTF = (uint8 *)TTFData;

    stbtt_fontinfo Font;
    int Ok = stbtt_InitFont(&Font, TTF, stbtt_GetFontOffsetForIndex(TTF, 0));
    Assert(Ok);

    real32 Scale = stbtt_ScaleForPixelHeight(&Font, PixelHeight);

    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    for (uint32 Index = 0; Index < CodepointCount; ++Index)
    {
        int32 X0, Y0, X1, Y1;
        stbtt_GetCodepointBitmapBox(&Font, Codepoints[Index], Scale, Scale, &X0, &Y0, &X1, &Y1);

        MinX = Minimum(MinX, X0);
        MinY = Minimum(MinY, Y0);
        MaxX = Maximum(MaxX, X1);
        MaxY = Maximum(MaxY, Y1);
    }

    int32 CellWidth  = MaxX - MinX;
    int32 CellHeight = MaxY - MinY;
    Assert(CellWidth > 0 && CellHeight > 0);

    uint32 Columns = AtlasSize / (uint32)CellWidth;
    uint32 Rows    = (CodepointCount + Columns) / Columns;
    if (!Columns || Rows * (uint32)CellHeight > AtlasSize)
    {
        DebugLog("BakeFont: %u cells of %dx%d do not fit into a %ux%u atlas\n", CodepointCount, CellWidth, CellHeight, AtlasSize, AtlasSize);
        Assert(!"font atlas too small");
    }

    memory_size MapBytes     = ENGA_MAX_CODEPOINT * sizeof(uint16);
    memory_size AdvanceBytes = ENGA_MAX_CODEPOINT * sizeof(real32);
    memory_size PixelBytes   = (memory_size)AtlasSize * AtlasSize * 4;

    uint8  *Blob     = (uint8 *)PushSize(Arena, MapBytes + AdvanceBytes + PixelBytes);
    uint16 *Map      = (uint16 *)Blob;
    real32 *Advances = (real32 *)(Blob + MapBytes);
    uint8  *Pixels   = Blob + MapBytes + AdvanceBytes;

    ZeroSize(MapBytes + AdvanceBytes, Blob);

    temporary_memory Temp = BeginTemporaryMemory(Arena);

    uint8 *Alpha = PushArray(Arena, (memory_size)AtlasSize * AtlasSize, uint8);
    ZeroSize((memory_size)AtlasSize * AtlasSize, Alpha);

    for (uint32 Index = 0; Index < CodepointCount; ++Index)
    {
        int32 Codepoint = Codepoints[Index];
        Assert(Codepoint > 0 && Codepoint < ENGA_MAX_CODEPOINT);

        int32 X0, Y0, X1, Y1;
        stbtt_GetCodepointBitmapBox(&Font, Codepoint, Scale, Scale, &X0, &Y0, &X1, &Y1);

        uint32 Cell  = Index + 1;
        int32  CellX = (int32)((Cell % Columns) * (uint32)CellWidth);
        int32  CellY = (int32)((Cell / Columns) * (uint32)CellHeight);

        uint8 *Dest = Alpha + (memory_size)(CellY - MinY + Y0) * AtlasSize + (CellX - MinX + X0);
        stbtt_MakeCodepointBitmap(&Font, Dest, X1 - X0, Y1 - Y0, (int)AtlasSize, Scale, Scale, Codepoint);

        int32 AdvanceUnits, BearingUnits;
        stbtt_GetCodepointHMetrics(&Font, Codepoint, &AdvanceUnits, &BearingUnits);

        Map[Codepoint]      = (uint16)Cell;
        Advances[Codepoint] = (real32)AdvanceUnits * Scale;
    }

    for (memory_size i = 0; i < (memory_size)AtlasSize * AtlasSize; ++i)
    {
        uint8 *Out = Pixels + i * 4;
        Out[0] = 255;
        Out[1] = 255;
        Out[2] = 255;
        Out[3] = Alpha[i];
    }

    EndTemporaryMemory(Temp);

    int32 AscentUnits, DescentUnits, LineGapUnits;
    stbtt_GetFontVMetrics(&Font, &AscentUnits, &DescentUnits, &LineGapUnits);

    Result.Blob             = Blob;
    Result.Map              = Map;
    Result.Advances         = Advances;
    Result.Pixels           = Pixels;
    Result.Info.AtlasSize   = AtlasSize;
    Result.Info.CellWidth   = (uint32)CellWidth;
    Result.Info.CellHeight  = (uint32)CellHeight;
    Result.Info.OriginX     = (real32)-MinX;
    Result.Info.OriginY     = (real32)-MinY;
    Result.Info.LineAdvance = (real32)(AscentUnits - DescentUnits + LineGapUnits) * Scale;

    return Result;
}

#endif
