#include "Vulkan.h"

#include "VulkanCore.cpp"
#include "VulkanDevice.cpp"
#include "VulkanResources.cpp"
#include "VulkanPasses.cpp"
#include "VulkanPipeline.cpp"
#include "VulkanFrame.cpp"

global_variable render_target   SceneTarget;
global_variable render_pass     Passes[Pass_Count];
global_variable render_pipeline Pipelines[Pipeline_Count];

internal bool32 CreateFramePasses(vulkan_context *context)
{
    SceneTarget = CreateRenderTarget(context, VK_FORMAT_R16G16B16A16_SFLOAT);
    if (SceneTarget.View == VK_NULL_HANDLE)
    {
        return false;
    }

    WriteImageDescriptor(context, GlobalResources.GlobalSet.Handle, BINDING_TEXTURES, TEXTURE_SLOT_SCENE, SceneTarget.View);

    Passes[Pass_Scene] = CreateScenePass(context, SceneTarget.View, context->depthImageView);
    Passes[Pass_Post]  = CreatePostPass(context, context->swapchainImageViews, context->swapchainImageCount);

    if (Passes[Pass_Scene].Handle == VK_NULL_HANDLE || Passes[Pass_Post].Handle == VK_NULL_HANDLE)
    {
        return false;
    }

    DebugLog("Frame passes created (%d)\n", Pass_Count);
    return true;
}

internal void DestroyFramePasses(vulkan_context *context)
{
    DestroyPass(context, &Passes[Pass_Scene]);
    DestroyPass(context, &Passes[Pass_Post]);
    DestroyRenderTarget(context, &SceneTarget);
}

internal void ResizeRenderer(vulkan_context *context)
{
    if (!RecreateSwapchain(context))
    {
        return;
    }

    DestroyFramePasses(context);
    CreateFramePasses(context);
}

internal void InitVulkan(HINSTANCE hinstance, HWND hwnd)
{
    vulkan_context *context = &GlobalVulkan;
    context->windowHandle = hwnd;

    if (!CheckInstanceVersion())
    {
        return;
    }

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
    if (!CreateCommandPool(context))                   return;
    if (!CreateCommandBuffer(context))                 return;
    if (!CreateSyncObjects(context))                   return;

    if (!CreateGeometryPools(context, &GlobalResources))       return;
    if (!CreateImagePool(context, &GlobalResources.ImagePool)) return;
    if (!CreateGlobalResources(context, &GlobalResources))     return;

    if (!CreateFramePasses(context))                   return;

    Pipelines[Pipeline_Unlit]  = UnlitPipeline();
    Pipelines[Pipeline_Skybox] = SkyboxPipeline();
    Pipelines[Pipeline_Post]   = PostPipeline();

    if (!BuildPipeline(context, &Pipelines[Pipeline_Unlit],  Passes[Pass_Scene].Handle, GlobalResources.GlobalSet.Layout)) return;
    if (!BuildPipeline(context, &Pipelines[Pipeline_Skybox], Passes[Pass_Scene].Handle, GlobalResources.GlobalSet.Layout)) return;
    if (!BuildPipeline(context, &Pipelines[Pipeline_Post],   Passes[Pass_Post].Handle,  GlobalResources.GlobalSet.Layout)) return;

    DebugLog("Vulkan ready\n");
}

internal void RenderVulkanFrame(render_commands *Commands)
{
    vulkan_context *context = &GlobalVulkan;

    if (Pipelines[Pipeline_Unlit].Handle == VK_NULL_HANDLE)
    {
        return;
    }

    WaitForFrame(context);

    ProcessLoadCommands(context, Commands);

    vulkan_frame Frame = BeginFrame(context);
    if (!Frame.Ready)
    {
        if (Frame.NeedsResize)
        {
            ResizeRenderer(context);
        }
        return;
    }

    BeginPass(Frame.Cmd, &Passes[Pass_Scene], context->swapchainExtent, Frame.ImageIndex);
    ExecuteRenderCommands(context, Frame.Cmd, &GlobalResources, Pipelines, Commands);
    EndPass(Frame.Cmd);

    BeginPass(Frame.Cmd, &Passes[Pass_Post], context->swapchainExtent, Frame.ImageIndex);
    DrawFullscreen(context, Frame.Cmd, &Pipelines[Pipeline_Post]);
    EndPass(Frame.Cmd);

    if (EndFrame(context, &Frame))
    {
        ResizeRenderer(context);
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

        DestroyFramePasses(context);

        for (uint32 i = 0; i < Pipeline_Count; ++i)
        {
            if (Pipelines[i].Handle != VK_NULL_HANDLE)
            {
                DestroyRenderPipeline(context, &Pipelines[i]);
            }
        }

        DestroyGeometryPools(context, &GlobalResources);
        DestroyGlobalResources(context, &GlobalResources);

        for (uint32 i = 0; i < MAX_TEXTURES; ++i)
        {
            vkDestroyImageView(context->device, GlobalResources.Textures[i].View, nullptr);
            vkDestroyImage(context->device, GlobalResources.Textures[i].Image, nullptr);
        }
        for (uint32 i = 0; i < MAX_CUBEMAPS; ++i)
        {
            vkDestroyImageView(context->device, GlobalResources.Cubemaps[i].View, nullptr);
            vkDestroyImage(context->device, GlobalResources.Cubemaps[i].Image, nullptr);
        }
        DestroyImagePool(context, &GlobalResources.ImagePool);

        vkDestroyImageView(context->device, context->depthImageView, nullptr);
        vkDestroyImage(context->device, context->depthImage, nullptr);
        vkFreeMemory(context->device, context->depthImageMemory, nullptr);

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
