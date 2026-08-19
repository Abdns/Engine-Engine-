#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "Types.h"
#include "EngineMath.h"

struct collision
{
    void   *Vertices;
    uint32 *Indices;
    uint32  VertexStride;
    uint32  VertexCount;
    uint32  IndexCount;
    Vector3 BoundsMin;
    Vector3 BoundsMax;
};

inline Vector3 GetCollisionMeshVertex(collision *Mesh, uint32 Index)
{
    return *(Vector3 *)((uint8 *)Mesh->Vertices + (uint64)Index * Mesh->VertexStride);
}

struct collider
{
    uint32    Handle;
    collision Mesh;
    Matrix4   LocalToWorld;
};

inline collider MakeCollider(uint32 Handle, Matrix4 LocalToWorld, collision Mesh)
{
    collider Result;
    Result.Handle       = Handle;
    Result.LocalToWorld = LocalToWorld;
    Result.Mesh         = Mesh;

    return Result;
}

struct hull_plane
{
    Vector3 Normal;
    real32  Distance;
};

struct hull
{
    hull_plane *Planes;
    uint32      PlaneCount;
};

inline uint32 BuildHullPlanes(collision *Mesh, hull_plane *Planes, uint32 MaxPlanes)
{
    uint32 Count = 0;

    for (uint32 Index = 0; Index + 3 <= Mesh->IndexCount && Count < MaxPlanes; Index += 3)
    {
        Vector3 A = GetCollisionMeshVertex(Mesh, Mesh->Indices[Index + 0]);
        Vector3 B = GetCollisionMeshVertex(Mesh, Mesh->Indices[Index + 1]);
        Vector3 C = GetCollisionMeshVertex(Mesh, Mesh->Indices[Index + 2]);

        Vector3 Normal = Cross(B - A, C - A);
        real32  Len    = Length(Normal);
        if (Len < Epsilon32)
        {
            continue;
        }

        hull_plane *Plane = Planes + Count++;
        Plane->Normal   = (1.0f / Len) * Normal;
        Plane->Distance = Dot(Plane->Normal, A);

        if (Plane->Distance < 0.0f)
        {
            Plane->Normal   = -1.0f * Plane->Normal;
            Plane->Distance = -Plane->Distance;
        }
    }

    return Count;
}

inline bool32 HullDeepestPlane(hull_plane *Planes, uint32 PlaneCount, Vector3 P, uint32 *OutPlane, real32 *OutDepth)
{
    real32 BestDepth = REAL32_LARGE;
    uint32 BestPlane = 0;

    for (uint32 Index = 0; Index < PlaneCount; ++Index)
    {
        real32 Depth = Planes[Index].Distance - Dot(Planes[Index].Normal, P);
        if (Depth < 0.0f)
        {
            return false;
        }

        if (Depth < BestDepth)
        {
            BestDepth = Depth;
            BestPlane = Index;
        }
    }

    *OutPlane = BestPlane;
    *OutDepth = BestDepth;

    return PlaneCount > 0;
}

inline void ComputeWorldAABB(Matrix4 LocalToWorld, Vector3 LocalMin, Vector3 LocalMax, Vector3 *OutMin, Vector3 *OutMax)
{
    Vector3 Min = Vector3(REAL32_LARGE, REAL32_LARGE, REAL32_LARGE);
    Vector3 Max = Vector3(-REAL32_LARGE, -REAL32_LARGE, -REAL32_LARGE);

    for (uint32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        Vector3 Corner = Vector3((CornerIndex & 1) ? LocalMax.X : LocalMin.X,
                                 (CornerIndex & 2) ? LocalMax.Y : LocalMin.Y,
                                 (CornerIndex & 4) ? LocalMax.Z : LocalMin.Z);

        Vector3 P = Mat4Transform(LocalToWorld, Corner, 1.0f);

        for (int Axis = 0; Axis < 3; ++Axis)
        {
            Min.Elements[Axis] = Minimum(Min.Elements[Axis], P.Elements[Axis]);
            Max.Elements[Axis] = Maximum(Max.Elements[Axis], P.Elements[Axis]);
        }
    }

    *OutMin = Min;
    *OutMax = Max;
}

#endif
