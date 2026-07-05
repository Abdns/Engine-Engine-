#ifndef PLATFORMAPI_H
#define PLATFORMAPI_H

// ============================================================================
//  Граница ИГРА <-> ПЛАТФОРМА — полное описание (как handmade_platform.h у Кейси).
//  Здесь и ДАННЫЕ, и ФУНКЦИИ, которые ходят через границу. ОБЕ стороны включают
//  ТОЛЬКО этот файл, чтобы говорить друг с другом. Игровые утилиты/состояние
//  сюда НЕ тянем — платформе они не нужны.
// ============================================================================

// --- Данные, что пересекают границу ---
#include "GameMemory.h"       // game_memory + файловые сервисы (debug_read_file_result)
#include "GameInput.h"        // game_input
#include "GameSound.h"        // game_sound_output_buffer
#include "GameRender.h"       // game_offscreen_buffer
#include "RenderCommands.h"   // render_commands

// --- Функции: две точки входа игры (игра ОПРЕДЕЛЯЕТ, платформа ЗОВЁТ по указателю) ---
#define GAME_UPDATE_AND_RENDER(name) void name(game_memory* Memory, game_input* Input, render_commands* RenderCommands)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory* Memory, game_sound_output_buffer* SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#endif
