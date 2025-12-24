// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/Device.hpp"
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Renderer.hpp"

luvk::Device::Device(const std::shared_ptr<Renderer>& RendererModule)
    : m_RendererModule(RendererModule) {}

void luvk::Device::SetPhysicalDevice(const VkPhysicalDevice Device)
{
    m_PhysicalDevice = Device;
    m_Extensions.SetDevice(Device);

    m_FeatureChain.pNext = nullptr;
    m_Vulkan11Features   = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    m_Vulkan12Features   = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    m_Vulkan13Features   = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    m_Vulkan14Features   = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};

    m_FeatureChain.pNext     = &m_Vulkan11Features;
    m_Vulkan11Features.pNext = &m_Vulkan12Features;
    m_Vulkan12Features.pNext = &m_Vulkan13Features;
    m_Vulkan13Features.pNext = &m_Vulkan14Features;

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

void luvk::Device::SetSurface(const VkSurfaceKHR Surface)
{
    m_Surface = Surface;

    std::uint32_t NumFormats = 0U;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &NumFormats, nullptr);

    m_SurfaceFormat.resize(NumFormats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &NumFormats, std::data(m_SurfaceFormat));

    GetEventSystem().Execute(DeviceEvents::OnSetSurface);
}

void luvk::Device::CreateLogicalDevice(std::unordered_map<std::uint32_t, std::uint32_t>&& QueueIndices, const void* pNext)
{
    const void* FeatureChain = ConfigureExtensions(pNext);

    std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
    QueueCreateInfos.reserve(std::size(QueueIndices));

    static constexpr std::array<float, 64U> Priorities = []
    {
        std::array<float, 64U> Out{};
        Out.fill(1.F);
        return Out;
    }();

    for (const auto& [Index, Num] : QueueIndices)
    {
        QueueCreateInfos.push_back(VkDeviceQueueCreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                           .queueFamilyIndex = Index,
                                                           .queueCount = Num,
                                                           .pQueuePriorities = std::data(Priorities)});
    }

    const auto Extensions = m_Extensions.GetEnabledExtensionsNames();
    const auto Layers     = m_Extensions.GetEnabledLayersNames();

    const VkDeviceCreateInfo DeviceCreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                              .pNext = FeatureChain,
                                              .queueCreateInfoCount = static_cast<std::uint32_t>(std::size(QueueCreateInfos)),
                                              .pQueueCreateInfos = std::data(QueueCreateInfos),
                                              .enabledLayerCount = static_cast<std::uint32_t>(std::size(Layers)),
                                              .ppEnabledLayerNames = std::data(Layers),
                                              .enabledExtensionCount = static_cast<std::uint32_t>(std::size(Extensions)),
                                              .ppEnabledExtensionNames = std::data(Extensions)};

    if (!LUVK_EXECUTE(vkCreateDevice(m_PhysicalDevice, &DeviceCreateInfo, nullptr, &m_LogicalDevice)))
    {
        throw std::runtime_error("Failed to create the logical device.");
    }

    volkLoadDevice(m_LogicalDevice);

    for (const auto& QueueCreateInfoIt : QueueCreateInfos)
    {
        std::vector<VkQueue> QueueList(QueueCreateInfoIt.queueCount);

        for (std::uint32_t Index = 0U; Index < QueueCreateInfoIt.queueCount; ++Index)
        {
            vkGetDeviceQueue(m_LogicalDevice, QueueCreateInfoIt.queueFamilyIndex, Index, &QueueList.at(Index));
        }

        m_Queues.emplace(QueueCreateInfoIt.queueFamilyIndex, std::move(QueueList));
    }

    GetEventSystem().Execute(DeviceEvents::OnCreatedLogicalDevice);
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

const void* luvk::Device::ConfigureExtensions(const void* pNext)
{
    const void*          FeatureChain = pNext;
    const RenderModules& Modules      = m_RendererModule->GetModules();

    auto ProcessModule = [&](const std::shared_ptr<IRenderModule>& Module)
    {
        if (Module == nullptr)
        {
            return;
        }

        if (const IExtensionsModule* const ExtModule = dynamic_cast<const IExtensionsModule*>(Module.get());
            ExtModule != nullptr)
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

        if (const IFeatureChainModule* const FeatChainModule = dynamic_cast<const IFeatureChainModule*>(Module.get());
            FeatChainModule != nullptr)
        {
            if (const void* const Chain = FeatChainModule->GetDeviceFeatureChain();
                Chain != nullptr)
            {
                VkBaseOutStructure* Base = const_cast<VkBaseOutStructure*>(static_cast<const VkBaseOutStructure*>(Chain));

                while (Base->pNext != nullptr)
                {
                    Base = Base->pNext;
                }

                Base->pNext  = const_cast<VkBaseOutStructure*>(static_cast<const VkBaseOutStructure*>(FeatureChain));
                FeatureChain = Chain;
            }
        }
    };

    ProcessModule(Modules.DeviceModule);
    ProcessModule(Modules.SynchronizationModule);

    for (const std::shared_ptr<IRenderModule>& ModuleIt : Modules.ExtraModules)
    {
        ProcessModule(ModuleIt);
    }

    if (m_Surface != VK_NULL_HANDLE)
    {
        m_Extensions.SetExtensionState("", VK_KHR_SWAPCHAIN_EXTENSION_NAME, true);
    }

    return FeatureChain;
}

void luvk::Device::FetchAvailableDevices(const VkInstance Instance)
{
    std::uint32_t NumDevices = 0U;
    vkEnumeratePhysicalDevices(Instance, &NumDevices, nullptr);

    m_AvailableDevices.resize(NumDevices);
    vkEnumeratePhysicalDevices(Instance, &NumDevices, std::data(m_AvailableDevices));
}

std::optional<std::uint32_t> luvk::Device::FindQueueFamilyIndex(const VkQueueFlags Flags) const
{
    for (std::uint32_t Index = 0U; Index < std::size(m_DeviceQueueFamilyProperties); ++Index)
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

void luvk::Device::Wait(const VkQueue Queue) const
{
    if (Queue != VK_NULL_HANDLE)
    {
        vkQueueWaitIdle(Queue);
    }
}

void luvk::Device::Wait(const VkFence Fence, const VkBool32 WaitAll, const std::uint64_t Timeout) const
{
    if (Fence != VK_NULL_HANDLE)
    {
        vkWaitForFences(m_LogicalDevice, 1, &Fence, WaitAll, Timeout);
    }
}
