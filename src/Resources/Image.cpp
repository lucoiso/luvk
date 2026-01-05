/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/Image.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Memory.hpp"
#include "luvk/Resources/Buffer.hpp"

using namespace luvk;

Image::Image(Device* DeviceModule, Memory* MemoryModule)
    : m_MemoryModule(MemoryModule),
      m_DeviceModule(DeviceModule) {}

Image::~Image()
{
    if (m_View != VK_NULL_HANDLE)
    {
        vkDestroyImageView(m_DeviceModule->GetLogicalDevice(), m_View, nullptr);
    }
    if (m_Image != VK_NULL_HANDLE)
    {
        vmaDestroyImage(m_MemoryModule->GetAllocator(), m_Image, m_Allocation);
    }
}

void Image::CreateImage(const ImageCreationArguments& Arguments)
{
    m_Width = Arguments.Extent.width;
    m_Height = Arguments.Extent.height;

    const VkImageUsageFlags Usage = Arguments.Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const VkImageCreateInfo Info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                 .imageType = VK_IMAGE_TYPE_2D,
                                 .format = Arguments.Format,
                                 .extent = Arguments.Extent,
                                 .mipLevels = 1U,
                                 .arrayLayers = 1U,
                                 .samples = VK_SAMPLE_COUNT_1_BIT,
                                 .tiling = VK_IMAGE_TILING_OPTIMAL,
                                 .usage = Usage,
                                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                 .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

    const VmaAllocationCreateInfo AllocInfo{.usage = Arguments.MemoryUsage, .priority = Arguments.Priority};

    if (!LUVK_EXECUTE(vmaCreateImage(m_MemoryModule->GetAllocator(), &Info, &AllocInfo, &m_Image, &m_Allocation, nullptr)))
    {
        throw std::runtime_error("Failed to create image");
    }

    const VkImageViewCreateInfo ViewInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                         .image = m_Image,
                                         .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                         .format = Arguments.Format,
                                         .subresourceRange = {.aspectMask = Arguments.Aspect,
                                                              .baseMipLevel = 0U,
                                                              .levelCount = 1U,
                                                              .baseArrayLayer = 0U,
                                                              .layerCount = 1U}};

    if (!LUVK_EXECUTE(vkCreateImageView(m_DeviceModule->GetLogicalDevice(), &ViewInfo, nullptr, &m_View)))
    {
        throw std::runtime_error("Failed to create image view");
    }

    if (!std::empty(Arguments.Name))
    {
        vmaSetAllocationName(m_MemoryModule->GetAllocator(), m_Allocation, std::data(Arguments.Name));
    }
}

void Image::Upload(std::span<const std::byte> Data) const
{
    const VkDevice Device = m_DeviceModule->GetLogicalDevice();

    if (vkCopyMemoryToImageEXT)
    {
        const VkHostImageLayoutTransitionInfoEXT Transition{.sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,
                                                            .image = m_Image,
                                                            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

        const VkMemoryToImageCopyEXT CopyRegion{.sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT,
                                                .pHostPointer = std::data(Data),
                                                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                                .imageExtent = {m_Width, m_Height, 1}};

        const VkCopyMemoryToImageInfoEXT CopyInfo{.sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT,
                                                  .dstImage = m_Image,
                                                  .dstImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                  .regionCount = 1,
                                                  .pRegions = &CopyRegion};

        vkTransitionImageLayoutEXT(Device, 1, &Transition);
        if (vkCopyMemoryToImageEXT(Device, &CopyInfo) == VK_SUCCESS)
        {
            return;
        }
    }

    const auto Staging = std::make_unique<Buffer>(m_DeviceModule, m_MemoryModule);
    Staging->CreateBuffer({.Size = std::size(Data),
                           .Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           .MemoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
                           .PersistentlyMapped = true,
                           .Name = "ImageStaging"});
    Staging->Upload(Data);

    m_DeviceModule->SubmitImmediate([&](const VkCommandBuffer Cmd)
    {
        const VkImageMemoryBarrier2 PreTransfer{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                .image = m_Image,
                                                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        const VkDependencyInfo PreInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &PreTransfer};
        vkCmdPipelineBarrier2(Cmd, &PreInfo);

        const VkBufferImageCopy Region{.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {m_Width, m_Height, 1}};
        vkCmdCopyBufferToImage(Cmd, Staging->GetHandle(), m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

        const VkImageMemoryBarrier2 PostTransfer{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                 .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                 .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                 .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                 .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                                                 .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                 .image = m_Image,
                                                 .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        const VkDependencyInfo PostInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &PostTransfer};
        vkCmdPipelineBarrier2(Cmd, &PostInfo);
    });
}
