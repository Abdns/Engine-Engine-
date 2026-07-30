#ifndef GLTF_H
#define GLTF_H

#include "Types.h"
#include "Memory.h"
#include "Strings.h"
#include "EngaFormat.h"

enum json_type
{
    JSON_Null = 0,
    JSON_Bool,
    JSON_Number,
    JSON_String,
    JSON_Array,
    JSON_Object,
};

struct json_value;

struct json_member
{
    char        *Key;
    json_value  *Value;
    json_member *Next;
};

struct json_value
{
    uint32       Type;
    real64       Number;
    char        *String;
    json_member *First;
    uint32       Count;
};

struct json_parser
{
    char         *At;
    char         *End;
    memory_arena *Arena;
    bool32        Error;
};

internal json_value *JsonParseValue(json_parser *P);

internal void JsonSkipWhitespace(json_parser *P)
{
    while (P->At < P->End && (*P->At == ' ' || *P->At == '\t' || *P->At == '\r' || *P->At == '\n'))
    {
        P->At++;
    }
}

internal bool32 JsonExpect(json_parser *P, char C)
{
    JsonSkipWhitespace(P);
    if (P->At < P->End && *P->At == C)
    {
        P->At++;
        return true;
    }

    P->Error = true;
    return false;
}

internal json_value *JsonNewValue(json_parser *P, uint32 Type)
{
    json_value *Result = PushStruct(P->Arena, json_value);
    ZeroStruct(*Result);
    Result->Type = Type;
    return Result;
}

internal char *JsonParseString(json_parser *P)
{
    if (!JsonExpect(P, '"'))
    {
        return 0;
    }

    char *Start = P->At;
    while (P->At < P->End && *P->At != '"')
    {
        if (*P->At == '\\')
        {
            P->At++;
        }
        P->At++;
    }

    if (P->At >= P->End)
    {
        P->Error = true;
        return 0;
    }

    char *OnePastLast = P->At;
    P->At++;

    char *Result = PushArray(P->Arena, (memory_index)(OnePastLast - Start) + 1, char);
    uint32 Length = 0;
    for (char *C = Start; C < OnePastLast; ++C)
    {
        char Ch = *C;
        if (Ch == '\\' && C + 1 < OnePastLast)
        {
            ++C;
            switch (*C)
            {
                case 'n': Ch = '\n'; break;
                case 't': Ch = '\t'; break;
                case 'r': Ch = '\r'; break;
                case 'b': Ch = '\b'; break;
                case 'f': Ch = '\f'; break;
                case 'u':
                {
                    Ch = '?';
                    C += 4;
                    if (C >= OnePastLast)
                    {
                        C = OnePastLast - 1;
                    }
                } break;
                default:
                {
                    Ch = *C;
                } break;
            }
        }
        Result[Length++] = Ch;
    }
    Result[Length] = 0;

    return Result;
}

internal real64 JsonParseNumber(json_parser *P)
{
    real64 Sign = 1.0;
    if (P->At < P->End && *P->At == '-')
    {
        Sign = -1.0;
        P->At++;
    }

    real64 Value = 0.0;
    while (P->At < P->End && *P->At >= '0' && *P->At <= '9')
    {
        Value = Value * 10.0 + (*P->At - '0');
        P->At++;
    }

    if (P->At < P->End && *P->At == '.')
    {
        P->At++;
        real64 Scale = 0.1;
        while (P->At < P->End && *P->At >= '0' && *P->At <= '9')
        {
            Value += Scale * (*P->At - '0');
            Scale *= 0.1;
            P->At++;
        }
    }

    if (P->At < P->End && (*P->At == 'e' || *P->At == 'E'))
    {
        P->At++;
        bool32 ExpNegative = false;
        if (P->At < P->End && (*P->At == '+' || *P->At == '-'))
        {
            ExpNegative = (*P->At == '-');
            P->At++;
        }

        int32 Exp = 0;
        while (P->At < P->End && *P->At >= '0' && *P->At <= '9')
        {
            Exp = Exp * 10 + (*P->At - '0');
            P->At++;
        }

        real64 Power = 1.0;
        for (int32 i = 0; i < Exp; ++i)
        {
            Power *= 10.0;
        }
        Value = ExpNegative ? Value / Power : Value * Power;
    }

    return Sign * Value;
}

internal bool32 JsonMatch(json_parser *P, const char *Word)
{
    char *At = P->At;
    while (*Word)
    {
        if (At >= P->End || *At != *Word)
        {
            P->Error = true;
            return false;
        }
        At++;
        Word++;
    }

    P->At = At;
    return true;
}

