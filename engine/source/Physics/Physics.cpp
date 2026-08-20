#include "Types.h"
#include "EngineMath.h"
#include "Memory.h"
#include "Collider.h"

#define PHYSICS_TICK              (1.0f / 60.0f)
#define PHYSICS_ACCUMULATOR_CAP   (4.0f * PHYSICS_TICK)
#define PHYSICS_SUBSTEPS          4
#define PHYSICS_RESTITUTION       0.5f
#define PHYSICS_RESTITUTION_MIN_SPEED 1.0f
#define PHYSICS_SOLVER_ITERATIONS 4
#define PHYSICS_MAX_BIAS_SPEED    1.5f
#define PHYSICS_MAX_CONTACTS      512
#define PHYSICS_MAX_PAIR_CONTACTS 8
#define PHYSICS_CONTACT_MERGE_DISTANCE_SQ 0.0001f
#define PHYSICS_BIAS_FACTOR       0.2f
#define PHYSICS_PENETRATION_SLOP  0.01f
#define PHYSICS_FRICTION          0.4f
#define PHYSICS_ANGULAR_DAMPING   0.5f
#define PHYSICS_GRAVITY_Y        -9.8f

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

struct physics_body
{
    Vector3   Position;
    Matrix4   Rotation;
    Vector3   Velocity;
    Vector3   AngularVelocity;
    real32    InvMass;
    real32    InvInertia;
    collider  Collider;
    hull      Hull;
    Vector3   BoundsMin;
    Vector3   BoundsMax;
};

struct contact
{
    physics_body *A;
    physics_body *B;
    Vector3 Point;
    Vector3 Normal;
    real32  Depth;
    real32  RestitutionBias;
    real32  AccumulatedNormal;
    Vector3 AccumulatedFriction;
};

struct physics_world
{
    physics_body *Bodies;
    uint32        BodyCount;
    uint32        BodyCapacity;

    hull  *MeshHulls;
    uint32 MeshHullCapacity;

    real32 Accumulator;
};

internal uint32 BuildHullPlanes(collision *Mesh, hull_plane *Planes, uint32 MaxPlanes)
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

internal bool32 HullDeepestPlane(hull_plane *Planes, uint32 PlaneCount, Vector3 P, uint32 *OutPlane, real32 *OutDepth)
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

internal hull BuildMeshHull(memory_arena *Arena, collision Mesh)
{
    uint32 MaxPlanes = Mesh.IndexCount / 3;

    hull Result;
    Result.Planes     = PushArray(Arena, MaxPlanes, hull_plane);
    Result.PlaneCount = BuildHullPlanes(&Mesh, Result.Planes, MaxPlanes);

    return Result;
}

internal void ComputeWorldAABB(Matrix4 LocalToWorld, Vector3 LocalMin, Vector3 LocalMax, Vector3 *OutMin, Vector3 *OutMax)
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

internal Vector3 ContactRelativeVelocity(contact *Contact)
{
    Vector3 rA = Contact->Point - Contact->A->Position;
    Vector3 rB = Contact->Point - Contact->B->Position;

    return (Contact->A->Velocity + Cross(Contact->A->AngularVelocity, rA))
         - (Contact->B->Velocity + Cross(Contact->B->AngularVelocity, rB));
}

internal Matrix4 BodyLocalToWorld(physics_body *Body)
{
    return Mat4Multiply(Mat4Translation(Body->Position.X, Body->Position.Y, Body->Position.Z), Body->Rotation);
}

internal void IntegrateOrientation(physics_body *Body, real32 dt)
{
    Vector3 AxisX = Mat4Column(Body->Rotation, 0);
    Vector3 AxisY = Mat4Column(Body->Rotation, 1);

    AxisX += dt * Cross(Body->AngularVelocity, AxisX);
    AxisY += dt * Cross(Body->AngularVelocity, AxisY);

    AxisX = Normalize(AxisX);
    Vector3 AxisZ = Normalize(Cross(AxisX, AxisY));
    AxisY = Cross(AxisZ, AxisX);

    Mat4SetColumn(&Body->Rotation, 0, AxisX);
    Mat4SetColumn(&Body->Rotation, 1, AxisY);
    Mat4SetColumn(&Body->Rotation, 2, AxisZ);
}

