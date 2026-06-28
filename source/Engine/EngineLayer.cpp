#include "EngineLayer.h"
#include <vulkan/vulkan.h>

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

// Day 027: рисование заполненного прямоугольника. Принимает float-координаты
// и цвет в нормализованном виде (0.0..1.0 на канал).
internal void DrawRectangle(game_offscreen_buffer* Buffer,
                            real32 MinX, real32 MinY,
                            real32 MaxX, real32 MaxY,
                            real32 R, real32 G, real32 B)
{
    int32 MinXi = RoundReal32ToInt32(MinX);
    int32 MinYi = RoundReal32ToInt32(MinY);
    int32 MaxXi = RoundReal32ToInt32(MaxX);
    int32 MaxYi = RoundReal32ToInt32(MaxY);

    if (MinXi < 0)              MinXi = 0;
    if (MinYi < 0)              MinYi = 0;
    if (MaxXi > Buffer->Width)  MaxXi = Buffer->Width;
    if (MaxYi > Buffer->Height) MaxYi = Buffer->Height;

    uint32 Color = ((RoundReal32ToUInt32(R * 255.0f) << 16) |
                    (RoundReal32ToUInt32(G * 255.0f) << 8) |
                     RoundReal32ToUInt32(B * 255.0f));

    uint8* Row = (uint8*)Buffer->Memory + MinXi * Buffer->BytesPerPixel + MinYi * Buffer->Pitch;
    for (int Y = MinYi; Y < MaxYi; ++Y)
    {
        uint32* Pixel = (uint32*)Row;
        for (int X = MinXi; X < MaxXi; ++X)
        {
            *Pixel++ = Color;
        }
        Row += Buffer->Pitch;
    }
}

extern "C" __declspec(dllexport)
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state* GameState = (game_state*)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        GameState->ToneHz  = 256;
        GameState->tSine   = 0.0f;
        GameState->PlayerX = 150.0f;
        GameState->PlayerY = 150.0f;
        Memory->IsInitialized = true;
    }

    // Day 027: простой тайлмап 9 строк × 17 столбцов.
    // 1 = стена (тёмный), 0 = пол (светлый).
    uint32 TileMap[9][17] =
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

    real32 UpperLeftX = -30.0f;
    real32 UpperLeftY = 0.0f;
    real32 TileWidth  = 60.0f;
    real32 TileHeight = 60.0f;

    // Обработка ввода: stick / WASD/стрелки сдвигают игрока.
    for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex)
    {
        game_controller_input* Controller = &Input->Controllers[ControllerIndex];

        real32 dPlayerX = 0.0f;
        real32 dPlayerY = 0.0f;

        if (Controller->IsAnalog)
        {
            dPlayerX = Controller->StickAverageX;
            dPlayerY = -Controller->StickAverageY;
        }
        else
        {
            if (Controller->Up.EndedDown)    dPlayerY = -1.0f;
            if (Controller->Down.EndedDown)  dPlayerY =  1.0f;
            if (Controller->Left.EndedDown)  dPlayerX = -1.0f;
            if (Controller->Right.EndedDown) dPlayerX =  1.0f;
        }

        real32 PlayerSpeed = 128.0f; // пикселей в секунду
        GameState->PlayerX += dPlayerX * PlayerSpeed * Input->dtForFrame;
        GameState->PlayerY += dPlayerY * PlayerSpeed * Input->dtForFrame;
    }

    // Фон: тёмно-серый.
    DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height,
                  0.2f, 0.2f, 0.2f);

    // Тайлы.
    for (int Row = 0; Row < 9; ++Row)
    {
        for (int Column = 0; Column < 17; ++Column)
        {
            uint32 TileID = TileMap[Row][Column];
            real32 Gray = (TileID == 1) ? 0.35f : 0.7f;

            real32 MinX = UpperLeftX + (real32)Column * TileWidth;
            real32 MinY = UpperLeftY + (real32)Row    * TileHeight;
            real32 MaxX = MinX + TileWidth;
            real32 MaxY = MinY + TileHeight;

            DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
        }
    }

    // Игрок — жёлтый прямоугольник.
    real32 PlayerWidth  = 0.75f * TileWidth;
    real32 PlayerHeight = TileHeight;
    real32 PlayerLeft   = GameState->PlayerX - 0.5f * PlayerWidth;
    real32 PlayerTop    = GameState->PlayerY - PlayerHeight;

    DrawRectangle(Buffer,
                  PlayerLeft, PlayerTop,
                  PlayerLeft + PlayerWidth, PlayerTop + PlayerHeight,
                  1.0f, 1.0f, 0.0f);
}

extern "C" __declspec(dllexport)
GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, GameState->ToneHz);
}
