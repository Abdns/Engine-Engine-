#ifndef GAME_H
#define GAME_H

// Умбрелла ИГРОВОГО слоя: граница + игровые утилиты + приватное состояние игры.
// (Границу игра<->платформа описывает PlatformAPI.h — его же включает и платформа.)
#include "Types.h"
#include "Intrinsics.h"      // игровая математика / интринсики
#include "HandmadeMath.h"
#include "PlatformAPI.h"     // ГРАНИЦА игра<->платформа: данные + функции (общая с платформой)
#include "GameState.h"     // приватное состояние игры (платформе не нужно)

#endif
