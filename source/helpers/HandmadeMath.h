#ifndef HANDMADEMATH_H
#define HANDMADEMATH_H

#include "Types.h"

// Day 026: базовые математические макросы и инлайны для геймплея.
// Сюда позже придут v2, v3, v4 структуры и операции над ними.

#define Square(x)     ((x) * (x))
#define Minimum(a, b) ((a) < (b) ? (a) : (b))
#define Maximum(a, b) ((a) > (b) ? (a) : (b))

inline real32 Lerp(real32 A, real32 t, real32 B)
{
    return (1.0f - t) * A + t * B;
}

inline real32 Clamp(real32 Min, real32 Value, real32 Max)
{
    real32 Result = Value;
    if (Result < Min) Result = Min;
    if (Result > Max) Result = Max;
    return Result;
}

inline real32 Clamp01(real32 Value)
{
    return Clamp(0.0f, Value, 1.0f);
}

#endif
