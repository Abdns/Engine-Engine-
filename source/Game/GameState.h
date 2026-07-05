#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "Types.h"
#include "Memory.h"
#include "HandmadeMath.h"   // v2
#include "Entity.h"         // entity_type, entities (game_state держит entities по значению)

// Персистентный мир: тайлмап живёт в WorldArena, а не пересоздаётся каждый кадр.
struct world
{
    uint32 TileCountX;
    uint32 TileCountY;
    real32 TileSideInPixels;
    v2     Origin;          // мировая позиция верхнего-левого тайла (пикс.)
    uint32 *Tiles;          // TileCountY*TileCountX, индекс [Row*TileCountX + Column]
};

struct game_state
{
    int    ToneHz;
    real32 tSine;

    v2 CameraP;             // камера в мировых координатах (пикс.); пока 0 — без скролла

    memory_arena WorldArena;
    world *World;

    entities Entities;
    uint32   PlayerEntityIndex;
};

#endif
