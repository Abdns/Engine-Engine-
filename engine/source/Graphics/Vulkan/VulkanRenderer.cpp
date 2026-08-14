#include "Vulkan.h"

#include "VulkanCore.cpp"
#include "VulkanDevice.cpp"
#include "VulkanResources.cpp"
#include "VulkanPasses.cpp"
#include "VulkanPipeline.cpp"
#include "VulkanFrame.cpp"

global_variable gpu_texture     SceneTarget;
global_variable gpu_texture     PostTarget;
global_variable render_pass     Passes[Pass_Count];
global_variable render_pipeline Pipelines[Pipeline_Count];

global_variable pipeline_desc PipelineDescs[] =
{
    { "unlit",  VK_TRUE,  VK_TRUE,  VK_FALSE },
    { "skybox", VK_FALSE, VK_FALSE, VK_FALSE },
    { "post",   VK_FALSE, VK_FALSE, VK_FALSE },
    { "UI",     VK_FALSE, VK_FALSE, VK_FALSE },
    { "uirect", VK_FALSE, VK_FALSE, VK_TRUE  },
};

static_assert(ArrayCount(PipelineDescs) == Pipeline_Count, "PipelineDescs must describe every pipeline_type");

internal void CreateFramePasses(vulkan_context *context)
{
    SceneTarget = CreateRenderTarget(context, VK_FORMAT_R16G16B16A16_SFLOAT);
    PostTarget  = CreateRenderTarget(context, VK_FORMAT_R16G16B16A16_SFLOAT);

    Passes[Pass_Scene] = CreateScenePass(context, SceneTarget.View);
    Passes[Pass_Post]  = CreatePostPass(context, PostTarget.View);
    Passes[Pass_UI]    = CreateUIPass(context, context->swapchainImageViews, context->swapchainImageCount);

    VkCommandBuffer cmd = BeginSingleTimeCommands(context);
    CmdImageToGeneral(cmd, SceneTarget.Image,     VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0);
    CmdImageToGeneral(cmd, PostTarget.Image,      VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0);
    CmdImageToGeneral(cmd, context->depth.Image,  VK_IMAGE_ASPECT_DEPTH_BIT, 1, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0);
    EndSingleTimeCommands(context, cmd);

}

internal void DestroyFramePasses(vulkan_context *context)
{
    Passes[Pass_Scene] = {};
    Passes[Pass_Post]  = {};
    Passes[Pass_UI]    = {};

    DestroyTexture(context, &SceneTarget);
    DestroyTexture(context, &PostTarget);
}

internal void WriteRenderTargetsToHeap(vulkan_context *context)
{
    WriteImageDescriptor(context, &GlobalResources.Heap, GlobalResources.Heap.TextureOffset, TEXTURE_SLOT_SCENE, SceneTarget.View);
    WriteImageDescriptor(context, &GlobalResources.Heap, GlobalResources.Heap.TextureOffset, TEXTURE_SLOT_POST,  PostTarget.View);
}

internal void ResizeRenderer(vulkan_context *context)
{
    if (!RecreateSwapchain(context))
    {
        return;
    }

    DestroyFramePasses(context);
    CreateFramePasses(context);
    WriteRenderTargetsToHeap(context);
}

