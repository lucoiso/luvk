// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "ContextInternals.hpp"
#include "ErrorInternals.hpp"
#include "luvk/Core/Context.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include "luvk/Core/Bindless.hpp"
#include "luvk/Core/Diagnostics.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/Module.hpp"
#include "luvk/Core/Upload.hpp"

namespace
{
    /** Debug messenger trampoline: counts validation errors and forwards every message to Context::EmitMessage. */
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT      Severity,
                                                 const VkDebugUtilsMessageTypeFlagsEXT             Types,
                                                 const VkDebugUtilsMessengerCallbackDataEXT* const Data,
                                                 void* const                                       UserData);
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

    /** Driver version in the vendor's own numbering scheme. */
    std::string DriverVersionString(const VkPhysicalDeviceProperties& Properties)
    {
        const std::uint32_t Version = Properties.driverVersion;
        char                Buffer[64] = {};
        if (Properties.vendorID == 0x10DEU)
        {
            std::snprintf(Buffer, sizeof(Buffer), "%u.%u.%u.%u", (Version >> 22U) & 0x3FFU, (Version >> 14U) & 0xFFU, (Version >> 6U) & 0xFFU, Version & 0x3FU);
        }
#ifdef _WIN32
        else if (Properties.vendorID == 0x8086U)
        {
            std::snprintf(Buffer, sizeof(Buffer), "%u.%u", Version >> 14U, Version & 0x3FFFU);
        }
#endif
        else
        {
            std::snprintf(Buffer, sizeof(Buffer), "%u.%u.%u", VK_API_VERSION_MAJOR(Version), VK_API_VERSION_MINOR(Version), VK_API_VERSION_PATCH(Version));
        }
        return Buffer;
    }

    /** Local time formatted with Format (strftime). */
    std::string LocalTime(const char* const Format)
    {
        const std::time_t Now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm           Local{};
#ifdef _WIN32
        localtime_s(&Local, &Now);
#else
        localtime_r(&Now, &Local);
#endif
        char Buffer[64] = {};
        std::strftime(Buffer, sizeof(Buffer), Format, &Local);
        return Buffer;
    }

    const char* DeviceTypeName(const VkPhysicalDeviceType Type)
    {
        switch (Type)
        {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
        default: return "other";
        }
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
    m_OnDebugMessage       = Desc.OnDebugMessage;
    m_MinimumSeverity      = Desc.MinimumSeverity;
    m_PipelineCachePath    = Desc.PipelineCachePath;
    m_Modules              = Desc.Modules;
    m_ExtraQueues          = Desc.ExtraQueues;
    m_CrashReportDirectory = Desc.CrashReportDirectory;

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

    m_Info = std::make_unique<DeviceInfo>();
    SelectPhysicalDevice(Desc, DeviceReqs);
    CreateDevice();
    QueryDeviceInfo();

    // From now on every checked Vulkan call returning VK_ERROR_DEVICE_LOST produces the device-lost report.
    SetDeviceLostObserver(this);

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
        // After a device loss nothing is executing anymore, and a wait could block on a hung GPU: skip it.
        if (!IsDeviceLost() && vkDeviceWaitIdle(m_Device) == VK_ERROR_DEVICE_LOST)
        {
            ReportDeviceLost("vkDeviceWaitIdle (Context shutdown)");
        }

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

        // A lost device may return incomplete or garbage cache data: keep the previous file.
        if (!IsDeviceLost())
        {
            SavePipelineCache();
        }
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

        m_ComputeQueue.Destroy();
        m_TransferQueue.Destroy();
        m_GraphicsQueue.Destroy();

        SetDeviceLostObserver(nullptr);
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }
    else
    {
        SetDeviceLostObserver(nullptr);
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

    // Validation layer features: VK_EXT_layer_settings (current layers), else VkValidationFeaturesEXT (older layers).
    const ValidationOptions&            Wanted = Desc.Validation;
    const bool                          AnyFeature = Wanted.GpuAssisted || Wanted.Synchronization || Wanted.BestPractices || Wanted.DebugPrintf;
    constexpr VkBool32                  SettingOn  = VK_TRUE;
    std::vector<VkLayerSettingEXT>      LayerSettings;
    std::vector<VkValidationFeatureEnableEXT> FeatureEnables;
    VkLayerSettingsCreateInfoEXT        LayerSettingsInfo{.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT};
    VkValidationFeaturesEXT             ValidationFeatures{.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    void*                               ValidationChain = nullptr;

    if (m_ValidationEnabled && AnyFeature)
    {
        const auto LayerExtensions = Enumerate<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, ValidationLayerName);
        const auto LayerHas        = [&](const std::string_view Name)
        {
            return std::ranges::any_of(LayerExtensions,
                                       [&](const VkExtensionProperties& It)
                                       {
                                           return Name == It.extensionName;
                                       });
        };

        const auto AddSetting = [&](const char* const Name)
        {
            LayerSettings.push_back(VkLayerSettingEXT{.pLayerName   = ValidationLayerName,
                                                      .pSettingName = Name,
                                                      .type         = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
                                                      .valueCount   = 1U,
                                                      .pValues      = &SettingOn});
        };

        if (HasExtension(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME) || LayerHas(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME))
        {
            AddUnique(Extensions, VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
            if (Wanted.GpuAssisted)
            {
                AddSetting("gpuav_enable");
            }
            if (Wanted.Synchronization)
            {
                AddSetting("validate_sync");
            }
            if (Wanted.BestPractices)
            {
                AddSetting("validate_best_practices");
            }
            if (Wanted.DebugPrintf)
            {
                AddSetting("printf_enable");
            }

            LayerSettingsInfo.settingCount = static_cast<std::uint32_t>(std::size(LayerSettings));
            LayerSettingsInfo.pSettings    = std::data(LayerSettings);
            ValidationChain                = &LayerSettingsInfo;
            m_Validation                   = Wanted;
        }
        else if (HasExtension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) || LayerHas(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME))
        {
            AddUnique(Extensions, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
            if (Wanted.GpuAssisted)
            {
                FeatureEnables.push_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
            }
            if (Wanted.Synchronization)
            {
                FeatureEnables.push_back(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
            }
            if (Wanted.BestPractices)
            {
                FeatureEnables.push_back(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
            }
            if (Wanted.DebugPrintf && !Wanted.GpuAssisted)
            {
                // Older layers cannot run GPU-assisted validation and debug printf together.
                FeatureEnables.push_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);
            }

            ValidationFeatures.enabledValidationFeatureCount = static_cast<std::uint32_t>(std::size(FeatureEnables));
            ValidationFeatures.pEnabledValidationFeatures    = std::data(FeatureEnables);
            ValidationChain                                  = &ValidationFeatures;
            m_Validation                                     = Wanted;
            m_Validation.DebugPrintf                         = Wanted.DebugPrintf && !Wanted.GpuAssisted;
        }
        else
        {
            EmitMessage(DebugSeverity::Warning, "[luvk] The validation layer supports neither VK_EXT_layer_settings nor VK_EXT_validation_features: "
                                                "GPU-assisted / synchronization / best practices / printf validation stay disabled.");
        }
    }

    VkDebugUtilsMessengerCreateInfoEXT MessengerInfo{.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                                                           .messageSeverity = SeverityMask(m_MinimumSeverity),
                                                           .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                                                           .pfnUserCallback = &DebugCallback,
                                                           .pUserData       = this};

    // Instance pNext chain: [debug messenger] -> [validation features].
    void* InstanceChain = ValidationChain;
    if (m_DebugUtilsEnabled)
    {
        MessengerInfo.pNext = ValidationChain;
        InstanceChain       = &MessengerInfo;
    }

    const VkApplicationInfo AppInfo{.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pApplicationName   = Desc.ApplicationName.c_str(),
                                    .applicationVersion = VK_MAKE_API_VERSION(0, Desc.ApplicationVersion[0], Desc.ApplicationVersion[1], Desc.ApplicationVersion[2]),
                                    .pEngineName        = "luvk",
                                    .engineVersion      = VK_MAKE_API_VERSION(0, 0, 1, 0),
                                    .apiVersion         = m_InstanceApiVersion};

    const std::vector<const char*> ExtensionNames = ToPointers(Extensions);
    const std::vector<const char*> LayerNames     = ToPointers(Layers);

    const VkInstanceCreateInfo CreateInfo{.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                          .pNext                   = InstanceChain,
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
        MessengerInfo.pNext = nullptr;
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

        // Queues handed out per family; every role gets its own VkQueue while the family has free ones.
        std::vector<std::uint32_t> Taken(std::size(Families), 0U);
        const auto                 Take = [&](const std::uint32_t Family) -> std::optional<std::uint32_t>
        {
            if (Taken[Family] < Families[Family].queueCount)
            {
                return Taken[Family]++;
            }
            return std::nullopt;
        };

        Result.GraphicsFamily = *Graphics;
        (void)Take(*Graphics);
        Result.TransferFamily = *Graphics;
        Result.TransferIndex  = 0U;

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
            Result.TransferIndex  = Take(*Dedicated).value_or(0U);
        }
        else
        {
            // A second queue of the graphics family when available, else the graphics queue itself.
            Result.TransferIndex = Take(*Graphics).value_or(0U);
        }

        const auto NonGraphics = [&](const std::uint32_t Index)
        {
            return (Families[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0U;
        };

        if (m_ExtraQueues.AsyncCompute)
        {
            // A compute family without graphics; prefer one the transfer queue does not use.
            for (const bool AvoidTransfer : {true, false})
            {
                for (std::uint32_t Index = 0U; Index < std::size(Families) && !Result.HasCompute; ++Index)
                {
                    if ((Families[Index].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0U || !NonGraphics(Index) ||
                        (AvoidTransfer && Index == Result.TransferFamily))
                    {
                        continue;
                    }

                    if (const std::optional<std::uint32_t> Queue = Take(Index))
                    {
                        Result.HasCompute    = true;
                        Result.ComputeFamily = Index;
                        Result.ComputeIndex  = *Queue;
                    }
                }
            }
        }

        if (m_ExtraQueues.Present && m_Surface != VK_NULL_HANDLE)
        {
            // Present-capable queue distinct from the graphics one: a non-graphics family first, then the graphics family.
            for (const bool WantNonGraphics : {true, false})
            {
                for (std::uint32_t Index = 0U; Index < std::size(Families) && !Result.HasPresent; ++Index)
                {
                    if (NonGraphics(Index) != WantNonGraphics || Taken[Index] >= Families[Index].queueCount)
                    {
                        continue;
                    }

                    VkBool32 PresentSupported = VK_FALSE;
                    Check(vkGetPhysicalDeviceSurfaceSupportKHR(Device, Index, m_Surface, &PresentSupported), "vkGetPhysicalDeviceSurfaceSupportKHR");
                    if (PresentSupported == VK_FALSE)
                    {
                        continue;
                    }

                    if (const std::optional<std::uint32_t> Queue = Take(Index))
                    {
                        Result.HasPresent    = true;
                        Result.PresentFamily = Index;
                        Result.PresentIndex  = *Queue;
                    }
                }
            }
        }

        if (m_ExtraQueues.ImageAcquire)
        {
            // Any family able to signal semaphores; prefer the present family, then non-graphics families.
            std::vector<std::uint32_t> Order;
            if (Result.HasPresent)
            {
                Order.push_back(Result.PresentFamily);
            }
            for (std::uint32_t Index = 0U; Index < std::size(Families); ++Index)
            {
                if (NonGraphics(Index) && (Families[Index].queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)) != 0U)
                {
                    Order.push_back(Index);
                }
            }
            Order.push_back(*Graphics);

            for (const std::uint32_t Index : Order)
            {
                if (const std::optional<std::uint32_t> Queue = Take(Index))
                {
                    Result.HasImageAcquire    = true;
                    Result.ImageAcquireFamily = Index;
                    Result.ImageAcquireIndex  = *Queue;
                    break;
                }
            }
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
    m_Info->EnabledFeatures = std::move(Best.Enabled);
    m_GraphicsFamily     = Best.GraphicsFamily;
    m_TransferFamily     = Best.TransferFamily;
    m_TransferQueueIndex = Best.TransferIndex;
    m_HasAsyncCompute    = Best.HasCompute;
    m_ComputeFamily      = Best.ComputeFamily;
    m_ComputeQueueIndex  = Best.ComputeIndex;

    m_PresentQueue      = QueueInfo{.Queue = VK_NULL_HANDLE, .Family = Best.PresentFamily, .Index = Best.PresentIndex, .UsedByLuvk = !Best.HasPresent};
    m_ImageAcquireQueue = QueueInfo{.Queue      = VK_NULL_HANDLE,
                                    .Family     = Best.ImageAcquireFamily,
                                    .Index      = Best.ImageAcquireIndex,
                                    .UsedByLuvk = !Best.HasImageAcquire};

    m_SharedFamilies = {m_GraphicsFamily};
    if (m_TransferFamily != m_GraphicsFamily)
    {
        m_SharedFamilies.push_back(m_TransferFamily);
    }
    if (m_HasAsyncCompute && std::ranges::find(m_SharedFamilies, m_ComputeFamily) == std::end(m_SharedFamilies))
    {
        m_SharedFamilies.push_back(m_ComputeFamily);
    }

    m_Info->Properties.pNext   = &m_Info->Properties11;
    m_Info->Properties11.pNext = &m_Info->Properties12;
    m_Info->Properties12.pNext = &m_Info->Properties13;
    m_Info->Properties13.pNext = nullptr;
    vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &m_Info->Properties);
    m_Info->Properties.pNext   = nullptr;
    m_Info->Properties11.pNext = nullptr;
    m_Info->Properties12.pNext = nullptr;
}

void luvk::Context::CreateDevice()
{
    // Number of queues per family: highest queue index assigned to any role + 1.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> Counts;
    const auto                                           Require = [&](const std::uint32_t Family, const std::uint32_t Index)
    {
        const auto It = std::ranges::find(Counts, Family, &std::pair<std::uint32_t, std::uint32_t>::first);
        if (It == std::end(Counts))
        {
            Counts.emplace_back(Family, Index + 1U);
        }
        else
        {
            It->second = std::max(It->second, Index + 1U);
        }
    };

    Require(m_GraphicsFamily, 0U);
    Require(m_TransferFamily, m_TransferQueueIndex);
    if (m_HasAsyncCompute)
    {
        Require(m_ComputeFamily, m_ComputeQueueIndex);
    }
    if (!m_PresentQueue.UsedByLuvk)
    {
        Require(m_PresentQueue.Family, m_PresentQueue.Index);
    }
    if (!m_ImageAcquireQueue.UsedByLuvk)
    {
        Require(m_ImageAcquireQueue.Family, m_ImageAcquireQueue.Index);
    }

    std::uint32_t MaxCount = 1U;
    for (const auto& [Family, Count] : Counts)
    {
        MaxCount = std::max(MaxCount, Count);
    }
    const std::vector<float> Priorities(MaxCount, 1.F);

    std::vector<VkDeviceQueueCreateInfo> QueueInfos;
    for (const auto& [Family, Count] : Counts)
    {
        QueueInfos.push_back(VkDeviceQueueCreateInfo{.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                     .queueFamilyIndex = Family,
                                                     .queueCount       = Count,
                                                     .pQueuePriorities = std::data(Priorities)});
    }

    const std::vector<const char*> Extensions = ToPointers(m_DeviceExtensions);

    const VkDeviceCreateInfo CreateInfo{.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                        .pNext                   = m_Info->EnabledFeatures.Link(m_ApiVersion, m_DeviceExtensions),
                                        .queueCreateInfoCount    = static_cast<std::uint32_t>(std::size(QueueInfos)),
                                        .pQueueCreateInfos       = std::data(QueueInfos),
                                        .enabledExtensionCount   = static_cast<std::uint32_t>(std::size(Extensions)),
                                        .ppEnabledExtensionNames = std::data(Extensions),
                                        .pEnabledFeatures        = nullptr};

    Check(vkCreateDevice(m_PhysicalDevice, &CreateInfo, nullptr, &m_Device), "vkCreateDevice");
    volkLoadDevice(m_Device);

    if (const auto* const Maintenance = m_Info->EnabledFeatures.Find<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR);
        Maintenance != nullptr && Maintenance->swapchainMaintenance1 != VK_FALSE)
    {
        m_PresentFences = IsDeviceExtensionEnabled(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME) ||
                          IsDeviceExtensionEnabled(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    }

    VkQueue GraphicsHandle = VK_NULL_HANDLE;
    vkGetDeviceQueue(m_Device, m_GraphicsFamily, 0U, &GraphicsHandle);
    m_GraphicsQueue.Initialize(m_Device, GraphicsHandle, m_GraphicsFamily, m_GraphicsQueueMutex, 0U, &m_DeviceLost);

    VkQueue TransferHandle = VK_NULL_HANDLE;
    vkGetDeviceQueue(m_Device, m_TransferFamily, m_TransferQueueIndex, &TransferHandle);

    const bool SharedHandle = TransferHandle == GraphicsHandle;
    m_TransferQueue.Initialize(m_Device,
                               TransferHandle,
                               m_TransferFamily,
                               SharedHandle ? m_GraphicsQueueMutex : m_TransferQueueMutex,
                               m_TransferQueueIndex,
                               &m_DeviceLost);

    SetDebugName(GraphicsHandle, VK_OBJECT_TYPE_QUEUE, "luvk graphics queue");
    if (!SharedHandle)
    {
        SetDebugName(TransferHandle, VK_OBJECT_TYPE_QUEUE, "luvk transfer queue");
    }
    SetDebugName(m_GraphicsQueue.Timeline(), VK_OBJECT_TYPE_SEMAPHORE, "luvk graphics timeline");
    SetDebugName(m_TransferQueue.Timeline(), VK_OBJECT_TYPE_SEMAPHORE, "luvk transfer timeline");

    if (m_HasAsyncCompute)
    {
        VkQueue ComputeHandle = VK_NULL_HANDLE;
        vkGetDeviceQueue(m_Device, m_ComputeFamily, m_ComputeQueueIndex, &ComputeHandle);
        m_ComputeQueue.Initialize(m_Device, ComputeHandle, m_ComputeFamily, m_ComputeQueueMutex, m_ComputeQueueIndex, &m_DeviceLost);
        SetDebugName(ComputeHandle, VK_OBJECT_TYPE_QUEUE, "luvk async compute queue");
        SetDebugName(m_ComputeQueue.Timeline(), VK_OBJECT_TYPE_SEMAPHORE, "luvk compute timeline");
    }

    if (!m_PresentQueue.UsedByLuvk)
    {
        vkGetDeviceQueue(m_Device, m_PresentQueue.Family, m_PresentQueue.Index, &m_PresentQueue.Queue);
        SetDebugName(m_PresentQueue.Queue, VK_OBJECT_TYPE_QUEUE, "luvk extra present queue");
    }

    if (!m_ImageAcquireQueue.UsedByLuvk)
    {
        vkGetDeviceQueue(m_Device, m_ImageAcquireQueue.Family, m_ImageAcquireQueue.Index, &m_ImageAcquireQueue.Queue);
        SetDebugName(m_ImageAcquireQueue.Queue, VK_OBJECT_TYPE_QUEUE, "luvk extra image acquire queue");
    }

    EmitMessage(DebugSeverity::Info, std::string("[luvk] Using ") + m_Info->Properties.properties.deviceName);
}

void luvk::Context::CreateAllocator()
{
    VmaAllocatorCreateFlags Flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    if (IsDeviceExtensionEnabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME))
    {
        Flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    }

    if (const auto* const Priority = m_Info->EnabledFeatures.Find<VkPhysicalDeviceMemoryPriorityFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT);
        Priority != nullptr && Priority->memoryPriority != VK_FALSE && IsDeviceExtensionEnabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME))
    {
        Flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
    }

    // Device-coherent memory (DiagnosticsModule breadcrumbs) is only exposed by VMA with this flag.
    if (const auto* const Coherent = m_Info->EnabledFeatures.Find<VkPhysicalDeviceCoherentMemoryFeaturesAMD>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD);
        Coherent != nullptr && Coherent->deviceCoherentMemory != VK_FALSE && IsDeviceExtensionEnabled(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME))
    {
        Flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
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
            const VkPhysicalDeviceProperties& Props = m_Info->Properties.properties;

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
    // After a device loss nothing executes anymore: every submitted value counts as complete (no wait, no submit).
    if (IsDeviceLost())
    {
        CollectGarbage();
        return;
    }

    const std::uint64_t Transfer = m_Uploads->Flush();

    const std::uint64_t Graphics = m_GraphicsQueue.Submit({});

    m_GraphicsQueue.Wait(Graphics);
    m_TransferQueue.Wait(Transfer);
    if (m_HasAsyncCompute)
    {
        m_ComputeQueue.Wait(m_ComputeQueue.LastSubmittedValue());
    }

    CollectGarbage();
}

luvk::QueueInfo luvk::Context::GetQueueInfo(const QueueRole Role) const noexcept
{
    const QueueInfo Graphics{.Queue = m_GraphicsQueue.Handle(), .Family = m_GraphicsQueue.Family(), .Index = m_GraphicsQueue.Index(), .UsedByLuvk = true};

    switch (Role)
    {
    case QueueRole::Graphics:
        return Graphics;
    case QueueRole::Transfer:
        return QueueInfo{.Queue = m_TransferQueue.Handle(), .Family = m_TransferQueue.Family(), .Index = m_TransferQueue.Index(), .UsedByLuvk = true};
    case QueueRole::Compute:
        return m_HasAsyncCompute ? QueueInfo{.Queue = m_ComputeQueue.Handle(), .Family = m_ComputeQueue.Family(), .Index = m_ComputeQueue.Index(), .UsedByLuvk = true}
                                 : Graphics;
    case QueueRole::Present:
        return m_PresentQueue.UsedByLuvk ? Graphics : m_PresentQueue;
    case QueueRole::ImageAcquire:
        if (!m_ImageAcquireQueue.UsedByLuvk)
        {
            return m_ImageAcquireQueue;
        }
        return m_PresentQueue.UsedByLuvk ? Graphics : m_PresentQueue;
    }
    return Graphics;
}

const luvk::DeviceLostReport* luvk::Context::LastDeviceLostReport() const noexcept
{
    return m_DeviceLostReported.load(std::memory_order_acquire) ? m_DeviceLostReport.get() : nullptr;
}

void luvk::Context::ReportDeviceLost(const std::string_view Where) noexcept
{
    m_DeviceLost.store(true, std::memory_order_release);

    std::scoped_lock Lock(m_DeviceLostMutex);
    if (m_DeviceLostReport)
    {
        return;
    }

    try
    {
        auto Report  = std::make_unique<DeviceLostReport>(BuildDeviceLostReport(Where));
        Report->File = WriteCrashReport(*Report);
        if (!Report->File.empty())
        {
            const std::string Location = "Full report: " + Report->File.string();
            Report->Summary += "\n" + Location;
            Report->Details += Location + '\n';
        }
        m_DeviceLostReport = std::move(Report);
    }
    catch (const std::exception& Exception)
    {
        m_DeviceLostReport          = std::make_unique<DeviceLostReport>();
        m_DeviceLostReport->Summary = "The GPU device was lost (VK_ERROR_DEVICE_LOST) in " + std::string(Where) + ".";
        m_DeviceLostReport->Details = m_DeviceLostReport->Summary + "\nThe report could not be completed: " + Exception.what() + '\n';
    }
    catch (...)
    {
        m_DeviceLostReport = std::make_unique<DeviceLostReport>();
    }

    std::fprintf(stderr, "\n[luvk] ===== GPU DEVICE LOST =====\n%s\n[luvk] ===========================\n", m_DeviceLostReport->Details.c_str());
    std::fflush(stderr);
    m_DeviceLostReported.store(true, std::memory_order_release);
}

luvk::DeviceLostReport luvk::Context::BuildDeviceLostReport(const std::string_view Where) const
{
    const VkPhysicalDeviceProperties& Props  = m_Info->Properties.properties;
    const std::string                 Driver = DriverVersionString(Props);

    std::string DriverName = std::string(m_Info->Properties12.driverName, strnlen(m_Info->Properties12.driverName, VK_MAX_DRIVER_NAME_SIZE));
    const std::string DriverInfo = std::string(m_Info->Properties12.driverInfo, strnlen(m_Info->Properties12.driverInfo, VK_MAX_DRIVER_INFO_SIZE));
    if (!DriverInfo.empty())
    {
        DriverName += std::empty(DriverName) ? DriverInfo : " (" + DriverInfo + ")";
    }

    const DeviceLostReport Fault       = DiagnosticsModule::DescribeDeviceFault(*this);
    const DeviceLostReport Breadcrumbs = m_Diagnostics != nullptr ? m_Diagnostics->DescribeBreadcrumbs() : DeviceLostReport{};

    char Ids[96] = {};
    std::snprintf(Ids, sizeof(Ids), "vendor 0x%04X, device 0x%04X, %s", Props.vendorID, Props.deviceID, DeviceTypeName(Props.deviceType));

    DeviceLostReport Report;
    Report.Summary = "The GPU stopped responding (VK_ERROR_DEVICE_LOST) in " + std::string(Where) + ".\n";
    Report.Summary += "GPU: " + std::string(Props.deviceName) + ", driver " + Driver + ".";
    if (!std::empty(Breadcrumbs.Summary))
    {
        Report.Summary += "\n" + Breadcrumbs.Summary;
    }
    if (!std::empty(Fault.Summary))
    {
        Report.Summary += "\n" + Fault.Summary;
    }
    if (m_Diagnostics == nullptr)
    {
        Report.Summary += "\nAdd luvk::DiagnosticsModule (breadcrumbs, device fault) and enable validation with GPU-assisted validation to locate the cause.";
    }

    std::string Details = "luvk GPU crash report\n=====================\n";
    Details += "Time:        " + LocalTime("%Y-%m-%d %H:%M:%S") + '\n';
    Details += "Observed in: " + std::string(Where) + '\n';
    Details += "GPU:         " + std::string(Props.deviceName) + " (" + Ids + ")\n";
    Details += "Driver:      " + Driver + (std::empty(DriverName) ? std::string{} : " - " + DriverName) + '\n';
    Details += "Vulkan:      " + std::to_string(VK_API_VERSION_MAJOR(m_ApiVersion)) + '.' + std::to_string(VK_API_VERSION_MINOR(m_ApiVersion)) + '\n';
    Details += "Queues:      graphics family " + std::to_string(m_GraphicsFamily) + ", transfer family " + std::to_string(m_TransferFamily) +
               (m_HasAsyncCompute ? ", async compute family " + std::to_string(m_ComputeFamily) : std::string(", no async compute")) + '\n';
    Details += "Timelines:   graphics " + std::to_string(m_GraphicsQueue.LastSubmittedValue()) + " submitted, transfer " +
               std::to_string(m_TransferQueue.LastSubmittedValue()) + " submitted" +
               (m_HasAsyncCompute ? ", compute " + std::to_string(m_ComputeQueue.LastSubmittedValue()) + " submitted" : std::string{}) + '\n';
    Details += "Validation:  " + std::string(m_ValidationEnabled ? "on" : "off") +
               (m_ValidationEnabled ? " (" + std::to_string(ValidationErrorCount()) + " errors so far" + (m_Validation.GpuAssisted ? ", GPU-assisted" : "") +
                                          (m_Validation.Synchronization ? ", synchronization" : "") + ")"
                                    : std::string{}) +
               '\n';
    Details += "Robustness:  robustBufferAccess2 " + std::string(DiagnosticsModule::IsRobustBufferAccess2Enabled(*this) ? "on" : "off") +
               ", robustImageAccess2 " + (DiagnosticsModule::IsRobustImageAccess2Enabled(*this) ? "on" : "off") + ", nullDescriptor " +
               (DiagnosticsModule::IsNullDescriptorEnabled(*this) ? "on" : "off") + '\n';
    Details += '\n' + Fault.Details;
    Details += '\n' + (m_Diagnostics != nullptr ? Breadcrumbs.Details : std::string("GPU breadcrumbs: not available (no DiagnosticsModule attached).\n"));
    Details += "\nSummary:\n" + Report.Summary + '\n';

    Report.Details = std::move(Details);
    return Report;
}

std::filesystem::path luvk::Context::WriteCrashReport(const DeviceLostReport& Report) const noexcept
{
    if (m_CrashReportDirectory.empty())
    {
        return {};
    }

    try
    {
        std::filesystem::create_directories(m_CrashReportDirectory);

        const std::string     Stem = "gpu-crash-" + LocalTime("%Y%m%d-%H%M%S");
        std::filesystem::path Path = m_CrashReportDirectory / (Stem + ".txt");
        for (std::uint32_t Suffix = 1U; std::filesystem::exists(Path) && Suffix < 100U; ++Suffix)
        {
            Path = m_CrashReportDirectory / (Stem + '-' + std::to_string(Suffix) + ".txt");
        }

        std::ofstream File(Path, std::ios::binary | std::ios::trunc);
        File << Report.Details;
        File.flush();
        return File ? Path : std::filesystem::path{};
    }
    catch (...)
    {
        return {};
    }
}


const luvk::DeviceLimits& luvk::Context::Limits() const noexcept
{
    return m_Info->Limits;
}

const VkPhysicalDeviceProperties& luvk::Context::Properties() const noexcept
{
    return m_Info->Properties.properties;
}

const VkPhysicalDeviceVulkan12Properties& luvk::Context::Properties12() const noexcept
{
    return m_Info->Properties12;
}

const VkPhysicalDeviceVulkan13Properties& luvk::Context::Properties13() const noexcept
{
    return m_Info->Properties13;
}

const luvk::FeatureChain& luvk::Context::EnabledFeatures() const noexcept
{
    return m_Info->EnabledFeatures;
}

void luvk::Context::QueryDeviceInfo()
{
    const VkPhysicalDeviceProperties& Props  = m_Info->Properties.properties;
    DeviceLimits&                     Limits = m_Info->Limits;

    Limits.DeviceName                      = Props.deviceName;
    Limits.VendorId                        = Props.vendorID;
    Limits.DriverVersion                   = Props.driverVersion;
    Limits.ApiVersion                      = Props.apiVersion;
    Limits.Discrete                        = Props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    Limits.TimestampPeriod                 = Props.limits.timestampPeriod;
    Limits.MaxImageDimension2D             = Props.limits.maxImageDimension2D;
    Limits.MaxImageDimension3D             = Props.limits.maxImageDimension3D;
    Limits.MaxComputeWorkGroupInvocations  = Props.limits.maxComputeWorkGroupInvocations;
    Limits.MaxSamplerAnisotropy            = Props.limits.maxSamplerAnisotropy;
    Limits.MinStorageBufferOffsetAlignment = Props.limits.minStorageBufferOffsetAlignment;
    Limits.SubgroupSize                    = m_Info->Properties11.subgroupSize;
    std::ranges::copy(Props.limits.maxComputeWorkGroupCount, std::begin(Limits.MaxComputeWorkGroupCount));

    VkBufferUsageFlags Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (IsDeviceExtensionEnabled(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
    {
        Usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    }
    m_DefaultBufferUsage = Usage;
}

luvk::ImageUsage luvk::Context::ResolveImageUsage(const Format Value, const ImageUsage Requested) const noexcept
{
    constexpr ImageUsage Transfer = ImageUsage::TransferSrc | ImageUsage::TransferDst;
    if (Requested != ImageUsage::Auto)
    {
        return Requested | Transfer;
    }

    const auto Index = static_cast<std::uint32_t>(Value);
    if (Index >= std::size(m_Info->FormatUsage))
    {
        return ImageUsage::Sampled | Transfer;
    }

    constexpr std::uint32_t Resolved = 0x80000000U;
    std::uint32_t           Cached   = m_Info->FormatUsage[Index].load(std::memory_order_relaxed);
    if ((Cached & Resolved) == 0U)
    {
        VkFormatProperties Properties = {};
        vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, ToVk(Value), &Properties);
        const VkFormatFeatureFlags Features = Properties.optimalTilingFeatures;

        ImageUsage Usage = Transfer;
        if ((Features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0U)
        {
            Usage |= ImageUsage::Sampled;
        }
        if ((Features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0U)
        {
            Usage |= ImageUsage::Storage;
        }
        if ((Features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0U)
        {
            Usage |= ImageUsage::ColorAttachment;
        }
        if ((Features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0U)
        {
            Usage |= ImageUsage::DepthStencilAttachment;
        }

        Cached = static_cast<std::uint32_t>(Usage) | Resolved;
        m_Info->FormatUsage[Index].store(Cached, std::memory_order_relaxed);
    }

    return static_cast<ImageUsage>(Cached & ~Resolved);
}

std::vector<luvk::HeapBudget> luvk::Context::MemoryBudget() const
{
    const VkPhysicalDeviceMemoryProperties* Memory = nullptr;
    vmaGetMemoryProperties(m_Allocator, &Memory);

    std::vector<VmaBudget> Budgets(Memory->memoryHeapCount);
    vmaGetHeapBudgets(m_Allocator, std::data(Budgets));

    std::vector<HeapBudget> Result;
    Result.reserve(std::size(Budgets));
    for (std::uint32_t Heap = 0U; Heap < Memory->memoryHeapCount; ++Heap)
    {
        Result.push_back({.Usage       = Budgets[Heap].usage,
                          .Budget      = Budgets[Heap].budget,
                          .DeviceLocal = (Memory->memoryHeaps[Heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0U});
    }
    return Result;
}

void luvk::Context::SetDebugName(const std::int32_t Type, const std::uint64_t Handle, const std::string_view Name) const
{
    if (!m_DebugUtilsEnabled || m_Device == VK_NULL_HANDLE || Handle == 0U || std::empty(Name))
    {
        return;
    }

    const std::string                   Copy(Name);
    const VkDebugUtilsObjectNameInfoEXT Info{.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                                             .objectType   = static_cast<VkObjectType>(Type),
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

namespace
{
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT      Severity,
                                                 const VkDebugUtilsMessageTypeFlagsEXT             Types,
                                                 const VkDebugUtilsMessengerCallbackDataEXT* const Data,
                                                 void* const                                       UserData)
    {
        auto* const Self = static_cast<luvk::Context*>(UserData);

        const luvk::DebugSeverity Level = ToSeverity(Severity);
        if (Level == luvk::DebugSeverity::Error && (Types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0U)
        {
            Self->NoteValidationError();
        }

        Self->EmitMessage(Level, Data != nullptr && Data->pMessage != nullptr ? Data->pMessage : "");
        return VK_FALSE;
    }
}
