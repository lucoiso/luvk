// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/Sampler.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Renderer.hpp"

void luvk::Sampler::CreateSampler(const std::shared_ptr<Device>& DeviceModule, const CreationArguments& Arguments)
{
    m_DeviceModule = DeviceModule;
    const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();

    const VkSamplerCreateInfo Info{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                   .pNext = nullptr,
                                   .flags = 0,
                                   .magFilter = Arguments.Filter,
                                   .minFilter = Arguments.Filter,
                                   .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                   .addressModeU = Arguments.AddressMode,
                                   .addressModeV = Arguments.AddressMode,
                                   .addressModeW = Arguments.AddressMode,
                                   .mipLodBias = 0.F,
                                   .anisotropyEnable = VK_FALSE,
                                   .maxAnisotropy = 1.F,
                                   .compareEnable = VK_FALSE,
                                   .compareOp = VK_COMPARE_OP_ALWAYS,
                                   .minLod = 0.F,
                                   .maxLod = VK_LOD_CLAMP_NONE,
                                   .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                                   .unnormalizedCoordinates = VK_FALSE};

    if (!LUVK_EXECUTE(vkCreateSampler(LogicalDevice, &Info, nullptr, &m_Sampler)))
    {
        throw std::runtime_error("Failed to create sampler.");
    }
}

luvk::Sampler::~Sampler()
{
    Destroy();
}

void luvk::Sampler::Destroy()
{
    if (m_DeviceModule && m_Sampler != VK_NULL_HANDLE)
    {
        const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();
        vkDestroySampler(LogicalDevice, m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }
}
