#ifndef ENTITY_H
#define ENTITY_H

#include "Types.h"
#include "EngineMath.h"
#include "Memory.h"

struct entities
{
    uint32 Count;
    uint32 MaxCount;

    Vector3 *Position;
    Vector3 *Velocity;
    real32  *Rotation;
    uint32  *MeshID;
    uint32  *TextureID;
};

#endif