internal const char *InitVulkan(HINSTANCE hinstance, HWND hwnd)
{
    vulkan_context *context = &GlobalVulkan;
    context->windowHandle = hwnd;

    if (!CheckInstanceVersion())
    {
        return "This machine has no Vulkan 1.3 loader, update the graphics drivers";
    }

    if (!CheckInstanceExtensionSupport(RequiredInstanceExtensions, ArrayCount(RequiredInstanceExtensions)))
    {
        return "This machine is missing the required Vulkan instance extensions";
    }

    const char *instanceExtensions[8];
    uint32      instanceExtensionCount = GatherInstanceExtensions(instanceExtensions, ArrayCount(instanceExtensions));

    VkApplicationInfo appInfo = AppInfo();
    VkInstanceCreateInfo instanceInfo = InstanceInfo(&appInfo, instanceExtensions, instanceExtensionCount);

#if ENGINE_INTERNAL
    VkDebugUtilsMessengerCreateInfoEXT messengerInfo = DebugMessengerInfo();

    if (CheckInstanceLayerSupport(ValidationLayers, (uint32)ArrayCount(ValidationLayers)))
    {
        instanceInfo.enabledLayerCount   = (uint32)ArrayCount(ValidationLayers);
        instanceInfo.ppEnabledLayerNames = ValidationLayers;
        instanceInfo.pNext               = &messengerInfo;
    }
#endif

    if (vkCreateInstance(&instanceInfo, nullptr, &context->instance) != VK_SUCCESS)
    {
        return "Vulkan instance could not be created";
    }

    DebugLog("Vulkan instance created\n");

#if ENGINE_INTERNAL
    if (instanceInfo.enabledLayerCount)
    {
        CreateDebugMessenger(context);
    }
#endif

    CreateSurface(context, hinstance, hwnd);
    SelectDevice(context);

    if (!context->physicalDevice)
    {
        return "No GPU on this machine supports the features the renderer needs";
    }

    CreateLogicalDevice(context);
    CreateSwapchain(context, hwnd);
    CreateSwapchainImageViews(context);
    CreateDepthResources(context);
    CreateCommandPool(context);
    CreateCommandBuffer(context);
    CreateSyncObjects(context);

    CreateResources(context, &GlobalResources);
    CreateDescriptorHeap(context, &GlobalResources);
    WriteSamplerDescriptor(context, &GlobalResources.Heap, GlobalResources.Heap.SamplerOffset, GlobalResources.Sampler);

    CreateFramePasses(context);

    for (uint32 i = 0; i < Pipeline_Count; ++i)
    {
        CreateRenderPipeline(context, &GlobalResources, &Pipelines[i], &PipelineDescs[i]);
    }

    WriteRenderTargetsToHeap(context);

    DebugLog("Vulkan ready\n");
    return nullptr;
}

internal void LoadAssets(vulkan_context *context, vulkan_resources *res, render_commands *commands)
{
    if (!commands->LoadCount)
    {
        return;
    }

    uint32 vertexCount   = 0;
    uint32 indexCount    = 0;
    uint32 materialCount = 0;

    uint32 offset = 0;
    for (command_type *header = NextRenderCommand(commands, &offset); header; header = NextRenderCommand(commands, &offset))
    {
        if (*header == Load_Mesh)
        {
            command_load_mesh *entry = (command_load_mesh *)header;
            vertexCount += entry->VertexCount;
            indexCount  += entry->IndexCount;
        }
        else if (*header == Load_Material)
        {
            command_load_material *entry = (command_load_material *)header;
            if (entry->MaterialHandle >= materialCount)
            {
                materialCount = entry->MaterialHandle + 1;
            }
        }
    }

    CreateAssetBuffers(context, res, vertexCount, indexCount, materialCount);

    shared_buffer   staging = CreateUploadBuffer(context, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, STAGING_MEMORY_SIZE);
    VkCommandBuffer cmd     = BeginSingleTimeCommands(context);

    offset = 0;
    for (command_type *header = NextRenderCommand(commands, &offset); header; header = NextRenderCommand(commands, &offset))
    {
        if (*header == Load_Mesh)
        {
            command_load_mesh *entry = (command_load_mesh *)header;

            shared_alloc vertices = SharedBufferWrite(&res->VertexBuffer, entry->Vertices, entry->VertexCount * sizeof(vertex), sizeof(vertex));
            shared_alloc indices  = SharedBufferWrite(&res->IndexBuffer,  entry->Indices,  entry->IndexCount  * sizeof(uint32), sizeof(uint32));

            CreateMesh(res, entry->MeshHandle, vertices.Offset, entry->VertexCount, indices.Offset, entry->IndexCount);
        }
        else if (*header == Load_Texture)
        {
            command_load_texture *entry = (command_load_texture *)header;

            VkDeviceSize imageSize = (VkDeviceSize)entry->Width * entry->Height * TextureFormatBytes(entry->Format);

            shared_alloc    upload  = SharedBufferWrite(&staging, entry->Pixels, imageSize, 16);
            gpu_texture *texture = CreateTexture(context, res, entry->TextureHandle, entry->Width, entry->Height, entry->SRGB, entry->Format);

            CmdUploadImage(cmd, staging.Buffer, upload.Offset, texture->Image, entry->Width, entry->Height, 1);
            WriteImageDescriptor(context, &res->Heap, res->Heap.TextureOffset, entry->TextureHandle, texture->View);
        }
        else if (*header == Load_Cubemap)
        {
            command_load_cubemap *entry = (command_load_cubemap *)header;

            VkDeviceSize imageSize = (VkDeviceSize)entry->FaceSize * entry->FaceSize * 6 * TextureFormatBytes(entry->Format);

            shared_alloc    upload = SharedBufferWrite(&staging, entry->Pixels, imageSize, 16);
            gpu_texture *cube   = CreateCubemap(context, res, entry->CubemapHandle, entry->FaceSize, entry->Format);

            CmdUploadImage(cmd, staging.Buffer, upload.Offset, cube->Image, entry->FaceSize, entry->FaceSize, 6);
            WriteImageDescriptor(context, &res->Heap, res->Heap.CubemapOffset, entry->CubemapHandle, cube->View);
        }
    }

    offset = 0;
    for (command_type *header = NextRenderCommand(commands, &offset); header; header = NextRenderCommand(commands, &offset))
    {
        if (*header == Load_Material)
        {
            command_load_material *entry = (command_load_material *)header;
            WriteMaterial(res, entry);
        }
    }

    EndSingleTimeCommands(context, cmd);

    DestroySharedBuffer(context, &staging);

    commands->LoadCount = 0;
}

