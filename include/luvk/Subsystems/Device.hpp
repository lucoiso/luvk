// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Modules/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Subsystems/Extensions.hpp"

#include <volk/volk.h>
#include <cstdint>
#include <unordered_map>
#include <optional>

namespace luvk
{
    class Renderer;

    /** Device Event Keys */
    enum class DeviceEvents : std::uint8_t
    {
        OnSelectedPhysicalDevice, OnChangedLogicalDevice
    };

    /** Render module responsible for device operations */
    class LUVKMODULE_API Device : public IRenderModule,
                                  public std::enable_shared_from_this<Device>
    {
        /** Logical device handle */
        VkDevice m_LogicalDevice{VK_NULL_HANDLE};

        /** Selected physical device */
        VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};

        /** Presentation surface */
        VkSurfaceKHR m_Surface{VK_NULL_HANDLE};

        /** Manager for device extensions */
        DeviceExtensions m_Extensions{};

        /** List of physical devices reported by Vulkan */
        std::vector<VkPhysicalDevice> m_AvailableDevices{};

        /** Head of the feature chain */
        VkPhysicalDeviceFeatures2 m_FeatureChain{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

        /** Reference to features within the chain */
        VkPhysicalDeviceFeatures& m_DeviceFeatures = m_FeatureChain.features;

        /** Vulkan version feature structures */
        VkPhysicalDeviceVulkan11Features m_Vulkan11Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features m_Vulkan12Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features m_Vulkan13Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan14Features m_Vulkan14Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};

        /** Queried properties of the physical device */
        VkPhysicalDeviceProperties m_DeviceProperties{};

        /** Supported surface formats */
        std::vector<VkSurfaceFormatKHR> m_SurfaceFormat{};

        /** Queue families available on the device */
        std::vector<VkQueueFamilyProperties> m_DeviceQueueFamilyProperties{};

        /** Queues retrieved from the logical device */
        std::unordered_map<std::uint32_t, std::vector<VkQueue>> m_Queues{};

        /** Associated Vulkan instance */
        VkInstance m_Instance{VK_NULL_HANDLE};

        /** Owning renderer */
        std::shared_ptr<Renderer> m_Renderer{nullptr};

    public:
        constexpr Device() = default;

        /** Set the preferred physical device */
        void SetPhysicalDevice(VkPhysicalDevice const& Device);

        /** Set the preferred physical device */
        void SetPhysicalDevice(std::uint8_t Index);

        /** Set the preferred physical device */
        void SetPhysicalDevice(VkPhysicalDeviceType Type);

        /** Set the preferred physical device */
        void SetSurface(VkSurfaceKHR const& Surface);

        /** Create the logical device - Note: QueueIndices is a map of <Family Index, Num Queues> */
        void CreateLogicalDevice(std::unordered_map<std::uint32_t, std::uint32_t>&& QueueIndices, void const* pNext);

        /** Get the current associated logical device */
        [[nodiscard]] constexpr VkDevice const& GetLogicalDevice() const
        {
            return m_LogicalDevice;
        }

        /** Get the current associated physical device */
        [[nodiscard]] constexpr VkPhysicalDevice const& GetPhysicalDevice() const
        {
            return m_PhysicalDevice;
        }

        /** Get the current associated surface */
        [[nodiscard]] constexpr VkSurfaceKHR const& GetSurface() const
        {
            return m_Surface;
        }

        /** Get the supported formats for the associated surface */
        [[nodiscard]] constexpr std::vector<VkSurfaceFormatKHR> const& GetSurfaceFormat() const
        {
            return m_SurfaceFormat;
        }

        /** Get the properties of the associated device */
        [[nodiscard]] constexpr VkPhysicalDeviceProperties const& GetDeviceProperties() const
        {
            return m_DeviceProperties;
        }

        /** Get the selected device Vulkan API version */
        [[nodiscard]] constexpr std::uint32_t GetVulkanVersion() const noexcept
        {
            return m_DeviceProperties.apiVersion;
        }

        /** Retrieve basic Vulkan 1.0 features */
        [[nodiscard]] constexpr VkPhysicalDeviceFeatures const& GetDeviceFeatures() const
        {
            return m_DeviceFeatures;
        }

        /** Retrieve Vulkan 1.1 features */
        [[nodiscard]] constexpr VkPhysicalDeviceVulkan11Features const& GetVulkan11Features() const
        {
            return m_Vulkan11Features;
        }

        /** Retrieve Vulkan 1.2 features */
        [[nodiscard]] constexpr VkPhysicalDeviceVulkan12Features const& GetVulkan12Features() const
        {
            return m_Vulkan12Features;
        }

        /** Retrieve Vulkan 1.3 features */
        [[nodiscard]] constexpr VkPhysicalDeviceVulkan13Features const& GetVulkan13Features() const
        {
            return m_Vulkan13Features;
        }

        /** Retrieve Vulkan 1.4 features */
        [[nodiscard]] constexpr VkPhysicalDeviceVulkan14Features const& GetVulkan14Features() const
        {
            return m_Vulkan14Features;
        }

        /** Check if the API version is at least the specified one */
        [[nodiscard]] constexpr bool IsVersionAtLeast(const std::uint32_t Major, const std::uint32_t Minor) const noexcept
        {
            return VK_API_VERSION_MAJOR(m_DeviceProperties.apiVersion) > Major || (VK_API_VERSION_MAJOR(m_DeviceProperties.apiVersion) == Major &&
                VK_API_VERSION_MINOR(m_DeviceProperties.apiVersion)
                >= Minor);
        }

        /** Get the device queue family properties of the associated device */
        [[nodiscard]] constexpr std::vector<VkQueueFamilyProperties> const& GetDeviceQueueFamilyProperties() const
        {
            return m_DeviceQueueFamilyProperties;
        }

        /** Get the queue map that we got from the logical device creation - Map: <Family Index - Queues> */
        [[nodiscard]] constexpr std::unordered_map<std::uint32_t, std::vector<VkQueue>> const& GetQueues() const
        {
            return m_Queues;
        }

        /** Access the extension manager */
        [[nodiscard]] DeviceExtensions& GetExtensions()
        {
            return m_Extensions;
        }

        /** Find the first queue family that supports the requested flags */
        [[nodiscard]] std::optional<std::uint32_t> FindQueueFamilyIndex(VkQueueFlags Flags) const;

        /** Get a queue handle from the created logical device */
        [[nodiscard]] VkQueue GetQueue(std::uint32_t FamilyIndex, std::uint32_t QueueIndex = 0U) const;

        /** Wait until the logical device is idle */
        void WaitIdle() const;

        /** Wait until the given queue is idle */
        void Wait(VkQueue Queue) const;

        /** Wait for the specified fence */
        void Wait(VkFence Fence, VkBool32 WaitAll = VK_TRUE, std::uint64_t Timeout = UINT64_MAX) const;

        /** Get the available physical devices */
        [[nodiscard]] constexpr std::vector<VkPhysicalDevice> const& GetAvailableDevices()
        {
            return m_AvailableDevices;
        }

        //~ Begin of IRenderModule interface
        ~Device() override
        {
            Device::ClearResources();
        }

        [[nodiscard]] void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const&) const noexcept override
        {
            return &m_FeatureChain;
        }

        [[nodiscard]] void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept override
        {
            return nullptr;
        }

    private: /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources() override;
        //~ End of IRenderModule interface

        /** Get the available devices */
        void FetchAvailableDevices(VkInstance const& Instance);
    };
} // namespace luvk
