#ifndef GLTF_H
#define GLTF_H

#include "Types.h"
#include "Memory.h"
#include "Strings.h"
#include "KBNFormat.h"

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

internal uint8 *GLTFViewData(gltf_file *File, uint32 ViewIndex, uint32 *OutSize)
{
    json_value *View = JsonAt(File->BufferViews, ViewIndex);
    if (!View)
    {
        return 0;
    }

    uint32 Offset = JsonU32(JsonGet(View, "byteOffset"), 0);
    uint32 Length = JsonU32(JsonGet(View, "byteLength"), 0);
    if (Offset + Length > File->BinSize)
    {
        return 0;
    }

    if (OutSize)
    {
        *OutSize = Length;
    }
    return File->Bin + Offset;
}

internal uint8 *GLTFAccessorData(gltf_file *File, json_value *AccessorIndex, uint32 *OutCount, uint32 *OutComponentType)
{
    *OutCount = 0;
    *OutComponentType = 0;

    json_value *Accessor = JsonAt(File->Accessors, JsonU32(AccessorIndex, 0));
    if (!AccessorIndex || !Accessor)
    {
        return 0;
    }

    uint32 ViewSize = 0;
    uint8 *ViewData = GLTFViewData(File, JsonU32(JsonGet(Accessor, "bufferView"), 0), &ViewSize);
    if (!ViewData)
    {
        return 0;
    }

    uint32 Offset = JsonU32(JsonGet(Accessor, "byteOffset"), 0);
    if (Offset > ViewSize)
    {
        return 0;
    }

    *OutCount = JsonU32(JsonGet(Accessor, "count"), 0);
    *OutComponentType = JsonU32(JsonGet(Accessor, "componentType"), 0);
    return ViewData + Offset;
}

internal real32 *GLTFMeshVertices(memory_arena *Arena, gltf_file *File, json_value *Mesh, uint32 *OutVertexCount)
{
    *OutVertexCount = 0;

    json_value *Prim       = JsonAt(JsonGet(Mesh, "primitives"), 0);
    json_value *Attributes = JsonGet(Prim, "attributes");

    uint32  PosCount = 0;
    uint32  PosType  = 0;
    real32 *Pos = (real32 *)GLTFAccessorData(File, JsonGet(Attributes, "POSITION"), &PosCount, &PosType);
    if (!Pos || PosType != GLTF_FLOAT || !PosCount)
    {
        DebugLog("GLTF: mesh has no float POSITION\n");
        return 0;
    }

    uint32  UVCount = 0;
    uint32  UVType  = 0;
    real32 *UV = (real32 *)GLTFAccessorData(File, JsonGet(Attributes, "TEXCOORD_0"), &UVCount, &UVType);
    if (UV && UVType != GLTF_FLOAT)
    {
        DebugLog("GLTF: TEXCOORD_0 is not float, ignored\n");
        UV = 0;
    }

    uint32 IndexCount = 0;
    uint32 IndexType  = 0;
    uint8 *Indices = GLTFAccessorData(File, JsonGet(Prim, "indices"), &IndexCount, &IndexType);
    if (Indices && IndexType != GLTF_USHORT && IndexType != GLTF_UINT)
    {
        DebugLog("GLTF: unsupported index type %u\n", IndexType);
        return 0;
    }

    uint32  VertexCount = Indices ? IndexCount : PosCount;
    real32 *Out = PushArray(Arena, (memory_index)VertexCount * KBN_VERTEX_FLOATS, real32);

    for (uint32 v = 0; v < VertexCount; ++v)
    {
        uint32 Src = v;
        if (Indices)
        {
            Src = (IndexType == GLTF_USHORT) ? ((uint16 *)Indices)[v] : ((uint32 *)Indices)[v];
        }
        if (Src >= PosCount)
        {
            Src = 0;
        }

        real32 *Dst = Out + (memory_index)v * KBN_VERTEX_FLOATS;
        Dst[0] = Pos[Src*3 + 0];
        Dst[1] = Pos[Src*3 + 1];
        Dst[2] = Pos[Src*3 + 2];
        Dst[3] = 1.0f;
        Dst[4] = 1.0f;
        Dst[5] = 1.0f;
        Dst[6] = (UV && Src < UVCount) ? UV[Src*2 + 0] : 0.0f;
        Dst[7] = (UV && Src < UVCount) ? UV[Src*2 + 1] : 0.0f;
    }

    *OutVertexCount = VertexCount;
    return Out;
}

#endif
