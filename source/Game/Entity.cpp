// Реализация системы сущностей (data-oriented, SoA). Чистая СИМУЛЯЦИЯ — здесь нет
// ничего про рендер (render_commands, камера). Инклудится РОВНО ОДИН РАЗ из Game.cpp
// (unity build), поэтому своего include-guard намеренно нет: он бы дал переопределение
// static-функций.

// Добавить сущность в SoA-хранилище: занимаем следующий слот, зануляем его поля,
// ставим тип, возвращаем ИНДЕКС (хэндл). PushArray память НЕ занулял — поэтому
// каждый слот инициализируем явно здесь.
internal uint32 AddEntity(entities* Entities, entity_type Type)
{
    Assert(Entities->Count < Entities->MaxCount);
    uint32 Index = Entities->Count++;

    Entities->Type[Index] = Type;
    Entities->P[Index]    = V3(0.0f, 0.0f, 0.0f);
    Entities->dP[Index]   = V3(0.0f, 0.0f, 0.0f);
    Entities->Dim[Index]  = V3(0.0f, 0.0f, 0.0f);

    return Index;
}

// Интегрирование движения по индексу. Пока без коллизий — добавим отдельным шагом,
// чтобы не отлаживать сразу две сложные вещи.
internal void MoveEntity(entities* Entities, uint32 Index, real32 dt)
{
    Entities->P[Index] += dt * Entities->dP[Index];
}

// Обновление всех сущностей за кадр: один цикл, switch по типу. Идём с 1 —
// индекс 0 это null-слот.
internal void UpdateEntities(entities* Entities, real32 dt)
{
    for (uint32 EntityIndex = 1; EntityIndex < Entities->Count; ++EntityIndex)
    {
        switch (Entities->Type[EntityIndex])
        {
            case EntityType_Hero:
            {
                MoveEntity(Entities, EntityIndex, dt);
            } break;

            default: break;
        }
    }
}
