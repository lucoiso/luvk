// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "ContextInternals.hpp"
#include "luvk/Core/Context.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include "luvk/Core/Bindless.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/Module.hpp"
#include "luvk/Core/Upload.hpp"

namespace
{
    constexpr auto ValidationLayerName = "VK_LAYER_KHRONOS_validation";

    std::atomic_bool g_ContextAlive = false;

    void InitializeLoader()
    {
        static VkResult LoaderResult = VK_ERROR_INITIALIZATION_FAILED;
        static std::once_flag LoaderOnce;
        std::call_once(LoaderOnce,
                       []
                       {
                           LoaderResult = volkInitialize();
                       });
        luvk::Check(LoaderResult, "volkInitialize (is a Vulkan loader installed?)");
    }

    bool Contains(const std::vector<std::string>& Values, const std::string_view Value)
    {
        return std::ranges::find(Values, Value) != std::end(Values);
    }

    void AddUnique(std::vector<std::string>& Values, const std::string_view Value)
    {
        if (!Contains(Values, Value))
        {
            Values.emplace_back(Value);
        }
    }

    std::vector<const char*> ToPointers(const std::vector<std::string>& Values)
    {
        std::vector<const char*> Output;
        Output.reserve(std::size(Values));
        for (const std::string& It : Values)
        {
            Output.push_back(It.c_str());
        }
        return Output;
    }

