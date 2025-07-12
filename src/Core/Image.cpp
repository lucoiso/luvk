// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Image.hpp"
#include "luvk/Core/Renderer.hpp"
#include "luvk/Core/Memory.hpp"
#include "luvk/Core/Device.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include <iterator>
#include <span>
#include <cstddef>
#include <cstring>

void luvk::Image::CreateImage(std::shared_ptr<Device> const& DeviceModule, std::shared_ptr<Memory> const& MemoryModule, CreationArguments const& Arguments)
{
    m_DeviceModule = DeviceModule;
    m_MemoryModule = MemoryModule;
    auto const Memory = MemoryModule;
    VmaAllocator const& Allocator = Memory->GetAllocator();

    auto const DevMod = DeviceModule;
    VkFormatProperties Props{};
    vkGetPhysicalDeviceFormatProperties(DevMod->GetPhysicalDevice(), Arguments.Format, &Props);

    VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
    if (Arguments.MemoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU && Props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
    {
        Tiling = VK_IMAGE_TILING_LINEAR;
    }

    VkImageCreateInfo const Info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
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

    VmaAllocationCreateInfo const AllocInfo{.flags = Arguments.MemoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU
                                                         ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                         : 0U,
                                            .usage = Arguments.MemoryUsage};

    if (!LUVK_EXECUTE(vmaCreateImage(Allocator, &Info, &AllocInfo, &m_Image, &m_Allocation, nullptr)))
    {
        throw std::runtime_error("Failed to create image.");
    }

    VkImageViewCreateInfo const ViewInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                         .image = m_Image,
                                         .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                         .format = Arguments.Format,
                                         .subresourceRange = {Arguments.Aspect, 0, 1, 0, 1}};

    auto const Dev = DeviceModule;
    VkDevice const& Device = Dev->GetLogicalDevice();

    if (!LUVK_EXECUTE(vkCreateImageView(Device, &ViewInfo, nullptr, &m_View)))
    {
        throw std::runtime_error("Failed to create image view.");
    }
}

void luvk::Image::Upload(const std::span<const std::byte> Data) const
{
    auto const Memory = m_MemoryModule;
    VmaAllocator const& Allocator = Memory->GetAllocator();
    void* Mapping = nullptr;
    if (!LUVK_EXECUTE(vmaMapMemory(Allocator, m_Allocation, &Mapping)))
    {
        throw std::runtime_error("Failed to map image memory.");
    }
    std::memcpy(Mapping, std::data(Data), std::size(Data));
    vmaFlushAllocation(Allocator, m_Allocation, 0, std::size(Data));
    vmaUnmapMemory(Allocator, m_Allocation);
}

luvk::Image::~Image()
{
    Destroy();
}

void luvk::Image::Destroy()
{
    if (m_DeviceModule && m_MemoryModule)
    {
        auto const Memory = m_MemoryModule;
        VmaAllocator const& Allocator = Memory->GetAllocator();
        const VkDevice Device = m_DeviceModule->GetLogicalDevice();

        if (m_View != VK_NULL_HANDLE)
        {
            vkDestroyImageView(Device, m_View, nullptr);
            m_View = VK_NULL_HANDLE;
        }

        if (m_Image != VK_NULL_HANDLE)
        {
            vmaDestroyImage(Allocator, m_Image, m_Allocation);
            m_Image = VK_NULL_HANDLE;
            m_Allocation = nullptr;
        }
    }
}