internal uint32 CollectHullContacts(physics_body *A, physics_body *B, contact *Contacts, uint32 MaxContacts)
{
    Matrix4 WorldFromB = B->Collider.LocalToWorld;
    Matrix4 BFromA     = Mat4Multiply(Mat4InverseRigid(WorldFromB), A->Collider.LocalToWorld);

    uint32 Count = 0;

    for (uint32 Index = 0; Index < A->Collider.Mesh.VertexCount && Count < MaxContacts; ++Index)
    {
        Vector3 P = Mat4Transform(BFromA, GetCollisionMeshVertex(&A->Collider.Mesh, Index), 1.0f);

        if (P.X < B->Collider.Mesh.BoundsMin.X || P.X > B->Collider.Mesh.BoundsMax.X ||
            P.Y < B->Collider.Mesh.BoundsMin.Y || P.Y > B->Collider.Mesh.BoundsMax.Y ||
            P.Z < B->Collider.Mesh.BoundsMin.Z || P.Z > B->Collider.Mesh.BoundsMax.Z)
        {
            continue;
        }

        uint32 PlaneIndex = 0;
        real32 Depth      = 0.0f;
        if (!HullDeepestPlane(B->Hull.Planes, B->Hull.PlaneCount, P, &PlaneIndex, &Depth))
        {
            continue;
        }

        Vector3 Normal = Mat4Transform(WorldFromB, B->Hull.Planes[PlaneIndex].Normal, 0.0f);
        if (Dot(Normal, A->Position - B->Position) <= 0.0f)
        {
            continue;
        }

        Vector3 WorldPoint = Mat4Transform(WorldFromB, P, 1.0f);

        bool32 Duplicate = false;
        for (uint32 Existing = 0; Existing < Count; ++Existing)
        {
            if (LengthSq(Contacts[Existing].Point - WorldPoint) < PHYSICS_CONTACT_MERGE_DISTANCE_SQ)
            {
                Duplicate = true;
                break;
            }
        }
        if (Duplicate)
        {
            continue;
        }

        contact *Contact = Contacts + Count++;
        Contact->A      = A;
        Contact->B      = B;
        Contact->Point  = WorldPoint;
        Contact->Normal = Normal;
        Contact->Depth  = Depth;
        Contact->RestitutionBias     = 0.0f;
        Contact->AccumulatedNormal   = 0.0f;
        Contact->AccumulatedFriction = Vector3(0.0f, 0.0f, 0.0f);
    }

    return Count;
}

internal bool32 BoundsOverlap(physics_body *A, physics_body *B)
{
    for (int Axis = 0; Axis < 3; ++Axis)
    {
        if (A->BoundsMax.Elements[Axis] < B->BoundsMin.Elements[Axis] ||
            B->BoundsMax.Elements[Axis] < A->BoundsMin.Elements[Axis])
        {
            return false;
        }
    }

    return true;
}

internal uint32 CollectPairContacts(physics_body *A, physics_body *B, contact *Contacts, uint32 MaxContacts)
{
    if (A->InvMass == 0.0f && B->InvMass == 0.0f)
    {
        return 0;
    }

    if (!BoundsOverlap(A, B))
    {
        return 0;
    }

    uint32 Count = 0;
    Count += CollectHullContacts(A, B, Contacts + Count, Minimum(MaxContacts - Count, PHYSICS_MAX_PAIR_CONTACTS));
    Count += CollectHullContacts(B, A, Contacts + Count, Minimum(MaxContacts - Count, PHYSICS_MAX_PAIR_CONTACTS));

    return Count;
}

