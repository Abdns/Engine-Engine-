#ifndef ENTITY_H
#define ENTITY_H

#include "Types.h"
#include "EngineMath.h"

struct entities
{
    uint32 Count;
    uint32 MaxCount;
    uint32 NextID;

    uint32  *ID;
    Vector3 *Position;
    Vector3 *Velocity;
    Vector3 *Rotation;
    Vector3 *AngularVelocity;
    Vector4 *Tint;
    uint32  *MeshID;
    uint32  *TextureID;
};

#endif
