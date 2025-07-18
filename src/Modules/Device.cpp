// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/Device.hpp"
#include <iterator>
#include <ranges>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Renderer.hpp"

void luvk::Device::SetPhysicalDevice(VkPhysicalDevice const& Device)
{
    m_PhysicalDevice = Device;
    m_Extensions.SetDevice(Device);

    m_FeatureChain.pNext = nullptr;
    m_Vulkan11Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    m_Vulkan12Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    m_Vulkan13Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    m_Vulkan14Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};

    m_FeatureChain.pNext = &m_Vulkan11Features;
    m_Vulkan11Features.pNext = &m_Vulkan12Features;
    m_Vulkan12Features.pNext = &m_Vulkan13Features;
    m_Vulkan13Features.pNext = &m_Vulkan14Features;
    m_Vulkan14Features.pNext = nullptr;

    vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &m_FeatureChain);

    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_DeviceProperties);

    std::uint32_t NumProperties = 0U;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &NumProperties, nullptr);

    m_DeviceQueueFamilyProperties.resize(NumProperties);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &NumProperties, std::data(m_DeviceQueueFamilyProperties));

    m_Extensions.FillExtensionsContainer();

    GetEventSystem().Execute(DeviceEvents::OnSelectedPhysicalDevice);
}

void luvk::Device::SetPhysicalDevice(std::uint8_t const Index)
{
    SetPhysicalDevice(m_AvailableDevices.at(Index));
}

void luvk::Device::SetPhysicalDevice(VkPhysicalDeviceType const Type)
{
    for (auto const& AvailableDeviceIt : m_AvailableDevices)
    {
        VkPhysicalDeviceProperties LocalProperty{};
        vkGetPhysicalDeviceProperties(AvailableDeviceIt, &LocalProperty);

        if (LocalProperty.deviceType == Type)
        {
            SetPhysicalDevice(AvailableDeviceIt);
            break;
        }
    }
}

void luvk::Device::SetSurface(VkSurfaceKHR const& Surface)
{
    m_Surface = Surface;

    std::uint32_t NumFormats = 0U;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &NumFormats, nullptr);

    m_SurfaceFormat.resize(NumFormats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &NumFormats, std::data(m_SurfaceFormat));
}

void luvk::Device::CreateLogicalDevice(std::unordered_map<std::uint32_t, std::uint32_t>&& QueueIndices, void const* pNext)
{
    void const* FeatureChain = pNext;

    if (m_Renderer)
    {
        for (auto const& [Index, Module] : m_Renderer->GetModules())
        {
            for (auto const& Ext : Module->GetRequiredDeviceExtensions())
            {
                m_Extensions.SetLayerState(Ext.first, true);

                for (std::string_view const ExtensionIt : Ext.second)
                {
                    m_Extensions.SetExtensionState(Ext.first, ExtensionIt, true);
                }
            }

            if (auto const Chain = Module->GetDeviceFeatureChain(shared_from_this()))
            {
                auto* Base = const_cast<VkBaseOutStructure*>(static_cast<VkBaseOutStructure const*>(Chain));

                while (Base->pNext)
                {
                    Base = const_cast<VkBaseOutStructure*>(static_cast<VkBaseOutStructure const*>(Base->pNext));
                }

                Base->pNext = const_cast<VkBaseOutStructure*>(static_cast<VkBaseOutStructure const*>(FeatureChain));
                FeatureChain = Chain;
            }
        }
    }

    if (m_Surface != VK_NULL_HANDLE)
    {
        m_Extensions.SetExtensionState("", VK_KHR_SWAPCHAIN_EXTENSION_NAME, true);
    }

    auto const QueueFamilyIndices = QueueIndices | std::views::keys;
    std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
    QueueCreateInfos.reserve(std::size(QueueFamilyIndices));

    for (auto const& [Index, Num] : QueueIndices)
    {
        constexpr std::array<float, 64U> Priorities{0.F};

        QueueCreateInfos.push_back(VkDeviceQueueCreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                           .queueFamilyIndex = Index,
                                                           .queueCount = Num,
                                                           .pQueuePriorities = std::data(Priorities)});
    }

    std::vector<const char*> Extensions = m_Extensions.GetEnabledExtensionsNames();
    std::vector<const char*> Layers = m_Extensions.GetEnabledLayersNames();

    const VkDeviceCreateInfo DeviceCreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                              .pNext = const_cast<void*>(FeatureChain),
                                              .queueCreateInfoCount = static_cast<std::uint32_t>(std::size(QueueCreateInfos)),
                                              .pQueueCreateInfos = std::data(QueueCreateInfos),
                                              .enabledLayerCount = static_cast<std::uint32_t>(std::size(Layers)),
                                              .ppEnabledLayerNames = std::data(Layers),
                                              .enabledExtensionCount = static_cast<std::uint32_t>(std::size(Extensions)),
                                              .ppEnabledExtensionNames = std::data(Extensions),
                                              .pEnabledFeatures = nullptr};

    if (!LUVK_EXECUTE(vkCreateDevice(m_PhysicalDevice, &DeviceCreateInfo, nullptr, &m_LogicalDevice)))
    {
        throw std::runtime_error("Failed to create the logical device.");
    }

    volkLoadDevice(m_LogicalDevice);

    for (VkDeviceQueueCreateInfo const& QueueCreateInfoIt : QueueCreateInfos)
    {
        std::vector<VkQueue> QueueList(QueueCreateInfoIt.queueCount);

        for (std::uint32_t Index = 0U; Index < QueueCreateInfoIt.queueCount;
             ++Index)
        {
            vkGetDeviceQueue(m_LogicalDevice, QueueCreateInfoIt.queueFamilyIndex, Index, &QueueList.at(Index));
        }

        m_Queues.emplace(QueueCreateInfoIt.queueFamilyIndex, QueueList);
    }

    GetEventSystem().Execute(DeviceEvents::OnChangedLogicalDevice);
}

