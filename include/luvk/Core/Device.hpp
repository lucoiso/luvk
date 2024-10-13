// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/Extensions.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <Volk/volk.h>

namespace luvk
{
    /** Render module responsible for device operations */
    class LUVKMODULE_API Device : public IRenderModule
    {
        VkPhysicalDevice m_PhysicalDevice {};

        VkPhysicalDeviceProperties m_DeviceProperties {};
        VkPhysicalDeviceProperties2 m_DeviceProperties2 {};

        VkPhysicalDeviceFeatures m_DeviceFeatures {};
        VkPhysicalDeviceFeatures2 m_DeviceFeatures2 {};

        DeviceExtensions m_Extensions { m_PhysicalDevice };
        std::vector<VkPhysicalDevice> m_AvailableDevices {};

    public:
        constexpr Device() = default;
        ~Device() override = default;

        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Get the current associated physical device */
        [[nodiscard]] constexpr inline VkPhysicalDevice const& GetPhysicalDevice() const
        {
            return m_PhysicalDevice;
        }

        /** Set the preferred physical device */
        void SetPhysicalDevice(std::uint8_t Index);

        /** Set the preferred physical device */
        void SetPhysicalDevice(VkPhysicalDeviceType Type);

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
    };
} // namespace luvk