internal void ApplyContactImpulse(contact *Contact, real32 InvDt)
{
    physics_body *A = Contact->A;
    physics_body *B = Contact->B;

    Vector3 rA = Contact->Point - A->Position;
    Vector3 rB = Contact->Point - B->Position;

    Vector3 RelVel = ContactRelativeVelocity(Contact);

    real32 NormalDenom = A->InvMass + B->InvMass
                       + A->InvInertia * LengthSq(Cross(rA, Contact->Normal))
                       + B->InvInertia * LengthSq(Cross(rB, Contact->Normal));
    if (NormalDenom <= 0.0f)
    {
        return;
    }

    real32 Bias = PHYSICS_BIAS_FACTOR * InvDt * Maximum(Contact->Depth - PHYSICS_PENETRATION_SLOP, 0.0f);
    Bias = Minimum(Bias, PHYSICS_MAX_BIAS_SPEED);
    Bias = Maximum(Bias, Contact->RestitutionBias);

    real32 NormalDelta = (Bias - Dot(RelVel, Contact->Normal)) / NormalDenom;
    real32 OldNormal   = Contact->AccumulatedNormal;
    Contact->AccumulatedNormal = Maximum(OldNormal + NormalDelta, 0.0f);
    NormalDelta = Contact->AccumulatedNormal - OldNormal;

    Vector3 Impulse = Contact->Normal * NormalDelta;
    A->Velocity        += Impulse * A->InvMass;
    A->AngularVelocity += A->InvInertia * Cross(rA, Impulse);
    B->Velocity        -= Impulse * B->InvMass;
    B->AngularVelocity -= B->InvInertia * Cross(rB, Impulse);

    RelVel = ContactRelativeVelocity(Contact);

    Vector3 Tangent    = RelVel - Contact->Normal * Dot(RelVel, Contact->Normal);
    real32  TangentLen = Length(Tangent);
    if (TangentLen < Epsilon32)
    {
        return;
    }
    Tangent = (1.0f / TangentLen) * Tangent;

    real32 TangentDenom = A->InvMass + B->InvMass
                        + A->InvInertia * LengthSq(Cross(rA, Tangent))
                        + B->InvInertia * LengthSq(Cross(rB, Tangent));
    if (TangentDenom <= 0.0f)
    {
        return;
    }

    real32 TangentDelta = -Dot(RelVel, Tangent) / TangentDenom;

    Vector3 OldFriction = Contact->AccumulatedFriction;
    Vector3 NewFriction = OldFriction + Tangent * TangentDelta;
    real32  MaxFriction = PHYSICS_FRICTION * Contact->AccumulatedNormal;
    real32  FrictionLen = Length(NewFriction);
    if (FrictionLen > MaxFriction)
    {
        NewFriction = (MaxFriction > 0.0f) ? NewFriction * (MaxFriction / FrictionLen) : Vector3(0.0f, 0.0f, 0.0f);
    }
    Contact->AccumulatedFriction = NewFriction;

    Impulse = NewFriction - OldFriction;
    A->Velocity        += Impulse * A->InvMass;
    A->AngularVelocity += A->InvInertia * Cross(rA, Impulse);
    B->Velocity        -= Impulse * B->InvMass;
    B->AngularVelocity -= B->InvInertia * Cross(rB, Impulse);
}