internal void ExecuteRenderCommands(vulkan_context *context, VkCommandBuffer cmd, vulkan_resources *res, render_pipeline *pipelines, render_commands *commands)
{
    render_pipeline *pipeline = &pipelines[Pipeline_Unlit];

    render_state current = {};
    render_state wanted  = {};
    BindPipelineState(context, cmd, res, pipeline, &current, &wanted);

    real32 aspect = (real32)context->swapchainExtent.width / (real32)context->swapchainExtent.height;

    shared_alloc cameraAlloc = SharedBufferAlloc(&res->FrameArena, sizeof(camera_uniforms), 16);
    camera_uniforms *camera = (camera_uniforms *)cameraAlloc.Cpu;

    vkCmdBindIndexBuffer(cmd, res->IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

    uint32 activeId = Pipeline_Unlit;
    uint32 offset   = 0;
    for (command_type *cmdBase = NextRenderCommand(commands, &offset); cmdBase; cmdBase = NextRenderCommand(commands, &offset))
    {
        switch (*cmdBase)
        {
            case Render_Camera:
            {
                command_render_camera *cameraCmd = (command_render_camera *)cmdBase;
                Matrix4 proj = Mat4Perspective(cameraCmd->FovY, aspect, 0.1f, 100.0f);
                camera->ViewProj = Mat4Multiply(proj, cameraCmd->View);

                Matrix4 *view = &cameraCmd->View;
                real32 rightScale = 1.0f / proj.Elements[0][0];
                real32 upScale    = 1.0f / proj.Elements[1][1];

                camera->SkyRight   = Vector4(view->Elements[0][0] * rightScale, view->Elements[1][0] * rightScale, view->Elements[2][0] * rightScale, 0.0f);
                camera->SkyUp      = Vector4(view->Elements[0][1] * upScale,    view->Elements[1][1] * upScale,    view->Elements[2][1] * upScale,    0.0f);
                camera->SkyForward = Vector4(-view->Elements[0][2], -view->Elements[1][2], -view->Elements[2][2], 0.0f);
            } break;

            case Set_Pipeline:
            {
                command_set_pipeline *pipelineCmd = (command_set_pipeline *)cmdBase;

                if (pipelines[pipelineCmd->PipelineType].Vert == VK_NULL_HANDLE)
                {
                    DebugLog("Set pipeline %d ignored: not ready\n", pipelineCmd->PipelineType);
                    break;
                }

                if ((uint32)pipelineCmd->PipelineType == activeId)
                {
                    break;
                }

                activeId = (uint32)pipelineCmd->PipelineType;
                pipeline = &pipelines[activeId];

                BindPipelineState(context, cmd, res, pipeline, &current, &wanted);

            } break;

            case Render_Skybox:
            {
                command_render_skybox *skyCmd = (command_render_skybox *)cmdBase;

                if (activeId != Pipeline_Skybox || skyCmd->CubemapHandle >= MAX_CUBEMAPS)
                {
                    break;
                }

                uint32 cubeSlot = skyCmd->CubemapHandle;
                if (!res->Cubemaps[cubeSlot].View)
                {
                    break;
                }

                ApplyRenderState(context, cmd, &current, &wanted);

                shared_alloc alloc = SharedBufferAlloc(&res->FrameArena, sizeof(skybox_params), 16);

                skybox_params params = {};
                params.Tint         = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                params.Camera       = cameraAlloc.Gpu;
                params.CubemapIndex = cubeSlot;

                *(skybox_params *)alloc.Cpu = params;

                BindParams(cmd, pipeline, alloc.Gpu);

                vkCmdDraw(cmd, 3, 1, 0, 0);
            } break;

            case Render_Mesh:
            {
                command_render_mesh *meshCmd = (command_render_mesh *)cmdBase;

                gpu_mesh *mesh = ResolveMesh(res, meshCmd->MeshHandle);
                if (!mesh || !mesh->IndexCount)
                {
                    break;
                }

                uint32 materialSlot = meshCmd->MaterialHandle;
                Assert(materialSlot < res->MaterialCount);

                material_state *material = &res->MaterialStates[materialSlot];

                if ((uint32)material->Pipeline != activeId)
                {
                    if (pipelines[material->Pipeline].Vert == VK_NULL_HANDLE)
                    {
                        break;
                    }

                    activeId = (uint32)material->Pipeline;
                    pipeline = &pipelines[activeId];

                    BindPipelineState(context, cmd, res, pipeline, &current, &wanted);
                }

                wanted.CullMode   = ToVulkanCullMode(material->CullMode);
                wanted.DepthTest  = material->DepthTest  ? VK_TRUE : VK_FALSE;
                wanted.DepthWrite = material->DepthWrite ? VK_TRUE : VK_FALSE;
                wanted.AlphaBlend = (material->BlendMode == Blend_Alpha) ? VK_TRUE : VK_FALSE;

                ApplyRenderState(context, cmd, &current, &wanted);

                shared_alloc alloc = SharedBufferAlloc(&res->FrameArena, sizeof(draw_params), 16);

                draw_params params = {};
                params.Model         = meshCmd->Transform;
                params.Tint          = meshCmd->Tint;
                params.Camera        = cameraAlloc.Gpu;
                params.Vertices      = res->VertexBuffer.Address;
                params.Materials    = res->MaterialBuffer.Address;
                params.MaterialSlot = materialSlot;

                *(draw_params *)alloc.Cpu = params;

                BindParams(cmd, pipeline, alloc.Gpu);

                vkCmdDrawIndexed(cmd, mesh->IndexCount, 1, mesh->FirstIndex, (int32)mesh->FirstVertex, 0);
            } break;
        }
    }
}

internal void ExecuteUICommands(vulkan_context *context, VkCommandBuffer cmd, vulkan_resources *res, render_pipeline *pipelines, render_commands *commands)
{
    render_pipeline *pipeline = &pipelines[Pipeline_UIRect];
    if (pipeline->Vert == VK_NULL_HANDLE)
    {
        return;
    }

    real32 width  = (real32)context->swapchainExtent.width;
    real32 height = (real32)context->swapchainExtent.height;

    uint32 rectCount = 0;
    uint32 offset    = 0;
    for (command_type *cmdBase = NextRenderCommand(commands, &offset); cmdBase; cmdBase = NextRenderCommand(commands, &offset))
    {
        if (*cmdBase == Render_Rect)
        {
            ++rectCount;
        }
    }

    if (!rectCount)
    {
        return;
    }

    shared_alloc alloc = SharedBufferAlloc(&res->FrameArena, rectCount * sizeof(rect_params), 16);

    rect_params *params = (rect_params *)alloc.Cpu;
    uint32       index  = 0;

    offset = 0;
    for (command_type *cmdBase = NextRenderCommand(commands, &offset); cmdBase; cmdBase = NextRenderCommand(commands, &offset))
    {
        if (*cmdBase != Render_Rect)
        {
            continue;
        }

        command_render_rect *rectCmd = (command_render_rect *)cmdBase;

        rect_params entry = {};
        entry.Rect        = Vector4(rectCmd->Min.X / width  * 2.0f - 1.0f,
                                    rectCmd->Min.Y / height * 2.0f - 1.0f,
                                    rectCmd->Max.X / width  * 2.0f - 1.0f,
                                    rectCmd->Max.Y / height * 2.0f - 1.0f);
        entry.UVRect      = rectCmd->UV;
        entry.Tint        = rectCmd->Color;
        entry.TextureSlot = rectCmd->TextureSlot;

        params[index++] = entry;
    }

    render_state current = {};
    render_state wanted  = {};
    BindPipelineState(context, cmd, res, pipeline, &current, &wanted);

    BindParams(cmd, pipeline, alloc.Gpu);

    vkCmdDraw(cmd, 6, rectCount, 0, 0);
}

internal void RenderVulkanFrame(render_commands *Commands)
{
    vulkan_context *context = &GlobalVulkan;

    if (Pipelines[Pipeline_Unlit].Vert == VK_NULL_HANDLE)
    {
        return;
    }

    WaitForFrame(context);

    LoadAssets(context, &GlobalResources, Commands);

    vulkan_frame Frame = BeginFrame(context, &GlobalResources);
    if (!Frame.Ready)
    {
        if (Frame.NeedsResize)
        {
            ResizeRenderer(context);
        }
        return;
    }

    VkImage swapchainImage = context->swapchainImages[Frame.ImageIndex];

    BeginPass(context, Frame.Cmd, &Passes[Pass_Scene], context->swapchainExtent, Frame.ImageIndex);
    ExecuteRenderCommands(context, Frame.Cmd, &GlobalResources, Pipelines, Commands);
    EndPass(Frame.Cmd);

    GpuBarrier(Frame.Cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

    BeginPass(context, Frame.Cmd, &Passes[Pass_Post], context->swapchainExtent, Frame.ImageIndex);
    DrawFullscreen(context, Frame.Cmd, &GlobalResources, &Pipelines[Pipeline_Post], TEXTURE_SLOT_SCENE);
    EndPass(Frame.Cmd);

    GpuBarrier(Frame.Cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    CmdImageToGeneral(Frame.Cmd, swapchainImage, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    BeginPass(context, Frame.Cmd, &Passes[Pass_UI], context->swapchainExtent, Frame.ImageIndex);
    DrawFullscreen(context, Frame.Cmd, &GlobalResources, &Pipelines[Pipeline_UI], TEXTURE_SLOT_POST);
    ExecuteUICommands(context, Frame.Cmd, &GlobalResources, Pipelines, Commands);
    EndPass(Frame.Cmd);

    CmdImageToPresent(Frame.Cmd, swapchainImage);

    EndFrame(context, &Frame);

    if (Frame.NeedsResize)
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

        for (uint32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroySemaphore(context->device, context->imageAvailableSemaphores[i], nullptr);
        }
        for (uint32 i = 0; i < MAX_SWAPCHAIN_IMAGES; ++i)
        {
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], nullptr);
        }
        vkDestroySemaphore(context->device, context->frameTimeline, nullptr);

        vkDestroyCommandPool(context->device, context->commandPool, nullptr);

        DestroyFramePasses(context);

        for (uint32 i = 0; i < Pipeline_Count; ++i)
        {
            if (Pipelines[i].Vert != VK_NULL_HANDLE)
            {
                DestroyRenderPipeline(context, &Pipelines[i]);
            }
        }

        DestroyDescriptorHeap(context, &GlobalResources);
        DestroyResources(context, &GlobalResources);

        DestroySwapchainResources(context);

        vkDestroySwapchainKHR(context->device, context->swapchain, nullptr);
        vkDestroyDevice(context->device, nullptr);
    }

    if (context->instance != VK_NULL_HANDLE)
    {
        if (context->surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(context->instance, context->surface, nullptr);
        }

#if ENGINE_INTERNAL
        DestroyDebugMessenger(context);
#endif

        vkDestroyInstance(context->instance, nullptr);
    }
}
