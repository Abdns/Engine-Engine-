#include "Vulkan.h"

internal VkCullModeFlags ToVulkanCullMode(cull_mode mode)
{
    switch (mode)
    {
        case Cull_Back:  return VK_CULL_MODE_BACK_BIT;
        case Cull_Front: return VK_CULL_MODE_FRONT_BIT;
        case Cull_None:  return VK_CULL_MODE_NONE;
    }

    return VK_CULL_MODE_NONE;
}

internal void ApplyRenderState(vulkan_context *context, VkCommandBuffer cmd, render_state *current, render_state *wanted)
{
    if (!current->Valid || current->CullMode != wanted->CullMode)
    {
        vkCmdSetCullMode(cmd, wanted->CullMode);
        current->CullMode = wanted->CullMode;
    }

    if (!current->Valid || current->DepthTest != wanted->DepthTest)
    {
        vkCmdSetDepthTestEnable(cmd, wanted->DepthTest);
        current->DepthTest = wanted->DepthTest;
    }

    if (!current->Valid || current->DepthWrite != wanted->DepthWrite)
    {
        vkCmdSetDepthWriteEnable(cmd, wanted->DepthWrite);
        current->DepthWrite = wanted->DepthWrite;
    }

    if (context->DynamicBlend && (!current->Valid || current->AlphaBlend != wanted->AlphaBlend))
    {
        VkBool32 enable = wanted->AlphaBlend;
        context->CmdSetColorBlendEnableEXT(cmd, 0, 1, &enable);
        current->AlphaBlend = wanted->AlphaBlend;
    }

    current->Valid = true;
}

internal void BindPipelineState(vulkan_context *context, VkCommandBuffer cmd, render_pipeline *pipeline, render_state *current, render_state *wanted)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Handle);

    if (pipeline->Set.Handle != VK_NULL_HANDLE)
    {
        BindPipelineSet(cmd, &pipeline->Set, pipeline->Layout);
    }

    vkCmdSetPrimitiveTopology(cmd, PIPELINE_TOPOLOGY);
    vkCmdSetFrontFace(cmd, PIPELINE_FRONT_FACE);
    vkCmdSetDepthCompareOp(cmd, VK_COMPARE_OP_LESS);

    *wanted = pipeline->DefaultState;
    current->Valid = false;

    ApplyRenderState(context, cmd, current, wanted);
}

internal void WaitForFrame(vulkan_context *context)
{
    vkWaitForFences(context->device, 1, &context->inFlightFence, VK_TRUE, UINT64_MAX);
}

internal vulkan_frame BeginFrame(vulkan_context *context)
{
    vulkan_frame Frame = {};

    VkResult acquire = vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX, context->imageAvailableSemaphore, VK_NULL_HANDLE, &Frame.ImageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
    {
        Frame.NeedsResize = true;
        return Frame;
    }

    vkResetFences(context->device, 1, &context->inFlightFence);

    VkCommandBuffer cmd = context->commandBuffer;
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width  = (float)context->swapchainExtent.width;
    viewport.height = (float)context->swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = context->swapchainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    Frame.Cmd   = cmd;
    Frame.Ready = true;
    return Frame;
}

