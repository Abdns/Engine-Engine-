#include "Game.h"

#include "Raycast.cpp"
#include "Physics.cpp"
#include "DataLake.cpp"
#include "Camera.cpp"
#include "Entity.cpp"
#include "Material.cpp"
#include "UI.cpp"
#include "Text.cpp"

#include "GameState.h"

internal void GameOutputSound(game_state* GameState, game_sound_output_buffer* SoundBuffer)
{
   /* int ToneHz = 256;
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
    }*/
}

internal texture_format TextureFormatFromAsset(uint32 AssetFormat)
{
    return (AssetFormat == (uint32)ImageFormat_RGBA16F) ? TextureFormat_RGBA16F : TextureFormat_RGBA8;
}

internal void PushLakeToRender(data_lake *Lake, render_commands *Commands)
{
    for (uint32 Slot = 0; Slot < Lake->MeshCount; ++Slot)
    {
        PushLoadMesh(Commands, Slot, LakeMeshVertices(Lake, Slot), Lake->MeshVertexCount[Slot], LakeMeshIndices(Lake, Slot), Lake->MeshIndexCount[Slot]);
    }

    for (uint32 Slot = 0; Slot < Lake->TextureCount; ++Slot)
    {
        PushLoadTexture(Commands, Slot, LakeTexturePixels(Lake, Slot), Lake->TextureWidth[Slot], Lake->TextureHeight[Slot], Lake->TextureSRGB[Slot], TextureFormatFromAsset(Lake->TextureFormat[Slot]));
    }

    for (uint32 Slot = 0; Slot < Lake->CubemapCount; ++Slot)
    {
        PushLoadCubemap(Commands, Slot, LakeCubemapPixels(Lake, Slot), Lake->CubemapFaceSize[Slot], TextureFormatFromAsset(Lake->CubemapFormat[Slot]));
    }
}

internal void LoadENGA(char* name, game_memory *Memory, game_state* GameState)
{
    file_data PackFile = Memory->PlatformReadEntireFile(name);
    LakeLoadENGA(&GameState->Lake, PackFile.Data, PackFile.Size);
    Memory->PlatformFreeFileMemory(PackFile.Data);
}

internal collision LakeCollisionMesh(data_lake *Lake, uint32 Slot)
{
    collision Mesh = {};
    Mesh.Vertices     = LakeMeshVertices(Lake, Slot);
    Mesh.VertexStride = sizeof(enga_vertex);
    Mesh.VertexCount  = Lake->MeshVertexCount[Slot];
    Mesh.Indices      = LakeMeshIndices(Lake, Slot);
    Mesh.IndexCount   = Lake->MeshIndexCount[Slot];
    Mesh.BoundsMin    = Lake->MeshBoundsMin[Slot];
    Mesh.BoundsMax    = Lake->MeshBoundsMax[Slot];

    return Mesh;
}

internal uint32 BuildEntityColliders(data_lake *Lake, real32 Alpha, collider *Colliders, uint32 MaxColliders)
{
    uint32 Count = Minimum(Lake->Transforms.Count, MaxColliders);

    for (uint32 Slot = 0; Slot < Count; ++Slot)
    {
        Colliders[Slot] = MakeCollider(Lake->Transforms.EntityID[Slot], EntityRenderTransform(Lake, Slot, Alpha), LakeCollisionMesh(Lake, Lake->Transforms.MeshHandle[Slot]));
    }

    return Count;
}

internal void BuildMeshHulls(game_state *GameState, memory_arena *Arena)
{
    data_lake *Lake = &GameState->Lake;

    for (uint32 Slot = 0; Slot < Lake->MeshCount; ++Slot)
    {
        PhysicsSetMeshHull(&GameState->Physics, Arena, Slot, LakeCollisionMesh(Lake, Slot));
    }
}

internal void AddPhysicsBody(game_state *GameState, uint32 Slot)
{
    data_lake      *Lake       = &GameState->Lake;
    transform_pool *Transforms = &Lake->Transforms;

    physics_body *Body = PhysicsAddBody(&GameState->Physics);
    if (!Body)
    {
        return;
    }

    uint32 MeshHandle = Transforms->MeshHandle[Slot];

    Body->Position        = Transforms->Position[Slot];
    Body->Rotation        = Transforms->Rotation[Slot];
    Body->Velocity        = Vector3(0.0f, 0.0f, 0.0f);
    Body->AngularVelocity = Vector3(0.0f, 0.0f, 0.0f);
    Body->Collider        = MakeCollider(Transforms->EntityID[Slot], EntityLocalToWorld(Lake, Slot), LakeCollisionMesh(Lake, MeshHandle));
    Body->Hull            = GameState->Physics.MeshHulls[MeshHandle];

    if (Transforms->Static[Slot])
    {
        Body->InvMass    = 0.0f;
        Body->InvInertia = 0.0f;
    }
    else
    {
        real32 Radius = 0.5f * Length(Body->Collider.Mesh.BoundsMax - Body->Collider.Mesh.BoundsMin);
        Body->InvMass    = 1.0f;
        Body->InvInertia = 1.0f / (0.4f * Radius * Radius);
    }

    ComputeWorldAABB(Body->Collider.LocalToWorld, Body->Collider.Mesh.BoundsMin, Body->Collider.Mesh.BoundsMax, &Body->BoundsMin, &Body->BoundsMax);
}

