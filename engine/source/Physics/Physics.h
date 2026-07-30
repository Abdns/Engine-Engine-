#ifndef PHYSICS_H
#define PHYSICS_H

#include "Types.h"
#include "EngineMath.h"

struct collision_mesh
{
    void   *Vertices;
    uint32  VertexStride;
    uint32 *Indices;
    uint32  IndexCount;
    Vector3 BoundsMin;
    Vector3 BoundsMax;
};

struct collider
{
    uint32         Handle;
    Matrix4        Transform;
    collision_mesh Mesh;
};

struct raycast_hit
{
    uint32  Handle;
    real32  Distance;
    Vector3 Point;
};

inline Vector3 CollisionMeshVertex(collision_mesh *Mesh, uint32 Index)
{
    return *(Vector3 *)((uint8 *)Mesh->Vertices + (uint64)Index * Mesh->VertexStride);
}

internal bool32 RayIntersectsAABB(ray Ray, Vector3 BoxMin, Vector3 BoxMax, real32 *OutT)
{
    real32 TNear = 0.0f;
    real32 TFar  = REAL32_LARGE;

    for (int Axis = 0; Axis < 3; ++Axis)
    {
        real32 D = Ray.Direction.E[Axis];
        real32 O = Ray.Origin.E[Axis];

        if (AbsoluteValue(D) < 1.0e-8f)
        {
            if (O < BoxMin.E[Axis] || O > BoxMax.E[Axis])
            {
                return false;
            }
        }
        else
        {
            real32 InvD = 1.0f / D;
            real32 T0 = (BoxMin.E[Axis] - O) * InvD;
            real32 T1 = (BoxMax.E[Axis] - O) * InvD;
            if (T0 > T1)
            {
                real32 Swap = T0;
                T0 = T1;
                T1 = Swap;
            }

            if (T0 > TNear) TNear = T0;
            if (T1 < TFar)  TFar  = T1;
            if (TNear > TFar)
            {
                return false;
            }
        }
    }

    *OutT = TNear;
    return true;
}

internal bool32 RayIntersectsTriangle(ray Ray, Vector3 A, Vector3 B, Vector3 C, real32 *OutT)
{
    Vector3 AB = B - A;
    Vector3 AC = C - A;
    Vector3 PVec = Cross(Ray.Direction, AC);
    real32  Det  = Dot(AB, PVec);

    if (AbsoluteValue(Det) < 1.0e-8f)
    {
        return false;
    }

    real32 InvDet = 1.0f / Det;

    Vector3 TVec = Ray.Origin - A;
    real32  U    = Dot(TVec, PVec) * InvDet;
    if (U < 0.0f || U > 1.0f)
    {
        return false;
    }

    Vector3 QVec = Cross(TVec, AB);
    real32  V    = Dot(Ray.Direction, QVec) * InvDet;
    if (V < 0.0f || U + V > 1.0f)
    {
        return false;
    }

    real32 T = Dot(AC, QVec) * InvDet;
    if (T < 0.0f)
    {
        return false;
    }

    *OutT = T;
    return true;
}

internal bool32 RayIntersectsMesh(collision_mesh *Mesh, ray LocalRay, real32 *OutT)
{
    if (!Mesh->Vertices || !Mesh->Indices || Mesh->IndexCount < 3)
    {
        return false;
    }

    real32 BoxT;
    if (!RayIntersectsAABB(LocalRay, Mesh->BoundsMin, Mesh->BoundsMax, &BoxT))
    {
        return false;
    }

    bool32 Hit   = false;
    real32 BestT = REAL32_LARGE;

    for (uint32 Index = 0; Index + 2 < Mesh->IndexCount; Index += 3)
    {
        Vector3 A = CollisionMeshVertex(Mesh, Mesh->Indices[Index + 0]);
        Vector3 B = CollisionMeshVertex(Mesh, Mesh->Indices[Index + 1]);
        Vector3 C = CollisionMeshVertex(Mesh, Mesh->Indices[Index + 2]);

        real32 T;
        if (RayIntersectsTriangle(LocalRay, A, B, C, &T) && T < BestT)
        {
            BestT = T;
            Hit   = true;
        }
    }

    if (Hit)
    {
        *OutT = BestT;
    }

    return Hit;
}

internal uint32 RayCastColliders(ray WorldRay, collider *Colliders, uint32 ColliderCount, raycast_hit *OutHit)
{
    raycast_hit Hit = {};
    real32 BestT = REAL32_LARGE;

    for (uint32 Index = 0; Index < ColliderCount; ++Index)
    {
        collider *Current = Colliders + Index;

        Matrix4 ToLocal = Mat4InverseRigid(Current->Transform);

        ray LocalRay;
        LocalRay.Origin    = Mat4TransformPoint(ToLocal, WorldRay.Origin);
        LocalRay.Direction = Mat4TransformDirection(ToLocal, WorldRay.Direction);

        real32 T;
        if (RayIntersectsMesh(&Current->Mesh, LocalRay, &T) && T < BestT)
        {
            BestT      = T;
            Hit.Handle = Current->Handle;
        }
    }

    if (Hit.Handle)
    {
        Hit.Distance = BestT;
        Hit.Point    = WorldRay.Origin + WorldRay.Direction * BestT;
    }

    if (OutHit)
    {
        *OutHit = Hit;
    }

    return Hit.Handle;
}

#endif