void luvk::Device::InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer)
{
    m_Renderer = std::dynamic_pointer_cast<Renderer>(MainRenderer);
    m_Instance = m_Renderer->GetInstance();
    FetchAvailableDevices(m_Instance);
}

void luvk::Device::ClearResources()
{
    if (m_LogicalDevice != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_LogicalDevice, nullptr);
        m_LogicalDevice = VK_NULL_HANDLE;
    }

    if (m_Surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }
}

void luvk::Device::FetchAvailableDevices(VkInstance const& Instance)
{
    std::uint32_t NumDevices = 0U;
    vkEnumeratePhysicalDevices(Instance, &NumDevices, nullptr);

    m_AvailableDevices.resize(NumDevices);
    vkEnumeratePhysicalDevices(Instance, &NumDevices, std::data(m_AvailableDevices));
}

std::optional<std::uint32_t> luvk::Device::FindQueueFamilyIndex(VkQueueFlags const Flags) const
{
    for (std::uint32_t Index = 0U;
         Index < std::size(m_DeviceQueueFamilyProperties); ++Index)
    {
        if ((m_DeviceQueueFamilyProperties.at(Index).queueFlags & Flags) == Flags)
        {
            return Index;
        }
    }

    return std::nullopt;
}

VkQueue luvk::Device::GetQueue(std::uint32_t const FamilyIndex, std::uint32_t const QueueIndex) const
{
    if (const auto It = m_Queues.find(FamilyIndex);

        It != std::end(m_Queues))
    {
        if (QueueIndex < std::size(It->second))
        {
            return It->second.at(QueueIndex);
        }
    }

    return VK_NULL_HANDLE;
}

void luvk::Device::WaitIdle() const
{
    if (m_LogicalDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_LogicalDevice);
    }
}

void luvk::Device::Wait(VkQueue const Queue) const
{
    if (Queue != VK_NULL_HANDLE)
    {
        vkQueueWaitIdle(Queue);
    }
}

void luvk::Device::Wait(VkFence const Fence, VkBool32 const WaitAll, std::uint64_t const Timeout) const
{
    if (Fence != VK_NULL_HANDLE)
    {
        vkWaitForFences(m_LogicalDevice, 1, &Fence, WaitAll, Timeout);
    }
}
