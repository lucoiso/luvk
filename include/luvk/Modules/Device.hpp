// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Resources/Extensions.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"

namespace luvk
{
    class Renderer;

    enum class DeviceEvents : std::uint8_t
    {
        OnSelectedPhysicalDevice, OnChangedLogicalDevice
    };

    class LUVKMODULE_API Device : public IRenderModule,
                                  public std::enable_shared_from_this<Device>
    {
        VkDevice m_LogicalDevice{VK_NULL_HANDLE};
        VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};
        VkSurfaceKHR m_Surface{VK_NULL_HANDLE};
        DeviceExtensions m_Extensions{};
        std::vector<VkPhysicalDevice> m_AvailableDevices{};
        VkPhysicalDeviceFeatures2 m_FeatureChain{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceFeatures& m_DeviceFeatures = m_FeatureChain.features;
        VkPhysicalDeviceVulkan11Features m_Vulkan11Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features m_Vulkan12Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features m_Vulkan13Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan14Features m_Vulkan14Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
        VkPhysicalDeviceProperties m_DeviceProperties{};
        std::vector<VkSurfaceFormatKHR> m_SurfaceFormat{};
        std::vector<VkQueueFamilyProperties> m_DeviceQueueFamilyProperties{};
        std::unordered_map<std::uint32_t, std::vector<VkQueue>> m_Queues{};
        VkInstance m_Instance{VK_NULL_HANDLE};
        std::shared_ptr<Renderer> m_Renderer{nullptr};

    public:
        constexpr Device() = default;

        void SetPhysicalDevice(VkPhysicalDevice const& Device);
        void SetPhysicalDevice(std::uint8_t Index);
        void SetPhysicalDevice(VkPhysicalDeviceType Type);
        void SetSurface(VkSurfaceKHR const& Surface);
        void CreateLogicalDevice(std::unordered_map<std::uint32_t, std::uint32_t>&& QueueIndices, void const* pNext);

        [[nodiscard]] constexpr VkDevice const& GetLogicalDevice() const
        {
            return m_LogicalDevice;
        }

        [[nodiscard]] constexpr VkPhysicalDevice const& GetPhysicalDevice() const
        {
            return m_PhysicalDevice;
        }

        [[nodiscard]] constexpr VkSurfaceKHR const& GetSurface() const
        {
            return m_Surface;
        }

        [[nodiscard]] constexpr std::vector<VkSurfaceFormatKHR> const& GetSurfaceFormat() const
        {
            return m_SurfaceFormat;
        }

        [[nodiscard]] constexpr VkPhysicalDeviceProperties const& GetDeviceProperties() const
        {
            return m_DeviceProperties;
        }

        [[nodiscard]] constexpr std::uint32_t GetVulkanVersion() const noexcept
        {
            return m_DeviceProperties.apiVersion;
        }

        [[nodiscard]] constexpr VkPhysicalDeviceFeatures const& GetDeviceFeatures() const
        {
            return m_DeviceFeatures;
        }

        [[nodiscard]] constexpr VkPhysicalDeviceVulkan11Features const& GetVulkan11Features() const
        {
            return m_Vulkan11Features;
        }

        [[nodiscard]] constexpr VkPhysicalDeviceVulkan12Features const& GetVulkan12Features() const
        {
            return m_Vulkan12Features;
        }

        [[nodiscard]] constexpr VkPhysicalDeviceVulkan13Features const& GetVulkan13Features() const
        {
            return m_Vulkan13Features;
        }

        [[nodiscard]] constexpr VkPhysicalDeviceVulkan14Features const& GetVulkan14Features() const
        {
            return m_Vulkan14Features;
        }

        [[nodiscard]] constexpr bool IsVersionAtLeast(const std::uint32_t Major, const std::uint32_t Minor) const noexcept
        {
            return VK_API_VERSION_MAJOR(m_DeviceProperties.apiVersion) > Major ||
                   (VK_API_VERSION_MAJOR(m_DeviceProperties.apiVersion) == Major &&
                   VK_API_VERSION_MINOR(m_DeviceProperties.apiVersion) >= Minor);
        }

        [[nodiscard]] constexpr std::vector<VkQueueFamilyProperties> const& GetDeviceQueueFamilyProperties() const
        {
            return m_DeviceQueueFamilyProperties;
        }

        [[nodiscard]] constexpr std::unordered_map<std::uint32_t, std::vector<VkQueue>> const& GetQueues() const
        {
            return m_Queues;
        }

        [[nodiscard]] DeviceExtensions& GetExtensions()
        {
            return m_Extensions;
        }

        [[nodiscard]] std::optional<std::uint32_t> FindQueueFamilyIndex(VkQueueFlags Flags) const;
        [[nodiscard]] VkQueue GetQueue(std::uint32_t FamilyIndex, std::uint32_t QueueIndex = 0U) const;
        void WaitIdle() const;
        void Wait(VkQueue Queue) const;
        void Wait(VkFence Fence, VkBool32 WaitAll = VK_TRUE, std::uint64_t Timeout = UINT64_MAX) const;

        [[nodiscard]] constexpr std::vector<VkPhysicalDevice> const& GetAvailableDevices()
        {
            return m_AvailableDevices;
        }

        ~Device() override
        {
            Device::ClearResources();
        }

        [[nodiscard]] void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const&) const noexcept override
        {
            return &m_FeatureChain;
        }

    private:
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;
        void ClearResources() override;
        void FetchAvailableDevices(VkInstance const& Instance);
    };
} // namespace luvk
