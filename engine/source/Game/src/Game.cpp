#include "Game.h"

enum ui_id
{
    UI_ID_PAUSE = 1,
    UI_ID_SPEED,
    UI_ID_SPAWN,
    UI_ID_CLEAR,
};

#define ENTITY_SPIN Vector3(0.7f, 1.0f, 0.0f)

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

    file_contents PackFile = Memory->PlatformReadEntireFile(name);
    LakeLoadPack(&GameState->Lake, PackFile.Data, PackFile.Size);
    Memory->PlatformFreeFileMemory(PackFile.Data);
}

internal collision_mesh LakeCollisionMesh(data_lake *Lake, uint32 Slot)
{
    collision_mesh Mesh = {};
    Mesh.Vertices     = LakeMeshVertices(Lake, Slot);
    Mesh.VertexStride = sizeof(enga_vertex);
    Mesh.Indices      = LakeMeshIndices(Lake, Slot);
    Mesh.IndexCount   = Lake->MeshIndexCount[Slot];
    Mesh.BoundsMin    = Lake->MeshBoundsMin[Slot];
    Mesh.BoundsMax    = Lake->MeshBoundsMax[Slot];

    return Mesh;
}

internal uint32 BuildEntityColliders(data_lake *Lake, collider *Colliders, uint32 MaxColliders)
{
    uint32 Count = 0;

    for (uint32 EntityIndex = 0; EntityIndex < Lake->EntityCount && Count < MaxColliders; ++EntityIndex)
    {
        entity Entity   = EntityFromIndex(Lake, EntityIndex);
        uint32 MeshSlot = EntityMeshSlot(Lake, Entity);
        if (MeshSlot == LAKE_INVALID_SLOT)
        {
            continue;
        }

        Colliders[Count++] = MakeCollider(Entity.ID, EntityTransform(Lake, Entity.Index), LakeCollisionMesh(Lake, MeshSlot));
    }

    return Count;
}

internal void PushEntitiesToRender(data_lake *Lake, render_commands *Commands, uint32 SelectedID)
{
    for (uint32 EntityIndex = 0; EntityIndex < Lake->EntityCount; ++EntityIndex)
    {
        entity Entity = EntityFromIndex(Lake, EntityIndex);

        Vector4 Tint = Lake->EntityTint[Entity.Index];
        if (SelectedID && Entity.ID == SelectedID)
        {
            Tint = Vector4(1.0f, 0.85f, 0.2f, Tint.W);
        }

        PushRenderMesh(Commands, EntityTransform(Lake, Entity.Index), Tint, Entity.MeshHandle, Entity.MaterialHandle);
    }
}

internal uint32 SpawnEntity(game_state *GameState)
{
    data_lake *Lake = &GameState->Lake;

    uint32 Index  = Lake->EntityCount;
    real32 Angle  = (real32)Index * 2.39996f;
    real32 Radius = 0.9f * SquareRoot((real32)Index + 1.0f);

    Vector3 Position = Vector3(Cos(Angle) * Radius, 0.0f, Sin(Angle) * Radius);

    uint32 MeshHandle     = GameState->SpawnMeshHandles[Index % ArrayCount(GameState->SpawnMeshHandles)];
    uint32 MaterialHandle = GameState->SpawnMaterialHandles[Index % ArrayCount(GameState->SpawnMaterialHandles)];

    uint32 EntityID = AddEntity(Lake, Position, MeshHandle, MaterialHandle);
    SetEntityAngularVelocity(Lake, EntityID, ENTITY_SPIN);

    return EntityID;
}

