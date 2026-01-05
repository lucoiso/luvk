/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/Device.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"

using namespace luvk;

void Device::OnInitialize(IServiceLocator* ServiceLocator)
{
    m_ServiceLocator = ServiceLocator;
    IModule::OnInitialize(ServiceLocator);
}

void Device::OnShutdown()
{
    m_TransferContext.reset();

    if (m_LogicalDevice != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_LogicalDevice, nullptr);
        m_LogicalDevice = VK_NULL_HANDLE;
    }

    IModule::OnShutdown();
}

void Device::FetchAvailableDevices(const VkInstance Instance)
{
    std::uint32_t Count = 0;
    vkEnumeratePhysicalDevices(Instance, &Count, nullptr);
    m_AvailableDevices.resize(Count);
    vkEnumeratePhysicalDevices(Instance, &Count, std::data(m_AvailableDevices));
}

void Device::SelectBestPhysicalDevice()
{
    if (m_AvailableDevices.empty())
    {
        throw std::runtime_error("No physical devices found");
    }

    VkPhysicalDevice SelectedDevice = VK_NULL_HANDLE;
    std::int32_t BestScore = -1;

    for (const auto& PhysDevice : m_AvailableDevices)
    {
        VkPhysicalDeviceProperties Props;
        vkGetPhysicalDeviceProperties(PhysDevice, &Props);

        std::int32_t Score = 0;
        if (Props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            Score += 1000;
        }
        else if (Props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        {
            Score += 100;
        }

        Score += static_cast<std::int32_t>(Props.limits.maxImageDimension2D);

        if (Score > BestScore)
        {
            BestScore = Score;
            SelectedDevice = PhysDevice;
        }
    }

    if (SelectedDevice == VK_NULL_HANDLE)
    {
        SelectedDevice = m_AvailableDevices[0];
    }

    m_PhysicalDevice = SelectedDevice;
    SetupPhysicalDeviceVariables();
}

void Device::SetPhysicalDevice(const std::uint32_t Index)
{
    if (Index < std::size(m_AvailableDevices))
    {
        m_PhysicalDevice = m_AvailableDevices[Index];
        SetupPhysicalDeviceVariables();
    }
}

void Device::SetupPhysicalDeviceVariables()
{
    m_Extensions.SetDevice(m_PhysicalDevice);
    m_Extensions.FillExtensionsContainer();

    m_DeviceFeatures.pNext = &m_Vulkan11Features;
    m_Vulkan11Features.pNext = &m_Vulkan12Features;
    m_Vulkan12Features.pNext = &m_Vulkan13Features;
    m_Vulkan13Features.pNext = &m_Vulkan14Features;

    m_MeshShaderFeatures.meshShader = VK_TRUE;
    m_MeshShaderFeatures.taskShader = VK_TRUE;

    vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &m_DeviceFeatures);
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_DeviceProperties);

    m_Vulkan14Features.pNext = &m_MeshShaderFeatures;;

    std::uint32_t QueueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &QueueCount, nullptr);
    m_QueueFamilyProperties.resize(QueueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &QueueCount, std::data(m_QueueFamilyProperties));
}

void Device::CreateLogicalDevice(std::unordered_map<std::uint32_t, std::uint32_t>&& QueueIndices)
{
    std::vector<VkDeviceQueueCreateInfo> QueueInfos;
    constexpr float Priority = 1.0f;

    for (const auto& [Family, Count] : QueueIndices)
    {
        QueueInfos.push_back({.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                              .queueFamilyIndex = Family,
                              .queueCount = Count,
                              .pQueuePriorities = &Priority});
    }

    const auto ExtNames = m_Extensions.GetEnabledExtensionsNames();

    const VkDeviceCreateInfo CreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                        .pNext = &m_DeviceFeatures,
                                        .queueCreateInfoCount = static_cast<std::uint32_t>(std::size(QueueInfos)),
                                        .pQueueCreateInfos = std::data(QueueInfos),
                                        .enabledExtensionCount = static_cast<std::uint32_t>(std::size(ExtNames)),
                                        .ppEnabledExtensionNames = std::data(ExtNames)};

    if (!LUVK_EXECUTE(vkCreateDevice(m_PhysicalDevice, &CreateInfo, nullptr, &m_LogicalDevice)))
    {
        throw std::runtime_error("Failed to create logical device");
    }

    volkLoadDevice(m_LogicalDevice);

    for (const auto& [Family, Count] : QueueIndices)
    {
        for (std::uint32_t It = 0; It < Count; ++It)
        {
            VkQueue Queue;
            vkGetDeviceQueue(m_LogicalDevice, Family, It, &Queue);
            m_Queues[Family].push_back(Queue);
        }
    }

    if (const auto Index = FindQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
    {
        m_TransferContext = std::make_unique<TransferContext>();
        m_TransferContext->Initialize(m_LogicalDevice, GetQueue(VK_QUEUE_GRAPHICS_BIT), Index.value());
    }
}

std::optional<std::uint32_t> Device::FindQueueFamilyIndex(const VkQueueFlags Flags) const
{
    for (std::uint32_t It = 0; It < std::size(m_QueueFamilyProperties); ++It)
    {
        if ((m_QueueFamilyProperties[It].queueFlags & Flags) == Flags)
        {
            return It;
        }
    }
    return std::nullopt;
}

VkQueue Device::GetQueue(const VkQueueFlags Flags, const std::uint32_t Index) const
{
    if (const auto Family = FindQueueFamilyIndex(Flags))
    {
        if (m_Queues.contains(Family.value()) && Index < m_Queues.at(Family.value()).size())
        {
            return m_Queues.at(Family.value())[Index];
        }
    }
    return VK_NULL_HANDLE;
}

void Device::WaitIdle() const
{
    if (m_LogicalDevice)
    {
        vkDeviceWaitIdle(m_LogicalDevice);
    }
}

void Device::WaitQueue(const VkQueueFlags Flags) const
{
    vkQueueWaitIdle(GetQueue(Flags));
}

void Device::SubmitImmediate(std::function<void(VkCommandBuffer)>&& Recorder) const
{
    if (m_TransferContext)
    {
        m_TransferContext->SubmitImmediate(std::move(Recorder));
    }
}