    luvk::DebugSeverity ToSeverity(const VkDebugUtilsMessageSeverityFlagBitsEXT Severity)
    {
        if ((Severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U)
        {
            return luvk::DebugSeverity::Error;
        }
        if ((Severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U)
        {
            return luvk::DebugSeverity::Warning;
        }
        if ((Severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0U)
        {
            return luvk::DebugSeverity::Info;
        }
        return luvk::DebugSeverity::Verbose;
    }

    VkDebugUtilsMessageSeverityFlagsEXT SeverityMask(const luvk::DebugSeverity Minimum)
    {
        VkDebugUtilsMessageSeverityFlagsEXT Mask = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        if (Minimum <= luvk::DebugSeverity::Warning)
        {
            Mask |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        }
        if (Minimum <= luvk::DebugSeverity::Info)
        {
            Mask |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        }
        if (Minimum <= luvk::DebugSeverity::Verbose)
        {
            Mask |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        }
        return Mask;
    }

    const char* SeverityName(const luvk::DebugSeverity Severity)
    {
        switch (Severity)
        {
        case luvk::DebugSeverity::Verbose: return "verbose";
        case luvk::DebugSeverity::Info: return "info";
        case luvk::DebugSeverity::Warning: return "warning";
        case luvk::DebugSeverity::Error: return "error";
        }
        return "?";
    }

    void AddBaseRequirements(luvk::DeviceRequirements& Requirements, const bool NeedsPresent, const std::vector<std::string>& InstanceExtensions)
    {
        auto& V12                                         = Requirements.Features.Vulkan12();
        V12.timelineSemaphore                             = VK_TRUE;
        V12.bufferDeviceAddress                           = VK_TRUE;
        V12.descriptorIndexing                            = VK_TRUE;
        V12.runtimeDescriptorArray                        = VK_TRUE;
        V12.descriptorBindingPartiallyBound               = VK_TRUE;
        V12.descriptorBindingSampledImageUpdateAfterBind  = VK_TRUE;
        V12.descriptorBindingStorageImageUpdateAfterBind  = VK_TRUE;
        V12.descriptorBindingUpdateUnusedWhilePending     = VK_TRUE;
        V12.shaderSampledImageArrayNonUniformIndexing     = VK_TRUE;
        V12.scalarBlockLayout                             = VK_TRUE;

        auto& V13            = Requirements.Features.Vulkan13();
        V13.dynamicRendering = VK_TRUE;
        V13.synchronization2 = VK_TRUE;
        V13.maintenance4     = VK_TRUE;

        auto& OptionalCore                     = Requirements.OptionalFeatures.Core();
        OptionalCore.samplerAnisotropy         = VK_TRUE;
        OptionalCore.shaderInt64               = VK_TRUE;
        OptionalCore.shaderInt16               = VK_TRUE;
        OptionalCore.fillModeNonSolid          = VK_TRUE;
        OptionalCore.multiDrawIndirect         = VK_TRUE;
        OptionalCore.drawIndirectFirstInstance = VK_TRUE;
        OptionalCore.depthClamp                = VK_TRUE;
        OptionalCore.independentBlend          = VK_TRUE;

        OptionalCore.shaderStorageImageReadWithoutFormat  = VK_TRUE;
        OptionalCore.shaderStorageImageWriteWithoutFormat = VK_TRUE;

        Requirements.OptionalFeatures.Vulkan11().shaderDrawParameters = VK_TRUE;

        auto& OptionalV12                                    = Requirements.OptionalFeatures.Vulkan12();
        OptionalV12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
        OptionalV12.drawIndirectCount                        = VK_TRUE;
        OptionalV12.samplerFilterMinmax                      = VK_TRUE;
        OptionalV12.shaderFloat16                            = VK_TRUE;
        OptionalV12.storageBuffer8BitAccess                  = VK_TRUE;
        OptionalV12.vulkanMemoryModel                        = VK_TRUE;
        OptionalV12.vulkanMemoryModelDeviceScope             = VK_TRUE;
        OptionalV12.hostQueryReset                           = VK_TRUE;

        AddUnique(Requirements.OptionalExtensions, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        AddUnique(Requirements.OptionalExtensions, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
        Requirements.OptionalFeatures
                    .Get<VkPhysicalDeviceMemoryPriorityFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT,
                                                                    VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)
                    .memoryPriority = VK_TRUE;

        if (NeedsPresent)
        {
            AddUnique(Requirements.Extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);

            const char* Extension = nullptr;
            if (Contains(InstanceExtensions, VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME))
            {
                Extension = VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
            }
            else if (Contains(InstanceExtensions, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME))
            {
                Extension = VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
            }

            if (Extension != nullptr)
            {
                AddUnique(Requirements.OptionalExtensions, Extension);
                Requirements.OptionalFeatures
                            .Get<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR, Extension)
                            .swapchainMaintenance1 = VK_TRUE;
            }
        }

        AddUnique(Requirements.OptionalExtensions, "VK_KHR_portability_subset");
    }

    std::vector<std::byte> ReadFile(const std::filesystem::path& Path)
    {
        std::ifstream File(Path, std::ios::binary | std::ios::ate);
        if (!File)
        {
            return {};
        }

        const std::streamsize  Size = File.tellg();
        std::vector<std::byte> Data(static_cast<std::size_t>(std::max<std::streamsize>(Size, 0)));
        File.seekg(0);
        File.read(reinterpret_cast<char*>(std::data(Data)), Size);
        return File ? Data : std::vector<std::byte>{};
    }
}

luvk::Context::Context(const ContextDesc& Desc)
{
    if (g_ContextAlive.exchange(true))
    {
        throw std::logic_error("[luvk] Only one luvk::Context may exist at a time (volk uses global dispatch).");
    }
    m_OwnsContextSlot = true;

    try
    {
        Initialize(Desc);
    }
    catch (...)
    {
        Shutdown();
        throw;
    }
}

void luvk::Context::TrackResource(const std::int64_t Delta) noexcept
{
    m_LiveResources.fetch_add(Delta, std::memory_order_relaxed);
}

luvk::Context::~Context()
{
    Shutdown();
}

void luvk::Context::Initialize(const ContextDesc& Desc)
{
    m_OnDebugMessage    = Desc.OnDebugMessage;
    m_MinimumSeverity   = Desc.MinimumSeverity;
    m_PipelineCachePath = Desc.PipelineCachePath;
    m_Modules           = Desc.Modules;

    InitializeLoader();

    InstanceRequirements InstanceReqs{.Extensions = Desc.InstanceExtensions};
    for (const std::shared_ptr<Module>& It : m_Modules)
    {
        It->ConfigureInstance(InstanceReqs);
    }

    CreateInstance(Desc, InstanceReqs);

    if (Desc.CreateSurface)
    {
        m_Surface = Desc.CreateSurface(m_Instance);
        if (m_Surface == VK_NULL_HANDLE)
        {
            throw std::runtime_error("[luvk] CreateSurface returned VK_NULL_HANDLE.");
        }
    }

    DeviceRequirements DeviceReqs = {};
    AddBaseRequirements(DeviceReqs, m_Surface != VK_NULL_HANDLE, m_InstanceExtensions);
    for (const std::shared_ptr<Module>& It : m_Modules)
    {
        It->ConfigureDevice(DeviceReqs);
    }

    SelectPhysicalDevice(Desc, DeviceReqs);
    CreateDevice();
    CreateAllocator();
    CreatePipelineCache();

    m_Bindless = std::make_unique<BindlessHeap>(*this, Desc.Bindless);
    m_Uploads  = std::make_unique<UploadManager>(*this);

    for (const std::shared_ptr<Module>& It : m_Modules)
    {
        It->OnContextCreated(*this);
        ++m_ModulesCreated;
    }
}

void luvk::Context::Shutdown() noexcept
{
    if (m_Device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_Device);

        // Only modules that received OnContextCreated are told the context is going away.
        const std::size_t Notified = m_ModulesCreated;
        for (auto It = std::rbegin(m_Modules) + static_cast<std::ptrdiff_t>(std::size(m_Modules) - Notified); It != std::rend(m_Modules); ++It)
        {
            try
            {
                (*It)->OnContextDestroying(*this);
            }
            catch (const std::exception& Exception)
            {
                EmitMessage(DebugSeverity::Error, Exception.what());
            }
        }
        m_Modules.clear();

        m_Uploads.reset();

        try
        {
            // The device is idle: whatever a frame retired without being sealed can go now.
            SealFrameRetirement(0U);
        }
        catch (const std::exception& Exception)
        {
            EmitMessage(DebugSeverity::Error, Exception.what());
        }

        m_DeletionQueue.CollectAll([this](RetiredObject& Object)
        {
            DestroyRetired(Object);
        });

        if (const std::int64_t Live = LiveResourceCount();
            Live > 0)
        {
            EmitMessage(DebugSeverity::Error,
                        "[luvk] " + std::to_string(Live) +
                        " resource(s) are still alive while the Context is being destroyed. Destroy every Buffer/Image/Pipeline/... before the Context.");
        }

        m_Bindless.reset();

        SavePipelineCache();
        if (m_PipelineCache != VK_NULL_HANDLE)
        {
            vkDestroyPipelineCache(m_Device, m_PipelineCache, nullptr);
            m_PipelineCache = VK_NULL_HANDLE;
        }

        if (m_Allocator != VK_NULL_HANDLE)
        {
            vmaDestroyAllocator(m_Allocator);
            m_Allocator = VK_NULL_HANDLE;
        }

        m_TransferQueue.Destroy();
        m_GraphicsQueue.Destroy();

        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }

    if (m_Instance != VK_NULL_HANDLE)
    {
        if (m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }

        if (m_Messenger != VK_NULL_HANDLE)
        {
            vkDestroyDebugUtilsMessengerEXT(m_Instance, m_Messenger, nullptr);
            m_Messenger = VK_NULL_HANDLE;
        }

        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }

    if (m_OwnsContextSlot)
    {
        m_OwnsContextSlot = false;
        g_ContextAlive.store(false);
    }
}

void luvk::Context::CreateInstance(const ContextDesc& Desc, const InstanceRequirements& Requirements)
{
    std::uint32_t InstanceVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion != nullptr)
    {
        Check(vkEnumerateInstanceVersion(&InstanceVersion), "vkEnumerateInstanceVersion");
    }

    if (InstanceVersion < VK_API_VERSION_1_3)
    {
        throw std::runtime_error("[luvk] The Vulkan loader does not support Vulkan 1.3.");
    }
    m_InstanceApiVersion = InstanceVersion >= VK_API_VERSION_1_4 ? VK_API_VERSION_1_4 : VK_API_VERSION_1_3;

    const auto AvailableExtensions = Enumerate<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, static_cast<const char*>(nullptr));
    const auto AvailableLayers     = Enumerate<VkLayerProperties>(vkEnumerateInstanceLayerProperties);

