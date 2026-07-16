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

#define MAX_EXTENSIONS        256
#define MAX_DEVICES           4
#define MAX_FAMILY_COUNT      8
#define MAX_DEVICE_EXTENSIONS 256
#define MAX_SURFACE_FORMATS   64
#define MAX_PRESENT_MODES     8
#define MAX_SWAPCHAIN_IMAGES  8
#define MAX_MESHES            16
#define MAX_TEXTURES          16
#define MAX_VERTEX_ATTRIBUTES 8
#define MAX_OBJECTS           256
#define MAX_PIPELINES         8
#define MAX_PIPELINE_RESOURCES 8
#define MAX_PIPELINE_NAME     64

struct vulkan_shader
{
    platform_file_raw vert;
    platform_file_raw frag;
};

struct gpu_mesh
{
    VkBuffer VertexBuffer;
    VkDeviceMemory VertexBufferMemory;
    uint32 VertexCount;
};

struct gpu_texture
{
    VkImage         Image;
    VkDeviceMemory  Memory;
    VkImageView     View;
    VkDescriptorSet DescriptorSet;
};

enum descriptor_frequency
{
    Frequency_Global      = SET_GLOBAL,
    Frequency_PerFrame    = SET_PER_FRAME,
    Frequency_PerMaterial = SET_PER_MATERIAL,
    Frequency_PerObject   = SET_PER_OBJECT,
    Frequency_Count,
};

enum resource_kind
{
    Resource_Uniform = 0,
    Resource_UniformDynamic,
    Resource_Texture,
};

struct resource_slot
{
    descriptor_frequency Frequency;
    uint32               Binding;
    resource_kind        Kind;
    VkShaderStageFlags   Stages;
    uint32               Size;
    uint32               Count;
};

struct pipeline_resource
{
    resource_slot  Slot;
    VkBuffer       Buffer;
    VkDeviceMemory Memory;
    void          *Mapped;
    uint32         Stride;
};

struct render_pipeline_config
{
    const char *ShaderName;

    uint32 VertexStride;
    uint32 AttributeCount;
    VkVertexInputAttributeDescription Attributes[MAX_VERTEX_ATTRIBUTES];

    const resource_slot *Resources;
    uint32               ResourceCount;

    uint32             PushConstantSize;
    VkShaderStageFlags PushConstantStages;

    VkPrimitiveTopology Topology;
    VkCullModeFlags     CullMode;
    VkFrontFace         FrontFace;
    bool32              DepthTest;
    bool32              DepthWrite;
    bool32              AlphaBlend;

    VkFilter             SamplerFilter;
    VkSamplerAddressMode SamplerAddressMode;
};

struct render_pipeline
{
    char Name[MAX_PIPELINE_NAME];

    VkDescriptorSetLayout SetLayouts[Frequency_Count];
    VkDescriptorSet       Sets[Frequency_Count];
    VkPipelineLayout      Layout;
    VkPipeline            Handle;
    VkDescriptorPool      DescriptorPool;
    VkSampler             Sampler;

    pipeline_resource Resources[MAX_PIPELINE_RESOURCES];
    uint32            ResourceCount;
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

    render_pipeline Pipelines[MAX_PIPELINES];
    uint32 PipelineCount;

    VkFramebuffer swapchainFramebuffers[MAX_SWAPCHAIN_IMAGES];

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphores[MAX_SWAPCHAIN_IMAGES];
    VkFence inFlightFence;

    gpu_mesh Meshes[MAX_MESHES];
    uint32 MeshCount;

    gpu_texture Textures[MAX_TEXTURES];
    uint32 TextureCount;
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
