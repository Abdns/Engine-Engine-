#ifndef ENGINEMATH_H
#define ENGINEMATH_H

#include "Types.h"
#include "Intrinsics.h"

inline real32 DegToRad(real32 Degrees)
{
    return Degrees * (Pi32 / 180.0f);
}

union Vector2
{
    struct { real32 X, Y; };
    real32 Elements[2];

    Vector2() = default;
    Vector2(real32 InX, real32 InY) { X = InX; Y = InY; }
};

inline Vector2 operator+(Vector2 A, Vector2 B)
{
    return Vector2(A.X + B.X, A.Y + B.Y);
}

inline Vector2 operator-(Vector2 A, Vector2 B)
{
    return Vector2(A.X - B.X, A.Y - B.Y);
}

inline Vector2 operator*(real32 S, Vector2 A)
{
    return Vector2(S * A.X, S * A.Y);
}

inline Vector2 operator*(Vector2 A, real32 S)
{
    return Vector2(S * A.X, S * A.Y);
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
    real32 Elements[3];

    Vector3() = default;
    Vector3(real32 InX, real32 InY, real32 InZ) { X = InX; Y = InY; Z = InZ; }
};

inline Vector3 operator+(Vector3 A, Vector3 B)
{
    return Vector3(A.X + B.X, A.Y + B.Y, A.Z + B.Z);
}

inline Vector3 operator-(Vector3 A, Vector3 B)
{
    return Vector3(A.X - B.X, A.Y - B.Y, A.Z - B.Z);
}

inline Vector3 operator*(real32 S, Vector3 A)
{
    return Vector3(S * A.X, S * A.Y, S * A.Z);
}

