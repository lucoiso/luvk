// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Upload.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/Vulkan.hpp"
#include "luvk/Render/CommandList.hpp"
#include "luvk/Resources/Buffer.hpp"
#include "../Resources/ImageInternals.hpp"
#include "luvk/Resources/Image.hpp"

luvk::UploadManager::UploadManager(Context& Ctx, const std::uint32_t BatchCount)
    : m_Context(Ctx)
{
    m_Batches.resize(std::max(1U, BatchCount));

    const VkCommandPoolCreateInfo PoolInfo{.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                           .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                           .queueFamilyIndex = Ctx.TransferQueue().Family()};

    try
    {
        for (Batch& It : m_Batches)
        {
            Check(vkCreateCommandPool(Ctx.Device(), &PoolInfo, nullptr, &It.Pool), "vkCreateCommandPool (upload)");

            const VkCommandBufferAllocateInfo AllocateInfo{.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                           .commandPool        = It.Pool,
                                                           .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                           .commandBufferCount = 1U};
            Check(vkAllocateCommandBuffers(Ctx.Device(), &AllocateInfo, &It.Commands), "vkAllocateCommandBuffers (upload)");
            Ctx.SetDebugName(It.Commands, VK_OBJECT_TYPE_COMMAND_BUFFER, "luvk upload commands");
        }
    }
    catch (...)
    {
        DestroyBatches();
        throw;
    }
}

luvk::UploadManager::~UploadManager()
{

    DestroyBatches();
}

void luvk::UploadManager::DestroyBatches() noexcept
{
    for (Batch& It : m_Batches)
    {
        if (It.Pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_Context.Device(), It.Pool, nullptr);
            It.Pool     = VK_NULL_HANDLE;
            It.Commands = VK_NULL_HANDLE;
        }
    }
}

luvk::UploadManager::StagedRange luvk::UploadManager::StageLocked(const std::span<const std::byte> Data, const VkDeviceSize Alignment)
{
    Batch&             Current = m_Batches[m_Current];
    const VkDeviceSize Offset  = (Current.StagingUsed + Alignment - 1U) / Alignment * Alignment;

    if (Current.Staging && Offset + std::size(Data) <= Current.Staging.Size())
    {
        Current.Staging.Write(Data, Offset);
        Current.StagingUsed = Offset + std::size(Data);
        return {.Buffer = Current.Staging.Handle(), .Offset = Offset};
    }

    if (Current.StagingUsed == 0U)
    {
        // Nothing staged in this batch yet (its previous submission completed): grow the batch's staging ring.
        Current.Staging     = Buffer(m_Context,
                                     {.Size   = std::max(StagingBlockSize, std::bit_ceil(static_cast<VkDeviceSize>(std::size(Data)))),
                                      .Memory = MemoryUsage::Upload,
                                      .Name   = "luvk staging"});
        Current.Staging.Write(Data);
        Current.StagingUsed = std::size(Data);
        return {.Buffer = Current.Staging.Handle(), .Offset = 0U};
    }

    // The batch's ring is full: a one-off buffer released together with the batch.
    Buffer OneOff(m_Context, {.Size = std::size(Data), .Memory = MemoryUsage::Upload, .Name = "luvk staging (overflow)"});
    OneOff.Write(Data);
    const StagedRange Range{.Buffer = OneOff.Handle(), .Offset = 0U};
    OneOff.Release(TimelinePoint{.Graphics = 0U, .Transfer = m_Context.TransferQueue().NextValue()});
    return Range;
}

void luvk::UploadManager::Upload(Buffer& Destination, const std::span<const std::byte> Data, const VkDeviceSize Offset)
{
    if (!Destination)
    {
        throw std::logic_error("[luvk] Upload into an empty Buffer.");
    }

    if (Offset > Destination.Size() || std::size(Data) > Destination.Size() - Offset)
    {
        throw std::out_of_range("[luvk] Upload out of range.");
    }

    if (std::empty(Data))
    {
        return;
    }

    std::scoped_lock      Lock(m_Mutex);
    const VkCommandBuffer Commands = BeginLocked();
    const StagedRange     Source   = StageLocked(Data, 16U);
    SerializeCopiesLocked(Commands);
    m_WaitGraphicsValue = std::max(m_WaitGraphicsValue, Destination.State().LastGraphicsUse);

#ifndef NDEBUG
    if (Destination.State().ImportedInFrame != 0U && Destination.State().ImportedInFrame == m_Context.RecordingFrame())
    {
        m_Context.EmitMessage(DebugSeverity::Warning, "[luvk] Buffer write after the frame being recorded imported the buffer: that frame sees the new data.");
    }
#endif

    const VkBufferCopy2 Region{.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2, .srcOffset = Source.Offset, .dstOffset = Offset, .size = std::size(Data)};

    const VkCopyBufferInfo2 CopyInfo{.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                                     .srcBuffer   = Source.Buffer,
                                     .dstBuffer   = Destination.Handle(),
                                     .regionCount = 1U,
                                     .pRegions    = &Region};
    vkCmdCopyBuffer2(Commands, &CopyInfo);

    Destination.NoteTransferUse(m_Context.TransferQueue().NextValue());
}