internal void SyncPhysicsBodies(game_state *GameState)
{
    uint32 SlotCount = Minimum(GameState->Lake.Transforms.Count, GameState->Physics.BodyCapacity);

    while (GameState->Physics.BodyCount < SlotCount)
    {
        AddPhysicsBody(GameState, GameState->Physics.BodyCount);
    }
}

internal void ApplyPhysicsPoses(data_lake *Lake, physics_body *Bodies, uint32 BodyCount)
{
    for (uint32 Slot = 0; Slot < BodyCount; ++Slot)
    {
        Lake->Transforms.Position[Slot] = Bodies[Slot].Position;
        Lake->Transforms.Rotation[Slot] = Bodies[Slot].Rotation;
    }
}

internal void PushEntitiesToRender(data_lake *Lake, render_commands *Commands, uint32 SelectedID, real32 Alpha)
{
    for (uint32 Slot = 0; Slot < Lake->Transforms.Count; ++Slot)
    {
        Vector4 Tint = Lake->Transforms.Tint[Slot];
        if (SelectedID && Lake->Transforms.EntityID[Slot] == SelectedID)
        {
            Tint = Vector4(1.0f, 0.85f, 0.2f, Tint.W);
        }

        PushRenderMesh(Commands, EntityRenderTransform(Lake, Slot, Alpha), Tint, Lake->Transforms.MeshHandle[Slot], Lake->Transforms.MaterialHandle[Slot]);
    }
}

internal uint32 AddUIButton(data_lake *Lake, const char *Name, rect2 Rect)
{
    entity *Entity = AddWidgetEntity(Lake, Name, Entity_UIButton, Rect.Min, Rect.Max, 0.0f);

    return Entity ? Entity->ID : 0;
}

internal rect2 EntityRect(data_lake *Lake, entity *Entity)
{
    Assert(Entity->Type == Entity_UIButton || Entity->Type == Entity_UISlider || Entity->Type == Entity_UIList);

    rect2 Result;
    Result.Min = Lake->Widgets.RectMin[Entity->Slot];
    Result.Max = Lake->Widgets.RectMax[Entity->Slot];

    return Result;
}

internal bool32 UIEntityButton(ui_context *UI, data_lake *Lake, uint32 FontHandle, uint32 Handle)
{
    entity *Entity = GetEntity(Lake, Handle);
    rect2   Rect   = EntityRect(Lake, Entity);

    bool32 Clicked = UIButton(UI, Handle, Rect);

    const char *Label = EntityName(Lake, Entity);
    Vector2 LabelP = Vector2(Rect.Min.X + 0.5f * ((Rect.Max.X - Rect.Min.X) - TextWidth(Lake, FontHandle, Label)),
                             Rect.Min.Y + 0.5f * ((Rect.Max.Y - Rect.Min.Y) - TextLineAdvance(Lake, FontHandle)));

    DrawText(UI->Commands, Lake, FontHandle, LabelP, Vector4(1.0f, 1.0f, 1.0f, 1.0f), Label);

    return Clicked;
}

internal uint32 SpawnEntity(game_state *GameState, Vector3 Position, uint32 MeshHandle, uint32 MaterialHandle)
{
    entity *Entity = AddMeshEntity(&GameState->Lake, 0, Position, MeshHandle, MaterialHandle);

    return Entity ? Entity->ID : 0;
}

#define BALL_SPEED 15.0f

internal void ShootBall(game_state *GameState, ray Aim)
{
    data_lake *Lake = &GameState->Lake;

    entity *Ball = AddMeshEntity(Lake, 0, Aim.Origin + Aim.Direction, GameState->SpawnMeshHandles[1], GameState->SpawnMaterialHandles[0]);
    if (!Ball)
    {
        return;
    }

    SyncPhysicsBodies(GameState);
    GameState->Physics.Bodies[Ball->Slot].Velocity = Aim.Direction * BALL_SPEED;
}

