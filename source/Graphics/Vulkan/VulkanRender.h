#ifndef VULKANRENDER_H
#define VULKANRENDER_H

#include "VulkanContext.h"   // vulkan_context

// Интерфейс модуля покадровой отрисовки (определён в VulkanRender.cpp). Только
// публичная точка входа — DrawFrame; RecordCommandBuffer приватный для .cpp.
// Это интерфейс-заголовок (как VulkanApi.h / VulkanShaders.h), а НЕ forward-decl
// разрезанной подсистемы: VulkanRender — самостоятельный модуль, который зовёт
// обёртка RenderVulkanFrame из VulkanApi.cpp.
internal bool32 DrawFrame(vulkan_context *context, render_commands *commands);   // true => swapchain устарел

#endif
