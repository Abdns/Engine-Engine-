#ifndef WIN32SOUND_H
#define WIN32SOUND_H

#include <windows.h>
#include <dsound.h>
#include "Types.h"
#include "EngineLayer.h"
#include "Win32GDI.h"

extern LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// ---- Structs --------------------------------------------------------------

struct win32_sound_output
{
    int32  SamplesPerSecond;
    uint32 RunningSampleIndex;
    int32  BytesPerSample;
    int32  SecondaryBufferSize;
    DWORD  ExpectedSoundBytesPerFrame;
    DWORD  SafetyBytes;
};

struct win32_sound_lock_region
{
    DWORD  ByteToLock;
    DWORD  BytesToWrite;
    bool32 IsValid;

    DWORD  PlayCursor;
    DWORD  WriteCursor;
    DWORD  ExpectedFrameBoundaryByte;
};

struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;
    DWORD ExpectedFlipPlayCursor;
    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

// ---- API ------------------------------------------------------------------

win32_sound_output       Win32MakeSoundOutput(int32 SamplesPerSecond, int32 GameUpdateHz);
void                     Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize);
void                     Win32ClearSoundBuffer(win32_sound_output* SoundOutput);
win32_sound_lock_region  Win32GetSoundLockRegion(win32_sound_output* SoundOutput,
                                                 real32 SecondsLeftUntilFlip,
                                                 real32 TargetSecondsPerFrame,
                                                 bool32* SoundIsValid);
void                     Win32FillSoundBuffer(win32_sound_output* SoundOutput,
                                              DWORD ByteToLock, DWORD BytesToWrite,
                                              game_sound_output_buffer* SourceBuffer);

void Win32DebugDrawVertical(win32_offscreen_buffer* Buffer, int X, int Top, int Bottom, uint32 Color);
void Win32DebugDrawSoundBufferMarker(win32_offscreen_buffer* Buffer,
                                     win32_sound_output* SoundOutput,
                                     real32 C, int PadX, int Top, int Bottom,
                                     DWORD Value, uint32 Color);
void Win32DebugSyncDisplay(win32_offscreen_buffer* Buffer,
                           int MarkerCount, win32_debug_time_marker* Markers,
                           int CurrentMarkerIndex,
                           win32_sound_output* SoundOutput, real32 TargetSecondsPerFrame);

void Win32RecordDebugMarker(win32_debug_time_marker* Markers,
                            int* MarkerIndex,
                            int MarkerCount,
                            win32_sound_lock_region* LockRegion);

#endif
