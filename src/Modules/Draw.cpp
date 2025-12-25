// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/Draw.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/SwapChain.hpp"
#include "luvk/Modules/Synchronization.hpp"

luvk::Draw::Draw(const std::shared_ptr<Device>&          DeviceModule,
                 const std::shared_ptr<SwapChain>&       SwapChainModule,
                 const std::shared_ptr<Synchronization>& SyncModule)
    : m_DeviceModule(DeviceModule),
      m_SwapChainModule(SwapChainModule),
      m_SyncModule(SyncModule) {}

void luvk::Draw::RecordCommands(const FrameData& Frame, const std::uint32_t ImageIndex)
{
    constexpr VkCommandBufferBeginInfo Begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                             .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    LUVK_EXECUTE(vkBeginCommandBuffer(Frame.CommandBuffer, &Begin));

    std::erase_if(m_PreRenderCallbacks,
                  [&](const DrawCallbackInfo& CB)
                  {
                      return !CB.Callback(Frame.CommandBuffer);
                  });

    const VkRenderPass  RenderPass  = m_SwapChainModule->GetRenderPass();
    const VkFramebuffer FrameBuffer = m_SwapChainModule->GetFramebuffer(ImageIndex);
    const VkExtent2D&   Extent      = m_SwapChainModule->GetExtent();

    const VkRenderPassBeginInfo BeginPass{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                          .renderPass = RenderPass,
                                          .framebuffer = FrameBuffer,
                                          .renderArea = {{0, 0}, {Extent.width, Extent.height}},
                                          .clearValueCount = static_cast<std::uint32_t>(std::size(m_ClearValues)),
                                          .pClearValues = std::data(m_ClearValues)};

    vkCmdBeginRenderPass(Frame.CommandBuffer, &BeginPass, VK_SUBPASS_CONTENTS_INLINE);

    const VkViewport Viewport{0.F, 0.F, static_cast<float>(Extent.width), static_cast<float>(Extent.height), 0.F, 1.F};
    const VkRect2D   Scissor{{0, 0}, Extent};

    vkCmdSetViewport(Frame.CommandBuffer, 0U, 1U, &Viewport);
    vkCmdSetScissor(Frame.CommandBuffer, 0U, 1U, &Scissor);

    std::erase_if(m_DrawCallbacks,
                  [&](const DrawCallbackInfo& CB)
                  {
                      return !CB.Callback(Frame.CommandBuffer);
                  });

    std::erase_if(m_PostRenderCallbacks,
                  [&](const DrawCallbackInfo& CB)
                  {
                      return !CB.Callback(Frame.CommandBuffer);
                  });

    vkCmdEndRenderPass(Frame.CommandBuffer);
    LUVK_EXECUTE(vkEndCommandBuffer(Frame.CommandBuffer));
}

void luvk::Draw::SubmitFrame(FrameData& Frame, const std::uint32_t ImageIndex) const
{
    constexpr VkPipelineStageFlags WaitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    const VkSemaphore Semaphore = m_SyncModule->GetRenderFinished(ImageIndex);

    const VkSubmitInfo Submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .waitSemaphoreCount = 1U,
                              .pWaitSemaphores = &Frame.ImageAvailable,
                              .pWaitDstStageMask = &WaitStages,
                              .commandBufferCount = 1U,
                              .pCommandBuffers = &Frame.CommandBuffer,
                              .signalSemaphoreCount = 1U,
                              .pSignalSemaphores = &Semaphore};

    const VkQueue GraphicsQueue = m_DeviceModule->GetQueue(m_DeviceModule->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT).value());

    LUVK_EXECUTE(vkQueueSubmit(GraphicsQueue, 1U, &Submit, Frame.InFlight));
    Frame.Submitted = true;

    const VkSwapchainKHR Handle = m_SwapChainModule->GetHandle();

    const VkPresentInfoKHR Present{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                   .waitSemaphoreCount = 1U,
                                   .pWaitSemaphores = &Semaphore,
                                   .swapchainCount = 1U,
                                   .pSwapchains = &Handle,
                                   .pImageIndices = &ImageIndex};

    const VkResult PresentResult = vkQueuePresentKHR(GraphicsQueue, &Present);
    if (PresentResult != VK_SUCCESS && PresentResult != VK_SUBOPTIMAL_KHR && PresentResult != VK_ERROR_OUT_OF_DATE_KHR)
    {
        throw std::runtime_error("Present failed");
    }

    m_SyncModule->AdvanceFrame();
}
