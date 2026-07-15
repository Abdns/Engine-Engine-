#ifndef ENGINEMATH_H
#define ENGINEMATH_H

#include "Types.h"
#include "Intrinsics.h"

union Vector2
{
    struct { real32 X, Y; };
    real32 E[2];
};

inline Vector2 V2(real32 X, real32 Y)
{
    Vector2 Result;
    Result.X = X;
    Result.Y = Y;
    return Result;
}

inline Vector2 operator+(Vector2 A, Vector2 B)
{
    return V2(A.X + B.X, A.Y + B.Y);
}

inline Vector2 operator-(Vector2 A, Vector2 B)
{
    return V2(A.X - B.X, A.Y - B.Y);
}

inline Vector2 operator*(real32 S, Vector2 A)
{
    return V2(S * A.X, S * A.Y);
}

inline Vector2 operator*(Vector2 A, real32 S)
{
    return V2(S * A.X, S * A.Y);
}

inline Vector2 &operator+=(Vector2 &A, Vector2 B)
{
    A = A + B;
    return A;
}

inline Vector2 &operator-=(Vector2 &A, Vector2 B)
{
    A = A - B;
    return A;
}

union Vector3
{
    struct { real32 X, Y, Z; };
    struct { Vector2 XY; real32 Z_; };
    real32 E[3];
};

inline Vector3 V3(real32 X, real32 Y, real32 Z)
{
    Vector3 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    return Result;
}

inline Vector3 operator+(Vector3 A, Vector3 B)
{
    return V3(A.X + B.X, A.Y + B.Y, A.Z + B.Z);
}

inline Vector3 operator-(Vector3 A, Vector3 B)
{
    return V3(A.X - B.X, A.Y - B.Y, A.Z - B.Z);
}

inline Vector3 operator*(real32 S, Vector3 A)
{
    return V3(S * A.X, S * A.Y, S * A.Z);
}

inline Vector3 operator*(Vector3 A, real32 S)
{
    return V3(S * A.X, S * A.Y, S * A.Z);
}

inline Vector3 &operator+=(Vector3 &A, Vector3 B)
{
    A = A + B;
    return A;
}

inline Vector3 &operator-=(Vector3 &A, Vector3 B)
{
    A = A - B;
    return A;
}

union Vector4
{
    struct { real32 X, Y, Z, W; };
    struct { Vector3 XYZ; real32 W_; };
    real32 E[4];
};

inline Vector4 V4(real32 X, real32 Y, real32 Z, real32 W)
{
    Vector4 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
    return Result;
}

struct Matrix4
{
    real32 E[4][4];
};

inline Matrix4 Mat4Identity(void)
{
    Matrix4 R = {};
    R.E[0][0] = 1.0f;
    R.E[1][1] = 1.0f;
    R.E[2][2] = 1.0f;
    R.E[3][3] = 1.0f;
    return R;
}

inline Matrix4 Mat4Multiply(Matrix4 A, Matrix4 B)
{
    Matrix4 R = {};
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            real32 Sum = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                Sum += A.E[k][r] * B.E[c][k];
            }
            R.E[c][r] = Sum;
        }
    }
    return R;
}

inline Matrix4 Mat4Translation(real32 X, real32 Y, real32 Z)
{
    Matrix4 R = Mat4Identity();
    R.E[3][0] = X;
    R.E[3][1] = Y;
    R.E[3][2] = Z;
    return R;
}

inline Matrix4 Mat4RotationX(real32 Angle)
{
    real32 c = Cos(Angle);
    real32 s = Sin(Angle);
    Matrix4 R = Mat4Identity();
    R.E[1][1] =  c;
    R.E[1][2] =  s;
    R.E[2][1] = -s;
    R.E[2][2] =  c;
    return R;
}

inline Matrix4 Mat4RotationY(real32 Angle)
{
    real32 c = Cos(Angle);
    real32 s = Sin(Angle);
    Matrix4 R = Mat4Identity();
    R.E[0][0] =  c;
    R.E[0][2] = -s;
    R.E[2][0] =  s;
    R.E[2][2] =  c;
    return R;
}

inline Matrix4 Mat4RotationZ(real32 Angle)
{
    real32 c = Cos(Angle);
    real32 s = Sin(Angle);
    Matrix4 R = Mat4Identity();
    R.E[0][0] =  c;
    R.E[0][1] =  s;
    R.E[1][0] = -s;
    R.E[1][1] =  c;
    return R;
}

inline Matrix4 Mat4Perspective(real32 FovYRadians, real32 Aspect, real32 Near, real32 Far)
{
    real32 TanHalf = tanf(FovYRadians * 0.5f);
    Matrix4 R = {};
    R.E[0][0] = 1.0f / (Aspect * TanHalf);
    R.E[1][1] = -1.0f / TanHalf;
    R.E[2][2] = Far / (Near - Far);
    R.E[2][3] = -1.0f;
    R.E[3][2] = (Far * Near) / (Near - Far);
    return R;
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
