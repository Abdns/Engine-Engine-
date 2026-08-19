#include "Types.h"
#include "EngineMath.h"
#include "Geometry.h"

#define PHYSICS_GRAVITY_Y        -9.8f
#define PHYSICS_MAX_DT            0.05f
#define PHYSICS_SUBSTEPS          4
#define PHYSICS_SOLVER_ITERATIONS 4
#define PHYSICS_MAX_BIAS_SPEED    1.5f
#define PHYSICS_MAX_CONTACTS      256
#define PHYSICS_MAX_PAIR_CONTACTS 8
#define PHYSICS_BIAS_FACTOR       0.2f
#define PHYSICS_PENETRATION_SLOP  0.01f
#define PHYSICS_FRICTION          0.4f
#define PHYSICS_ANGULAR_DAMPING   0.5f

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
};

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

        contact *Contact = Contacts + Count++;
        Contact->A      = A;
        Contact->B      = B;
        Contact->Point  = Mat4Transform(WorldFromB, P, 1.0f);
        Contact->Normal = Normal;
        Contact->Depth  = Depth;
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

    Vector3 RelVel = (A->Velocity + Cross(A->AngularVelocity, rA))
                   - (B->Velocity + Cross(B->AngularVelocity, rB));

    real32 NormalDenom = A->InvMass + B->InvMass
                       + A->InvInertia * LengthSq(Cross(rA, Contact->Normal))
                       + B->InvInertia * LengthSq(Cross(rB, Contact->Normal));
    if (NormalDenom <= 0.0f)
    {
        return;
    }

    real32 Bias = PHYSICS_BIAS_FACTOR * InvDt * Maximum(Contact->Depth - PHYSICS_PENETRATION_SLOP, 0.0f);
    Bias = Minimum(Bias, PHYSICS_MAX_BIAS_SPEED);
    real32 NormalImpulse = (Bias - Dot(RelVel, Contact->Normal)) / NormalDenom;
    if (NormalImpulse <= 0.0f)
    {
        return;
    }

    Vector3 Impulse = Contact->Normal * NormalImpulse;
    A->Velocity        += Impulse * A->InvMass;
    A->AngularVelocity += A->InvInertia * Cross(rA, Impulse);
    B->Velocity        -= Impulse * B->InvMass;
    B->AngularVelocity -= B->InvInertia * Cross(rB, Impulse);

    RelVel = (A->Velocity + Cross(A->AngularVelocity, rA))
           - (B->Velocity + Cross(B->AngularVelocity, rB));

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

    real32 TangentImpulse = -Dot(RelVel, Tangent) / TangentDenom;
    real32 MaxFriction    = PHYSICS_FRICTION * NormalImpulse;
    if (TangentImpulse >  MaxFriction) TangentImpulse =  MaxFriction;
    if (TangentImpulse < -MaxFriction) TangentImpulse = -MaxFriction;

    Impulse = Tangent * TangentImpulse;
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

    contact Contacts[PHYSICS_MAX_CONTACTS];
    uint32  ContactCount = 0;

    for (uint32 IndexA = 0; IndexA < BodyCount && ContactCount < PHYSICS_MAX_CONTACTS; ++IndexA)
    {
        for (uint32 IndexB = IndexA + 1; IndexB < BodyCount && ContactCount < PHYSICS_MAX_CONTACTS; ++IndexB)
        {
            ContactCount += CollectPairContacts(Bodies + IndexA, Bodies + IndexB, Contacts + ContactCount, PHYSICS_MAX_CONTACTS - ContactCount);
        }
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

internal void PhysicsStep(physics_body *Bodies, uint32 BodyCount, real32 dt)
{
    dt = Minimum(dt, PHYSICS_MAX_DT);
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
