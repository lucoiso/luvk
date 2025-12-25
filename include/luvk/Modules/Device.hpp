// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <volk.h>
#include "luvk/Interfaces/IEventModule.hpp"
#include "luvk/Interfaces/IFeatureChainModule.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"
#include "luvk/Resources/Extensions.hpp"

namespace luvk
{
    class Renderer;

    enum class DeviceEvents : std::uint8_t
    {
        OnSelectedPhysicalDevice,
        OnCreatedLogicalDevice,
        OnSetSurface,
    };

    class LUVK_API Device : public IRenderModule,
                            public IEventModule,
                            public IFeatureChainModule
    {
    protected:
        VkDevice                                                m_LogicalDevice{VK_NULL_HANDLE};
        VkPhysicalDevice                                        m_PhysicalDevice{VK_NULL_HANDLE};
        VkSurfaceKHR                                            m_Surface{VK_NULL_HANDLE};
        std::shared_ptr<Renderer>                               m_RendererModule{};
        DeviceExtensions                                        m_Extensions{};
        VkPhysicalDeviceFeatures2                               m_DeviceFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceVulkan11Features                        m_Vulkan11Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features                        m_Vulkan12Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features                        m_Vulkan13Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan14Features                        m_Vulkan14Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
        VkPhysicalDeviceProperties                              m_DeviceProperties{};
        std::vector<VkPhysicalDevice>                           m_AvailableDevices{};
        std::vector<VkSurfaceFormatKHR>                         m_SurfaceFormat{};
        std::vector<VkQueueFamilyProperties>                    m_DeviceQueueFamilyProperties{};
        std::unordered_map<std::uint32_t, std::vector<VkQueue>> m_Queues{};

    public:
        Device() = delete;
        explicit Device(const std::shared_ptr<Renderer>& RendererModule);

        ~Device() override
        {
            Device::ClearResources();
        }

        void SetPhysicalDevice(VkPhysicalDevice Device);
        void SetPhysicalDevice(std::uint8_t Index);
        void SetPhysicalDevice(VkPhysicalDeviceType Type);
        void SetSurface(VkSurfaceKHR Surface);
        void CreateLogicalDevice(std::unordered_map<std::uint32_t, std::uint32_t>&& QueueIndices, const void* pNext);

        [[nodiscard]] constexpr VkDevice GetLogicalDevice() const noexcept
        {
            return m_LogicalDevice;
        }

        [[nodiscard]] constexpr VkPhysicalDevice GetPhysicalDevice() const noexcept
        {
            return m_PhysicalDevice;
        }

        [[nodiscard]] constexpr VkSurfaceKHR GetSurface() const noexcept
        {
            return m_Surface;
        }

        [[nodiscard]] constexpr std::span<const VkSurfaceFormatKHR> GetSurfaceFormat() const noexcept
        {
            return m_SurfaceFormat;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceProperties& GetDeviceProperties() const noexcept
        {
            return m_DeviceProperties;
        }

        [[nodiscard]] constexpr std::uint32_t GetVulkanVersion() const noexcept
        {
            return m_DeviceProperties.apiVersion;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceFeatures2& GetDeviceFeatures() const noexcept
        {
            return m_DeviceFeatures;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceVulkan11Features& GetVulkan11Features() const noexcept
        {
            return m_Vulkan11Features;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceVulkan12Features& GetVulkan12Features() const noexcept
        {
            return m_Vulkan12Features;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceVulkan13Features& GetVulkan13Features() const noexcept
        {
            return m_Vulkan13Features;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceVulkan14Features& GetVulkan14Features() const noexcept
        {
            return m_Vulkan14Features;
        }

        [[nodiscard]] constexpr std::span<const VkQueueFamilyProperties> GetDeviceQueueFamilyProperties() const noexcept
        {
            return m_DeviceQueueFamilyProperties;
        }

        [[nodiscard]] constexpr const std::unordered_map<std::uint32_t, std::vector<VkQueue>>& GetQueues() const noexcept
        {
            return m_Queues;
        }

        [[nodiscard]] constexpr DeviceExtensions& GetExtensions() noexcept
        {
            return m_Extensions;
        }

        [[nodiscard]] constexpr std::span<const VkPhysicalDevice> GetAvailableDevices() noexcept
        {
            return m_AvailableDevices;
        }

        [[nodiscard]] const void* GetDeviceFeatureChain() const noexcept override
        {
            return &m_DeviceFeatures;
        }

        [[nodiscard]] std::optional<std::uint32_t> FindQueueFamilyIndex(VkQueueFlags Flags) const;
        [[nodiscard]] VkQueue                      GetQueue(std::uint32_t FamilyIndex, std::uint32_t QueueIndex = 0U) const;

        void WaitIdle() const;
        void Wait(VkQueue Queue) const;
        void Wait(VkFence Fence, VkBool32 WaitAll = VK_TRUE, std::uint64_t Timeout = UINT64_MAX) const;

    protected:
        void InitializeResources() override;
        void ClearResources() override;

        [[nodiscard]] const void* ConfigureExtensions(const void* pNext);

    private:
        void FetchAvailableDevices(VkInstance Instance);
    };
} // namespace luvk
