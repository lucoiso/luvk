// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/Extensions.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <volk.h>
#include <cstdint>

namespace luvk
{
    /** Render module responsible for device operations */
    class LUVKMODULE_API Device : public IRenderModule
    {
        VkDevice m_LogicalDevice {};
        VkPhysicalDevice m_PhysicalDevice {};
        VkSurfaceKHR m_Surface {};

        Pair<std::vector<VkSurfaceFormatKHR>, std::vector<VkSurfaceFormat2KHR>> m_SurfaceFormat {};
        Pair<VkPhysicalDeviceProperties, VkPhysicalDeviceProperties2> m_DeviceProperties {};
        Pair<VkPhysicalDeviceFeatures, VkPhysicalDeviceFeatures2> m_DeviceFeatures {};

        DeviceExtensions m_Extensions { m_PhysicalDevice };
        std::vector<VkPhysicalDevice> m_AvailableDevices {};

    public:
        constexpr Device() = default;
        ~Device() override;

        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Set the preferred physical device */
        void SetPhysicalDevice(std::uint8_t Index);

        /** Set the preferred physical device */
        void SetPhysicalDevice(VkPhysicalDeviceType Type);

        /** Set the preferred physical device */
        inline void SetSurface(VkSurfaceKHR const& Surface)
        {
            m_Surface = Surface;
        }

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
        [[nodiscard]] constexpr inline Pair<std::vector<VkSurfaceFormatKHR>, std::vector<VkSurfaceFormat2KHR>> const& GetSurfaceFormat() const
        {
            return m_SurfaceFormat;
        }

        /** Get the properties of the associated device */
        [[nodiscard]] constexpr inline Pair<VkPhysicalDeviceProperties, VkPhysicalDeviceProperties2> const& GetDeviceProperties() const
        {
            return m_DeviceProperties;
        }

        /** Get the supported features for the associated device */
        [[nodiscard]] constexpr inline Pair<VkPhysicalDeviceFeatures, VkPhysicalDeviceFeatures2> const& GetDeviceFeatures() const
        {
            return m_DeviceFeatures;
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
        /** Get the available devices */
        void FetchAvailableDevices(VkInstance const& Instance);

        /** Initialize the logical device */
        void InitializeLogicalDevice();
    };
} // namespace luvk