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

#include "Win32Window.cpp"
#include "Win32Input.cpp"
#include "Win32Sound.cpp"
#include "Win32GDI.cpp"
#include "Win32FileIO.cpp"
#include "Win32Timer.cpp"
#include "Win32GameCode.cpp"
#include "Win32Replay.cpp"

internal void Win32AllocateGameMemory(game_memory* GameMemory, win32_state* State)
{
#if HANDMADE_INTERNAL
    LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
    LPVOID BaseAddress = 0;
#endif

    GameMemory->PermanentStorageSize = Megabytes(64);
    GameMemory->TransientStorageSize = Gigabytes(1);

    State->TotalSize = GameMemory->PermanentStorageSize + GameMemory->TransientStorageSize;
    State->GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)State->TotalSize,
                                          MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    GameMemory->PermanentStorage = State->GameMemoryBlock;
    GameMemory->TransientStorage = (uint8*)GameMemory->PermanentStorage + GameMemory->PermanentStorageSize;

#if HANDMADE_INTERNAL
    GameMemory->DEBUGPlatformReadEntireFile  = DEBUGPlatformReadEntireFile;
    GameMemory->DEBUGPlatformFreeFileMemory  = DEBUGPlatformFreeFileMemory;
    GameMemory->DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
#endif
}

internal void Win32PrepareKeyboardInput(game_input* NewInput, game_input* OldInput)
{
    game_controller_input* OldKb = &OldInput->Controllers[0];
    game_controller_input* NewKb = &NewInput->Controllers[0];
    *NewKb = {};
    NewKb->IsConnected = true;
    for (int i = 0; i < ArrayCount(NewKb->Buttons); ++i)
    {
        NewKb->Buttons[i].EndedDown = OldKb->Buttons[i].EndedDown;
    }
}

internal void Win32ProcessMouseButton(game_button_state* NewState, game_button_state* OldState, bool32 IsDown)
{
    NewState->EndedDown = IsDown;
    NewState->HalfTransitionCount = (OldState->EndedDown != IsDown) ? 1 : 0;
}

internal void Win32ProcessMouseInput(HWND Window, game_input* NewInput, game_input* OldInput)
{
    POINT MouseP;
    GetCursorPos(&MouseP);
    ScreenToClient(Window, &MouseP);
    NewInput->MouseX = MouseP.x;
    NewInput->MouseY = MouseP.y;
    NewInput->MouseZ = 0; // колесо мыши через WM_MOUSEWHEEL — отдельная история

    Win32ProcessMouseButton(&NewInput->MouseButtons[0], &OldInput->MouseButtons[0],
                            GetKeyState(VK_LBUTTON) & (1 << 15));
    Win32ProcessMouseButton(&NewInput->MouseButtons[1], &OldInput->MouseButtons[1],
                            GetKeyState(VK_MBUTTON) & (1 << 15));
    Win32ProcessMouseButton(&NewInput->MouseButtons[2], &OldInput->MouseButtons[2],
                            GetKeyState(VK_RBUTTON) & (1 << 15));
    Win32ProcessMouseButton(&NewInput->MouseButtons[3], &OldInput->MouseButtons[3],
                            GetKeyState(VK_XBUTTON1) & (1 << 15));
    Win32ProcessMouseButton(&NewInput->MouseButtons[4], &OldInput->MouseButtons[4],
                            GetKeyState(VK_XBUTTON2) & (1 << 15));
}

internal void Win32FillGameOffscreenBuffer(game_offscreen_buffer* Out, win32_offscreen_buffer* Src)
{
    Out->Memory        = Src->Memory;
    Out->Width         = Src->Width;
    Out->Height        = Src->Height;
    Out->Pitch         = Src->Pitch;
    Out->BytesPerPixel = Src->BytesPerPixel;
}

