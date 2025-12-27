/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/Draw.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Synchronization.hpp"

luvk::Draw::Draw(const std::shared_ptr<Device>& DeviceModule, const std::shared_ptr<Synchronization>& SyncModule)
    : m_DeviceModule(DeviceModule),
      m_SyncModule(SyncModule) {}

void luvk::Draw::RecordCommands(const FrameData& Frame, const RenderTarget& Target)
{
    constexpr VkCommandBufferBeginInfo Begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                             .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    if (!LUVK_EXECUTE(vkBeginCommandBuffer(Frame.CommandBuffer, &Begin)))
    {
        throw std::runtime_error("Failed to begin command buffer");
    }

    std::erase_if(m_BeginFrameCallbacks,
                  [&](const DrawCallbackInfo& Info)
                  {
                      return !Info.Callback(Frame.CommandBuffer);
                  });

    const VkRenderPass  RenderPass  = Target.RenderPass;
    const VkFramebuffer FrameBuffer = Target.Framebuffer;
    const VkExtent2D&   Extent      = Target.Extent;

    const VkRenderPassBeginInfo BeginPass{.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                          .renderPass  = RenderPass,
                                          .framebuffer = FrameBuffer,
                                          .renderArea  = {{0,
                                                           0},
                                                          {Extent.width,
                                                           Extent.height}},
                                          .clearValueCount = static_cast<std::uint32_t>(std::size(Target.ClearValues)),
                                          .pClearValues    = std::data(Target.ClearValues)};

    vkCmdBeginRenderPass(Frame.CommandBuffer, &BeginPass, VK_SUBPASS_CONTENTS_INLINE);

    const VkViewport Viewport{0.F,
                              0.F,
                              static_cast<float>(Extent.width),
                              static_cast<float>(Extent.height),
                              0.F,
                              1.F};
    const VkRect2D Scissor{{0,
                            0},
                           Extent};

    vkCmdSetViewport(Frame.CommandBuffer, 0U, 1U, &Viewport);
    vkCmdSetScissor(Frame.CommandBuffer, 0U, 1U, &Scissor);

    std::erase_if(m_RecordFrameCallbacks,
                  [&](const DrawCallbackInfo& Info)
                  {
                      return !Info.Callback(Frame.CommandBuffer);
                  });

    vkCmdEndRenderPass(Frame.CommandBuffer);

    if (!LUVK_EXECUTE(vkEndCommandBuffer(Frame.CommandBuffer)))
    {
        throw std::runtime_error("Failed to end command buffer");
    }
}

void luvk::Draw::SubmitFrame(FrameData& Frame, const std::uint32_t ImageIndex) const
{
    constexpr VkPipelineStageFlags WaitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    const VkSemaphore Semaphore = m_SyncModule->GetRenderFinished(ImageIndex);

    const VkSubmitInfo Submit{.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .waitSemaphoreCount   = 1U,
                              .pWaitSemaphores      = &Frame.ImageAvailable,
                              .pWaitDstStageMask    = &WaitStages,
                              .commandBufferCount   = 1U,
                              .pCommandBuffers      = &Frame.CommandBuffer,
                              .signalSemaphoreCount = 1U,
                              .pSignalSemaphores    = &Semaphore};

    const VkQueue GraphicsQueue = m_DeviceModule->GetQueue(VK_QUEUE_GRAPHICS_BIT);

    if (!LUVK_EXECUTE(vkQueueSubmit(GraphicsQueue, 1U, &Submit, Frame.InFlight)))
    {
        throw std::runtime_error("Failed to submit graphics queue");
    }

    Frame.Submitted = true;
    m_SyncModule->AdvanceFrame();
}