internal void JsonAppendMember(json_parser *P, json_value *Container, json_member **Last, char *Key, json_value *Value)
{
    json_member *Member = PushStruct(P->Arena, json_member);
    Member->Key   = Key;
    Member->Value = Value;
    Member->Next  = 0;

    if (*Last)
    {
        (*Last)->Next = Member;
    }
    else
    {
        Container->First = Member;
    }
    *Last = Member;
    Container->Count++;
}

internal json_value *JsonParseObject(json_parser *P)
{
    json_value *Result = JsonNewValue(P, JSON_Object);
    P->At++;

    JsonSkipWhitespace(P);
    if (P->At < P->End && *P->At == '}')
    {
        P->At++;
        return Result;
    }

    json_member *Last = 0;
    for (;;)
    {
        char *Key = JsonParseString(P);
        if (P->Error || !JsonExpect(P, ':'))
        {
            return Result;
        }

        json_value *Value = JsonParseValue(P);
        if (P->Error)
        {
            return Result;
        }

        JsonAppendMember(P, Result, &Last, Key, Value);

        JsonSkipWhitespace(P);
        if (P->At < P->End && *P->At == ',')
        {
            P->At++;
            continue;
        }
        if (P->At < P->End && *P->At == '}')
        {
            P->At++;
            return Result;
        }

        P->Error = true;
        return Result;
    }
}

internal json_value *JsonParseArray(json_parser *P)
{
    json_value *Result = JsonNewValue(P, JSON_Array);
    P->At++;

    JsonSkipWhitespace(P);
    if (P->At < P->End && *P->At == ']')
    {
        P->At++;
        return Result;
    }

    json_member *Last = 0;
    for (;;)
    {
        json_value *Value = JsonParseValue(P);
        if (P->Error)
        {
            return Result;
        }

        JsonAppendMember(P, Result, &Last, 0, Value);

        JsonSkipWhitespace(P);
        if (P->At < P->End && *P->At == ',')
        {
            P->At++;
            continue;
        }
        if (P->At < P->End && *P->At == ']')
        {
            P->At++;
            return Result;
        }

        P->Error = true;
        return Result;
    }
}

internal json_value *JsonParseValue(json_parser *P)
{
    JsonSkipWhitespace(P);
    if (P->At >= P->End)
    {
        P->Error = true;
        return 0;
    }

    char C = *P->At;
    if (C == '{')
    {
        return JsonParseObject(P);
    }
    if (C == '[')
    {
        return JsonParseArray(P);
    }
    if (C == '"')
    {
        json_value *Result = JsonNewValue(P, JSON_String);
        Result->String = JsonParseString(P);
        return Result;
    }
    if (C == 't')
    {
        json_value *Result = JsonNewValue(P, JSON_Bool);
        Result->Number = 1.0;
        JsonMatch(P, "true");
        return Result;
    }
    if (C == 'f')
    {
        json_value *Result = JsonNewValue(P, JSON_Bool);
        JsonMatch(P, "false");
        return Result;
    }
    if (C == 'n')
    {
        json_value *Result = JsonNewValue(P, JSON_Null);
        JsonMatch(P, "null");
        return Result;
    }
    if (C == '-' || (C >= '0' && C <= '9'))
    {
        json_value *Result = JsonNewValue(P, JSON_Number);
        Result->Number = JsonParseNumber(P);
        return Result;
    }

    P->Error = true;
    return 0;
}

internal json_value *JsonGet(json_value *Object, const char *Key)
{
    if (Object && Object->Type == JSON_Object)
    {
        for (json_member *Member = Object->First; Member; Member = Member->Next)
        {
            if (StringsAreEqual(Member->Key, Key))
            {
                return Member->Value;
            }
        }
    }

    return 0;
}

internal json_value *JsonAt(json_value *Array, uint32 Index)
{
    if (Array && Index < Array->Count)
    {
        json_member *Member = Array->First;
        for (uint32 i = 0; i < Index; ++i)
        {
            Member = Member->Next;
        }
        return Member->Value;
    }

    return 0;
}

internal uint32 JsonU32(json_value *Value, uint32 Default)
{
    return (Value && Value->Type == JSON_Number) ? (uint32)Value->Number : Default;
}

internal char *JsonCString(json_value *Value)
{
    return (Value && Value->Type == JSON_String) ? Value->String : 0;
}

#define GLTF_FLOAT  5126
#define GLTF_USHORT 5123
#define GLTF_UINT   5125