extern "C" __declspec(dllexport)
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state* GameState = (game_state*)Memory->PermanentStorage;
    data_lake* Lake = &GameState->Lake;

    if (!Memory->IsInitialized)
    {
        GameState->tSine = 0.0f;

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), (uint8*)Memory->PermanentStorage + sizeof(game_state));
        memory_arena* WorldArena = &GameState->WorldArena;

        LoadAssetPack("assets.enga", Memory, GameState);
        PushLakeToRender(Lake, RenderCommands);

        GameState->ColliderCapacity = Lake->EntityCapacity;
        GameState->Colliders = PushArray(WorldArena, GameState->ColliderCapacity, collider);

        uint32    TexTestHandle = LakeGetTextureHandle(Lake, "test");

        GameState->SkyHandle = LakeGetCubemapHandle(Lake, "sky");

        GameState->SpawnMeshHandles[0] = LakeGetMeshHandle(Lake, "cube");
        GameState->SpawnMeshHandles[1] = LakeGetMeshHandle(Lake, "sphere");

        materials* Materials = &GameState->Materials;
        GameState->SpawnMaterialHandles[0] = AddMaterial(Materials, UnlitMaterial(Vector4(1.0f, 1.0f, 1.0f, 1.0f), TexTestHandle));
        GameState->SpawnMaterialHandles[1] = AddMaterial(Materials, UnlitMaterial(Vector4(1.0f, 0.5f, 0.5f, 1.0f), TexTestHandle));
        GameState->SpawnMaterialHandles[2] = AddMaterial(Materials, UnlitMaterial(Vector4(0.5f, 1.0f, 0.5f, 1.0f), TexTestHandle));
        PushMaterialsToRender(Materials, RenderCommands);

        for (uint32 SpawnIndex = 0; SpawnIndex < 3; ++SpawnIndex)
        {
            SpawnEntity(GameState);
        }

        InitCamera(&GameState->Camera, Vector3(0.0f, 0.0f, 4.0f), DegToRad(75.0f));

        GameState->SelectedEntityID = 0;

        GameState->UI.Style   = DefaultUIStyle();
        GameState->SpinSpeed  = 1.0f;
        GameState->SpinPaused = false;

        Memory->IsInitialized = true;
    }

    ui_context *UI = &GameState->UI;
    BeginUI(UI, Input, RenderCommands);

    if (UIButton(UI, UI_ID_PAUSE, RectMinDim(20.0f, 20.0f, 140.0f, 36.0f)))
    {
        GameState->SpinPaused = !GameState->SpinPaused;
    }

    UISlider(UI, UI_ID_SPEED, RectMinDim(20.0f, 66.0f, 140.0f, 24.0f), &GameState->SpinSpeed);

    if (UIButton(UI, UI_ID_SPAWN, RectMinDim(20.0f, 100.0f, 140.0f, 36.0f)))
    {
        GameState->SelectedEntityID = SpawnEntity(GameState);
    }

    if (UIButton(UI, UI_ID_CLEAR, RectMinDim(20.0f, 146.0f, 140.0f, 36.0f)))
    {
        GameState->SelectedEntityID = 0;
    }

    real32 SpinScale = GameState->SpinPaused ? 0.0f : GameState->SpinSpeed;
    UpdateEntities(Lake, Input->dtForFrame * SpinScale);

    camera* Camera = &GameState->Camera;
    UpdateCamera(Camera, Input);

    bool32 PickedThisFrame = (UI->MousePressed && !UI->Active);
    if (PickedThisFrame && Input->RenderWidth > 0 && Input->RenderHeight > 0)
    {
        ray PickRay = CameraRayFromScreen(Camera, (real32)Input->MouseX, (real32)Input->MouseY, (real32)Input->RenderWidth, (real32)Input->RenderHeight);
        uint32 ColliderCount = BuildEntityColliders(Lake, GameState->Colliders, GameState->ColliderCapacity);
        GameState->SelectedEntityID = RayCastColliders(PickRay, GameState->Colliders, ColliderCount, 0);
    }

    PushRenderCamera(RenderCommands, CameraView(Camera), Camera->FovY);

    PushRenderPipeline(RenderCommands, Pipeline_Skybox);
    PushRenderSkybox(RenderCommands, GameState->SkyHandle);

    PushEntitiesToRender(Lake, RenderCommands, GameState->SelectedEntityID);

    EndUI(UI);
}

extern "C" __declspec(dllexport)
GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer);
}
