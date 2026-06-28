#ifndef WIN32TIMER_H
#define WIN32TIMER_H

#include <windows.h>
#include "Types.h"

extern int64 GlobalPerfCountFrequency;

void Win32InitTimer(bool32* SleepIsGranular);
int32 Win32GetMonitorRefreshHz(HDC DeviceContext);

inline LARGE_INTEGER Win32GetWallClock(void)
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    return (real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCountFrequency;
}

real32 Win32WaitForFrameEnd(LARGE_INTEGER* LastCounter,
                            real32 TargetSecondsPerFrame,
                            bool32 SleepIsGranular);

// Распечатать "Xms/f, YF/s, Zmc/f" в OutputDebugString.
void Win32OutputFrameStats(real32 MSPerFrame, int64 CycleElapsed);

#endif
