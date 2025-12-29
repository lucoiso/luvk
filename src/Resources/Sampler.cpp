/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/Sampler.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"

using namespace luvk;

Sampler::Sampler(Device* DeviceModule)
    : m_DeviceModule(DeviceModule) {}

Sampler::~Sampler()
{
    if (m_Sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_DeviceModule->GetLogicalDevice(), m_Sampler, nullptr);
    }
}

void Sampler::CreateSampler(const SamplerCreationArguments& Arguments)
{
    const VkSamplerCreateInfo Info{.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                   .magFilter        = Arguments.Filter,
                                   .minFilter        = Arguments.Filter,
                                   .mipmapMode       = Arguments.MipmapMode,
                                   .addressModeU     = Arguments.AddressMode,
                                   .addressModeV     = Arguments.AddressMode,
                                   .addressModeW     = Arguments.AddressMode,
                                   .anisotropyEnable = Arguments.MaxAnisotropy > 1.F ? VK_TRUE : VK_FALSE,
                                   .maxAnisotropy    = Arguments.MaxAnisotropy,
                                   .maxLod           = VK_LOD_CLAMP_NONE};

    if (!LUVK_EXECUTE(vkCreateSampler(m_DeviceModule->GetLogicalDevice(), &Info, nullptr, &m_Sampler)))
    {
        throw std::runtime_error("Failed to create sampler");
    }
}