void luvk::UploadManager::Upload(Image& Destination, const std::span<const std::byte> Data, const ImageRegion& Region)
{
    if (Region.MipLevel >= Destination.MipLevels() || Region.ArrayLayer >= Destination.ArrayLayers())
    {
        throw std::out_of_range("[luvk] Image upload: mip level or array layer out of range.");
    }

    if (!HasFlags(Destination.Usage(), ImageUsage::TransferDst))
    {
        throw std::logic_error("[luvk] Image upload requires transfer destination usage.");
    }

    if (!Destination.IsSharedWithTransferQueue())
    {
        throw std::logic_error("[luvk] Image upload: images created with GraphicsQueueOnly cannot be written by the transfer queue.");
    }

    // Depth/stencil copies are only legal on graphics-capable queues, and one copy can only fill one aspect of a
    // combined depth/stencil format: refuse instead of silently dropping data.
    constexpr VkImageAspectFlags DepthStencil = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    if ((Destination.Aspect() & DepthStencil) != 0U)
    {
        if (Destination.Aspect() == DepthStencil)
        {
            throw std::logic_error("[luvk] Image upload: combined depth/stencil formats cannot be uploaded in one copy.");
        }
        if (m_Context.HasDedicatedTransferQueue())
        {
            throw std::logic_error("[luvk] Image upload: depth/stencil images cannot be written by the dedicated transfer queue; "
                                   "render or copy them on the graphics queue instead.");
        }
    }

    const Extent3D MipExtent{std::max(1U, Destination.Extent().Width >> Region.MipLevel),
                             std::max(1U, Destination.Extent().Height >> Region.MipLevel),
                             std::max(1U, Destination.Extent().Depth >> Region.MipLevel)};

    const Extent3D Extent = Region.Extent.Width == 0U ? MipExtent : Region.Extent;
    if (Region.Offset.X < 0 || Region.Offset.Y < 0 || Region.Offset.Z < 0 ||
        static_cast<std::uint32_t>(Region.Offset.X) + Extent.Width > MipExtent.Width ||
        static_cast<std::uint32_t>(Region.Offset.Y) + Extent.Height > MipExtent.Height ||
        static_cast<std::uint32_t>(Region.Offset.Z) + Extent.Depth > MipExtent.Depth)
    {
        throw std::out_of_range("[luvk] Image upload: region out of the mip level bounds.");
    }

    // Rows may be longer than the region (RowLength, in texels of uncompressed formats): only the last row is shorter.
    const VkDeviceSize TexelSize = FormatRegionSize(Destination.Format(), {1U, 1U, 1U});
    if (TexelSize == 0U || (Region.RowLength != 0U && Region.RowLength < Extent.Width))
    {
        throw std::invalid_argument("[luvk] Image upload: unsupported format or row length.");
    }

    const VkDeviceSize Rows     = static_cast<VkDeviceSize>(Extent.Height) * Extent.Depth;
    const VkDeviceSize Expected = Region.RowLength == 0U ? FormatRegionSize(Destination.Format(), Extent)
                                                         : (Rows - 1U) * Region.RowLength * TexelSize + Extent.Width * TexelSize;
    if (std::size(Data) != Expected)
    {
        throw std::invalid_argument("[luvk] Image upload: expected " + std::to_string(Expected) + " bytes, got " +
                                    std::to_string(std::size(Data)) + ".");
    }

    std::scoped_lock      Lock(m_Mutex);
    const VkCommandBuffer Commands = BeginLocked();
    // The layout transition covers the whole image, which the frames that last used it may still read.
    m_WaitGraphicsValue = std::max(m_WaitGraphicsValue, Destination.State().LastGraphicsUse);

    // bufferOffset must be a multiple of 4 and of the texel (block) size.
    const StagedRange Source = StageLocked(Data, std::lcm(VkDeviceSize{16U}, TexelSize));

    ResourceState&                State = Destination.State();
    const VkImageSubresourceRange Range = {Destination.Aspect(), 0U, VK_REMAINING_MIP_LEVELS, 0U, VK_REMAINING_ARRAY_LAYERS};

    // The previous layout is kept (not UNDEFINED), so a partial upload preserves the rest of the image.
    const VkImageMemoryBarrier2 ToTransfer{.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                           .srcStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                           .srcAccessMask       = VK_ACCESS_2_MEMORY_WRITE_BIT,
                                           .dstStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
                                           .dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                           .oldLayout           = static_cast<VkImageLayout>(State.Layout),
                                           .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                           .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                           .image               = Destination.Handle(),
                                           .subresourceRange    = Range};

    const VkDependencyInfo ToTransferDependency{.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                                .imageMemoryBarrierCount = 1U,
                                                .pImageMemoryBarriers    = &ToTransfer};
    vkCmdPipelineBarrier2(Commands, &ToTransferDependency);

    const VkBufferImageCopy2 Copy{.sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                                  .bufferOffset      = Source.Offset,
                                  .bufferRowLength   = Region.RowLength,
                                  .bufferImageHeight = 0U,
                                  .imageSubresource  = {Destination.Aspect(), Region.MipLevel, Region.ArrayLayer, 1U},
                                  .imageOffset       = ToVk(Region.Offset),
                                  .imageExtent       = ToVk(Extent)};

    const VkCopyBufferToImageInfo2 CopyInfo{.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
                                            .srcBuffer      = Source.Buffer,
                                            .dstImage       = Destination.Handle(),
                                            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            .regionCount    = 1U,
                                            .pRegions       = &Copy};
    vkCmdCopyBufferToImage2(Commands, &CopyInfo);

    const VkImageMemoryBarrier2 ToReadOnly{.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                           .srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
                                           .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                           .dstStageMask        = VK_PIPELINE_STAGE_2_NONE,
                                           .dstAccessMask       = VK_ACCESS_2_NONE,
                                           .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                           .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                           .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                           .image               = Destination.Handle(),
                                           .subresourceRange    = Range};

    const VkDependencyInfo ToReadOnlyDependency{.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                                .imageMemoryBarrierCount = 1U,
                                                .pImageMemoryBarriers    = &ToReadOnly};
    vkCmdPipelineBarrier2(Commands, &ToReadOnlyDependency);

    State = ResourceState{.Layout          = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          .WriteStages     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                          .LastGraphicsUse = State.LastGraphicsUse,
                          .ImportedInFrame = State.ImportedInFrame};
    m_PendingCopy = true;

    Destination.NoteTransferUse(m_Context.TransferQueue().NextValue());
}