inline Vector3 operator*(Vector3 A, real32 S)
{
    return Vector3(S * A.X, S * A.Y, S * A.Z);
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

inline real32 Dot(Vector3 A, Vector3 B)
{
    return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}

inline Vector3 Cross(Vector3 A, Vector3 B)
{
    return Vector3(A.Y * B.Z - A.Z * B.Y,
                   A.Z * B.X - A.X * B.Z,
                   A.X * B.Y - A.Y * B.X);
}

inline real32 LengthSq(Vector3 A)
{
    return Dot(A, A);
}

inline real32 Length(Vector3 A)
{
    return SquareRoot(Dot(A, A));
}

inline Vector3 Normalize(Vector3 A)
{
    real32 Len = Length(A);
    if (Len > 0.0f)
    {
        return (1.0f / Len) * A;
    }
    return A;
}

union Vector4
{
    struct { real32 X, Y, Z, W; };
    struct { Vector3 XYZ; real32 W_; };
    real32 Elements[4];

    Vector4() = default;
    Vector4(real32 InX, real32 InY, real32 InZ, real32 InW) { X = InX; Y = InY; Z = InZ; W = InW; }
};

struct Matrix4
{
    real32 Elements[4][4];
};

inline Matrix4 Mat4Identity(void)
{
    Matrix4 Result = {};
    Result.Elements[0][0] = 1.0f;
    Result.Elements[1][1] = 1.0f;
    Result.Elements[2][2] = 1.0f;
    Result.Elements[3][3] = 1.0f;

    return Result;
}

inline Matrix4 Mat4Multiply(Matrix4 A, Matrix4 B)
{
    Matrix4 Result = {};
    for (int Column = 0; Column < 4; ++Column)
    {
        for (int Row = 0; Row < 4; ++Row)
        {
            real32 Sum = 0.0f;
            for (int Inner = 0; Inner < 4; ++Inner)
            {
                Sum += A.Elements[Inner][Row] * B.Elements[Column][Inner];
            }
            Result.Elements[Column][Row] = Sum;
        }
    }

    return Result;
}

inline Matrix4 Mat4Translation(real32 X, real32 Y, real32 Z)
{
    Matrix4 Result = Mat4Identity();
    Result.Elements[3][0] = X;
    Result.Elements[3][1] = Y;
    Result.Elements[3][2] = Z;

    return Result;
}

inline Matrix4 Mat4RotationX(real32 Angle)
{
    real32 Cosine = Cos(Angle);
    real32 Sine   = Sin(Angle);
    Matrix4 Result = Mat4Identity();
    Result.Elements[1][1] =  Cosine;
    Result.Elements[1][2] =  Sine;
    Result.Elements[2][1] = -Sine;
    Result.Elements[2][2] =  Cosine;

    return Result;
}

inline Matrix4 Mat4RotationY(real32 Angle)
{
    real32 Cosine = Cos(Angle);
    real32 Sine   = Sin(Angle);
    Matrix4 Result = Mat4Identity();
    Result.Elements[0][0] =  Cosine;
    Result.Elements[0][2] = -Sine;
    Result.Elements[2][0] =  Sine;
    Result.Elements[2][2] =  Cosine;

    return Result;
}

inline Matrix4 Mat4RotationZ(real32 Angle)
{
    real32 Cosine = Cos(Angle);
    real32 Sine   = Sin(Angle);
    Matrix4 Result = Mat4Identity();
    Result.Elements[0][0] =  Cosine;
    Result.Elements[0][1] =  Sine;
    Result.Elements[1][0] = -Sine;
    Result.Elements[1][1] =  Cosine;

    return Result;
}

inline Matrix4 Mat4Perspective(real32 FovYRadians, real32 Aspect, real32 Near, real32 Far)
{
    real32 TanHalf = tanf(FovYRadians * 0.5f);
    Matrix4 Result = {};
    Result.Elements[0][0] = 1.0f / (Aspect * TanHalf);
    Result.Elements[1][1] = -1.0f / TanHalf;
    Result.Elements[2][2] = Far / (Near - Far);
    Result.Elements[2][3] = -1.0f;
    Result.Elements[3][2] = (Far * Near) / (Near - Far);

    return Result;
}

inline Matrix4 Mat4InverseRigid(Matrix4 M)
{
    Matrix4 Result = Mat4Identity();

    for (int Column = 0; Column < 3; ++Column)
    {
        for (int Row = 0; Row < 3; ++Row)
        {
            Result.Elements[Column][Row] = M.Elements[Row][Column];
        }
    }

    real32 Tx = M.Elements[3][0];
    real32 Ty = M.Elements[3][1];
    real32 Tz = M.Elements[3][2];

    Result.Elements[3][0] = -(Result.Elements[0][0] * Tx + Result.Elements[1][0] * Ty + Result.Elements[2][0] * Tz);
    Result.Elements[3][1] = -(Result.Elements[0][1] * Tx + Result.Elements[1][1] * Ty + Result.Elements[2][1] * Tz);
    Result.Elements[3][2] = -(Result.Elements[0][2] * Tx + Result.Elements[1][2] * Ty + Result.Elements[2][2] * Tz);

    return Result;
}

inline Vector3 Mat4TransformPoint(Matrix4 M, Vector3 P)
{
    return Vector3(M.Elements[0][0] * P.X + M.Elements[1][0] * P.Y + M.Elements[2][0] * P.Z + M.Elements[3][0],
                   M.Elements[0][1] * P.X + M.Elements[1][1] * P.Y + M.Elements[2][1] * P.Z + M.Elements[3][1],
                   M.Elements[0][2] * P.X + M.Elements[1][2] * P.Y + M.Elements[2][2] * P.Z + M.Elements[3][2]);
}

inline Vector3 Mat4TransformDirection(Matrix4 M, Vector3 D)
{
    return Vector3(M.Elements[0][0] * D.X + M.Elements[1][0] * D.Y + M.Elements[2][0] * D.Z,
                   M.Elements[0][1] * D.X + M.Elements[1][1] * D.Y + M.Elements[2][1] * D.Z,
                   M.Elements[0][2] * D.X + M.Elements[1][2] * D.Y + M.Elements[2][2] * D.Z);
}

#define REAL32_LARGE 1.0e30f

struct ray
{
    Vector3 Origin;
    Vector3 Direction;
};

inline ray RayFromScreen(real32 MouseX, real32 MouseY, real32 ViewportWidth, real32 ViewportHeight, real32 FovYRadians, Vector3 CameraPosition, Vector3 CameraRight, Vector3 CameraUp, Vector3 CameraForward)
{
    real32 NdcX = 2.0f * (MouseX + 0.5f) / ViewportWidth  - 1.0f;
    real32 NdcY = 1.0f - 2.0f * (MouseY + 0.5f) / ViewportHeight;

    real32 TanHalf = tanf(FovYRadians * 0.5f);
    real32 Aspect  = ViewportWidth / ViewportHeight;

    ray Result;
    Result.Origin    = CameraPosition;
    Result.Direction = Normalize(CameraRight * (NdcX * TanHalf * Aspect) + CameraUp * (NdcY * TanHalf) + CameraForward);

    return Result;
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
