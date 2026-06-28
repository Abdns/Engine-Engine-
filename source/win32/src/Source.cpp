#include <windows.h>
#include "Types.h"
#include "EngineLayer.h"
#include "Win32Window.h"
#include "Win32Input.h"
#include "Win32Sound.h"
#include "Win32GDI.h"
#include "Win32FileIO.h"
#include "Win32Timer.h"
#include "Win32GameCode.h"
#include "Win32Replay.h"
#include "VulkanApi.h"
#include "VulkanShaders.h"

#include "Win32Window.cpp"
#include "Win32Input.cpp"
#include "Win32Sound.cpp"
#include "Win32GDI.cpp"
#include "Win32FileIO.cpp"
#include "Win32Timer.cpp"
#include "Win32GameCode.cpp"
#include "Win32Replay.cpp"
#include "VulkanShaders.cpp"
#include "VulkanApi.cpp"


int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    bool32 SleepIsGranular;
    Win32InitTimer(&SleepIsGranular);

    win32_exe_paths Paths;
    Win32GetEXEPaths(&Paths);

    win32_state State = {};
    for (int i = 0; Paths.EXEFileName[i]; ++i) State.EXEFileName[i] = Paths.EXEFileName[i];
    State.OnePastLastSlash = State.EXEFileName + (Paths.OnePastLastSlash - Paths.EXEFileName);

    Win32LoadXInput();
    Win32ResizeDIB(&GlobalBackBuffer, 960, 540);

    HWND Window = Win32CreateMainWindow(Instance, L"MyEngine", L"Window", 960, 540);
    if (!Window) return 0;

    HDC DeviceContext = GetDC(Window);

    int32  GameUpdateHz          = Win32GetMonitorRefreshHz(DeviceContext);
    real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

    win32_sound_output SoundOutput = Win32MakeSoundOutput(48000, GameUpdateHz);
    int16* Samples = Win32StartSound(Window, &SoundOutput);

    game_memory GameMemory = {};
    Win32AllocateGameMemory(&GameMemory, &State);
    Assert(Samples && GameMemory.PermanentStorage);

    game_input  Input[2] = {};
    game_input* NewInput = &Input[0];
    game_input* OldInput = &Input[1];

    InitVulkan(Instance, Window);

    isRunning = true;
    bool32 SoundIsValid = false;

#if HANDMADE_INTERNAL
    win32_debug_time_marker DebugTimeMarkers[30] = {};
    int DebugTimeMarkerIndex = 0;
#endif

    win32_game_code Game = Win32LoadGameCode(Paths.SourceGameCodeDLLFullPath,Paths.TempGameCodeDLLFullPath);

    LARGE_INTEGER LastCounter    = Win32GetWallClock();
    LARGE_INTEGER FlipWallClock  = Win32GetWallClock();
    int64         LastCycleCount = __rdtsc();

    while (isRunning)
    {
        HMODULE PreviousDLL = Game.GameCodeDLL;
        Win32ReloadGameCodeIfChanged(&Game, Paths.SourceGameCodeDLLFullPath,
                                     Paths.TempGameCodeDLLFullPath, Paths.GameCodeLockFullPath);
        GameMemory.ExecutableReloaded = (Game.GameCodeDLL != PreviousDLL);

        NewInput->dtForFrame = TargetSecondsPerFrame;
        Win32ProcessInput(NewInput, OldInput, &State, Window);
#if HANDMADE_INTERNAL
        Win32UpdateRecordAndPlayback(&State, NewInput);
#endif

        game_offscreen_buffer Buffer = {};
        Win32FillGameOffscreenBuffer(&Buffer, &GlobalBackBuffer);
        Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);

        win32_sound_lock_region LockRegion = Win32UpdateAudio(
            &SoundOutput, FlipWallClock, TargetSecondsPerFrame, &SoundIsValid,
            Game.GetSoundSamples, &GameMemory, Samples);

        real32 MSPerFrame = Win32WaitForFrameEnd(&LastCounter, TargetSecondsPerFrame, SleepIsGranular);

#if HANDMADE_INTERNAL
        Win32DebugSyncDisplay(&GlobalBackBuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers,
                              DebugTimeMarkerIndex - 1, &SoundOutput, TargetSecondsPerFrame);
#endif
        Win32DisplayFrame(Window, DeviceContext);
        FlipWallClock = Win32GetWallClock();

#if HANDMADE_INTERNAL
        Win32RecordDebugMarker(DebugTimeMarkers, &DebugTimeMarkerIndex,
                               ArrayCount(DebugTimeMarkers), &LockRegion);
#endif

        game_input* Temp = NewInput; NewInput = OldInput; OldInput = Temp;

        int64 EndCycleCount = __rdtsc();
        int64 CycleElapsed  = EndCycleCount - LastCycleCount;
        LastCycleCount      = EndCycleCount;
        Win32OutputFrameStats(MSPerFrame, CycleElapsed);
    }

    ShutdownVulkan();

    return 0;
}
