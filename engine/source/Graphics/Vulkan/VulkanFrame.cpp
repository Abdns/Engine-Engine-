#include "Vulkan.h"

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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Handle);

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

    real32 aspect = (real32)context->swapchainExtent.width / (real32)context->swapchainExtent.height;

    camera_uniforms *camera = (camera_uniforms *)FrameUniforms(pipeline);
    camera->ViewProj = Mat4Multiply(Mat4Perspective(0.785398f, aspect, 0.1f, 100.0f), Mat4Identity());

    BindPipelineSet(cmd, pipeline, Set_PerFrame);
    BindPipelineSet(cmd, pipeline, Set_PerMaterial);

    uint32 drawIndex     = 0;
    uint32 skippedDraws  = 0;
    uint32 offset        = 0;
    for (render_entry_header *header = NextRenderEntry(commands, &offset); header; header = NextRenderEntry(commands, &offset))
    {
        switch (header->Type)
        {
            case RenderEntry_Camera:
            {
                render_entry_camera *entry = (render_entry_camera *)header;
                Matrix4 proj = Mat4Perspective(entry->FovY, aspect, 0.1f, 100.0f);
                camera->ViewProj = Mat4Multiply(proj, entry->View);
            } break;

            case RenderEntry_Mesh:
            {
                render_entry_mesh *entry = (render_entry_mesh *)header;

                if (drawIndex >= MAX_OBJECTS)
                {
                    skippedDraws++;
                    break;
                }

                if (entry->MeshID >= MAX_MESHES || context->Meshes[entry->MeshID].VertexBuffer == VK_NULL_HANDLE)
                {
                    break;
                }

                uint32 texId = (entry->TextureID < MAX_TEXTURES && context->Textures[entry->TextureID].View) ? entry->TextureID : 0;

                object_uniforms *object = (object_uniforms *)BindNextObjectUniforms(cmd, pipeline, drawIndex);
                object->Tint         = entry->Tint;
                object->TextureIndex = texId;

                primitive_push_constants pc;
                pc.Model = entry->Transform;
                vkCmdPushConstants(cmd, pipeline->Layout, VK_SHADER_STAGE_VERTEX_BIT, 0, (uint32)sizeof(pc), &pc);

                gpu_mesh *mesh = &context->Meshes[entry->MeshID];

                VkBuffer vertexBuffers[] = { mesh->VertexBuffer };
                VkDeviceSize offsets[] = { 0 };

                vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
                vkCmdDraw(cmd, mesh->VertexCount, 1, 0, 0);

                drawIndex++;
            } break;

            case RenderEntry_LoadMesh:
            case RenderEntry_LoadTexture:
            {
            } break;
        }
    }

    if (skippedDraws)
    {
        DebugLog("Draw overflow: %u meshes skipped (max %d objects)\n", skippedDraws, MAX_OBJECTS);
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

internal bool32 DrawFrame(vulkan_context *context, render_commands *commands)
{
    render_pipeline *pipeline = context->PrimitivePipeline;
    if (!pipeline || pipeline->Handle == VK_NULL_HANDLE)
    {
        return false;
    }

    vkWaitForFences(context->device, 1, &context->inFlightFence, VK_TRUE, UINT64_MAX);

    // Descriptor writes into the material set are only safe once the previous frame finished
    ProcessLoadCommands(context, commands);

    uint32 imageIndex = 0;
    VkResult acquire = vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX,
                                             context->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
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
