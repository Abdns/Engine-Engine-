#include "Game.h"
#include "Entity.cpp"   // реализация сущностей (симуляция); unity: включается ровно раз

// Значение тайла по (Column, Row). Один аксессор вместо ручного Row*TileCountX+Column
// в нескольких местах — плюс бесплатная проверка границ.
inline uint32 GetTileValue(world* World, uint32 Column, uint32 Row)
{
    Assert((Column < World->TileCountX) && (Row < World->TileCountY));
    return World->Tiles[Row * World->TileCountX + Column];
}

internal void GameOutputSound(game_state* GameState, game_sound_output_buffer* SoundBuffer, int ToneHz)
{
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

    int16* SampleOut = SoundBuffer->Samples;

    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        real32 SineValue = Sin(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);

        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        GameState->tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
    }
}

// world -> screen. Пока камера тривиальна: 1 мировая единица == 1 пиксель (Scale = 1),
// экранного центрирования нет. Позже сюда придут масштаб и слежение за игроком.
inline v2 WorldToScreen(game_state* GameState, v2 WorldP)
{
    v2 Result = WorldP - GameState->CameraP;
    return Result;
}

// Прямоугольник, заданный в МИРОВЫХ координатах: обе точки прогоняем через камеру,
// чтобы тайлы и игрок жили в одном пространстве и не разъезжались.
internal void PushWorldRect(render_commands* RenderCommands, game_state* GameState,
                            v2 WorldMin, v2 WorldMax, real32 R, real32 G, real32 B)
{
    v2 ScreenMin = WorldToScreen(GameState, WorldMin);
    v2 ScreenMax = WorldToScreen(GameState, WorldMax);
    PushRenderRect(RenderCommands, ScreenMin.X, ScreenMin.Y, ScreenMax.X, ScreenMax.Y, R, G, B);
}

