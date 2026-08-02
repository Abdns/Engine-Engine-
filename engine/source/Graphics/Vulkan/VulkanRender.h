#ifndef VULKANRENDER_H
#define VULKANRENDER_H

#include "Types.h"
#include "EngineMath.h"
#include "RenderCommands.h"

#include <vulkan/vulkan.h>

#include "shaders/ShaderInterop.h"

#define MAX_SWAPCHAIN_IMAGES  8
#define MAX_MESHES            256
#define MAX_POOL_VERTICES     (1u << 20)
#define MAX_POOL_INDICES      (1u << 21)
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
    uint32         Used;
};

struct gpu_mesh
{
    uint32 FirstVertex;
    uint32 VertexCount;
    uint32 FirstIndex;
    uint32 IndexCount;
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

struct material_state
{
    pipeline_type Pipeline;
    cull_mode     CullMode;
    blend_mode    BlendMode;
    bool32        DepthTest;
    bool32        DepthWrite;
};

struct vulkan_resources
{
    VkDescriptorPool DescriptorPool;
    descriptor_set   GlobalSet;
    VkSampler        Sampler;

    gpu_pool CameraBuffer;
    gpu_pool MaterialBuffer;
    gpu_pool VertexPool;
    gpu_pool IndexPool;

    gpu_mesh          Meshes[MAX_MESHES];
    gpu_texture       Textures[MAX_TEXTURES];
    gpu_texture       Cubemaps[MAX_CUBEMAPS];
    material_state    MaterialStates[MAX_MATERIALS];
    image_memory_pool ImagePool;
};

enum pass_id
{
    Pass_Scene = 0,
    Pass_Post,
    Pass_Count,
};

struct render_target
{
    VkImage        Image;
    VkDeviceMemory Memory;
    VkImageView    View;
};

struct pass_desc
{
    const char *Name;

    VkFormat            ColorFormat;
    VkAttachmentLoadOp  ColorLoad;
    VkAttachmentStoreOp ColorStore;
    VkImageLayout       ColorFinalLayout;
    Vector4             ClearColor;

    bool32 UseDepth;
};

struct render_pass
{
    pass_desc Desc;

    VkRenderPass  Handle;
    VkFramebuffer Framebuffers[MAX_SWAPCHAIN_IMAGES];
    uint32        FramebufferCount;
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

struct vulkan_frame
{
    VkCommandBuffer Cmd;
    uint32          ImageIndex;
    bool32          Ready;
    bool32          NeedsResize;
};

#endif