struct gltf_file
{
    json_value *Root;
    json_value *Accessors;
    json_value *BufferViews;
    uint8      *Bin;
    uint32      BinSize;
};

internal gltf_file ParseGLTF(memory_arena *Arena, void *Data, uint32 Size)
{
    gltf_file Result = {};

    if (!Data || !Size)
    {
        DebugLog("GLTF: empty file\n");
        return Result;
    }

    json_parser Parser = {};
    Parser.At    = (char *)Data;
    Parser.End   = Parser.At + Size;
    Parser.Arena = Arena;

    Result.Root = JsonParseValue(&Parser);
    if (Parser.Error)
    {
        DebugLog("GLTF: JSON parse error at byte %u\n", (uint32)(Parser.At - (char *)Data));
        Result.Root = 0;
        return Result;
    }

    Result.Accessors   = JsonGet(Result.Root, "accessors");
    Result.BufferViews = JsonGet(Result.Root, "bufferViews");

    return Result;
}

internal uint32 GLTFComponentSize(uint32 ComponentType)
{
    switch (ComponentType)
    {
        case GLTF_FLOAT:  return 4;
        case GLTF_UINT:   return 4;
        case GLTF_USHORT: return 2;
    }

    return 0;
}

internal uint8 *GLTFViewData(gltf_file *File, uint32 ViewIndex, uint32 *OutSize, uint32 *OutStride)
{
    json_value *View = JsonAt(File->BufferViews, ViewIndex);
    if (!View)
    {
        return 0;
    }

    if (JsonU32(JsonGet(View, "buffer"), 0) != 0)
    {
        DebugLog("GLTF: bufferView %u refers to a second buffer, unsupported\n", ViewIndex);
        return 0;
    }

    uint32 Offset = JsonU32(JsonGet(View, "byteOffset"), 0);
    uint32 Length = JsonU32(JsonGet(View, "byteLength"), 0);
    if (Offset > File->BinSize || Length > File->BinSize - Offset)
    {
        DebugLog("GLTF: bufferView %u runs past the binary chunk\n", ViewIndex);
        return 0;
    }

    *OutSize   = Length;
    *OutStride = JsonU32(JsonGet(View, "byteStride"), 0);
    return File->Bin + Offset;
}

internal uint8 *GLTFAccessorData(gltf_file *File, json_value *AccessorIndex, uint32 ComponentCount, uint32 *OutCount, uint32 *OutComponentType, uint32 *OutStride)
{
    *OutCount         = 0;
    *OutComponentType = 0;
    *OutStride        = 0;

    if (!AccessorIndex)
    {
        return 0;
    }

    json_value *Accessor = JsonAt(File->Accessors, JsonU32(AccessorIndex, 0));
    if (!Accessor)
    {
        DebugLog("GLTF: accessor %u does not exist\n", JsonU32(AccessorIndex, 0));
        return 0;
    }

    json_value *ViewIndex = JsonGet(Accessor, "bufferView");
    if (!ViewIndex)
    {
        DebugLog("GLTF: accessor without a bufferView is unsupported\n");
        return 0;
    }

    uint32 ComponentType = JsonU32(JsonGet(Accessor, "componentType"), 0);
    uint32 ElementSize   = GLTFComponentSize(ComponentType) * ComponentCount;
    if (!ElementSize)
    {
        DebugLog("GLTF: unsupported componentType %u\n", ComponentType);
        return 0;
    }

    uint32 ViewSize   = 0;
    uint32 ViewStride = 0;
    uint8 *ViewData   = GLTFViewData(File, JsonU32(ViewIndex, 0), &ViewSize, &ViewStride);
    if (!ViewData)
    {
        return 0;
    }

    uint32 Stride = ViewStride ? ViewStride : ElementSize;
    if (Stride < ElementSize)
    {
        DebugLog("GLTF: byteStride %u is smaller than the %u byte element\n", ViewStride, ElementSize);
        return 0;
    }

    uint32 Offset = JsonU32(JsonGet(Accessor, "byteOffset"), 0);
    uint32 Count  = JsonU32(JsonGet(Accessor, "count"), 0);
    if (!Count || Offset > ViewSize)
    {
        DebugLog("GLTF: accessor has no elements or starts past its bufferView\n");
        return 0;
    }

    uint64 Span = (uint64)(Count - 1) * Stride + ElementSize;
    if (Span > (uint64)(ViewSize - Offset))
    {
        DebugLog("GLTF: accessor spans %llu bytes but only %u remain in the bufferView\n", Span, ViewSize - Offset);
        return 0;
    }

    *OutCount         = Count;
    *OutComponentType = ComponentType;
    *OutStride        = Stride;
    return ViewData + Offset;
}

