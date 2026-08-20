#include "Types.h"
#include "EngineMath.h"
#include "Collider.h"

struct raycast_hit
{
    uint32  ColliderHandle;
    real32  Distance;
    Vector3 Point;
};

internal bool32 RayIntersectsAABB(ray Ray, Vector3 BoxMin, Vector3 BoxMax, real32 *OutDistance)
{
    real32 EnterDistance = 0.0f;
    real32 ExitDistance  = REAL32_LARGE;

    for (int Axis = 0; Axis < 3; ++Axis)
    {
        real32 Direction = Ray.Direction.Elements[Axis];
        real32 Origin    = Ray.Origin.Elements[Axis];

        if (Abs(Direction) < Epsilon32)
        {
            if (Origin < BoxMin.Elements[Axis] || Origin > BoxMax.Elements[Axis])
            {
                return false;
            }
        }
        else
        {
            real32 InvDirection = 1.0f / Direction;
            real32 StepToMin = (BoxMin.Elements[Axis] - Origin) * InvDirection;
            real32 StepToMax = (BoxMax.Elements[Axis] - Origin) * InvDirection;

            real32 StepToEnter = Minimum(StepToMin, StepToMax);
            real32 StepToExit  = Maximum(StepToMin, StepToMax);

            EnterDistance = Maximum(EnterDistance, StepToEnter);
            ExitDistance  = Minimum(ExitDistance, StepToExit);

            if (EnterDistance > ExitDistance)
            {
                return false;
            }
        }
    }

    *OutDistance = EnterDistance;
    return true;
}

internal bool32 RayIntersectsTriangle(ray Ray, Vector3 A, Vector3 B, Vector3 C, real32 *OutDistance)
{
    Vector3 AB = B - A;
    Vector3 AC = C - A;
    Vector3 PVec = Cross(Ray.Direction, AC);
    real32  Det  = Dot(AB, PVec);

    if (Abs(Det) < Epsilon32)
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

    real32 Distance = Dot(AC, QVec) * InvDet;
    if (Distance < 0.0f)
    {
        return false;
    }

    *OutDistance = Distance;

    return true;
}

internal bool32 RayIntersectsMesh(collision *Mesh, ray LocalRay, real32 *OutDistance)
{
    real32 BoxDistance;
    if (!RayIntersectsAABB(LocalRay, Mesh->BoundsMin, Mesh->BoundsMax, &BoxDistance))
    {
        return false;
    }

    bool32 Hit = false;
    real32 BestDistance = REAL32_LARGE;

    for (uint32 Index = 0; Index + 3 <= Mesh->IndexCount; Index += 3)
    {
        Vector3 A = GetCollisionMeshVertex(Mesh, Mesh->Indices[Index + 0]);
        Vector3 B = GetCollisionMeshVertex(Mesh, Mesh->Indices[Index + 1]);
        Vector3 C = GetCollisionMeshVertex(Mesh, Mesh->Indices[Index + 2]);

        real32 Distance;
        if (RayIntersectsTriangle(LocalRay, A, B, C, &Distance) && Distance < BestDistance)
        {
            BestDistance = Distance;
            Hit          = true;
        }
    }

    if (Hit)
    {
        *OutDistance = BestDistance;
    }

    return Hit;
}

internal uint32 RayCastColliders(ray WorldRay, collider *Colliders, uint32 ColliderCount, raycast_hit *OutHit)
{
    raycast_hit Hit = {};
    real32 BestDistance = REAL32_LARGE;

    for (uint32 Index = 0; Index < ColliderCount; ++Index)
    {
        collider *Current = Colliders + Index;

        Matrix4 ToLocal = Mat4InverseRigid(Current->LocalToWorld);

        ray LocalRay;
        LocalRay.Origin    = Mat4Transform(ToLocal, WorldRay.Origin, 1.0f);
        LocalRay.Direction = Mat4Transform(ToLocal, WorldRay.Direction, 0.0f);

        real32 Distance;
        if (RayIntersectsMesh(&Current->Mesh, LocalRay, &Distance) && Distance < BestDistance)
        {
            BestDistance = Distance;
            Hit.ColliderHandle   = Current->Handle;
        }
    }

    if (Hit.ColliderHandle)
    {
        Hit.Distance = BestDistance;
        Hit.Point    = WorldRay.Origin + WorldRay.Direction * BestDistance;
    }

    if (OutHit)
    {
        *OutHit = Hit;
    }

    return Hit.ColliderHandle;
}
