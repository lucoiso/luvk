// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/Image.hpp"
#include <cstddef>
#include <iterator>
#include <span>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Memory.hpp"
#include "luvk/Resources/Buffer.hpp"

luvk::Image::Image(const std::shared_ptr<Device>& DeviceModule, const std::shared_ptr<Memory>& MemoryModule)
    : m_MemoryModule(MemoryModule),
      m_DeviceModule(DeviceModule) {}

luvk::Image::~Image()
{
    const VmaAllocator& Allocator = m_MemoryModule->GetAllocator();
    const VkDevice&     Device    = m_DeviceModule->GetLogicalDevice();

    if (m_View != VK_NULL_HANDLE)
    {
        vkDestroyImageView(Device, m_View, nullptr);
        m_View = VK_NULL_HANDLE;
    }

    if (m_Image != VK_NULL_HANDLE)
    {
        vmaDestroyImage(Allocator, m_Image, m_Allocation);
        m_Image      = VK_NULL_HANDLE;
        m_Allocation = nullptr;
    }
}

void luvk::Image::CreateImage(const CreationArguments& Arguments)
{
    m_Width  = Arguments.Extent.width;
    m_Height = Arguments.Extent.height;

    const VmaAllocator& Allocator = m_MemoryModule->GetAllocator();

    constexpr VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;

    const VkImageCreateInfo Info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                 .imageType = VK_IMAGE_TYPE_2D,
                                 .format = Arguments.Format,
                                 .extent = Arguments.Extent,
                                 .mipLevels = 1,
                                 .arrayLayers = 1,
                                 .samples = VK_SAMPLE_COUNT_1_BIT,
                                 .tiling = Tiling,
                                 .usage = Arguments.Usage,
                                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                 .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

    const VmaAllocationCreateInfo AllocInfo{.flags = Arguments.MemoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU
                                                         ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                         : 0U,
                                            .usage = Arguments.MemoryUsage,
                                            .priority = Arguments.Priority};

    if (!LUVK_EXECUTE(vmaCreateImage(Allocator, &Info, &AllocInfo, &m_Image, &m_Allocation, nullptr)))
    {
        throw std::runtime_error("Failed to create image.");
    }

    const VkImageViewCreateInfo ViewInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                         .image = m_Image,
                                         .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                         .format = Arguments.Format,
                                         .subresourceRange = {Arguments.Aspect, 0, 1, 0, 1}};

    if (!LUVK_EXECUTE(vkCreateImageView(m_DeviceModule->GetLogicalDevice(), &ViewInfo, nullptr, &m_View)))
    {
        throw std::runtime_error("Failed to create image view.");
    }
}

void luvk::Image::Upload(const std::span<const std::byte>& Data) const
{
    const VmaAllocator& Allocator = m_MemoryModule->GetAllocator();
    void*               Mapping   = nullptr;
    if (!LUVK_EXECUTE(vmaMapMemory(Allocator, m_Allocation, &Mapping)))
    {
        throw std::runtime_error("Failed to map image memory.");
    }
    std::memcpy(Mapping, std::data(Data), std::size(Data));
    vmaFlushAllocation(Allocator, m_Allocation, 0, std::size(Data));
    vmaUnmapMemory(Allocator, m_Allocation);
}

void luvk::Image::Upload(const std::shared_ptr<Buffer>& Staging) const
{
    const VkDevice&     LogicalDevice = m_DeviceModule->GetLogicalDevice();
    const std::uint32_t QueueFamily   = m_DeviceModule->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT).value();
    const VkQueue&      Queue         = m_DeviceModule->GetQueue(QueueFamily);

    const VkCommandPoolCreateInfo PoolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                           .pNext = nullptr,
                                           .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                           .queueFamilyIndex = QueueFamily};

    VkCommandPool Pool{VK_NULL_HANDLE};

    if (!LUVK_EXECUTE(vkCreateCommandPool(LogicalDevice, &PoolInfo, nullptr, &Pool)))
    {
        throw std::runtime_error("Failed to create command pool for image upload");
    }

    const VkCommandBufferAllocateInfo AllocationInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                     .pNext = nullptr,
                                                     .commandPool = Pool,
                                                     .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                     .commandBufferCount = 1};

    VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};

    if (!LUVK_EXECUTE(vkAllocateCommandBuffers(LogicalDevice, &AllocationInfo, &CommandBuffer)))
    {
        vkDestroyCommandPool(LogicalDevice, Pool, nullptr);
        throw std::runtime_error("Failed to allocate command buffer for image upload");
    }

    constexpr VkCommandBufferBeginInfo CommandBufferBeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                              .pNext = nullptr,
                                                              .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                              .pInheritanceInfo = nullptr};
    vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

    const VkImageMemoryBarrier ToTransfer{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                          .pNext = nullptr,
                                          .srcAccessMask = 0,
                                          .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                          .image = m_Image,
                                          .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    vkCmdPipelineBarrier(CommandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &ToTransfer);

    const VkBufferImageCopy Region{.bufferOffset = 0,
                                   .bufferRowLength = 0,
                                   .bufferImageHeight = 0,
                                   .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                   .imageOffset = {0, 0, 0},
                                   .imageExtent = {m_Width, m_Height, 1}};

    vkCmdCopyBufferToImage(CommandBuffer,
                           Staging->GetHandle(),
                           m_Image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &Region);

    const VkImageMemoryBarrier ToShaderRead{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                            .pNext = nullptr,
                                            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                            .image = m_Image,
                                            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    vkCmdPipelineBarrier(CommandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &ToShaderRead);

    vkEndCommandBuffer(CommandBuffer);

    const VkSubmitInfo QueueSubmitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                       .pNext = nullptr,
                                       .waitSemaphoreCount = 0,
                                       .pWaitSemaphores = nullptr,
                                       .pWaitDstStageMask = nullptr,
                                       .commandBufferCount = 1,
                                       .pCommandBuffers = &CommandBuffer,
                                       .signalSemaphoreCount = 0,
                                       .pSignalSemaphores = nullptr};

    if (!LUVK_EXECUTE(vkQueueSubmit(Queue, 1, &QueueSubmitInfo, VK_NULL_HANDLE)))
    {
        vkFreeCommandBuffers(LogicalDevice, Pool, 1, &CommandBuffer);
        vkDestroyCommandPool(LogicalDevice, Pool, nullptr);
        throw std::runtime_error("Failed to submit image upload command");
    }

    vkQueueWaitIdle(Queue);

    vkFreeCommandBuffers(LogicalDevice, Pool, 1, &CommandBuffer);
    vkDestroyCommandPool(LogicalDevice, Pool, nullptr);
}
