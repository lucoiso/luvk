// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once
/** DescriptorPool.hpp definitions */

#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"

#include <volk/volk.h>
#include <span>
#include <vector>
#include <string_view>

namespace luvk
{
    /** Render module responsible for descriptor pool management */
    class LUVKMODULE_API DescriptorPool : public IRenderModule
    {
        /** Handle to the Vulkan descriptor pool */
        VkDescriptorPool m_Pool{VK_NULL_HANDLE};

        /** Device module used for pool creation */
        std::shared_ptr<IRenderModule> m_DeviceModule{};

    public:
        constexpr DescriptorPool() = default;

        ~DescriptorPool() override
        {
            DescriptorPool::ClearResources();
        }

        /** Create descriptor pool */
        void CreateDescriptorPool(std::shared_ptr<IRenderModule> const& DeviceModule,
                                  std::uint32_t MaxSets,
                                  std::span<VkDescriptorPoolSize const> PoolSizes,
                                  VkDescriptorPoolCreateFlags Flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

        /** Retrieve the descriptor pool handle */
        [[nodiscard]] constexpr VkDescriptorPool const& GetHandle() const
        {
            return m_Pool;
        }

        [[nodiscard]] std::unordered_map<std::string_view, std::vector<std::string_view>> GetRequiredDeviceExtensions() const override
        {
            return {};
        }

        [[nodiscard]] void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const&) const noexcept override
        {
            return nullptr;
        }

        [[nodiscard]] void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept override
        {
            return nullptr;
        }

    private: /** Set up any required dependencies */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const&) override;

        /** Destroy the internal Vulkan descriptor pool */
        void ClearResources() override;
    };
} // namespace luvk