internal real32 UpdatePhysics(game_state* GameState, real32 dt)
{
    data_lake* Lake = &GameState->Lake;

    physics_world *World = &GameState->Physics;

    SyncPhysicsBodies(GameState);
    PhysicsAccumulate(World, dt);

    while (PhysicsNextTick(World))
    {
        EntityUpdatePreviousTrasform(Lake);

        if (!GameState->Paused)
        {
            PhysicsStep(World);
            ApplyPhysicsPoses(Lake, World->Bodies, World->BodyCount);
        }
    }

    return PhysicsRenderAlpha(World);
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

        LakeInit(&GameState->Lake, &GameState->WorldArena);

        LoadENGA(ENGA_PACK_PATH, Memory, GameState);
        PushLakeToRender(Lake, RenderCommands);

        InitEntities(Lake, WorldArena);

        GameState->ColliderCapacity = Lake->EntityCapacity;
        GameState->Colliders = PushArray(WorldArena, GameState->ColliderCapacity, collider);

        PhysicsInit(&GameState->Physics, WorldArena, Lake->EntityCapacity, Lake->MeshCapacity);
        BuildMeshHulls(GameState, WorldArena);

        uint32    TexTestHandle = LakeGetTextureHandle(Lake, "test");

        GameState->SkyHandle  = LakeGetCubemapHandle(Lake, "sky");
        GameState->FontHandle = LakeGetFontHandle(Lake, "DejaVuSansMono24");

        GameState->SpawnMeshHandles[0] = LakeGetMeshHandle(Lake, "cube");
        GameState->SpawnMeshHandles[1] = LakeGetMeshHandle(Lake, "sphere");

        materials* Materials = &GameState->Materials;
        GameState->SpawnMaterialHandles[0] = AddMaterial(Materials, UnlitMaterial(Vector4(1.0f, 1.0f, 1.0f, 1.0f), TexTestHandle));
        GameState->SpawnMaterialHandles[1] = GameState->SpawnMaterialHandles[0];
        GameState->SpawnMaterialHandles[2] = GameState->SpawnMaterialHandles[0];

        uint32 LitMaterialHandle   = AddMaterial(Materials, LitMaterial(Vector4(0.9f, 0.5f, 0.2f, 1.0f)));
        uint32 FloorMaterialHandle = AddMaterial(Materials, LitMaterial(Vector4(0.45f, 0.45f, 0.5f, 1.0f)));

        PushMaterialsToRender(Materials, RenderCommands);

        entity *Floor = AddMeshEntity(Lake, "floor", Vector3(0.0f, -2.1f, 0.0f), LakeGetMeshHandle(Lake, "plane"), FloorMaterialHandle);
        if (Floor)
        {
            Lake->Transforms.Static[Floor->Slot] = true;
        }

        for (uint32 SpawnIndex = 0; SpawnIndex < 3; ++SpawnIndex)
        {
            uint32 Index  = Lake->Transforms.Count;
            real32 Angle  = (real32)Index * 2.39996f;
            real32 Radius = 0.9f * SquareRoot((real32)Index + 1.0f);

            Vector3 Position = Vector3(Cos(Angle) * Radius, 0.0f, Sin(Angle) * Radius);

            uint32 MeshHandle     = GameState->SpawnMeshHandles[Index % ArrayCount(GameState->SpawnMeshHandles)];
            uint32 MaterialHandle = GameState->SpawnMaterialHandles[Index % ArrayCount(GameState->SpawnMaterialHandles)];

            SpawnEntity(GameState, Position, MeshHandle, MaterialHandle);
        }

        SpawnEntity(GameState, Vector3(-2.5f, 0.0f, 0.0f), GameState->SpawnMeshHandles[1], LitMaterialHandle);
        SpawnEntity(GameState, Vector3( 2.5f, 0.0f, 0.0f), GameState->SpawnMeshHandles[0], LitMaterialHandle);

        InitCamera(&GameState->Camera, Vector3(0.0f, 0.0f, 4.0f), DegToRad(75.0f));

        GameState->SelectedEntityID = 0;

        GameState->UI.Style = DefaultUIStyle();
        GameState->Paused   = false;


        GameState->PauseButton = AddUIButton(Lake, "pause", RectMinDim(20.0f, 20.0f, 140.0f, 36.0f));
        GameState->SpawnButton = AddUIButton(Lake, "spawn", RectMinDim(20.0f, 66.0f, 140.0f, 36.0f));

        Memory->IsInitialized = true;
    }

    ui_context *UI = &GameState->UI;
    BeginUI(UI, Input, RenderCommands);

    if (UIEntityButton(UI, Lake, GameState->FontHandle, GameState->PauseButton))
    {
        GameState->Paused = !GameState->Paused;
    }

    real32 RenderAlpha = UpdatePhysics(GameState, Input->dtForFrame);

    camera* Camera = &GameState->Camera;
    UpdateCamera(Camera, Input);

    bool32 PickedThisFrame = (UI->MousePressed && !UI->Active);
    if (PickedThisFrame && Input->RenderWidth > 0 && Input->RenderHeight > 0)
    {
        ray PickRay = CameraRayFromScreen(Camera, (real32)Input->MouseX, (real32)Input->MouseY, (real32)Input->RenderWidth, (real32)Input->RenderHeight);
        uint32 ColliderCount = BuildEntityColliders(Lake, RenderAlpha, GameState->Colliders, GameState->ColliderCapacity);
        GameState->SelectedEntityID = RayCastColliders(PickRay, GameState->Colliders, ColliderCount, 0);

        ShootBall(GameState, PickRay);
    }

    PushRenderCamera(RenderCommands, CameraView(Camera), Camera->FovY);
    PushRenderLight(RenderCommands, Vector3(30,0,0));
    PushRenderSkybox(RenderCommands, GameState->SkyHandle);
    PushEntitiesToRender(Lake, RenderCommands, GameState->SelectedEntityID, RenderAlpha);

    EndUI(UI);
}

extern "C" __declspec(dllexport)
GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer);
}
