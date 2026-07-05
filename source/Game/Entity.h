#ifndef ENTITY_H
#define ENTITY_H

#include "Types.h"
#include "HandmadeMath.h"   // v3

enum entity_type
{
    EntityType_Null = 0,
    EntityType_Hero,
    EntityType_Wall,
};

// SoA-хранилище сущностей: ОДНО поле — ОДИН непрерывный массив (data-oriented).
// Ссылка на сущность — целочисленный индекс (хэндл), а не сырой указатель:
// это позволяет позже сменить layout (hot/cold split и т.д.) не трогая геймплей.
// Индекс 0 зарезервирован под "нет сущности" (null).
struct entities
{
    uint32 Count;
    uint32 MaxCount;

    entity_type *Type;
    v3          *P;         // мировая позиция (3D-ready; рендерим пока по .XY)
    v3          *dP;        // скорость
    v3          *Dim;       // размеры бокса
};

#endif
