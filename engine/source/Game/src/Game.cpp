#include "Game.h"

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

        Colliders[Count++] = MakeCollider(Entity->ID, EntityTransform(Lake, Entity), LakeCollisionMesh(Lake, MeshHandle));
    }

    return Count;
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

        PushRenderMesh(Commands, EntityTransform(Lake, Entity), Tint, Entity->MeshHandle, Entity->MaterialHandle);
    }
}

internal uint32 AddUIButton(data_lake *Lake, const char *Name, rect2 Rect)
{
    entity *Entity = AddWidgetEntity(Lake, Name, Entity_UIButton, Rect.Min, Rect.Max, 0.0f);

    return Entity ? Entity->ID : 0;
}

internal uint32 AddUISlider(data_lake *Lake, const char *Name, rect2 Rect, real32 Value)
{
    entity *Entity = AddWidgetEntity(Lake, Name, Entity_UISlider, Rect.Min, Rect.Max, Value);

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

internal bool32 UIEntitySlider(ui_context *UI, data_lake *Lake, uint32 Handle)
{
    entity *Entity = GetEntity(Lake, Handle);

    return UISlider(UI, Handle, EntityRect(Lake, Entity), Lake->Widgets.Value + Entity->Slot);
}

internal uint32 AddUIList(data_lake *Lake, const char *Name, rect2 Rect)
{
    entity *Entity = AddWidgetEntity(Lake, Name, Entity_UIList, Rect.Min, Rect.Max, 0.0f);

    return Entity ? Entity->ID : 0;
}

internal uint32 UIEntityList(ui_context *UI, data_lake *Lake, uint32 Handle, uint32 *EntityIDs, uint32 Count, uint32 SelectedID)
{
    const char *Names[MAX_ENTITIES];
    int32 SelectedIndex = -1;

    for (uint32 Index = 0; Index < Count; ++Index)
    {
        Names[Index] = EntityName(Lake, GetEntity(Lake, EntityIDs[Index]));
        if (SelectedID && EntityIDs[Index] == SelectedID)
        {
            SelectedIndex = (int32)Index;
        }
    }

    entity *List = GetEntity(Lake, Handle);

    int32 Clicked = UIList(UI, Handle, EntityRect(Lake, List), Names, Count, 5, SelectedIndex, Lake->Widgets.Value + List->Slot);

    return (Clicked >= 0) ? EntityIDs[Clicked] : 0;
}

internal uint32 SpawnEntity(game_state *GameState)
{
    data_lake *Lake = &GameState->Lake;

    uint32 Index  = Lake->Transforms.Count;
    real32 Angle  = (real32)Index * 2.39996f;
    real32 Radius = 0.9f * SquareRoot((real32)Index + 1.0f);

    Vector3 Position = Vector3(Cos(Angle) * Radius, 0.0f, Sin(Angle) * Radius);

    uint32 MeshHandle     = GameState->SpawnMeshHandles[Index % ArrayCount(GameState->SpawnMeshHandles)];
    uint32 MaterialHandle = GameState->SpawnMaterialHandles[Index % ArrayCount(GameState->SpawnMaterialHandles)];

    entity *Entity = AddMeshEntity(Lake, 0, Position, MeshHandle, MaterialHandle);
    if (!Entity)
    {
        return 0;
    }

    Lake->Transforms.AngularVelocity[Entity->Slot] = ENTITY_SPIN;

    return Entity->ID;
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

        uint32    TexTestHandle = LakeGetTextureHandle(Lake, "test");

        GameState->SkyHandle  = LakeGetCubemapHandle(Lake, "sky");
        GameState->FontHandle = LakeGetFontHandle(Lake, "DejaVuSansMono24");

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
        GameState->SpinPaused = false;

        GameState->PauseButton = AddUIButton(Lake, "pause", RectMinDim(20.0f, 20.0f, 140.0f, 36.0f));
        GameState->SpeedSlider = AddUISlider(Lake, "speed", RectMinDim(20.0f, 66.0f, 140.0f, 24.0f), 1.0f);
        GameState->SpawnButton = AddUIButton(Lake, "spawn", RectMinDim(20.0f, 100.0f, 140.0f, 36.0f));
        GameState->ClearButton = AddUIButton(Lake, "clear", RectMinDim(20.0f, 146.0f, 140.0f, 36.0f));
        GameState->EntityList  = AddUIList(Lake, "list", RectMinDim(20.0f, 192.0f, 140.0f, 120.0f));

        Memory->IsInitialized = true;
    }

    ui_context *UI = &GameState->UI;
    BeginUI(UI, Input, RenderCommands);

    uint32 FontHandle = GameState->FontHandle;

    if (UIEntityButton(UI, Lake, FontHandle, GameState->PauseButton))
    {
        GameState->SpinPaused = !GameState->SpinPaused;
    }

    UIEntitySlider(UI, Lake, GameState->SpeedSlider);

    if (UIEntityButton(UI, Lake, FontHandle, GameState->SpawnButton))
    {
        GameState->SelectedEntityID = SpawnEntity(GameState);
    }

    if (UIEntityButton(UI, Lake, FontHandle, GameState->ClearButton))
    {
        GameState->SelectedEntityID = 0;
    }

    uint32 MeshIDs[MAX_ENTITIES];
    uint32 MeshCount = 0;
    for (uint32 Index = 0; Index < Lake->EntityCount; ++Index)
    {
        entity *Entity = Lake->Entities + Index;
        if (Entity->Type == Entity_Mesh)
        {
            MeshIDs[MeshCount++] = Entity->ID;
        }
    }

    uint32 ClickedID = UIEntityList(UI, Lake, GameState->EntityList, MeshIDs, MeshCount, GameState->SelectedEntityID);
    if (ClickedID)
    {
        GameState->SelectedEntityID = ClickedID;
    }

    real32 SpinScale = GameState->SpinPaused ? 0.0f : EntityValue(Lake, GameState->SpeedSlider);
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
