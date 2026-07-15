#include "Game.h"
#include "Entity.cpp"

internal void GameOutputSound(game_state* GameState, game_sound_output_buffer* SoundBuffer)
{
    int ToneHz = 256;
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

internal Matrix4 UpdateCamera(game_state* GameState, game_input* Input)
{
    real32 MouseX = (real32)Input->MouseX;
    real32 MouseY = (real32)Input->MouseY;
    real32 dMouseX = MouseX - GameState->LastMouseX;
    real32 dMouseY = MouseY - GameState->LastMouseY;
    GameState->LastMouseX = MouseX;
    GameState->LastMouseY = MouseY;

    if (Input->MouseButtons[2].EndedDown)
    {
        real32 Sensitivity = 0.003f;
        GameState->CameraYaw   -= dMouseX * Sensitivity;
        GameState->CameraPitch -= dMouseY * Sensitivity;
        GameState->CameraPitch = Clamp(-1.55f, GameState->CameraPitch, 1.55f);
    }

    real32 Yaw   = GameState->CameraYaw;
    real32 Pitch = GameState->CameraPitch;
    real32 cy = Cos(Yaw);
    real32 sy = Sin(Yaw);
    real32 cp = Cos(Pitch);
    real32 sp = Sin(Pitch);

    Vector3 Forward = V3(-sy * cp, sp, -cy * cp);
    Vector3 Right   = V3(cy, 0.0f, -sy);
    Vector3 WorldUp = V3(0.0f, 1.0f, 0.0f);

    game_controller_input* Keyboard = &Input->Controllers[0];
    Vector3 Move = V3(0.0f, 0.0f, 0.0f);
    if (Keyboard->Up.EndedDown)            Move += Forward;
    if (Keyboard->Down.EndedDown)          Move -= Forward;
    if (Keyboard->Right.EndedDown)         Move += Right;
    if (Keyboard->Left.EndedDown)          Move -= Right;
    if (Keyboard->RightShoulder.EndedDown) Move += WorldUp;
    if (Keyboard->LeftShoulder.EndedDown)  Move -= WorldUp;

    real32 Speed = 4.0f;
    GameState->CameraP += (Speed * Input->dtForFrame) * Move;

    Vector3 P = GameState->CameraP;
    Matrix4 View = Mat4Multiply(Mat4RotationX(-Pitch), Mat4Multiply(Mat4RotationY(-Yaw), Mat4Translation(-P.X, -P.Y, -P.Z)));

    return View;
}

extern "C" __declspec(dllexport)
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state* GameState = (game_state*)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        GameState->tSine = 0.0f;

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), (uint8*)Memory->PermanentStorage + sizeof(game_state));
        memory_arena* WorldArena = &GameState->WorldArena;

        entities* Entities = &GameState->Entities;

        InitEntities(Entities, WorldArena);

        platform_file PackFile = Memory->PlatformReadEntireFile("assets.kbn");
        LoadGameAssets(&GameState->Assets, WorldArena, PackFile.Data, PackFile.Size);
        Memory->PlatformFreeFileMemory(PackFile.Data);
        game_assets* Assets = &GameState->Assets;

        for (uint32 Index = 0; Index < Assets->MeshCount; ++Index)
        {
            PushRenderLoadMesh(RenderCommands, Index, Assets->MeshVertexCount[Index], MeshVertices(Assets, Index));
        }
        for (uint32 Index = 0; Index < Assets->TextureCount; ++Index)
        {
            PushRenderLoadTexture(RenderCommands, Index, Assets->TextureWidth[Index], Assets->TextureHeight[Index], Assets->TextureSRGB[Index], TexturePixels(Assets, Index));
        }

        uint32 MeshCube   = GetMeshID(Assets, "cube");
        uint32 MeshSphere = GetMeshID(Assets, "sphere");
        uint32 TexTest    = GetTextureID(Assets, "test");

        Vector3 TumbleSpin = V3(0.7f, 1.0f, 0.0f);

        uint32 CubeA  = AddEntity(Entities, V3(-1.5f, 0.0f,  0.0f), MeshCube,   TexTest);
        uint32 Sphere = AddEntity(Entities, V3( 0.0f, 0.0f, -1.5f), MeshSphere, TexTest);
        uint32 CubeB  = AddEntity(Entities, V3( 1.5f, 0.0f,  0.0f), MeshCube,   TexTest);

        SetEntityAngularVelocity(Entities, CubeA,  TumbleSpin);
        SetEntityAngularVelocity(Entities, Sphere, TumbleSpin);
        SetEntityAngularVelocity(Entities, CubeB,  TumbleSpin);

        SetEntityTint(Entities, Sphere, V4(1.0f, 0.5f, 0.5f, 1.0f));
        SetEntityTint(Entities, CubeB,  V4(0.5f, 1.0f, 0.5f, 1.0f));

        GameState->CameraP     = V3(0.0f, 0.0f, 4.0f);
        GameState->CameraYaw   = 0.0f;
        GameState->CameraPitch = 0.0f;
        GameState->LastMouseX  = (real32)Input->MouseX;
        GameState->LastMouseY  = (real32)Input->MouseY;

        Memory->IsInitialized = true;
    }

    UpdateEntities(&GameState->Entities, Input->dtForFrame);

    Matrix4 View = UpdateCamera(GameState, Input);
    real32 FovY = 1.047f;

    SetRenderClear(RenderCommands, 0.05f, 0.05f, 0.08f);
    SetRenderCamera(RenderCommands, View, FovY);
    PushEntitiesToRender(&GameState->Entities, RenderCommands);
}

extern "C" __declspec(dllexport)
GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer);
}
