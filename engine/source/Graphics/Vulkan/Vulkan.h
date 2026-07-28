#ifndef VULKAN_H
#define VULKAN_H

#include "Types.h"
#include "Memory.h"
#include "PlatformAPI.h"
#include "RenderCommands.h"
#include "KBNFormat.h"
#include "EngineMath.h"

#include <vulkan/vulkan.h>

#include "shaders/ShaderInterop.h"

static_assert(sizeof(vertex) == KBN_VERTEX_FLOATS * sizeof(real32), "vertex must match the packed asset layout");

#define MAX_SURFACE_FORMATS   64
#define MAX_PRESENT_MODES     8
#define MAX_SWAPCHAIN_IMAGES  8
#define MAX_PIPELINES         8
static_assert(MAX_TEXTURES == RENDER_MAX_TEXTURES, "shader texture array must match the render limit");
#define IMAGE_POOL_SIZE       Megabytes(64)

struct vulkan_shader
{
    platform_file_raw vert;
    platform_file_raw frag;
};

struct gpu_pool
{
    VkBuffer       Buffer;
    VkDeviceMemory Memory;
    void          *Mapped;
    uint32         Stride;
    uint32         Capacity;
};

struct gpu_texture
{
    VkImage     Image;
    VkImageView View;
};

struct image_memory_pool
{
    VkDeviceMemory Memory;
    VkDeviceSize   Capacity;
    VkDeviceSize   Used;
};

struct descriptor_set
{
    VkDescriptorSetLayout Layout;
    VkDescriptorSet       Handle;
};

struct render_state
{
    VkCullModeFlags CullMode;
    VkBool32        DepthTest;
    VkBool32        DepthWrite;
    VkBool32        AlphaBlend;
    bool32          Valid;
};

struct render_pipeline
{
    const char *ShaderName;

    uint32             PushConstantSize;
    VkShaderStageFlags PushConstantStages;

    VkPrimitiveTopology Topology;
    VkFrontFace         FrontFace;
    render_state        DefaultState;

    VkPipeline       Handle;
    VkPipelineLayout Layout;
};

struct vulkan_context
{
    VkInstance instance;
    VkSurfaceKHR surface;
    HWND windowHandle;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32 graphicsFamilyIndex;
    uint32 presentFamilyIndex;

    VkSwapchainKHR swapchain;
    VkImage swapchainImages[MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchainImageViews[MAX_SWAPCHAIN_IMAGES];
    uint32 swapchainImageCount;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;

    VkRenderPass renderPass;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    VkFormat depthFormat;

    descriptor_set   GlobalSet;

    VkDescriptorPool GlobalDescriptorPool;
    VkSampler        Sampler;
    gpu_pool         CameraBuffer;

    render_pipeline Pipelines[MAX_PIPELINES];
    uint32 PipelineCount;

    VkFramebuffer swapchainFramebuffers[MAX_SWAPCHAIN_IMAGES];

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphores[MAX_SWAPCHAIN_IMAGES];
    VkFence inFlightFence;

    gpu_pool VertexPool;
    gpu_pool IndexPool;

    gpu_texture       Textures[MAX_TEXTURES];
    image_memory_pool ImagePool;

    bool32                          DynamicBlend;
    PFN_vkCmdSetColorBlendEnableEXT CmdSetColorBlendEnableEXT;
};

struct queue_family_indices
{
    uint32 graphicsIndex;
    uint32 presentIndex;
    bool32 graphicsSupported;
    bool32 presentSupported;
};

struct swapchain_support_details
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR formats[MAX_SURFACE_FORMATS];
    uint32 formatCount;
    VkPresentModeKHR presentModes[MAX_PRESENT_MODES];
    uint32 presentModeCount;
};

internal void InitVulkan(HINSTANCE hinstance, HWND hwnd);
internal void RenderVulkanFrame(render_commands *Commands);
internal void ShutdownVulkan();


#endif
