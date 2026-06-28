#ifndef DEBUG_H
#define DEBUG_H

// Отладочный вывод. Подключай ЭТОТ заголовок вместо <stdio.h>.
// DebugLog(...) — как printf, но пишет только в internal-сборке,
// а в релизе (HANDMADE_INTERNAL=0) исчезает без следа (аргументы не вычисляются).

#if HANDMADE_INTERNAL
#include <stdio.h>
#define DebugLog(...) printf(__VA_ARGS__)
#else
#define DebugLog(...)
#endif

#endif
