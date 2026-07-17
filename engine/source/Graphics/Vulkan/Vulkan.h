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

#define MAX_SURFACE_FORMATS   64
#define MAX_PRESENT_MODES     8
#define MAX_SWAPCHAIN_IMAGES  8
#define MAX_MESHES            16
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
    VkImage        Image;
    VkDeviceMemory Memory;
    VkImageView    View;
};

enum descriptor_set_index
{
    Set_Global      = SET_GLOBAL,
    Set_PerFrame    = SET_PER_FRAME,
    Set_PerMaterial = SET_PER_MATERIAL,
    Set_PerObject   = SET_PER_OBJECT,
    Set_Count,
};

struct resource_binding_description
{
    descriptor_set_index Set;
    uint32               Binding;
    VkDescriptorType     Type;
    VkShaderStageFlags   Stages;
    uint32               Size;
    uint32               Count;
};

// At most one uniform buffer per set: one block per update frequency
struct pipeline_buffer
{
    VkBuffer       Buffer;
    VkDeviceMemory Memory;
    void          *Mapped;
    uint32         Stride;
};

struct render_pipeline_config
{
    const char *ShaderName;

    uint32 VertexStride;
    // Attributes and Resources borrow caller-owned arrays; they must stay alive until CreateRenderPipeline returns
    const VkVertexInputAttributeDescription *Attributes;
    uint32                                   AttributeCount;

    const resource_binding_description *ResourcesDescription;
    uint32                              ResourceDescriptionCount;

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

    VkDescriptorSetLayout SetLayouts[Set_Count];
    VkDescriptorSet       Sets[Set_Count];
    VkPipelineLayout      Layout;
    VkPipeline            Handle;
    VkDescriptorPool      DescriptorPool;
    VkSampler             Sampler;

    pipeline_buffer Buffers[Set_Count];
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
    uint32 uniformBufferAlignment;

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
    render_pipeline *PrimitivePipeline;

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
