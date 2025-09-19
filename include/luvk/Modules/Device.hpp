// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <optional>
#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Resources/Extensions.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Types/UnorderedMap.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    class Renderer;

    enum class DeviceEvents : std::uint8_t
    {
        OnSelectedPhysicalDevice,
        OnChangedLogicalDevice
    };

    class LUVKMODULE_API Device : public IRenderModule,
                                  public std::enable_shared_from_this<Device>
    {
        VkDevice m_LogicalDevice{VK_NULL_HANDLE};
        VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};
        VkSurfaceKHR m_Surface{VK_NULL_HANDLE};
        DeviceExtensions m_Extensions{};
        Vector<VkPhysicalDevice> m_AvailableDevices{};
        VkPhysicalDeviceFeatures2 m_FeatureChain{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceFeatures& m_DeviceFeatures = m_FeatureChain.features;
        VkPhysicalDeviceVulkan11Features m_Vulkan11Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features m_Vulkan12Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features m_Vulkan13Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan14Features m_Vulkan14Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
        VkPhysicalDeviceProperties m_DeviceProperties{};
        Vector<VkSurfaceFormatKHR> m_SurfaceFormat{};
        Vector<VkQueueFamilyProperties> m_DeviceQueueFamilyProperties{};
        UnorderedMap<std::uint32_t, Vector<VkQueue>> m_Queues{};
        VkInstance m_Instance{VK_NULL_HANDLE};
        std::shared_ptr<Renderer> m_Renderer{nullptr};

    public:
        constexpr Device() = default;

        ~Device() override
        {
            Device::ClearResources();
        }

        void SetPhysicalDevice(const VkPhysicalDevice& Device);
        void SetPhysicalDevice(std::uint8_t Index);
        void SetPhysicalDevice(VkPhysicalDeviceType Type);
        void SetSurface(const VkSurfaceKHR& Surface);
        void CreateLogicalDevice(UnorderedMap<std::uint32_t, std::uint32_t>&& QueueIndices, const void* pNext);

        [[nodiscard]] constexpr const VkDevice& GetLogicalDevice() const
        {
            return m_LogicalDevice;
        }

        [[nodiscard]] constexpr const VkPhysicalDevice& GetPhysicalDevice() const
        {
            return m_PhysicalDevice;
        }

        [[nodiscard]] constexpr const VkSurfaceKHR& GetSurface() const
        {
            return m_Surface;
        }

        [[nodiscard]] constexpr const Vector<VkSurfaceFormatKHR>& GetSurfaceFormat() const
        {
            return m_SurfaceFormat;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceProperties& GetDeviceProperties() const
        {
            return m_DeviceProperties;
        }

        [[nodiscard]] constexpr std::uint32_t GetVulkanVersion() const noexcept
        {
            return m_DeviceProperties.apiVersion;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceFeatures& GetDeviceFeatures() const
        {
            return m_DeviceFeatures;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceVulkan11Features& GetVulkan11Features() const
        {
            return m_Vulkan11Features;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceVulkan12Features& GetVulkan12Features() const
        {
            return m_Vulkan12Features;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceVulkan13Features& GetVulkan13Features() const
        {
            return m_Vulkan13Features;
        }

        [[nodiscard]] constexpr const VkPhysicalDeviceVulkan14Features& GetVulkan14Features() const
        {
            return m_Vulkan14Features;
        }

        [[nodiscard]] constexpr const Vector<VkQueueFamilyProperties>& GetDeviceQueueFamilyProperties() const
        {
            return m_DeviceQueueFamilyProperties;
        }

        [[nodiscard]] constexpr const UnorderedMap<std::uint32_t, Vector<VkQueue>>& GetQueues() const
        {
            return m_Queues;
        }

        [[nodiscard]] DeviceExtensions& GetExtensions()
        {
            return m_Extensions;
        }

        [[nodiscard]] constexpr const Vector<VkPhysicalDevice>& GetAvailableDevices()
        {
            return m_AvailableDevices;
        }

        [[nodiscard]] const void* GetDeviceFeatureChain(const std::shared_ptr<IRenderModule>&) const noexcept override
        {
            return &m_FeatureChain;
        }

        [[nodiscard]] std::optional<std::uint32_t> FindQueueFamilyIndex(VkQueueFlags Flags) const;
        [[nodiscard]] VkQueue GetQueue(std::uint32_t FamilyIndex, std::uint32_t QueueIndex = 0U) const;

        void WaitIdle() const;
        void Wait(const VkQueue& Queue) const;
        void Wait(const VkFence& Fence, VkBool32 WaitAll = VK_TRUE, std::uint64_t Timeout = UINT64_MAX) const;

    private:
        void InitializeDependencies(const std::shared_ptr<IRenderModule>& MainRenderer) override;
        void ClearResources() override;
        void FetchAvailableDevices(const VkInstance& Instance);
    };
} // namespace luvk
