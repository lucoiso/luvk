// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Device.hpp"
#include "luvk/Core/Renderer.hpp"

void luvk::Device::InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer)
{
    FetchAvailableDevices(dynamic_cast<luvk::Renderer*>(MainRenderer.get())->GetInstance());
}

void luvk::Device::SetPhysicalDevice(std::uint8_t const Index)
{
    if (m_Surface == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Ensure that you called SetSurface before this.");
    }

    m_PhysicalDevice = m_AvailableDevices.at(Index);

    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_DeviceProperties.First);
    vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &m_DeviceProperties.Second);

    vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_DeviceFeatures.First);
    vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &m_DeviceFeatures.Second);

    {
        std::uint32_t NumFormats = 0U;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &NumFormats, nullptr);

        m_SurfaceFormat.First.resize(NumFormats);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &NumFormats, std::data(m_SurfaceFormat.First));
    }

    {
        VkPhysicalDeviceSurfaceInfo2KHR const SurfaceInfo {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
                                                           .surface = m_Surface};
        std::uint32_t NumFormats = 0U;
        vkGetPhysicalDeviceSurfaceFormats2KHR(m_PhysicalDevice, &SurfaceInfo, &NumFormats, nullptr);

        m_SurfaceFormat.Second.resize(NumFormats);
        vkGetPhysicalDeviceSurfaceFormats2KHR(m_PhysicalDevice, &SurfaceInfo, &NumFormats, std::data(m_SurfaceFormat.Second));
    }

    m_Extensions.FillExtensionsContainer();

    // TODO : Surface register - needs to be generic and sent by the application using the library
    // TODO : Extension & Logical Devices Management
}

void luvk::Device::SetPhysicalDevice(VkPhysicalDeviceType const Type)
{
    for (std::size_t Iterator = 0U; Iterator < std::size(m_AvailableDevices); ++Iterator)
    {
        VkPhysicalDeviceProperties LocalProperty {};
        vkGetPhysicalDeviceProperties(m_AvailableDevices.at(Iterator), &LocalProperty);

        if (LocalProperty.deviceType == Type)
        {
            SetPhysicalDevice(Iterator);
            break;
        }
    }
}

void luvk::Device::FetchAvailableDevices(VkInstance const& Instance)
{
    std::uint32_t NumDevices = 0U;
    vkEnumeratePhysicalDevices(Instance, &NumDevices, nullptr);

    m_AvailableDevices.resize(NumDevices);
    vkEnumeratePhysicalDevices(Instance, &NumDevices, std::data(m_AvailableDevices));
}