std::uint64_t luvk::UploadManager::Record(const std::function<void(CommandList&)>& Commands)
{
    std::scoped_lock      Lock(m_Mutex);
    const VkCommandBuffer Buffer = BeginLocked();
    SerializeCopiesLocked(Buffer);
    m_WaitGraphicsValue = m_Context.GraphicsQueue().LastSubmittedValue();
    CommandList List(m_Context, Buffer);
    Commands(List);
    m_PendingCopy = true;
    return m_Context.TransferQueue().NextValue();
}

std::uint64_t luvk::UploadManager::Flush()
{
    std::scoped_lock Lock(m_Mutex);

    if (m_Recording)
    {
        Batch& Current = m_Batches[m_Current];
        m_Recording    = false;
        m_PendingCopy  = false;

        Check(vkEndCommandBuffer(Current.Commands), "vkEndCommandBuffer (upload)");

        Queue&                         Graphics      = m_Context.GraphicsQueue();
        const std::uint64_t            GraphicsValue = std::min(std::exchange(m_WaitGraphicsValue, 0U), Graphics.LastSubmittedValue());
        const VkSemaphoreSubmitInfo    Wait{.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                            .semaphore = Graphics.Timeline(),
                                            .value     = GraphicsValue,
                                            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
        const VkCommandBufferSubmitInfo CommandInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = Current.Commands};

        Current.Value = m_Context.TransferQueue().Submit({&CommandInfo, 1U},
                                                         GraphicsValue > 0U ? std::span<const VkSemaphoreSubmitInfo>(&Wait, 1U)
                                                                            : std::span<const VkSemaphoreSubmitInfo>{});
        m_LastSubmitted.store(Current.Value, std::memory_order_release);
        m_Current = (m_Current + 1U) % std::size(m_Batches);
    }

    return m_LastSubmitted.load(std::memory_order_acquire);
}

void luvk::UploadManager::FlushAndWait()
{
    m_Context.TransferQueue().Wait(Flush());
}

VkCommandBuffer luvk::UploadManager::BeginLocked()
{
    Batch& Current = m_Batches[m_Current];

    if (!m_Recording)
    {
        m_Context.TransferQueue().Wait(Current.Value);
        Check(vkResetCommandPool(m_Context.Device(), Current.Pool, 0U), "vkResetCommandPool (upload)");

        constexpr VkCommandBufferBeginInfo BeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                     .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        Check(vkBeginCommandBuffer(Current.Commands, &BeginInfo), "vkBeginCommandBuffer (upload)");
        m_Recording         = true;
        m_PendingCopy       = false;
        Current.StagingUsed = 0U;
    }

    return Current.Commands;
}

void luvk::UploadManager::SerializeCopiesLocked(const VkCommandBuffer Commands)
{

    if (m_PendingCopy)
    {
        constexpr VkMemoryBarrier2 Barrier{.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                           .srcStageMask  = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                                           .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                           .dstStageMask  = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                                           .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT};

        const VkDependencyInfo Dependency{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1U, .pMemoryBarriers = &Barrier};
        vkCmdPipelineBarrier2(Commands, &Dependency);
    }
    m_PendingCopy = true;
}
