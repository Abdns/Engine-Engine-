#ifndef SOFTWARERENDER_H
#define SOFTWARERENDER_H

#include "Types.h"
#include "Intrinsics.h"      // RoundReal32ToInt32 / RoundReal32ToUInt32
#include "GameRender.h"      // game_offscreen_buffer
#include "RenderCommands.h"  // render_commands

// ПРОГРАММНЫЙ (CPU) бэкенд рендера — платформо-НЕЗАВИСИМЫЙ потребитель буфера
// команд (как software-рендерер у Кейси). Ничего не знает про Win32/Vulkan —
// просто исполняет команды в пиксельный буфер. Альтернатива Vulkan-бэкенду.
internal void SoftwareRenderCommands(render_commands *Commands, game_offscreen_buffer *Buffer);

#endif
