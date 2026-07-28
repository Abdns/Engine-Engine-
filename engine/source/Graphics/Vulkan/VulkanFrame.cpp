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

    vkCmdSetPrimitiveTopology(cmd, pipeline->Topology);
    vkCmdSetFrontFace(cmd, pipeline->FrontFace);
    vkCmdSetDepthCompareOp(cmd, VK_COMPARE_OP_LESS);

    *wanted = pipeline->DefaultState;
    current->Valid = false;

    ApplyRenderState(context, cmd, current, wanted);
}

internal void RecordCommandBuffer(vulkan_context *context, render_pipeline *pipeline, VkCommandBuffer cmd, uint32 imageIndex, render_commands *commands)
{
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkClearValue clearValues[2] = {};
    clearValues[0].color.float32[0] = 0.05f;
    clearValues[0].color.float32[1] = 0.05f;
    clearValues[0].color.float32[2] = 0.08f;
    clearValues[0].color.float32[3] = 1.0f;
    clearValues[1].depthStencil.depth = 1.0f;
    clearValues[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = context->renderPass;
    renderPassInfo.framebuffer = context->swapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset.x = 0;
    renderPassInfo.renderArea.offset.y = 0;
    renderPassInfo.renderArea.extent = context->swapchainExtent;
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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

    render_state current = {};
    render_state wanted  = {};
    BindPipelineState(context, cmd, pipeline, &current, &wanted);

    real32 aspect = (real32)context->swapchainExtent.width / (real32)context->swapchainExtent.height;
    camera_uniforms *camera = (camera_uniforms *)CameraUniforms(context);

    BindGlobalSet(cmd, context, pipeline);

    vkCmdBindIndexBuffer(cmd, context->IndexPool.Buffer, 0, VK_INDEX_TYPE_UINT32);

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
            } break;

            case Set_Pipeline:
            {
                command_set_pipeline *pipelineCmd = (command_set_pipeline *)cmdBase;

                if ((uint32)pipelineCmd->PipelineType >= MAX_PIPELINES || context->Pipelines[pipelineCmd->PipelineType].Handle == VK_NULL_HANDLE)
                {
                    DebugLog("Set pipeline %d ignored: not ready\n", pipelineCmd->PipelineType);
                    break;
                }

                if ((uint32)pipelineCmd->PipelineType == activeId)
                {
                    break;
                }

                activeId = (uint32)pipelineCmd->PipelineType;
                pipeline = &context->Pipelines[activeId];

                BindPipelineState(context, cmd, pipeline, &current, &wanted);

            } break;

            case Set_RenderState:
            {
                command_set_render_state *stateCmd = (command_set_render_state *)cmdBase;

                wanted.CullMode   = ToVulkanCullMode(stateCmd->CullMode);
                wanted.DepthTest  = stateCmd->DepthTest  ? VK_TRUE : VK_FALSE;
                wanted.DepthWrite = stateCmd->DepthWrite ? VK_TRUE : VK_FALSE;
                wanted.AlphaBlend = (stateCmd->BlendMode == Blend_Alpha) ? VK_TRUE : VK_FALSE;
            } break;

            case Render_Mesh:
            {
                command_render_mesh *meshCmd = (command_render_mesh *)cmdBase;

                mesh_handle *mesh = &meshCmd->Mesh;
                if (!mesh->IndexCount)
                {
                    break;
                }

                ApplyRenderState(context, cmd, &current, &wanted);

                uint32 texId = (meshCmd->Texture.Index < MAX_TEXTURES && context->Textures[meshCmd->Texture.Index].View) ? meshCmd->Texture.Index : 0;

                draw_push_constants pc;
                pc.Model        = meshCmd->Transform;
                pc.Tint         = meshCmd->Tint;
                pc.TextureIndex = texId;
                vkCmdPushConstants(cmd, pipeline->Layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32)sizeof(pc), &pc);

                vkCmdDrawIndexed(cmd, mesh->IndexCount, 1, mesh->FirstIndex, (int32)mesh->FirstVertex, 0);
            } break;
        }
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

internal bool32 DrawFrame(vulkan_context *context, render_commands *commands)
{
    render_pipeline *pipeline = &context->Pipelines[Pipeline_Unlit];
    if (pipeline->Handle == VK_NULL_HANDLE)
    {
        return false;
    }

    vkWaitForFences(context->device, 1, &context->inFlightFence, VK_TRUE, UINT64_MAX);

    ProcessLoadCommands(context, commands);

    uint32 imageIndex = 0;
    VkResult acquire = vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX, context->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return true;
    }

    vkResetFences(context->device, 1, &context->inFlightFence);

    RecordCommandBuffer(context, pipeline, context->commandBuffer, imageIndex, commands);

    VkSemaphore          waitSems[]   = { context->imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore          signalSems[] = { context->renderFinishedSemaphores[imageIndex] };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSems;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &context->commandBuffer;
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
    presentInfo.pImageIndices = &imageIndex;

    VkResult present = vkQueuePresentKHR(context->presentQueue, &presentInfo);
    if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR)
    {
        return true;
    }

    return false;
}
