// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/Device.hpp"
#include <iterator>
#include <ranges>

#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Renderer.hpp"

luvk::Device::Device(const std::shared_ptr<Renderer>& RendererModule)
    : m_RendererModule(RendererModule) {}

void luvk::Device::SetPhysicalDevice(const VkPhysicalDevice& Device)
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

void luvk::Device::SetPhysicalDevice(const std::uint8_t Index)
{
    SetPhysicalDevice(m_AvailableDevices.at(Index));
}

void luvk::Device::SetPhysicalDevice(const VkPhysicalDeviceType Type)
{
    for (const auto& AvailableDeviceIt : m_AvailableDevices)
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

void luvk::Device::SetSurface(const VkSurfaceKHR& Surface)
{
    m_Surface = Surface;

    std::uint32_t NumFormats = 0U;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &NumFormats, nullptr);

    m_SurfaceFormat.resize(NumFormats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &NumFormats, std::data(m_SurfaceFormat));
}

void luvk::Device::CreateLogicalDevice(UnorderedMap<std::uint32_t, std::uint32_t>&& QueueIndices, const void* pNext)
{
    const void* FeatureChain = pNext;

    for (const auto& [Index, Module] : m_RendererModule->GetModules())
    {
        if (const auto* const ExtModule = dynamic_cast<const IExtensionsModule*>(Module.get()))
        {
            for (const auto& [LayerIt, ExtensionContainerIt] : ExtModule->GetDeviceExtensions())
            {
                m_Extensions.SetLayerState(LayerIt, true);

                for (const std::string_view ExtensionIt : ExtensionContainerIt)
                {
                    m_Extensions.SetExtensionState(LayerIt, ExtensionIt, true);
                }
            }
        }

        if (const auto* const FeatChainModule = dynamic_cast<const IFeatureChainModule*>(Module.get()))
        {
            if (const auto Chain = FeatChainModule->GetDeviceFeatureChain())
            {
                auto Base = const_cast<VkBaseOutStructure*>(static_cast<const VkBaseOutStructure*>(Chain));

                while (Base->pNext)
                {
                    Base = Base->pNext;
                }

                Base->pNext = const_cast<VkBaseOutStructure*>(static_cast<const VkBaseOutStructure*>(FeatureChain));
                FeatureChain = Chain;
            }
        }
    }

    if (m_Surface != VK_NULL_HANDLE)
    {
        m_Extensions.SetExtensionState("", VK_KHR_SWAPCHAIN_EXTENSION_NAME, true);
    }

    const auto NumFamilies = std::size(QueueIndices);
    Vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
    QueueCreateInfos.reserve(NumFamilies);

    for (const auto& [Index, Num] : QueueIndices)
    {
        constexpr std::array<float, 64U> Priorities{0.F};

        QueueCreateInfos.push_back(VkDeviceQueueCreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                           .queueFamilyIndex = Index,
                                                           .queueCount = Num,
                                                           .pQueuePriorities = std::data(Priorities)});
    }

    Vector<const char*> Extensions = m_Extensions.GetEnabledExtensionsNames();
    Vector<const char*> Layers = m_Extensions.GetEnabledLayersNames();

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

    for (const VkDeviceQueueCreateInfo& QueueCreateInfoIt : QueueCreateInfos)
    {
        Vector<VkQueue> QueueList(QueueCreateInfoIt.queueCount);

        for (std::uint32_t Index = 0U; Index < QueueCreateInfoIt.queueCount;
             ++Index)
        {
            vkGetDeviceQueue(m_LogicalDevice, QueueCreateInfoIt.queueFamilyIndex, Index, &QueueList.at(Index));
        }

        m_Queues.emplace(QueueCreateInfoIt.queueFamilyIndex, QueueList);
    }

    GetEventSystem().Execute(DeviceEvents::OnChangedLogicalDevice);
}

void luvk::Device::InitializeResources()
{
    FetchAvailableDevices(m_RendererModule->GetInstance());
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
        vkDestroySurfaceKHR(m_RendererModule->GetInstance(), m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }
}

void luvk::Device::FetchAvailableDevices(const VkInstance& Instance)
{
    std::uint32_t NumDevices = 0U;
    vkEnumeratePhysicalDevices(Instance, &NumDevices, nullptr);

    m_AvailableDevices.resize(NumDevices);
    vkEnumeratePhysicalDevices(Instance, &NumDevices, std::data(m_AvailableDevices));
}

std::optional<std::uint32_t> luvk::Device::FindQueueFamilyIndex(const VkQueueFlags Flags) const
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

VkQueue luvk::Device::GetQueue(const std::uint32_t FamilyIndex, const std::uint32_t QueueIndex) const
{
    if (const auto It = m_Queues.find(FamilyIndex);

        It != std::end(m_Queues))
    {
        if (QueueIndex < std::size(It->Second))
        {
            return It->Second.at(QueueIndex);
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

void luvk::Device::Wait(const VkQueue& Queue) const
{
    if (Queue != VK_NULL_HANDLE)
    {
        vkQueueWaitIdle(Queue);
    }
}

void luvk::Device::Wait(const VkFence& Fence, const VkBool32 WaitAll, const std::uint64_t Timeout) const
{
    if (Fence != VK_NULL_HANDLE)
    {
        vkWaitForFences(m_LogicalDevice, 1, &Fence, WaitAll, Timeout);
    }
}
