#include "Game.h"
#include "Entity.cpp"

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
        GameState->ToneHz = 256;
        GameState->tSine  = 0.0f;

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), (uint8*)Memory->PermanentStorage + sizeof(game_state));
        memory_arena* WorldArena = &GameState->WorldArena;

        entities* Entities = &GameState->Entities;

        InitEntities(Entities, WorldArena);

        AddEntity(Entities, V3(-1.5f, 0.0f,  0.0f), Mesh_Cube,    Texture_Photo);
        AddEntity(Entities, V3( 0.0f, 0.0f, -1.5f), Mesh_Pyramid, Texture_Checker);
        AddEntity(Entities, V3( 1.5f, 0.0f,  0.0f), Mesh_Cube,    Texture_Checker);

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

    PushRenderClear(RenderCommands, 0.05f, 0.05f, 0.08f);
    PushRenderCamera(RenderCommands, View, FovY);

    entities* Entities = &GameState->Entities;
    for (uint32 EntityIndex = 0; EntityIndex < Entities->Count; ++EntityIndex)
    {
        PushRenderMesh(RenderCommands, Entities->Position[EntityIndex], Entities->Rotation[EntityIndex], Entities->MeshID[EntityIndex], Entities->TextureID[EntityIndex]);
    }
}

extern "C" __declspec(dllexport)
GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, GameState->ToneHz);
}