internal void Win32OutputFrameStats(real32 MSPerFrame, int64 CycleElapsed)
{
    real32 FPS  = 1000.0f / MSPerFrame;
    real32 MCPF = (real32)CycleElapsed / (1000.0f * 1000.0f);

    char TextBuffer[256];
    wsprintfA(TextBuffer, "%dms/f, %dF/s, %dmc/f\n",
              (int)MSPerFrame, (int)FPS, (int)MCPF);
    OutputDebugStringA(TextBuffer);
}

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

    int32  MonitorRefreshHz      = Win32GetMonitorRefreshHz(DeviceContext);
    int32  GameUpdateHz          = MonitorRefreshHz;
    real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

    win32_sound_output SoundOutput = Win32MakeSoundOutput(48000, GameUpdateHz);
    Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
    Win32ClearSoundBuffer(&SoundOutput);
    if (GlobalSecondaryBuffer)
    {
        GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
    }

    int16* Samples = (int16*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                          MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    game_memory GameMemory = {};
    Win32AllocateGameMemory(&GameMemory, &State);
    Assert(Samples && GameMemory.PermanentStorage);

    game_input  Input[2] = {};
    game_input* NewInput = &Input[0];
    game_input* OldInput = &Input[1];

    isRunning = true;
    bool32 SoundIsValid = false;

#if HANDMADE_INTERNAL
    win32_debug_time_marker DebugTimeMarkers[30] = {};
    int DebugTimeMarkerIndex = 0;
#endif

    win32_game_code Game = Win32LoadGameCode(Paths.SourceGameCodeDLLFullPath,
                                             Paths.TempGameCodeDLLFullPath);

    LARGE_INTEGER LastCounter    = Win32GetWallClock();
    LARGE_INTEGER FlipWallClock  = Win32GetWallClock();
    int64         LastCycleCount = __rdtsc();

    while (isRunning)
    {
        // Day 024: запоминаем dll до проверки, чтобы понять — была ли перезагрузка.
        HMODULE PreviousDLL = Game.GameCodeDLL;
        Win32ReloadGameCodeIfChanged(&Game,
                                     Paths.SourceGameCodeDLLFullPath,
                                     Paths.TempGameCodeDLLFullPath,
                                     Paths.GameCodeLockFullPath);
        GameMemory.ExecutableReloaded = (Game.GameCodeDLL != PreviousDLL);

        // Day 023: даём игре длительность кадра и состояние мыши.
        NewInput->dtForFrame = TargetSecondsPerFrame;

        Win32PrepareKeyboardInput(NewInput, OldInput);
        Win32ProcessPendingMessages(&State, &NewInput->Controllers[0]);
        Win32ProcessMouseInput(Window, NewInput, OldInput);
        Win32ProcessXInput(NewInput, OldInput);

#if HANDMADE_INTERNAL
        if (State.InputRecordingIndex)
        {
            Win32RecordInput(&State, NewInput);
        }
        if (State.InputPlayingIndex)
        {
            Win32PlayBackInput(&State, NewInput);
        }
#endif

        game_offscreen_buffer Buffer = {};
        Win32FillGameOffscreenBuffer(&Buffer, &GlobalBackBuffer);

        // Day 024: сначала игра обновляет логику + рендер. Звук — отдельным вызовом ниже.
        Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);

        LARGE_INTEGER AudioWallClock          = Win32GetWallClock();
        real32        FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);
        real32        SecondsLeftUntilFlip    = TargetSecondsPerFrame - FromBeginToAudioSeconds;
        if (SecondsLeftUntilFlip < 0) SecondsLeftUntilFlip = 0;

        win32_sound_lock_region LockRegion =
            Win32GetSoundLockRegion(&SoundOutput, SecondsLeftUntilFlip,
                                    TargetSecondsPerFrame, &SoundIsValid);

        if (LockRegion.IsValid)
        {
            game_sound_output_buffer SoundBuffer = {};
            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
            SoundBuffer.SampleCount      = LockRegion.BytesToWrite / SoundOutput.BytesPerSample;
            SoundBuffer.Samples          = Samples;

            Game.GetSoundSamples(&GameMemory, &SoundBuffer);
            Win32FillSoundBuffer(&SoundOutput, LockRegion.ByteToLock,
                                 LockRegion.BytesToWrite, &SoundBuffer);
        }

        real32 MSPerFrame = Win32WaitForFrameEnd(&LastCounter, TargetSecondsPerFrame, SleepIsGranular);

        win32_window_dimensions Dimension = GetWindowDimension(Window);

#if HANDMADE_INTERNAL
        Win32DebugSyncDisplay(&GlobalBackBuffer,
                              ArrayCount(DebugTimeMarkers), DebugTimeMarkers,
                              DebugTimeMarkerIndex - 1,
                              &SoundOutput, TargetSecondsPerFrame);
#endif

        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
        FlipWallClock = Win32GetWallClock();

#if HANDMADE_INTERNAL
        Win32RecordDebugMarker(DebugTimeMarkers, &DebugTimeMarkerIndex,
                               ArrayCount(DebugTimeMarkers), &LockRegion);
#endif

        game_input* Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;

        int64 EndCycleCount = __rdtsc();
        int64 CycleElapsed  = EndCycleCount - LastCycleCount;
        LastCycleCount      = EndCycleCount;

        Win32OutputFrameStats(MSPerFrame, CycleElapsed);
    }

    return 0;
}