internal void ExecuteRenderCommands(vulkan_context *context, VkCommandBuffer cmd, vulkan_resources *res, render_pipeline *pipelines, render_commands *commands)
{
    render_pipeline *pipeline = &pipelines[Pipeline_Unlit];

    render_state current = {};
    render_state wanted  = {};
    BindPipelineState(context, cmd, pipeline, &current, &wanted);

    real32 aspect = (real32)context->swapchainExtent.width / (real32)context->swapchainExtent.height;
    camera_uniforms *camera = (camera_uniforms *)CameraUniforms(res);

    BindGlobalSet(cmd, res, pipeline->Layout);

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

                if (pipelines[pipelineCmd->PipelineType].Handle == VK_NULL_HANDLE)
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

                BindPipelineState(context, cmd, pipeline, &current, &wanted);

            } break;

            case Render_Skybox:
            {
                command_render_skybox *skyCmd = (command_render_skybox *)cmdBase;

                if (activeId != Pipeline_Skybox || !skyCmd->CubemapHandle || skyCmd->CubemapHandle > MAX_CUBEMAPS)
                {
                    break;
                }

                uint32 cubeSlot = skyCmd->CubemapHandle - 1;
                if (!res->Cubemaps[cubeSlot].View)
                {
                    break;
                }

                ApplyRenderState(context, cmd, &current, &wanted);

                draw_push_constants pc;
                pc.Model         = Mat4Identity();
                pc.Tint          = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                pc.MaterialIndex = 0;
                pc.CubemapIndex  = cubeSlot;
                vkCmdPushConstants(cmd, pipeline->Layout, PIPELINE_PUSH_STAGES, 0, (uint32)sizeof(pc), &pc);

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

                uint32 materialSlot = 0;
                if (meshCmd->MaterialHandle && meshCmd->MaterialHandle <= MAX_MATERIALS)
                {
                    materialSlot = meshCmd->MaterialHandle - 1;
                }

                material_state *material = &res->MaterialStates[materialSlot];

                if ((uint32)material->Pipeline != activeId)
                {
                    if (pipelines[material->Pipeline].Handle == VK_NULL_HANDLE)
                    {
                        break;
                    }

                    activeId = (uint32)material->Pipeline;
                    pipeline = &pipelines[activeId];

                    BindPipelineState(context, cmd, pipeline, &current, &wanted);
                }

                wanted.CullMode   = ToVulkanCullMode(material->CullMode);
                wanted.DepthTest  = material->DepthTest  ? VK_TRUE : VK_FALSE;
                wanted.DepthWrite = material->DepthWrite ? VK_TRUE : VK_FALSE;
                wanted.AlphaBlend = (material->BlendMode == Blend_Alpha) ? VK_TRUE : VK_FALSE;

                ApplyRenderState(context, cmd, &current, &wanted);

                draw_push_constants pc;
                pc.Model         = meshCmd->Transform;
                pc.Tint          = meshCmd->Tint;
                pc.MaterialIndex = materialSlot;
                pc.CubemapIndex  = 0;
                vkCmdPushConstants(cmd, pipeline->Layout, PIPELINE_PUSH_STAGES, 0, (uint32)sizeof(pc), &pc);

                vkCmdDrawIndexed(cmd, mesh->IndexCount, 1, mesh->FirstIndex, (int32)mesh->FirstVertex, 0);
            } break;
        }
    }
}

internal void ExecuteUICommands(vulkan_context *context, VkCommandBuffer cmd, render_pipeline *pipelines, render_commands *commands)
{
    render_pipeline *pipeline = &pipelines[Pipeline_UIRect];
    if (pipeline->Handle == VK_NULL_HANDLE)
    {
        return;
    }

    real32 width  = (real32)context->swapchainExtent.width;
    real32 height = (real32)context->swapchainExtent.height;

    render_state current = {};
    render_state wanted  = {};
    bool32 bound = false;

    uint32 offset = 0;
    for (command_type *cmdBase = NextRenderCommand(commands, &offset); cmdBase; cmdBase = NextRenderCommand(commands, &offset))
    {
        if (*cmdBase != Render_Rect)
        {
            continue;
        }

        command_render_rect *rectCmd = (command_render_rect *)cmdBase;

        if (!bound)
        {
            BindPipelineState(context, cmd, pipeline, &current, &wanted);
            bound = true;
        }

        draw_push_constants pc;
        pc.Model         = Mat4Identity();
        pc.Tint          = rectCmd->Color;
        pc.Rect          = Vector4(rectCmd->Min.X / width  * 2.0f - 1.0f,
                                   rectCmd->Min.Y / height * 2.0f - 1.0f,
                                   rectCmd->Max.X / width  * 2.0f - 1.0f,
                                   rectCmd->Max.Y / height * 2.0f - 1.0f);
        pc.MaterialIndex = 0;
        pc.CubemapIndex  = 0;
        vkCmdPushConstants(cmd, pipeline->Layout, PIPELINE_PUSH_STAGES, 0, (uint32)sizeof(pc), &pc);

        vkCmdDraw(cmd, 6, 1, 0, 0);
    }
}

internal void DrawFullscreen(vulkan_context *context, VkCommandBuffer cmd, render_pipeline *pipeline)
{
    if (pipeline->Handle == VK_NULL_HANDLE)
    {
        return;
    }

    render_state current = {};
    render_state wanted  = {};
    BindPipelineState(context, cmd, pipeline, &current, &wanted);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}

internal bool32 EndFrame(vulkan_context *context, vulkan_frame *Frame)
{
    vkEndCommandBuffer(Frame->Cmd);

    VkSemaphore          waitSems[]   = { context->imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore          signalSems[] = { context->renderFinishedSemaphores[Frame->ImageIndex] };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSems;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &Frame->Cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSems;

    if (vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, context->inFlightFence) != VK_SUCCESS)
    {
        DebugLog("Fail to submit draw command buffer\n");
        return false;
    }

    VkSwapchainKHR swapchains[] = { context->swapchain };
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSems;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &Frame->ImageIndex;

    VkResult present = vkQueuePresentKHR(context->presentQueue, &presentInfo);
    if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR)
    {
        return true;
    }

    return false;
}
