#ifndef ENGINELAYER_H
#define ENGINELAYER_H

#include "Types.h"
#include "Intrinsics.h"
#include "HandmadeMath.h"
#include "GameInput.h"
#include "GameRender.h"
#include "GameSound.h"
#include "GameMemory.h"
#include "GameState.h"

// Day 024: API игры разбит на две функции, чтобы платформа могла дёргать звук
// отдельно от логики/картинки. Это даёт более точную синхронизацию аудио.
#define GAME_UPDATE_AND_RENDER(name) void name(game_memory* Memory, game_input* Input, \
                                               game_offscreen_buffer* Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory* Memory, game_sound_output_buffer* SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#endif
