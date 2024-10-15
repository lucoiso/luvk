// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Device.hpp"
#include "luvk/Core/Renderer.hpp"

void luvk::Device::SetPhysicalDevice(VkPhysicalDevice const &Device)
{
    m_PhysicalDevice = Device;

    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_DeviceProperties);
    vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_DeviceFeatures);

    m_Extensions.FillExtensionsContainer();

    std::uint32_t NumProperties = 0U;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &NumProperties, nullptr);

    m_DeviceQueueFamilyProperties.resize(NumProperties);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &NumProperties, std::data(m_DeviceQueueFamilyProperties));
}

void luvk::Device::SetPhysicalDevice(std::uint8_t const Index)
{
    SetPhysicalDevice(m_AvailableDevices.at(Index));
}

void luvk::Device::SetPhysicalDevice(VkPhysicalDeviceType const Type)
{
    for (auto const& AvailableDeviceIt : m_AvailableDevices)
    {
        VkPhysicalDeviceProperties LocalProperty {};
        vkGetPhysicalDeviceProperties(AvailableDeviceIt, &LocalProperty);

        if (LocalProperty.deviceType == Type)
        {
            SetPhysicalDevice(AvailableDeviceIt);
            break;
        }
    }
}

void luvk::Device::SetSurface(VkSurfaceKHR const &Surface)
{
    m_Surface = Surface;

    std::uint32_t NumFormats = 0U;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &NumFormats, nullptr);

    m_SurfaceFormat.resize(NumFormats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &NumFormats, std::data(m_SurfaceFormat));
}

void luvk::Device::CreateLogicalDevice(std::unordered_map<std::uint32_t, std::uint32_t> const& QueueIndices, void* const& pNext)
{
    auto const QueueFamilyIndices = QueueIndices | std::views::keys;
    std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos(std::size(QueueFamilyIndices));

    constexpr std::array<float, 64U> Priorities { 0.F }; // 0.F, 0.F, 0.F...
    for (auto const& [Index, Num] : QueueIndices)
    {
        QueueCreateInfos.push_back(VkDeviceQueueCreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                           .queueFamilyIndex = Index,
                                                           .queueCount = Num,
                                                           .pQueuePriorities = std::data(Priorities)});
    }

    auto const Layers = m_Extensions.GetEnabledLayersNames();
    auto const Extensions = m_Extensions.GetEnabledExtensionsNames();

    VkDeviceCreateInfo const DeviceCreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                              .pNext = pNext,
                                              .queueCreateInfoCount = static_cast<std::uint32_t>(std::size(QueueCreateInfos)),
                                              .pQueueCreateInfos = std::data(QueueCreateInfos),
                                              .enabledLayerCount = static_cast<std::uint32_t>(std::size(Layers)),
                                              .ppEnabledLayerNames = std::data(Layers),
                                              .enabledExtensionCount = static_cast<std::uint32_t>(std::size(Extensions)),
                                              .ppEnabledExtensionNames = std::data(Extensions)};

    vkCreateDevice(m_PhysicalDevice, &DeviceCreateInfo, nullptr, &m_LogicalDevice);
    volkLoadDevice(m_LogicalDevice);

    for (VkDeviceQueueCreateInfo const& QueueCreateInfoIt : QueueCreateInfos)
    {
        std::vector<VkQueue> QueueList(QueueCreateInfoIt.queueCount);

        for (std::uint32_t Index = 0U; Index < QueueCreateInfoIt.queueCount; ++Index)
        {
            vkGetDeviceQueue(m_LogicalDevice, QueueCreateInfoIt.queueFamilyIndex, Index, &QueueList.at(Index));
        }

        m_Queues.emplace(QueueCreateInfoIt.queueFamilyIndex, QueueList);
    }
}

void luvk::Device::InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer)
{
    FetchAvailableDevices(dynamic_cast<luvk::Renderer*>(MainRenderer.get())->GetInstance());
}

void luvk::Device::ClearResources(IRenderModule* const MainRenderer)
{
    if (m_LogicalDevice != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_LogicalDevice, nullptr);
    }

    if (m_Surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(dynamic_cast<luvk::Renderer*>(MainRenderer)->GetInstance(), m_Surface, nullptr);
    }
}

void luvk::Device::FetchAvailableDevices(VkInstance const& Instance)
{
    std::uint32_t NumDevices = 0U;
    vkEnumeratePhysicalDevices(Instance, &NumDevices, nullptr);

    m_AvailableDevices.resize(NumDevices);
    vkEnumeratePhysicalDevices(Instance, &NumDevices, std::data(m_AvailableDevices));
}