    auto HasExtension = [&](const std::string_view Name)
    {
        return std::ranges::any_of(AvailableExtensions,
                                   [&](const VkExtensionProperties& It)
                                   {
                                       return Name == It.extensionName;
                                   });
    };

    auto HasLayer = [&](const std::string_view Name)
    {
        return std::ranges::any_of(AvailableLayers,
                                   [&](const VkLayerProperties& It)
                                   {
                                       return Name == It.layerName;
                                   });
    };

    std::vector<std::string> Extensions;
    std::vector<std::string> Missing;

    for (const std::string& It : Requirements.Extensions)
    {
        if (HasExtension(It))
        {
            AddUnique(Extensions, It);
        }
        else
        {
            Missing.push_back(It);
        }
    }

    for (const std::string& It : Requirements.OptionalExtensions)
    {
        if (HasExtension(It))
        {
            AddUnique(Extensions, It);
        }
    }

    if (!std::empty(Missing))
    {
        std::string Message = "[luvk] Missing required instance extensions:";
        for (const std::string& It : Missing)
        {
            Message += ' ' + It;
        }
        throw std::runtime_error(Message);
    }

    m_DebugUtilsEnabled = (Desc.EnableDebugUtils || Desc.EnableValidation) && HasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (m_DebugUtilsEnabled)
    {
        AddUnique(Extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (Desc.CreateSurface && HasExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME))
    {
        if (HasExtension(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME))
        {
            AddUnique(Extensions, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            AddUnique(Extensions, VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
        }
        else if (HasExtension(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME))
        {
            AddUnique(Extensions, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            AddUnique(Extensions, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
        }
    }

    VkInstanceCreateFlags Flags = 0U;
    if (HasExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
    {
        AddUnique(Extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        Flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    std::vector<std::string> Layers;
    for (const std::string& It : Requirements.Layers)
    {
        if (!HasLayer(It))
        {
            throw std::runtime_error("[luvk] Missing required instance layer: " + It);
        }
        AddUnique(Layers, It);
    }

    if (Desc.EnableValidation)
    {
        if (HasLayer(ValidationLayerName))
        {
            AddUnique(Layers, ValidationLayerName);
            m_ValidationEnabled = true;
        }
        else
        {
            EmitMessage(DebugSeverity::Warning, "[luvk] Validation requested but VK_LAYER_KHRONOS_validation is not installed (install the Vulkan SDK).");
        }
    }

    const VkDebugUtilsMessengerCreateInfoEXT MessengerInfo{.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                                                           .messageSeverity = SeverityMask(m_MinimumSeverity),
                                                           .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                                                           .pfnUserCallback = &Context::DebugCallback,
                                                           .pUserData       = this};

    const VkApplicationInfo AppInfo{.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pApplicationName   = Desc.ApplicationName.c_str(),
                                    .applicationVersion = Desc.ApplicationVersion,
                                    .pEngineName        = "luvk",
                                    .engineVersion      = VK_MAKE_API_VERSION(0, 0, 1, 0),
                                    .apiVersion         = m_InstanceApiVersion};

    const std::vector<const char*> ExtensionNames = ToPointers(Extensions);
    const std::vector<const char*> LayerNames     = ToPointers(Layers);

    const VkInstanceCreateInfo CreateInfo{.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                          .pNext                   = m_DebugUtilsEnabled ? &MessengerInfo : nullptr,
                                          .flags                   = Flags,
                                          .pApplicationInfo        = &AppInfo,
                                          .enabledLayerCount       = static_cast<std::uint32_t>(std::size(LayerNames)),
                                          .ppEnabledLayerNames     = std::data(LayerNames),
                                          .enabledExtensionCount   = static_cast<std::uint32_t>(std::size(ExtensionNames)),
                                          .ppEnabledExtensionNames = std::data(ExtensionNames)};

    Check(vkCreateInstance(&CreateInfo, nullptr, &m_Instance), "vkCreateInstance");
    m_InstanceExtensions = Extensions;
    volkLoadInstanceOnly(m_Instance);

    if (m_DebugUtilsEnabled)
    {
        Check(vkCreateDebugUtilsMessengerEXT(m_Instance, &MessengerInfo, nullptr, &m_Messenger), "vkCreateDebugUtilsMessengerEXT");
    }
}

void luvk::Context::SelectPhysicalDevice(const ContextDesc& Desc, DeviceRequirements& Requirements)
{

    const auto Devices = Enumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, m_Instance);
    if (std::empty(Devices))
    {
        throw std::runtime_error("[luvk] No Vulkan physical device found.");
    }

    std::string Rejections;

    auto Evaluate = [&](const VkPhysicalDevice Device) -> Candidate
    {
        Candidate Result{.Device = Device};

        VkPhysicalDeviceProperties Properties = {};
        vkGetPhysicalDeviceProperties(Device, &Properties);

        auto Reject = [&](const std::string& Reason)
        {
            Rejections += "\n  - ";
            Rejections += Properties.deviceName;
            Rejections += ": ";
            Rejections += Reason;
            Result.Score = -1;
            return Result;
        };

        Result.ApiVersion = std::min(Properties.apiVersion, m_InstanceApiVersion);
        if (Result.ApiVersion < VK_API_VERSION_1_3)
        {
            return Reject("Vulkan 1.3 is not supported");
        }

        const auto Available = Enumerate<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, Device, static_cast<const char*>(nullptr));
        auto       Has       = [&](const std::string_view Name)
        {
            return std::ranges::any_of(Available,
                                       [&](const VkExtensionProperties& It)
                                       {
                                           return Name == It.extensionName;
                                       });
        };

        for (const std::string& It : Requirements.Extensions)
        {
            if (!Has(It))
            {
                return Reject("missing extension " + It);
            }
            AddUnique(Result.Extensions, It);
        }

        for (const std::string& It : Requirements.OptionalExtensions)
        {
            if (Has(It))
            {
                AddUnique(Result.Extensions, It);
            }
        }

        FeatureChain Wanted = Requirements.Features;
        Wanted.AddTypesFrom(Requirements.OptionalFeatures);
        FeatureChain Supported = Wanted.MakeQuery(Result.ApiVersion, Result.Extensions);
        vkGetPhysicalDeviceFeatures2(Device, static_cast<VkPhysicalDeviceFeatures2*>(Supported.Head()));

        if (std::vector<std::string> MissingFeatures;
            !Requirements.Features.IsSupportedBy(Supported, &MissingFeatures))
        {
            std::string Reason = "missing features:";
            for (const std::string& It : MissingFeatures)
            {
                Reason += ' ' + It;
            }
            return Reject(Reason);
        }

        Result.Enabled = Requirements.Features;
        Result.Enabled.MergeSupported(Requirements.OptionalFeatures, Supported);

        const auto Families = Enumerate<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, Device);

        std::optional<std::uint32_t> Graphics;
        for (std::uint32_t Index = 0U; Index < std::size(Families); ++Index)
        {
            constexpr VkQueueFlags Needed = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if ((Families[Index].queueFlags & Needed) != Needed)
            {
                continue;
            }

            if (m_Surface != VK_NULL_HANDLE)
            {
                VkBool32 PresentSupported = VK_FALSE;
                Check(vkGetPhysicalDeviceSurfaceSupportKHR(Device, Index, m_Surface, &PresentSupported), "vkGetPhysicalDeviceSurfaceSupportKHR");
                if (PresentSupported == VK_FALSE)
                {
                    continue;
                }
            }

            Graphics = Index;
            break;
        }

        if (!Graphics.has_value())
        {
            return Reject(m_Surface != VK_NULL_HANDLE ? "no graphics queue family that can present to the surface" : "no graphics queue family");
        }

        Result.GraphicsFamily = *Graphics;
        Result.TransferFamily = *Graphics;
        Result.TransferIndex  = Families[*Graphics].queueCount > 1U ? 1U : 0U;

        std::optional<std::uint32_t> Dedicated;
        for (std::uint32_t Index = 0U; Index < std::size(Families); ++Index)
        {
            const VkQueueFlags Flags = Families[Index].queueFlags;
            if ((Flags & VK_QUEUE_TRANSFER_BIT) == 0U || (Flags & VK_QUEUE_GRAPHICS_BIT) != 0U)
            {
                continue;
            }

            if (!Dedicated.has_value() || (Flags & VK_QUEUE_COMPUTE_BIT) == 0U)
            {
                Dedicated = Index;
            }
        }

        if (Dedicated.has_value())
        {
            Result.TransferFamily = *Dedicated;
            Result.TransferIndex  = 0U;
        }

        const bool Discrete   = Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        const bool Integrated = Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
        const bool LowPower   = Desc.Preference == DevicePreference::LowPower;

        Result.Score = 100;
        if (Discrete)
        {
            Result.Score += LowPower ? 500 : 1000;
        }
        else if (Integrated)
        {
            Result.Score += LowPower ? 1000 : 500;
        }
        else if (Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        {
            Result.Score = 1;
        }

        if (Result.ApiVersion >= VK_API_VERSION_1_4)
        {
            Result.Score += 10;
        }

        return Result;
    };

    Candidate Best = {};

    if (Desc.PhysicalDeviceIndex.has_value())
    {
        if (*Desc.PhysicalDeviceIndex >= std::size(Devices))
        {
            throw std::out_of_range("[luvk] PhysicalDeviceIndex is out of range.");
        }
        Best = Evaluate(Devices[*Desc.PhysicalDeviceIndex]);
    }
    else
    {
        for (const VkPhysicalDevice Device : Devices)
        {
            if (Candidate Current = Evaluate(Device);
                Current.Score > Best.Score)
            {
                Best = std::move(Current);
            }
        }
    }

    if (Best.Score < 0)
    {
        throw std::runtime_error("[luvk] No suitable physical device:" + Rejections);
    }

    m_PhysicalDevice     = Best.Device;
    m_ApiVersion         = Best.ApiVersion;
    m_DeviceExtensions   = std::move(Best.Extensions);
    m_EnabledFeatures    = std::move(Best.Enabled);
    m_GraphicsFamily     = Best.GraphicsFamily;
    m_TransferFamily     = Best.TransferFamily;
    m_TransferQueueIndex = Best.TransferIndex;

    m_SharedFamilies = {m_GraphicsFamily};
    if (m_TransferFamily != m_GraphicsFamily)
    {
        m_SharedFamilies.push_back(m_TransferFamily);
    }

    m_Properties.pNext   = &m_Properties12;
    m_Properties12.pNext = &m_Properties13;
    m_Properties13.pNext = nullptr;
    vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &m_Properties);
    m_Properties.pNext   = nullptr;
    m_Properties12.pNext = nullptr;
}

void luvk::Context::CreateDevice()
{
    constexpr std::array Priorities = {1.F, 1.F};

    std::vector<VkDeviceQueueCreateInfo> QueueInfos;
    const bool                           SameFamily = m_TransferFamily == m_GraphicsFamily;

    QueueInfos.push_back(VkDeviceQueueCreateInfo{.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                 .queueFamilyIndex = m_GraphicsFamily,
                                                 .queueCount       = SameFamily && m_TransferQueueIndex == 1U ? 2U : 1U,
                                                 .pQueuePriorities = std::data(Priorities)});

    if (!SameFamily)
    {
        QueueInfos.push_back(VkDeviceQueueCreateInfo{.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                     .queueFamilyIndex = m_TransferFamily,
                                                     .queueCount       = 1U,
                                                     .pQueuePriorities = std::data(Priorities)});
    }

    const std::vector<const char*> Extensions = ToPointers(m_DeviceExtensions);

    const VkDeviceCreateInfo CreateInfo{.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                        .pNext                   = m_EnabledFeatures.Link(m_ApiVersion, m_DeviceExtensions),
                                        .queueCreateInfoCount    = static_cast<std::uint32_t>(std::size(QueueInfos)),
                                        .pQueueCreateInfos       = std::data(QueueInfos),
                                        .enabledExtensionCount   = static_cast<std::uint32_t>(std::size(Extensions)),
                                        .ppEnabledExtensionNames = std::data(Extensions),
                                        .pEnabledFeatures        = nullptr};

    Check(vkCreateDevice(m_PhysicalDevice, &CreateInfo, nullptr, &m_Device), "vkCreateDevice");
    volkLoadDevice(m_Device);

    if (const auto* const Maintenance = m_EnabledFeatures.Find<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR);
        Maintenance != nullptr && Maintenance->swapchainMaintenance1 != VK_FALSE)
    {
        m_PresentFences = IsDeviceExtensionEnabled(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME) ||
                          IsDeviceExtensionEnabled(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    }

    VkQueue GraphicsHandle = VK_NULL_HANDLE;
    vkGetDeviceQueue(m_Device, m_GraphicsFamily, 0U, &GraphicsHandle);
    m_GraphicsQueue.Initialize(m_Device, GraphicsHandle, m_GraphicsFamily, m_GraphicsQueueMutex);

    VkQueue TransferHandle = VK_NULL_HANDLE;
    vkGetDeviceQueue(m_Device, m_TransferFamily, m_TransferQueueIndex, &TransferHandle);

    const bool SharedHandle = TransferHandle == GraphicsHandle;
    m_TransferQueue.Initialize(m_Device, TransferHandle, m_TransferFamily, SharedHandle ? m_GraphicsQueueMutex : m_TransferQueueMutex);

    SetDebugName(GraphicsHandle, VK_OBJECT_TYPE_QUEUE, "luvk graphics queue");
    if (!SharedHandle)
    {
        SetDebugName(TransferHandle, VK_OBJECT_TYPE_QUEUE, "luvk transfer queue");
    }
    SetDebugName(m_GraphicsQueue.Timeline(), VK_OBJECT_TYPE_SEMAPHORE, "luvk graphics timeline");
    SetDebugName(m_TransferQueue.Timeline(), VK_OBJECT_TYPE_SEMAPHORE, "luvk transfer timeline");

    EmitMessage(DebugSeverity::Info, std::string("[luvk] Using ") + m_Properties.properties.deviceName);
}

void luvk::Context::CreateAllocator()
{
    VmaAllocatorCreateFlags Flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    if (IsDeviceExtensionEnabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME))
    {
        Flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    }

    if (const auto* const Priority = m_EnabledFeatures.Find<VkPhysicalDeviceMemoryPriorityFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT);
        Priority != nullptr && Priority->memoryPriority != VK_FALSE && IsDeviceExtensionEnabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME))
    {
        Flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
    }

    const VmaVulkanFunctions Functions{.vkGetInstanceProcAddr = vkGetInstanceProcAddr, .vkGetDeviceProcAddr = vkGetDeviceProcAddr};

    const VmaAllocatorCreateInfo CreateInfo{.flags            = Flags,
                                            .physicalDevice   = m_PhysicalDevice,
                                            .device           = m_Device,
                                            .pVulkanFunctions = &Functions,
                                            .instance         = m_Instance,
                                            .vulkanApiVersion = m_ApiVersion};

    Check(vmaCreateAllocator(&CreateInfo, &m_Allocator), "vmaCreateAllocator");
}

void luvk::Context::CreatePipelineCache()
{
    std::vector<std::byte> InitialData;

    if (!m_PipelineCachePath.empty())
    {
        InitialData = ReadFile(m_PipelineCachePath);

        if (VkPipelineCacheHeaderVersionOne Header{};
            std::size(InitialData) >= sizeof(Header))
        {
            std::memcpy(&Header, std::data(InitialData), sizeof(Header));
            const VkPhysicalDeviceProperties& Props = m_Properties.properties;

            const bool Valid = Header.headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE &&
                               Header.headerSize >= sizeof(Header) &&
                               Header.vendorID == Props.vendorID &&
                               Header.deviceID == Props.deviceID &&
                               std::memcmp(Header.pipelineCacheUUID, Props.pipelineCacheUUID, VK_UUID_SIZE) == 0;
            if (!Valid)
            {
                InitialData.clear();
            }
        }
        else
        {
            InitialData.clear();
        }
    }

    const VkPipelineCacheCreateInfo CreateInfo{.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
                                               .initialDataSize = std::size(InitialData),
                                               .pInitialData    = std::empty(InitialData) ? nullptr : std::data(InitialData)};

    Check(vkCreatePipelineCache(m_Device, &CreateInfo, nullptr, &m_PipelineCache), "vkCreatePipelineCache");
}

void luvk::Context::SavePipelineCache() const noexcept
{
    if (m_PipelineCachePath.empty() || m_PipelineCache == VK_NULL_HANDLE)
    {
        return;
    }

    try
    {
        std::size_t Size = 0U;
        Check(vkGetPipelineCacheData(m_Device, m_PipelineCache, &Size, nullptr), "vkGetPipelineCacheData");

        std::vector<std::byte> Data(Size);
        Check(vkGetPipelineCacheData(m_Device, m_PipelineCache, &Size, std::data(Data)), "vkGetPipelineCacheData");
        Data.resize(Size);

        if (m_PipelineCachePath.has_parent_path())
        {
            std::filesystem::create_directories(m_PipelineCachePath.parent_path());
        }

        std::filesystem::path Temporary = m_PipelineCachePath;
        Temporary += ".tmp";
        {
            std::ofstream File(Temporary, std::ios::binary | std::ios::trunc);
            File.write(reinterpret_cast<const char*>(std::data(Data)), static_cast<std::streamsize>(std::size(Data)));
            if (!File)
            {
                return;
            }
        }
        std::filesystem::rename(Temporary, m_PipelineCachePath);
    }
    catch (const std::exception& Exception)
    {
        EmitMessage(DebugSeverity::Warning, std::string("[luvk] Could not save the pipeline cache: ") + Exception.what());
    }
}

bool luvk::Context::IsDeviceExtensionEnabled(const std::string_view Name) const noexcept
{
    return Contains(m_DeviceExtensions, Name);
}

bool luvk::Context::IsInstanceExtensionEnabled(const std::string_view Name) const noexcept
{
    return Contains(m_InstanceExtensions, Name);
}

std::span<const std::uint32_t> luvk::Context::SharedQueueFamilies() const noexcept
{
    return m_SharedFamilies;
}


void luvk::Context::WaitIdle()
{

    const std::uint64_t Transfer = m_Uploads->Flush();

    const std::uint64_t Graphics = m_GraphicsQueue.Submit({});

    m_GraphicsQueue.Wait(Graphics);
    m_TransferQueue.Wait(Transfer);

    CollectGarbage();
}


void luvk::Context::SetDebugName(const VkObjectType Type, const std::uint64_t Handle, const std::string_view Name) const
{
    if (!m_DebugUtilsEnabled || m_Device == VK_NULL_HANDLE || Handle == 0U || std::empty(Name))
    {
        return;
    }

    const std::string                   Copy(Name);
    const VkDebugUtilsObjectNameInfoEXT Info{.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                                             .objectType   = Type,
                                             .objectHandle = Handle,
                                             .pObjectName  = Copy.c_str()};
    vkSetDebugUtilsObjectNameEXT(m_Device, &Info);
}

void luvk::Context::EmitMessage(const DebugSeverity Severity, const std::string_view Message) const
{
    if (Severity < m_MinimumSeverity)
    {
        return;
    }

    if (m_OnDebugMessage)
    {
        m_OnDebugMessage(Severity, Message);
        return;
    }

    std::fprintf(stderr, "[%s] %.*s\n", SeverityName(Severity), static_cast<int>(std::size(Message)), std::data(Message));
}

VKAPI_ATTR VkBool32 VKAPI_CALL luvk::Context::DebugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT      Severity,
                                                           const VkDebugUtilsMessageTypeFlagsEXT             Types,
                                                           const VkDebugUtilsMessengerCallbackDataEXT* const Data,
                                                           void* const                                       UserData)
{
    auto* const Self = static_cast<Context*>(UserData);

    const DebugSeverity Level = ToSeverity(Severity);
    if (Level == DebugSeverity::Error && (Types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0U)
    {
        Self->m_ValidationErrors.fetch_add(1U, std::memory_order_relaxed);
    }

    Self->EmitMessage(Level, Data != nullptr && Data->pMessage != nullptr ? Data->pMessage : "");
    return VK_FALSE;
}
