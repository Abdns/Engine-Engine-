#include "Game.h"
#include "Camera.cpp"
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

internal texture_format TextureFormatFromAsset(uint32 AssetFormat)
{
    return (AssetFormat == (uint32)ImageFormat_RGBA16F) ? TextureFormat_RGBA16F : TextureFormat_RGBA8;
}

internal void PushLakeToRender(data_lake *Lake, render_commands *Commands)
{
    for (uint32 Slot = 0; Slot < Lake->MeshCount; ++Slot)
    {
        PushLoadMesh(Commands, Slot + 1, LakeMeshVertices(Lake, Slot), Lake->MeshVertexCount[Slot], LakeMeshIndices(Lake, Slot), Lake->MeshIndexCount[Slot]);
    }

    for (uint32 Slot = 0; Slot < Lake->TextureCount; ++Slot)
    {
        PushLoadTexture(Commands, Slot + 1, LakeTexturePixels(Lake, Slot), Lake->TextureWidth[Slot], Lake->TextureHeight[Slot], Lake->TextureSRGB[Slot], TextureFormatFromAsset(Lake->TextureFormat[Slot]));
    }

    for (uint32 Slot = 0; Slot < Lake->CubemapCount; ++Slot)
    {
        PushLoadCubemap(Commands, Slot + 1, LakeCubemapPixels(Lake, Slot), Lake->CubemapFaceSize[Slot], TextureFormatFromAsset(Lake->CubemapFormat[Slot]));
    }
}

internal void LoadAssetPack(char* name, game_memory *Memory, game_state* GameState)
{
    LakeInit(&GameState->Lake, &GameState->WorldArena);

    platform_file_raw PackFile = Memory->PlatformReadEntireFile(name);
    LakeLoadPack(&GameState->Lake, PackFile.Data, PackFile.Size);
    Memory->PlatformFreeFileMemory(PackFile.Data);
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

        GameState->ColliderCapacity = Entities->MaxCount;
        GameState->Colliders = PushArray(WorldArena, GameState->ColliderCapacity, collider);

        LoadAssetPack("assets.enga", Memory, GameState);
        data_lake* Lake = &GameState->Lake;
        PushLakeToRender(Lake, RenderCommands);

        uint32    MeshCubeHandle = LakeGetMeshHandle(Lake, "cube");
        uint32    MeshSphereHandle = LakeGetMeshHandle(Lake, "sphere");
        uint32    TexTestHandle = LakeGetTextureHandle(Lake, "test");

        GameState->SkyHandle = LakeGetCubemapHandle(Lake, "sky");

        Vector3 TumbleSpin = Vector3(0.7f, 1.0f, 0.0f);

        uint32 CubeA = AddEntity(Entities, Vector3(-1.5f, 0.0f, 0.0f), MeshCubeHandle, TexTestHandle);
        uint32 Sphere = AddEntity(Entities, Vector3(0.0f, 0.0f, -1.5f), MeshSphereHandle, TexTestHandle);
        uint32 CubeB = AddEntity(Entities, Vector3(1.5f, 0.0f, 0.0f), MeshCubeHandle, TexTestHandle);

        SetEntityAngularVelocity(Entities, CubeA, TumbleSpin);
        SetEntityAngularVelocity(Entities, Sphere, TumbleSpin);
        SetEntityAngularVelocity(Entities, CubeB, TumbleSpin);

        SetEntityTint(Entities, Sphere, Vector4(1.0f, 0.5f, 0.5f, 1.0f));
        SetEntityTint(Entities, CubeB, Vector4(0.5f, 1.0f, 0.5f, 1.0f));

        InitCamera(&GameState->Camera, Vector3(0.0f, 0.0f, 4.0f), DegToRad(75.0f));

        GameState->SelectedEntityID = 0;

        Memory->IsInitialized = true;
    }

    UpdateEntities(&GameState->Entities, Input->dtForFrame);

    camera* Camera = &GameState->Camera;
    UpdateCamera(Camera, Input);

    bool32 ClickedThisFrame = (Input->MouseButtons[0].EndedDown && Input->MouseButtons[0].HalfTransitionCount);
    if (ClickedThisFrame && Input->RenderWidth > 0 && Input->RenderHeight > 0)
    {
        ray PickRay = CameraRayFromScreen(Camera, (real32)Input->MouseX, (real32)Input->MouseY, (real32)Input->RenderWidth, (real32)Input->RenderHeight);
        uint32 ColliderCount = BuildEntityColliders(&GameState->Entities, &GameState->Lake, GameState->Colliders, GameState->ColliderCapacity);
        GameState->SelectedEntityID = RayCastColliders(PickRay, GameState->Colliders, ColliderCount, 0);
    }

    PushRenderCamera(RenderCommands, CameraView(Camera), Camera->FovY);

    PushRenderPipeline(RenderCommands, Pipeline_Skybox);
    PushRenderSkybox(RenderCommands, GameState->SkyHandle);

    PushRenderPipeline(RenderCommands, Pipeline_Unlit);
    PushEntitiesToRender(&GameState->Entities, RenderCommands, GameState->SelectedEntityID);
}

extern "C" __declspec(dllexport)
GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer);
}
