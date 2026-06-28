#ifndef VULKANSHADERS_H
#define VULKANSHADERS_H

#include "Win32FileIO.h"   // debug_read_file_result (поля структуры) + файловый I/O

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
