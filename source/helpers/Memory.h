#ifndef MEMORY_H
#define MEMORY_H

#include "Types.h"

// ============================================================================
//  Аренный (линейный / bump) аллокатор в стиле Handmade Hero.
//  Группируй выделения по ВРЕМЕНИ ЖИЗНИ (перманент / уровень / кадр), а не по объектам.
// ============================================================================

#define DEFAULT_ALIGNMENT 4
typedef uint64 memory_index;

struct memory_arena
{
    uint8 *Base;
    memory_index Size;
    memory_index Used;
    int32 TempCount;     // сколько открытых temporary_memory (для самопроверки)
};

internal void InitializeArena(memory_arena *Arena, memory_index Size, void *Base)
{
    Arena->Size = Size;
    Arena->Base = (uint8 *)Base;
    Arena->Used = 0;
    Arena->TempCount = 0;
}

// Смещение до ближайшего адреса, кратного Alignment (Alignment — степень двойки).
internal memory_index GetAlignmentOffset(memory_arena *Arena, memory_index Alignment)
{
    memory_index AlignmentOffset = 0;
    memory_index ResultPointer = (memory_index)Arena->Base + Arena->Used;
    memory_index AlignmentMask = Alignment - 1;
    if (ResultPointer & AlignmentMask)
    {
        AlignmentOffset = Alignment - (ResultPointer & AlignmentMask);
    }
    return AlignmentOffset;
}

// Сердце аллокатора: выдать Size байт (с учётом выравнивания) и сдвинуть Used.
// Alignment по умолчанию = DEFAULT_ALIGNMENT; передай больше (16/64) под SIMD/кэш.
internal void *PushSize_(memory_arena *Arena, memory_index SizeInit, memory_index Alignment = DEFAULT_ALIGNMENT)
{
    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Alignment);
    memory_index Size = SizeInit + AlignmentOffset;

    Assert((Arena->Used + Size) <= Arena->Size);   // не вылезли за блок

    void *Result = Arena->Base + Arena->Used + AlignmentOffset;
    Arena->Used += Size;

    Assert(Size >= SizeInit);

    return Result;
}

#define PushStruct(Arena, type)                         (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type)                   (type *)PushSize_(Arena, (Count) * sizeof(type))
#define PushSize(Arena, Size)                           PushSize_(Arena, Size)
#define PushStructAligned(Arena, type, Alignment)       (type *)PushSize_(Arena, sizeof(type), Alignment)
#define PushArrayAligned(Arena, Count, type, Alignment) (type *)PushSize_(Arena, (Count) * sizeof(type), Alignment)

internal void ResetArena(memory_arena *Arena)
{
    Arena->Used = 0;
}

// Под-арена: вырезать кусок родительской арены как самостоятельную арену
// (так HH делит PermanentStorage на WorldArena и т.п.).
internal void SubArena(memory_arena *Result, memory_arena *Parent, memory_index Size, memory_index Alignment = 16)
{
    Result->Size = Size;
    Result->Base = (uint8 *)PushSize_(Parent, Size, Alignment);
    Result->Used = 0;
    Result->TempCount = 0;
}

// ============================================================================
//  Temporary memory: пометил позицию -> навыделял царапки -> откатил назад.
//  Идеально для пер-кадровой временной памяти и локальных буферов.
// ============================================================================

struct temporary_memory
{
    memory_arena *Arena;
    memory_index Used;
};

internal temporary_memory BeginTemporaryMemory(memory_arena *Arena)
{
    temporary_memory Result;
    Result.Arena = Arena;
    Result.Used = Arena->Used;   // запоминаем метку
    ++Arena->TempCount;
    return Result;
}

internal void EndTemporaryMemory(temporary_memory TempMem)
{
    memory_arena *Arena = TempMem.Arena;
    Assert(Arena->Used >= TempMem.Used);
    Arena->Used = TempMem.Used;   // откат до метки = «освободили» всю царапку
    Assert(Arena->TempCount > 0);
    --Arena->TempCount;
}

// Проверка, что все temporary_memory закрыты (звать, например, в конце кадра).
internal void CheckArena(memory_arena *Arena)
{
    Assert(Arena->TempCount == 0);
}

// ============================================================================
//  Обнуление (PushSize не зануляет память — делай это сам при необходимости).
// ============================================================================

internal void ZeroSize(memory_index Size, void *Ptr)
{
    uint8 *Byte = (uint8 *)Ptr;
    while (Size--)
    {
        *Byte++ = 0;
    }
}

#define ZeroStruct(Instance)      ZeroSize(sizeof(Instance), &(Instance))
#define ZeroArray(Count, Pointer) ZeroSize((Count) * sizeof((Pointer)[0]), Pointer)

#endif