extern "C" __declspec(dllexport)
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    // game_state ЛЕЖИТ в начале PermanentStorage (просто кладём его туда кастом —
    // никакого выделения, память уже есть из VirtualAlloc).
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        GameState->ToneHz  = 256;
        GameState->tSine   = 0.0f;
        GameState->CameraP = V2(0.0f, 0.0f);

        // Арену кладём СРАЗУ ЗА game_state и отдаём ей весь остаток перманентного
        // блока. Дальше всё постоянное (мир, сущности, ассеты) пушим отсюда —
        // PushStruct/PushArray, без единого malloc. Это и есть WorldArena из HH.
        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), (uint8*)Memory->PermanentStorage + sizeof(game_state));

        // Day 027: тайлмап 9×17 переезжает из тела кадра в персистентный world
        // в WorldArena. 1 = стена (тёмный), 0 = пол (светлый). Заполняем ОДИН раз.
        uint32 InitialTiles[9][17] =
        {
            { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
            { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
            { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
            { 1, 0, 0, 0,  0, 0, 0, 0,  1,  0, 0, 0, 0,  0, 0, 0, 1 },
            { 0, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 0 },
            { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
            { 1, 0, 0, 0,  0, 0, 0, 0,  1,  0, 0, 0, 0,  0, 0, 0, 1 },
            { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
            { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
        };

        world* World = PushStruct(&GameState->WorldArena, world);
        World->TileCountX = 17;
        World->TileCountY = 9;
        World->TileSideInPixels = 60.0f;
        World->Origin = V2(-0.5f * World->TileSideInPixels, 0.0f);   // полтайла влево (было -30)
        World->Tiles = PushArray(&GameState->WorldArena, World->TileCountX * World->TileCountY, uint32);
        for (uint32 Row = 0; Row < World->TileCountY; ++Row)
        {
            for (uint32 Column = 0; Column < World->TileCountX; ++Column)
            {
                World->Tiles[Row * World->TileCountX + Column] = InitialTiles[Row][Column];
            }
        }
        GameState->World = World;

        // ---- SoA-хранилище сущностей: массивы из WorldArena ----
        real32 TileSide = World->TileSideInPixels;

        entities* Entities = &GameState->Entities;
        Entities->Count    = 0;
        Entities->MaxCount = 4096;
        Entities->Type = PushArray(&GameState->WorldArena, Entities->MaxCount, entity_type);
        Entities->P    = PushArray(&GameState->WorldArena, Entities->MaxCount, v3);
        Entities->dP   = PushArray(&GameState->WorldArena, Entities->MaxCount, v3);
        Entities->Dim  = PushArray(&GameState->WorldArena, Entities->MaxCount, v3);

        // Слот 0 — null-сущность, чтобы индекс 0 значил "никого".
        AddEntity(Entities, EntityType_Null);

        // Стены из тайлмапа становятся сущностями (Casey Day-56 "tiles -> entities").
        for (uint32 Row = 0; Row < World->TileCountY; ++Row)
        {
            for (uint32 Column = 0; Column < World->TileCountX; ++Column)
            {
                if (GetTileValue(World, Column, Row) == 1)
                {
                    uint32 WallIndex = AddEntity(Entities, EntityType_Wall);
                    v2 TileMin = World->Origin + V2((real32)Column * TileSide, (real32)Row * TileSide);
                    Entities->P[WallIndex]   = V3(TileMin.X + 0.5f * TileSide, TileMin.Y + 0.5f * TileSide, 0.0f);
                    Entities->Dim[WallIndex] = V3(TileSide, TileSide, TileSide);
                }
            }
        }

        // Игрок — сущность-герой; запоминаем её индекс (хэндл).
        uint32 HeroIndex = AddEntity(Entities, EntityType_Hero);
        Entities->P[HeroIndex]   = V3(150.0f, 150.0f, 0.0f);
        Entities->Dim[HeroIndex] = V3(0.75f * TileSide, TileSide, TileSide);
        GameState->PlayerEntityIndex = HeroIndex;

        Memory->IsInitialized = true;
    }

    entities* Entities = &GameState->Entities;
    real32 dt = Input->dtForFrame;

    // Ввод пишет СКОРОСТЬ герою (а не двигает позицию напрямую) — тогда одно
    // интегрирование в MoveEntity обслуживает любой тип сущности.
    v3 HeroDir = V3(0.0f, 0.0f, 0.0f);
    for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex)
    {
        game_controller_input* Controller = &Input->Controllers[ControllerIndex];

        if (Controller->IsAnalog)
        {
            HeroDir.X = Controller->StickAverageX;
            HeroDir.Y = -Controller->StickAverageY;
        }
        else
        {
            if (Controller->Up.EndedDown)    HeroDir.Y = -1.0f;
            if (Controller->Down.EndedDown)  HeroDir.Y =  1.0f;
            if (Controller->Left.EndedDown)  HeroDir.X = -1.0f;
            if (Controller->Right.EndedDown) HeroDir.X =  1.0f;
        }
    }

    real32 PlayerSpeed = 128.0f; // пикселей в секунду
    Entities->dP[GameState->PlayerEntityIndex] = PlayerSpeed * HeroDir;

    // ---- Обновление сущностей (симуляция; см. Entity.cpp) ----
    UpdateEntities(Entities, dt);

    // Игра ОПИСЫВАЕТ кадр командами; бэкенд (Vulkan/Software) их исполнит.
    // Никаких прямых вызовов рисования — только пуш в буфер команд.
    PushRenderClear(RenderCommands, 0.2f, 0.2f, 0.2f);   // фон

    world* World = GameState->World;
    real32 TileSide = World->TileSideInPixels;

    // Пол (клетки-не-стены) — фон под сущностями. Стены теперь СУЩНОСТИ, рисуются ниже.
    for (uint32 Row = 0; Row < World->TileCountY; ++Row)
    {
        for (uint32 Column = 0; Column < World->TileCountX; ++Column)
        {
            if (GetTileValue(World, Column, Row) == 0)
            {
                v2 Min = World->Origin + V2((real32)Column * TileSide, (real32)Row * TileSide);
                v2 Max = Min + V2(TileSide, TileSide);
                PushWorldRect(RenderCommands, GameState, Min, Max, 0.7f, 0.7f, 0.7f);
            }
        }
    }

    // Сущности (SoA). Пока рисуем 2D-плейсхолдерами по .XY (3D-рендер придёт позже).
    // Идём от 1 (0 — null). Герой добавлен последним => рисуется поверх стен.
    for (uint32 EntityIndex = 1; EntityIndex < Entities->Count; ++EntityIndex)
    {
        v2 P       = Entities->P[EntityIndex].XY;
        v2 HalfDim = 0.5f * Entities->Dim[EntityIndex].XY;
        v2 Min     = P - HalfDim;
        v2 Max     = P + HalfDim;

        real32 R = 1.0f, G = 1.0f, B = 1.0f;
        switch (Entities->Type[EntityIndex])
        {
            case EntityType_Wall: R = 0.35f; G = 0.35f; B = 0.35f; break;
            case EntityType_Hero: R = 1.0f;  G = 1.0f;  B = 0.0f;  break;
            default: break;
        }
        PushWorldRect(RenderCommands, GameState, Min, Max, R, G, B);
    }
}

extern "C" __declspec(dllexport)
GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, GameState->ToneHz);
}
