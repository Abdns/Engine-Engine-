#ifndef VULKANSHADERS_H
#define VULKANSHADERS_H

#include "Win32FileIO.h"   // debug_read_file_result (+ DEBUGPlatformReadEntireFile/Free)

// Интерфейс отдельного модуля загрузки шейдеров. Это НЕ тот анти-паттерн, что был
// в VulkanCommon.h (forward-decl static-функций разрезанной подсистемы) — здесь
// заголовок = настоящий интерфейс самостоятельного модуля (как VulkanApi.h).
struct vulkan_shader
{
    debug_read_file_result vert;
    debug_read_file_result frag;
    bool32 valid;
};

// Загрузить шейдер по имени: ищет CompiledShaders/<name>.vert.spv и <name>.frag.spv
internal vulkan_shader LoadShader(const char *name);
internal void FreeShader(vulkan_shader *shader);

#endif