internal void PhysicsSubStep(physics_body *Bodies, uint32 BodyCount, real32 dt)
{
    for (uint32 Index = 0; Index < BodyCount; ++Index)
    {
        physics_body *Body = Bodies + Index;

        if (Body->InvMass > 0.0f)
        {
            Body->Velocity.Y += PHYSICS_GRAVITY_Y * dt;
            Body->Position   += Body->Velocity * dt;
            Body->AngularVelocity = Body->AngularVelocity * (1.0f / (1.0f + PHYSICS_ANGULAR_DAMPING * dt));
            IntegrateOrientation(Body, dt);
        }

        Body->Collider.LocalToWorld = BodyLocalToWorld(Body);
        ComputeWorldAABB(Body->Collider.LocalToWorld, Body->Collider.Mesh.BoundsMin, Body->Collider.Mesh.BoundsMax, &Body->BoundsMin, &Body->BoundsMax);
    }

    local_persist contact Contacts[PHYSICS_MAX_CONTACTS];
    uint32 ContactCount = 0;

    for (uint32 IndexA = 0; IndexA < BodyCount && ContactCount < PHYSICS_MAX_CONTACTS; ++IndexA)
    {
        for (uint32 IndexB = IndexA + 1; IndexB < BodyCount && ContactCount < PHYSICS_MAX_CONTACTS; ++IndexB)
        {
            ContactCount += CollectPairContacts(Bodies + IndexA, Bodies + IndexB, Contacts + ContactCount, PHYSICS_MAX_CONTACTS - ContactCount);
        }
    }

    for (uint32 Index = 0; Index < ContactCount; ++Index)
    {
        contact *Contact = Contacts + Index;

        real32 ApproachSpeed = -Dot(ContactRelativeVelocity(Contact), Contact->Normal);
        Contact->RestitutionBias = (ApproachSpeed > PHYSICS_RESTITUTION_MIN_SPEED) ? PHYSICS_RESTITUTION * ApproachSpeed : 0.0f;
    }

    real32 InvDt = 1.0f / dt;
    for (uint32 Iteration = 0; Iteration < PHYSICS_SOLVER_ITERATIONS; ++Iteration)
    {
        for (uint32 Index = 0; Index < ContactCount; ++Index)
        {
            ApplyContactImpulse(Contacts + Index, InvDt);
        }
    }
}

internal void PhysicsStepBodies(physics_body *Bodies, uint32 BodyCount, real32 dt)
{
    if (dt <= 0.0f)
    {
        return;
    }

    real32 SubDt = dt / (real32)PHYSICS_SUBSTEPS;
    for (uint32 Substep = 0; Substep < PHYSICS_SUBSTEPS; ++Substep)
    {
        PhysicsSubStep(Bodies, BodyCount, SubDt);
    }
}

internal void PhysicsInit(physics_world *World, memory_arena *Arena, uint32 BodyCapacity, uint32 MeshHullCapacity)
{
    World->Bodies       = PushArray(Arena, BodyCapacity, physics_body);
    World->BodyCount    = 0;
    World->BodyCapacity = BodyCapacity;

    World->MeshHulls        = PushArray(Arena, MeshHullCapacity, hull);
    World->MeshHullCapacity = MeshHullCapacity;

    World->Accumulator = 0.0f;
}

internal void PhysicsSetMeshHull(physics_world *World, memory_arena *Arena, uint32 MeshHandle, collision Mesh)
{
    Assert(MeshHandle < World->MeshHullCapacity);

    World->MeshHulls[MeshHandle] = BuildMeshHull(Arena, Mesh);
}

internal physics_body *PhysicsAddBody(physics_world *World)
{
    if (World->BodyCount >= World->BodyCapacity)
    {
        return 0;
    }

    return World->Bodies + World->BodyCount++;
}

internal void PhysicsAccumulate(physics_world *World, real32 dt)
{
    World->Accumulator = Minimum(World->Accumulator + dt, PHYSICS_ACCUMULATOR_CAP);
}

internal bool32 PhysicsNextTick(physics_world *World)
{
    if (World->Accumulator < PHYSICS_TICK)
    {
        return false;
    }

    World->Accumulator -= PHYSICS_TICK;

    return true;
}

internal real32 PhysicsRenderAlpha(physics_world *World)
{
    return World->Accumulator / PHYSICS_TICK;
}

internal void PhysicsStep(physics_world *World)
{
    PhysicsStepBodies(World->Bodies, World->BodyCount, PHYSICS_TICK);
}