struct gltf_geometry
{
    void       *Blob;
    uint64      BlobSize;
    enga_vertex *Vertices;
    uint32     *Indices;
    uint32      VertexCount;
    uint32      IndexCount;
};

internal gltf_geometry GLTFMeshGeometry(memory_arena *Arena, gltf_file *File, json_value *Mesh)
{
    gltf_geometry Result = {};

    json_value *Prim       = JsonAt(JsonGet(Mesh, "primitives"), 0);
    json_value *Attributes = JsonGet(Prim, "attributes");

    uint32 PosCount  = 0;
    uint32 PosType   = 0;
    uint32 PosStride = 0;
    uint8 *Pos = GLTFAccessorData(File, JsonGet(Attributes, "POSITION"), 3, &PosCount, &PosType, &PosStride);
    if (!Pos || PosType != GLTF_FLOAT || !PosCount)
    {
        DebugLog("GLTF: mesh has no float POSITION\n");
        return Result;
    }

    uint32 UVCount  = 0;
    uint32 UVType   = 0;
    uint32 UVStride = 0;
    uint8 *UV = GLTFAccessorData(File, JsonGet(Attributes, "TEXCOORD_0"), 2, &UVCount, &UVType, &UVStride);
    if (UV && UVType != GLTF_FLOAT)
    {
        DebugLog("GLTF: TEXCOORD_0 is not float, ignored\n");
        UV = 0;
    }

    json_value *IndicesRef = JsonGet(Prim, "indices");

    uint32 SourceIndexCount = 0;
    uint32 IndexType        = 0;
    uint32 IndexStride      = 0;
    uint8 *SourceIndices = GLTFAccessorData(File, IndicesRef, 1, &SourceIndexCount, &IndexType, &IndexStride);
    if (IndicesRef && !SourceIndices)
    {
        DebugLog("GLTF: primitive declares indices but they could not be read\n");
        return Result;
    }

    if (SourceIndices && IndexType != GLTF_USHORT && IndexType != GLTF_UINT)
    {
        DebugLog("GLTF: unsupported index type %u\n", IndexType);
        return Result;
    }

    uint32 VertexCount = PosCount;
    uint32 IndexCount  = SourceIndices ? SourceIndexCount : PosCount;

    uint64 VertexBytes = (uint64)VertexCount * sizeof(enga_vertex);
    uint64 IndexBytes  = (uint64)IndexCount * sizeof(uint32);

    uint8 *Blob = (uint8 *)PushSize(Arena, VertexBytes + IndexBytes);

    enga_vertex *Out     = (enga_vertex *)Blob;
    uint32     *OutIndx = (uint32 *)(Blob + VertexBytes);

    for (uint32 v = 0; v < VertexCount; ++v)
    {
        real32 *SrcPos = (real32 *)(Pos + (memory_index)v * PosStride);

        enga_vertex *Dst = Out + v;
        Dst->Pos[0]   = SrcPos[0];
        Dst->Pos[1]   = SrcPos[1];
        Dst->Pos[2]   = SrcPos[2];
        Dst->Color[0] = 1.0f;
        Dst->Color[1] = 1.0f;
        Dst->Color[2] = 1.0f;
        Dst->UV[0]    = 0.0f;
        Dst->UV[1]    = 0.0f;

        if (UV && v < UVCount)
        {
            real32 *SrcUV = (real32 *)(UV + (memory_index)v * UVStride);
            Dst->UV[0] = SrcUV[0];
            Dst->UV[1] = SrcUV[1];
        }
    }

    for (uint32 i = 0; i < IndexCount; ++i)
    {
        uint32 Src = i;
        if (SourceIndices)
        {
            uint8 *SrcIndex = SourceIndices + (memory_index)i * IndexStride;
            Src = (IndexType == GLTF_USHORT) ? *(uint16 *)SrcIndex : *(uint32 *)SrcIndex;
        }
        if (Src >= VertexCount)
        {
            Src = 0;
        }

        OutIndx[i] = Src;
    }

    Result.Blob        = Blob;
    Result.BlobSize    = VertexBytes + IndexBytes;
    Result.Vertices    = Out;
    Result.Indices     = OutIndx;
    Result.VertexCount = VertexCount;
    Result.IndexCount  = IndexCount;

    return Result;
}

#endif
