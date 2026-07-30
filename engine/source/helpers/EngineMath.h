#ifndef ENGINEMATH_H
#define ENGINEMATH_H

#include "Types.h"
#include "Intrinsics.h"

union Vector2
{
    struct { real32 X, Y; };
    real32 E[2];

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
    real32 E[3];

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
    real32 E[4];

    Vector4() = default;
    Vector4(real32 InX, real32 InY, real32 InZ, real32 InW) { X = InX; Y = InY; Z = InZ; W = InW; }
};

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

inline Matrix4 Mat4InverseRigid(Matrix4 M)
{
    Matrix4 R = Mat4Identity();

    for (int c = 0; c < 3; ++c)
    {
        for (int r = 0; r < 3; ++r)
        {
            R.E[c][r] = M.E[r][c];
        }
    }

    real32 Tx = M.E[3][0];
    real32 Ty = M.E[3][1];
    real32 Tz = M.E[3][2];

    R.E[3][0] = -(R.E[0][0] * Tx + R.E[1][0] * Ty + R.E[2][0] * Tz);
    R.E[3][1] = -(R.E[0][1] * Tx + R.E[1][1] * Ty + R.E[2][1] * Tz);
    R.E[3][2] = -(R.E[0][2] * Tx + R.E[1][2] * Ty + R.E[2][2] * Tz);

    return R;
}

inline Vector3 Mat4TransformPoint(Matrix4 M, Vector3 P)
{
    return Vector3(M.E[0][0] * P.X + M.E[1][0] * P.Y + M.E[2][0] * P.Z + M.E[3][0],
                   M.E[0][1] * P.X + M.E[1][1] * P.Y + M.E[2][1] * P.Z + M.E[3][1],
                   M.E[0][2] * P.X + M.E[1][2] * P.Y + M.E[2][2] * P.Z + M.E[3][2]);
}

inline Vector3 Mat4TransformDirection(Matrix4 M, Vector3 D)
{
    return Vector3(M.E[0][0] * D.X + M.E[1][0] * D.Y + M.E[2][0] * D.Z,
                   M.E[0][1] * D.X + M.E[1][1] * D.Y + M.E[2][1] * D.Z,
                   M.E[0][2] * D.X + M.E[1][2] * D.Y + M.E[2][2] * D.Z);
}

#define REAL32_LARGE 1.0e30f

struct ray
{
    Vector3 Origin;
    Vector3 Direction;
};

inline ray RayFromScreen(real32 MouseX, real32 MouseY, real32 ViewportWidth, real32 ViewportHeight, real32 FovYRadians,
                         Vector3 CameraP, Vector3 CameraRight, Vector3 CameraUp, Vector3 CameraForward)
{
    real32 NdcX = 2.0f * (MouseX + 0.5f) / ViewportWidth  - 1.0f;
    real32 NdcY = 1.0f - 2.0f * (MouseY + 0.5f) / ViewportHeight;

    real32 TanHalf = tanf(FovYRadians * 0.5f);
    real32 Aspect  = ViewportWidth / ViewportHeight;

    ray Result;
    Result.Origin    = CameraP;
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
