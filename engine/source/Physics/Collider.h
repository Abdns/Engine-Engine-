#ifndef COLLIDER_H
#define COLLIDER_H

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

inline Vector3 GetCollisionMeshVertex(collision *Mesh, uint32 Index)
{
    return *(Vector3 *)((uint8 *)Mesh->Vertices + (uint64)Index * Mesh->VertexStride);
}

#endif
