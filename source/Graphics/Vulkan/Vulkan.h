#ifndef VULKAN_H
#define VULKAN_H

#include "Types.h"
#include "PlatformAPI.h"
#include "RenderCommands.h"
#include "EngineMath.h"

#include <vulkan/vulkan.h>

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
#define MAX_DESCRIPTOR_BINDINGS 8
#define MAX_OBJECTS           256
#define OBJECT_STRIDE         256

struct vulkan_shader
{
    debug_read_file_result vert;
    debug_read_file_result frag;
    bool32 valid;
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
    Frequency_Global = 0,
    Frequency_PerFrame,
    Frequency_PerMaterial,
    Frequency_PerObject,
    Frequency_Count,
};

struct descriptor_set_slots
{
    VkDescriptorSetLayoutBinding Bindings[MAX_DESCRIPTOR_BINDINGS];
    uint32 BindingCount;
};

struct render_pipeline_config
{
    const char *ShaderName;

    uint32 VertexStride;
    uint32 AttributeCount;
    VkVertexInputAttributeDescription Attributes[MAX_VERTEX_ATTRIBUTES];

    descriptor_set_slots Sets[Frequency_Count];

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
    VkDescriptorSetLayout SetLayouts[Frequency_Count];
    VkPipelineLayout      Layout;
    VkPipeline            Handle;
    VkDescriptorPool      DescriptorPool;
    VkDescriptorSet       CameraSet;
    VkDescriptorSet       ObjectSet;
    VkSampler             Sampler;

    VkBuffer       CameraBuffer;
    VkDeviceMemory CameraBufferMemory;
    void          *CameraMapped;

    VkBuffer       ObjectBuffer;
    VkDeviceMemory ObjectBufferMemory;
    void          *ObjectMapped;
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

    render_pipeline Pipeline;
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

struct cube_push_constants
{
    Matrix4 Model;
};

struct camera_uniforms
{
    Matrix4 ViewProj;
};

struct object_uniforms
{
    v4 Tint;
};

internal void InitVulkan(HINSTANCE hinstance, HWND hwnd);
internal void RenderVulkanFrame(render_commands *Commands);
internal void ShutdownVulkan();

#endif
