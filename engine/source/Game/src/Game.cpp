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

internal uint32 BuildEntityColliders(data_lake *Lake, collider *Colliders, uint32 MaxColliders)
{
    uint32 Count = 0;

    for (uint32 Index = 0; Index < Lake->EntityCount && Count < MaxColliders; ++Index)
    {
        entity *Entity = Lake->Entities + Index;
        if (Entity->Type != Entity_Mesh)
        {
            continue;
        }

        uint32 MeshHandle = Entity->MeshHandle;
        if (MeshHandle >= Lake->MeshCount)
        {
            continue;
        }

        Colliders[Count++] = MakeCollider(Entity->ID, EntityLocalToWorld(Lake, Entity), LakeCollisionMesh(Lake, MeshHandle));
    }

    return Count;
}

internal void BuildMeshHulls(game_state *GameState, memory_arena *Arena)
{
    data_lake *Lake = &GameState->Lake;

    GameState->MeshHulls = PushArray(Arena, Lake->MeshCapacity, hull);

    for (uint32 Slot = 0; Slot < Lake->MeshCount; ++Slot)
    {
        collision Mesh      = LakeCollisionMesh(Lake, Slot);
        uint32    MaxPlanes = Mesh.IndexCount / 3;

        hull *Hull = GameState->MeshHulls + Slot;
        Hull->Planes     = PushArray(Arena, MaxPlanes, hull_plane);
        Hull->PlaneCount = BuildHullPlanes(&Mesh, Hull->Planes, MaxPlanes);
    }
}

internal uint32 BuildPhysicsBodies(game_state *GameState, physics_body *Bodies, uint32 *BodySlots, uint32 MaxBodies)
{
    data_lake *Lake  = &GameState->Lake;
    uint32 Count = 0;

    for (uint32 Index = 0; Index < Lake->EntityCount && Count < MaxBodies; ++Index)
    {
        entity *Entity = Lake->Entities + Index;
        if (Entity->Type != Entity_Mesh)
        {
            continue;
        }

        uint32 MeshHandle = Entity->MeshHandle;
        if (MeshHandle >= Lake->MeshCount)
        {
            continue;
        }

        physics_body *Body = Bodies + Count;
        Body->Position        = Lake->Transforms.Position[Entity->Slot];
        Body->Rotation        = Lake->Transforms.Rotation[Entity->Slot];
        Body->Velocity        = Lake->Transforms.Velocity[Entity->Slot];
        Body->AngularVelocity = Lake->Transforms.AngularVelocity[Entity->Slot];
        Body->Collider        = MakeCollider(Entity->ID, EntityLocalToWorld(Lake, Entity), LakeCollisionMesh(Lake, MeshHandle));
        Body->Hull            = GameState->MeshHulls[MeshHandle];

        if (Entity->Static)
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

        BodySlots[Count] = Entity->Slot;
        ++Count;
    }

    return Count;
}

internal void ApplyPhysicsBodies(data_lake *Lake, physics_body *Bodies, uint32 *BodySlots, uint32 BodyCount)
{
    for (uint32 Index = 0; Index < BodyCount; ++Index)
    {
        Lake->Transforms.Position[BodySlots[Index]]        = Bodies[Index].Position;
        Lake->Transforms.Rotation[BodySlots[Index]]        = Bodies[Index].Rotation;
        Lake->Transforms.Velocity[BodySlots[Index]]        = Bodies[Index].Velocity;
        Lake->Transforms.AngularVelocity[BodySlots[Index]] = Bodies[Index].AngularVelocity;
    }
}

internal void PushEntitiesToRender(data_lake *Lake, render_commands *Commands, uint32 SelectedID)
{
    for (uint32 Index = 0; Index < Lake->EntityCount; ++Index)
    {
        entity *Entity = Lake->Entities + Index;
        if (Entity->Type != Entity_Mesh)
        {
            continue;
        }

        Vector4 Tint = Lake->Transforms.Tint[Entity->Slot];
        if (SelectedID && Entity->ID == SelectedID)
        {
            Tint = Vector4(1.0f, 0.85f, 0.2f, Tint.W);
        }

        PushRenderMesh(Commands, EntityLocalToWorld(Lake, Entity), Tint, Entity->MeshHandle, Entity->MaterialHandle);
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

    Lake->Transforms.Velocity[Ball->Slot] = Aim.Direction * BALL_SPEED;
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
        BuildMeshHulls(GameState, WorldArena);

        InitEntities(Lake, WorldArena);

        GameState->ColliderCapacity = Lake->EntityCapacity;
        GameState->Colliders = PushArray(WorldArena, GameState->ColliderCapacity, collider);
        GameState->Bodies    = PushArray(WorldArena, GameState->ColliderCapacity, physics_body);
        GameState->BodySlots = PushArray(WorldArena, GameState->ColliderCapacity, uint32);

        uint32    TexTestHandle = LakeGetTextureHandle(Lake, "test");

        GameState->SkyHandle  = LakeGetCubemapHandle(Lake, "sky");
        GameState->FontHandle = LakeGetFontHandle(Lake, "DejaVuSansMono24");

        GameState->SpawnMeshHandles[0] = LakeGetMeshHandle(Lake, "cube");
        GameState->SpawnMeshHandles[1] = LakeGetMeshHandle(Lake, "sphere");

        materials* Materials = &GameState->Materials;
        GameState->SpawnMaterialHandles[0] = AddMaterial(Materials, UnlitMaterial(Vector4(1.0f, 1.0f, 1.0f, 1.0f), TexTestHandle));

        uint32 LitMaterialHandle   = AddMaterial(Materials, LitMaterial(Vector4(0.9f, 0.5f, 0.2f, 1.0f)));
        uint32 FloorMaterialHandle = AddMaterial(Materials, LitMaterial(Vector4(0.45f, 0.45f, 0.5f, 1.0f)));

        PushMaterialsToRender(Materials, RenderCommands);

        entity *Floor = AddMeshEntity(Lake, "floor", Vector3(0.0f, -2.1f, 0.0f), LakeGetMeshHandle(Lake, "plane"), FloorMaterialHandle);
        if (Floor)
        {
            Floor->Static = true;
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

    if (!GameState->Paused)
    {
        uint32 BodyCount = BuildPhysicsBodies(GameState, GameState->Bodies, GameState->BodySlots, GameState->ColliderCapacity);
        PhysicsStep(GameState->Bodies, BodyCount, Input->dtForFrame);
        ApplyPhysicsBodies(Lake, GameState->Bodies, GameState->BodySlots, BodyCount);
    }

    camera* Camera = &GameState->Camera;
    UpdateCamera(Camera, Input);

    bool32 PickedThisFrame = (UI->MousePressed && !UI->Active);
    if (PickedThisFrame && Input->RenderWidth > 0 && Input->RenderHeight > 0)
    {
        ray PickRay = CameraRayFromScreen(Camera, (real32)Input->MouseX, (real32)Input->MouseY, (real32)Input->RenderWidth, (real32)Input->RenderHeight);
        uint32 ColliderCount = BuildEntityColliders(Lake, GameState->Colliders, GameState->ColliderCapacity);
        GameState->SelectedEntityID = RayCastColliders(PickRay, GameState->Colliders, ColliderCount, 0);

        ShootBall(GameState, PickRay);
    }

    PushRenderCamera(RenderCommands, CameraView(Camera), Camera->FovY);
    PushRenderLight(RenderCommands, Vector3(30,0,0));
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
