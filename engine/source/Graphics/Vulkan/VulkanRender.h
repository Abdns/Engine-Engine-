#ifndef VULKANRENDER_H
#define VULKANRENDER_H

#include "Types.h"
#include "EngineMath.h"
#include "RenderCommands.h"

#include <vulkan/vulkan.h>

#include "shaders/ShaderInterop.h"

#define MAX_SWAPCHAIN_IMAGES  8
#define MAX_MESHES            256
#define MAX_VERTICES          (1u << 20)
#define MAX_INDICES           (1u << 21)
#define IMAGE_ARENA_SIZE      Megabytes(64)

#define MAX_PASS_DEPENDENCIES 2

#define PIPELINE_TOPOLOGY     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
#define PIPELINE_FRONT_FACE   VK_FRONT_FACE_CLOCKWISE
#define PIPELINE_PUSH_STAGES  (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)

struct vulkan_shader
{
    platform_file_raw vert;
    platform_file_raw frag;
};

struct gpu_buffer
{
    VkDeviceMemory Memory;
    VkBuffer       Buffer;
    uint32         Stride;
    uint32         Capacity;
    uint32         Used;
    void          *Mapped;
};

struct gpu_memory_arena
{
    VkDeviceMemory Memory;
    VkDeviceSize   Capacity;
    VkDeviceSize   Used;
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

    gpu_buffer CameraBuffer;
    gpu_buffer MaterialBuffer;
    gpu_buffer VertexBuffer;
    gpu_buffer IndexBuffer;

    gpu_mesh          Meshes[MAX_MESHES];
    gpu_texture       Textures[MAX_TEXTURES];
    gpu_texture       Cubemaps[MAX_CUBEMAPS];
    material_state    MaterialStates[MAX_MATERIALS];
    gpu_memory_arena  ImageArena;
};

enum pass_id
{
    Pass_Scene = 0,
    Pass_Post,
    Pass_Count,
};

enum pass_sync
{
    Sync_None = 0,
    Sync_WriteThenSample,
    Sync_WriteThenPresent,
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
    bool32 PerSwapchainImage;

    pass_sync Sync;
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

struct pipeline_desc
{
    const char *ShaderName;
    pass_id     Pass;

    VkBool32 DepthTest;
    VkBool32 DepthWrite;
    bool32   OwnSet;
};

struct render_pipeline
{
    pipeline_desc Desc;
    render_state  DefaultState;

    VkPipeline       Handle;
    VkPipelineLayout Layout;
    descriptor_set   Set;
};

struct vulkan_frame
{
    VkCommandBuffer Cmd;
    uint32          ImageIndex;
    bool32          Ready;
    bool32          NeedsResize;
};

#endif
