/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>
#include <volk.h>
#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Interfaces/IFeatureChainModule.hpp"
#include "luvk/Interfaces/IModule.hpp"
#include "luvk/Managers/Extensions.hpp"
#include "luvk/Managers/TransferContext.hpp"

namespace luvk
{
    /**
     * Manages Logical and Physical Device.
     * Enforces Vulkan 1.4 features (Sync2, DynamicRendering, etc.).
     */
    class LUVK_API Device : public IModule
                          , public IFeatureChainModule
                          , public IExtensionsModule
    {
    protected:
        /** The created Vulkan logical device handle. */
        VkDevice m_LogicalDevice{VK_NULL_HANDLE};

        /** The selected Vulkan physical device handle. */
        VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};

        /** Manager for device-level extensions. */
        DeviceExtensions m_Extensions{};

        /** Base structure for chaining device features. */
        VkPhysicalDeviceFeatures2 m_DeviceFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

        /** Vulkan 1.1 features structure. */
        VkPhysicalDeviceVulkan11Features m_Vulkan11Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};

        /** Vulkan 1.2 features structure. */
        VkPhysicalDeviceVulkan12Features m_Vulkan12Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};

        /** Vulkan 1.3 features structure. */
        VkPhysicalDeviceVulkan13Features m_Vulkan13Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

        /** Vulkan 1.4 features structure (currently a proxy for 1.3 + extensions). */
        VkPhysicalDeviceVulkan14Features m_Vulkan14Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};

        /** Mesh Shader features structure. */
        VkPhysicalDeviceMeshShaderFeaturesEXT m_MeshShaderFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};

        /** Properties of the selected physical device. */
        VkPhysicalDeviceProperties m_DeviceProperties{};

        /** List of all physical devices found in the system. */
        std::vector<VkPhysicalDevice> m_AvailableDevices{};

        /** Properties of the available queue families on the physical device. */
        std::vector<VkQueueFamilyProperties> m_QueueFamilyProperties{};

        /** Map of Queue Family Index -> List of Queues retrieved. */
        std::unordered_map<std::uint32_t, std::vector<VkQueue>> m_Queues{};

        /** Dedicated context for submitting immediate transfer commands. */
        std::unique_ptr<TransferContext> m_TransferContext{};

        /** Pointer to the central service locator. */
        IServiceLocator* m_ServiceLocator{nullptr};

    public:
        /** Default destructor. */
        ~Device() override = default;

        /** Called upon module initialization. */
        void OnInitialize(IServiceLocator* ServiceLocator) override;

        /** Called upon module shutdown (destroys logical device and transfer context). */
        void OnShutdown() override;

        /**
         * Queries and stores all available physical devices on the system.
         * @param Instance The Vulkan instance to query from.
         */
        void FetchAvailableDevices(VkInstance Instance);

        /** Get the required device extensions (currently Mesh Shader). */
        [[nodiscard]] ExtensionMap GetDeviceExtensions() const noexcept override
        {
            return {{"", {VK_EXT_MESH_SHADER_EXTENSION_NAME}}};
        }

        /**
         * Select the discrete GPU if available, falling back to Integrated.
         * Populates features and extensions based on the selection.
         * @param Surface The window surface to check for presentation support.
         */
        void SelectBestPhysicalDevice(VkSurfaceKHR Surface);

        /**
         * Manual selection by index
         * @param Index The index in the m_AvailableDevices vector.
         */
        void SetPhysicalDevice(std::uint32_t Index);

        /**
         * Creates logical device forcing Vulkan 1.4 features.
         * @param QueueIndices Map of Queue Family Index -> Queue Count to create.
         */
        void CreateLogicalDevice(std::unordered_map<std::uint32_t, std::uint32_t>&& QueueIndices);

        /** Get the created logical device handle. */
        [[nodiscard]] constexpr VkDevice GetLogicalDevice() const noexcept
        {
            return m_LogicalDevice;
        }

        /** Get the selected physical device handle. */
        [[nodiscard]] constexpr VkPhysicalDevice GetPhysicalDevice() const noexcept
        {
            return m_PhysicalDevice;
        }

        /** Get the device extensions manager. */
        [[nodiscard]] constexpr DeviceExtensions& GetExtensions() noexcept
        {
            return m_Extensions;
        }

        /**
         * Finds the index of a queue family that supports the given flags.
         * @param Flags The required VkQueueFlags.
         * @return Optional containing the index or nullopt if not found.
         */
        [[nodiscard]] std::optional<std::uint32_t> FindQueueFamilyIndex(VkQueueFlags Flags) const;

        /**
         * Retrieves a specific queue from a family supporting the given flags.
         * @param Flags The required VkQueueFlags to find the family.
         * @param Index The index of the queue within that family.
         * @return The VkQueue handle.
         */
        [[nodiscard]] VkQueue GetQueue(VkQueueFlags Flags, std::uint32_t Index = 0) const;

        /** Waits for the logical device to become idle. */
        void WaitIdle() const;

        /** Waits for a specific queue to become idle. */
        void WaitQueue(VkQueueFlags Flags) const;

        /**
         * Submits a command for immediate execution on a transfer queue.
         * @param Recorder Function to record commands into the transient command buffer.
         */
        void SubmitImmediate(std::function<void(VkCommandBuffer)>&& Recorder) const;

    private:
        /** Populates the device properties, queue family properties, and feature structures. */
        void SetupPhysicalDeviceVariables();
    };
}
