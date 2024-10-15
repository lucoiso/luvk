// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/Extensions.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <volk.h>
#include <cstdint>
#include <unordered_map>

namespace luvk
{
    /** Render module responsible for device operations */
    class LUVKMODULE_API Device : public IRenderModule
    {
        VkDevice m_LogicalDevice {};
        VkPhysicalDevice m_PhysicalDevice {};
        VkSurfaceKHR m_Surface {};

        DeviceExtensions m_Extensions { m_PhysicalDevice };
        std::vector<VkPhysicalDevice> m_AvailableDevices {};

        VkPhysicalDeviceFeatures m_DeviceFeatures {};
        VkPhysicalDeviceProperties m_DeviceProperties {};
        std::vector<VkSurfaceFormatKHR> m_SurfaceFormat {};
        std::vector<VkQueueFamilyProperties> m_DeviceQueueFamilyProperties {};

        std::unordered_map<std::uint32_t, std::vector<VkQueue>> m_Queues {};

    public:
        constexpr Device() = default;
        ~Device() override = default;

        /** Set the preferred physical device */
        void SetPhysicalDevice(VkPhysicalDevice const& Device);

        /** Set the preferred physical device */
        void SetPhysicalDevice(std::uint8_t Index);

        /** Set the preferred physical device */
        void SetPhysicalDevice(VkPhysicalDeviceType Type);

        /** Set the preferred physical device */
        void SetSurface(VkSurfaceKHR const& Surface);

        /** Create the logical device - Note: QueueIndices is a map of <Family Index, Num Queues> */
        void CreateLogicalDevice(std::unordered_map<std::uint32_t, std::uint32_t> const& QueueIndices, void* const& pNext);

        /** Get the current associated logical device */
        [[nodiscard]] constexpr inline VkDevice const& GetLogicalDevice() const
        {
            return m_LogicalDevice;
        }

        /** Get the current associated physical device */
        [[nodiscard]] constexpr inline VkPhysicalDevice const& GetPhysicalDevice() const
        {
            return m_PhysicalDevice;
        }

        /** Get the current associated surface */
        [[nodiscard]] constexpr inline VkSurfaceKHR const& GetSurface() const
        {
            return m_Surface;
        }

        /** Get the supported formats for the associated surface */
        [[nodiscard]] constexpr inline std::vector<VkSurfaceFormatKHR> const& GetSurfaceFormat() const
        {
            return m_SurfaceFormat;
        }

        /** Get the properties of the associated device */
        [[nodiscard]] constexpr inline VkPhysicalDeviceProperties const& GetDeviceProperties() const
        {
            return m_DeviceProperties;
        }

        /** Get the supported features for the associated device */
        [[nodiscard]] constexpr inline VkPhysicalDeviceFeatures const& GetDeviceFeatures() const
        {
            return m_DeviceFeatures;
        }

        /** Get the device queue family properties of the associated device */
        [[nodiscard]] constexpr inline std::vector<VkQueueFamilyProperties> const& GetDeviceQueueFamilyProperties() const
        {
            return m_DeviceQueueFamilyProperties;
        }

        /** Get the queue map that we got from the logical device creation - Map: <Family Index - Queues> */
        [[nodiscard]] constexpr inline std::unordered_map<std::uint32_t, std::vector<VkQueue>> const& GetQueues() const
        {
            return m_Queues;
        }

        /** Get the available device layers and extensions */
        [[nodiscard]] inline DeviceExtensions& GetExtensions()
        {
            return m_Extensions;
        }

        /** Get the available device layers and extensions */
        [[nodiscard]] constexpr inline std::vector<VkPhysicalDevice> const& GetAvailableDevices()
        {
            return m_AvailableDevices;
        }

    private:
        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources(IRenderModule* MainRenderer) override;

        /** Get the available devices */
        void FetchAvailableDevices(VkInstance const& Instance);
    };
} // namespace luvk