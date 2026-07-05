#ifndef HANDMADEMATH_H
#define HANDMADEMATH_H

#include "Types.h"

// Day 026: базовые математические макросы и инлайны для геймплея.
// v2 уже здесь; v3, v4 придут позже.

// ---- v2: 2D-вектор ----
union v2
{
    struct { real32 X, Y; };
    real32 E[2];
};

inline v2 V2(real32 X, real32 Y)
{
    v2 Result;
    Result.X = X;
    Result.Y = Y;
    return Result;
}

inline v2 operator+(v2 A, v2 B)
{
    return V2(A.X + B.X, A.Y + B.Y);
}

inline v2 operator-(v2 A, v2 B)
{
    return V2(A.X - B.X, A.Y - B.Y);
}

inline v2 operator*(real32 S, v2 A)
{
    return V2(S * A.X, S * A.Y);
}

inline v2 operator*(v2 A, real32 S)
{
    return V2(S * A.X, S * A.Y);
}

inline v2 &operator+=(v2 &A, v2 B)
{
    A = A + B;
    return A;
}

inline v2 &operator-=(v2 &A, v2 B)
{
    A = A - B;
    return A;
}

// ---- v3: 3D-вектор (позиции сущностей — 3D-ready) ----
union v3
{
    struct { real32 X, Y, Z; };
    struct { v2 XY; real32 Z_; };   // .XY -> плоскость земли (для 2D-рендера-плейсхолдера)
    real32 E[3];
};

inline v3 V3(real32 X, real32 Y, real32 Z)
{
    v3 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    return Result;
}

inline v3 operator+(v3 A, v3 B)
{
    return V3(A.X + B.X, A.Y + B.Y, A.Z + B.Z);
}

inline v3 operator-(v3 A, v3 B)
{
    return V3(A.X - B.X, A.Y - B.Y, A.Z - B.Z);
}

inline v3 operator*(real32 S, v3 A)
{
    return V3(S * A.X, S * A.Y, S * A.Z);
}

inline v3 operator*(v3 A, real32 S)
{
    return V3(S * A.X, S * A.Y, S * A.Z);
}

inline v3 &operator+=(v3 &A, v3 B)
{
    A = A + B;
    return A;
}

inline v3 &operator-=(v3 &A, v3 B)
{
    A = A - B;
    return A;
}

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
