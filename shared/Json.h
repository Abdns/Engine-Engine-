#ifndef JSON_H
#define JSON_H

#include "Types.h"
#include "Memory.h"
#include "Strings.h"

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

    Assert(P->At < P->End);

    char *OnePastLast = P->At;
    P->At++;

    char *Result = PushArray(P->Arena, (memory_size)(OnePastLast - Start) + 1, char);
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
        Assert(At < P->End && *At == *Word);

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

        Assert(!"malformed JSON object");
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

        Assert(!"malformed JSON array");
        return Result;
    }
}

internal json_value *JsonParseValue(json_parser *P)
{
    JsonSkipWhitespace(P);
    Assert(P->At < P->End);

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

    Assert(!"unexpected character in JSON");
    return 0;
}

internal json_value *JsonParse(memory_arena *Arena, void *Data, uint32 Size, uint32 *OutErrorOffset)
{
    json_parser Parser = {};
    Parser.At    = (char *)Data;
    Parser.End   = Parser.At + Size;
    Parser.Arena = Arena;

    json_value *Result = JsonParseValue(&Parser);
    if (Parser.Error)
    {
        if (OutErrorOffset)
        {
            *OutErrorOffset = (uint32)(Parser.At - (char *)Data);
        }
        return 0;
    }

    return Result;
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

#endif
