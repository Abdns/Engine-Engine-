#include "Vulkan.h"

#include "VulkanCore.cpp"
#include "VulkanDevice.cpp"
#include "VulkanSwapchain.cpp"
#include "VulkanPipeline.cpp"
#include "VulkanAssets.cpp"
#include "VulkanFrame.cpp"

internal void InitVulkan(HINSTANCE hinstance, HWND hwnd)
{
    vulkan_context *context = &GlobalVulkan;
    context->windowHandle = hwnd;

    if (!CheckInstanceExtensionSupport(RequiredInstanceExtensions, ArrayCount(RequiredInstanceExtensions)))
    {
        DebugLog("Required instance extensions missing\n");
        return;
    }

    VkApplicationInfo appInfo = VkGetInfo();
    VkInstanceCreateInfo InstanceInfo = GetInstanceInfo(&appInfo, RequiredInstanceExtensions, ArrayCount(RequiredInstanceExtensions));

    if (vkCreateInstance(&InstanceInfo, nullptr, &context->instance) != VK_SUCCESS)
    {
        DebugLog("Fail to create vulkan instance\n");
        return;
    }
    DebugLog("Vulkan instance created\n");

    if (!CreateSurface(context, hinstance, hwnd))      return;
    if (!SelectDevice(context))                        { DebugLog("No suitable GPU found\n"); return; }
    if (!CreateLogicalDevice(context))                 return;
    if (!CreateSwapchain(context, hwnd))               return;
    if (!CreateImageViews(context))                    return;
    if (!CreateDepthResources(context))                return;
    // Render pass takes depthFormat from CreateDepthResources and feeds CreateFramebuffers
    if (!CreateRenderPass(context))                    return;
    if (!CreateFramebuffers(context))                  return;
    if (!CreateCommandPool(context))                   return;
    if (!CreateCommandBuffer(context))                 return;
    if (!CreateSyncObjects(context))                   return;

    if (!CreatePipeline(context, Pipeline_Primitive))  return;

    DebugLog("Vulkan ready: %u pipeline(s) up\n", context->PipelineCount);
}

internal void RenderVulkanFrame(render_commands *Commands)
{
    if (DrawFrame(&GlobalVulkan, Commands))
    {
        RecreateSwapchain(&GlobalVulkan);
    }
}

internal void ShutdownVulkan()
{
    vulkan_context *context = &GlobalVulkan;

    if (context->device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(context->device);

        vkDestroySemaphore(context->device, context->imageAvailableSemaphore, nullptr);
        for (uint32 i = 0; i < context->swapchainImageCount; ++i)
        {
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], nullptr);
        }
        vkDestroyFence(context->device, context->inFlightFence, nullptr);

        vkDestroyCommandPool(context->device, context->commandPool, nullptr);

        for (uint32 i = 0; i < context->swapchainImageCount; ++i)
        {
            vkDestroyFramebuffer(context->device, context->swapchainFramebuffers[i], nullptr);
        }

        for (uint32 i = 0; i < context->MeshCount; ++i)
        {
            vkDestroyBuffer(context->device, context->Meshes[i].VertexBuffer, nullptr);
            vkFreeMemory(context->device, context->Meshes[i].VertexBufferMemory, nullptr);
        }

        for (uint32 i = 0; i < MAX_PIPELINES; ++i)
        {
            if (context->Pipelines[i].Handle != VK_NULL_HANDLE)
            {
                DestroyRenderPipeline(context, &context->Pipelines[i]);
            }
        }
        context->PipelineCount = 0;
        for (uint32 i = 0; i < context->TextureCount; ++i)
        {
            vkDestroyImageView(context->device, context->Textures[i].View, nullptr);
            vkDestroyImage(context->device, context->Textures[i].Image, nullptr);
            vkFreeMemory(context->device, context->Textures[i].Memory, nullptr);
        }

        vkDestroyImageView(context->device, context->depthImageView, nullptr);
        vkDestroyImage(context->device, context->depthImage, nullptr);
        vkFreeMemory(context->device, context->depthImageMemory, nullptr);

        vkDestroyRenderPass(context->device, context->renderPass, nullptr);

        for (uint32 i = 0; i < context->swapchainImageCount; ++i)
        {
            vkDestroyImageView(context->device, context->swapchainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(context->device, context->swapchain, nullptr);
        vkDestroyDevice(context->device, nullptr);
    }

    if (context->instance != VK_NULL_HANDLE)
    {
        if (context->surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(context->instance, context->surface, nullptr);
        }
        vkDestroyInstance(context->instance, nullptr);
    }
}
