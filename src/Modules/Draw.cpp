/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/Draw.hpp"
#include "luvk/Interfaces/IServiceLocator.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Renderer.hpp"
#include "luvk/Modules/SwapChain.hpp"
#include "luvk/Modules/Synchronization.hpp"

using namespace luvk;

void Draw::OnInitialize(IServiceLocator* ServiceLocator)
{
    m_ServiceLocator = ServiceLocator;
    IModule::OnInitialize(ServiceLocator);
}

void Draw::AddBeginFrameCallback(DrawCallback&& Callback)
{
    m_BeginFrameCallbacks.push_back(std::move(Callback));
}

void Draw::ClearBeginFrameCallbacks()
{
    m_BeginFrameCallbacks.clear();
}

void Draw::AddDrawCallback(DrawCallback&& Callback)
{
    m_DrawCallbacks.push_back(std::move(Callback));
}

void Draw::ClearDrawCallbacks()
{
    m_DrawCallbacks.clear();
}

void Draw::RenderFrame() const
{
    auto* SyncMod      = m_ServiceLocator->GetModule<Synchronization>();
    auto* SwapChainMod = m_ServiceLocator->GetModule<SwapChain>();
    auto* DeviceMod    = m_ServiceLocator->GetModule<Device>();

    SyncMod->WaitForCurrentFrame();

    FrameData& Frame      = SyncMod->GetCurrentFrame();
    auto       ImageIndex = SwapChainMod->AcquireNextImage(Frame.ImageAvailable);

    if (!ImageIndex.has_value())
    {
        return;
    }

    vkResetCommandBuffer(Frame.CommandBuffer, 0);

    constexpr VkCommandBufferBeginInfo BeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(Frame.CommandBuffer, &BeginInfo);

    for (const auto& Callback : m_BeginFrameCallbacks)
    {
        Callback(Frame.CommandBuffer, SyncMod->GetCurrentFrameIndex());
    }

    VkImage SwapImage = SwapChainMod->GetImages()[ImageIndex.value()];

    const VkImageMemoryBarrier2 TransitionToRender{.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                   .srcStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                   .srcAccessMask    = VK_ACCESS_2_NONE,
                                                   .dstStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                   .dstAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                   .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                                                   .newLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                   .image            = SwapImage,
                                                   .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    const VkDependencyInfo DepInfoRender{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &TransitionToRender};
    vkCmdPipelineBarrier2(Frame.CommandBuffer, &DepInfoRender);

    const VkRenderingAttachmentInfo ColorAttachment{.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                    .imageView   = SwapChainMod->GetViews()[ImageIndex.value()],
                                                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                    .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                    .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                                                    .clearValue  = {.color = {0.0f, 0.0f, 0.0f, 1.0f}}};

    const VkExtent2D      Extent = SwapChainMod->GetExtent();
    const VkRenderingInfo RenderingInfo{.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                        .renderArea           = {.offset = {0, 0}, .extent = Extent},
                                        .layerCount           = 1,
                                        .colorAttachmentCount = 1,
                                        .pColorAttachments    = &ColorAttachment};

    vkCmdBeginRendering(Frame.CommandBuffer, &RenderingInfo);

    const VkViewport Viewport{0.0f, 0.0f, static_cast<float>(Extent.width), static_cast<float>(Extent.height), 0.0f, 1.0f};
    const VkRect2D   Scissor{{0, 0}, Extent};
    vkCmdSetViewport(Frame.CommandBuffer, 0, 1, &Viewport);
    vkCmdSetScissor(Frame.CommandBuffer, 0, 1, &Scissor);

    for (const auto& Callback : m_DrawCallbacks)
    {
        Callback(Frame.CommandBuffer, SyncMod->GetCurrentFrameIndex());
    }

    vkCmdEndRendering(Frame.CommandBuffer);

    const VkImageMemoryBarrier2 TransitionToPresent{.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                    .srcStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                    .srcAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                    .dstStageMask     = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                                    .dstAccessMask    = VK_ACCESS_2_NONE,
                                                    .oldLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                    .newLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                    .image            = SwapImage,
                                                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    const VkDependencyInfo DepInfoPresent{.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                          .imageMemoryBarrierCount = 1,
                                          .pImageMemoryBarriers    = &TransitionToPresent};
    vkCmdPipelineBarrier2(Frame.CommandBuffer, &DepInfoPresent);

    vkEndCommandBuffer(Frame.CommandBuffer);

    const VkCommandBufferSubmitInfo CmdInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = Frame.CommandBuffer};

    const VkSemaphoreSubmitInfo WaitInfo{.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                         .semaphore = Frame.ImageAvailable,
                                         .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};

    const VkSemaphoreSubmitInfo SignalInfo{.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                           .semaphore = Frame.RenderFinished,
                                           .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT};

    const VkSubmitInfo2 Submit{.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                               .waitSemaphoreInfoCount   = 1,
                               .pWaitSemaphoreInfos      = &WaitInfo,
                               .commandBufferInfoCount   = 1,
                               .pCommandBufferInfos      = &CmdInfo,
                               .signalSemaphoreInfoCount = 1,
                               .pSignalSemaphoreInfos    = &SignalInfo};

    if (!LUVK_EXECUTE(vkQueueSubmit2(DeviceMod->GetQueue(VK_QUEUE_GRAPHICS_BIT), 1, &Submit, Frame.InFlight)))
    {
        throw std::runtime_error("Failed to submit queue");
    }

    std::array WaitSemaphores = {Frame.RenderFinished};
    SwapChainMod->Present(ImageIndex.value(), WaitSemaphores);

    SyncMod->AdvanceFrame();
}
