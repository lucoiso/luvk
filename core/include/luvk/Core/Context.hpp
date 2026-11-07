// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "luvk/Core/DeletionQueue.hpp"
#include "luvk/Core/FeatureChain.hpp"
#include "luvk/Core/Common.hpp"
#include "luvk/Core/Queue.hpp"
#include "luvk/Core/Vma.hpp"

namespace luvk
{
    class Module;
    class BindlessHeap;
    class UploadManager;

    /** Physical device selection policy used when ContextDesc::PhysicalDeviceIndex is not set. */
    enum class DevicePreference : std::uint8_t
    {
        HighPerformance,
        LowPower,
    };

    /** Severity levels for debug messages, ordered from least to most severe. */
    enum class DebugSeverity : std::uint8_t
    {
        Verbose,
        Info,
        Warning,
        Error,
    };

    /** Sizes of the bindless descriptor arrays (clamped to the device's update-after-bind limits). */
    struct BindlessLimits
    {
        /** Maximum number of sampled images. */
        std::uint32_t SampledImages = 65536U;

        /** Number of samplers. */
        std::uint32_t Samplers = 1024U;

        /** Number of storage images. */
        std::uint32_t StorageImages = 8192U;

        /** Only used when the RayTracingModule is enabled. */
        std::uint32_t AccelerationStructures = 64U;
    };

    /** User callback receiving debug messages (defaults to printing to stderr). */
    using DebugMessageCallback = std::function<void(DebugSeverity, std::string_view)>;

    /** Callable creating a VkSurfaceKHR from the instance; the Context takes ownership of the result. */
    using SurfaceFactory       = std::function<VkSurfaceKHR(VkInstance)>;

    /** Parameters for creating a Context. */
    struct ContextDesc
    {
        /** Application name reported to the driver through VkApplicationInfo. */
        std::string ApplicationName = "luvk";

        /** Application version as a packed VK_MAKE_API_VERSION value. */
        std::uint32_t ApplicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);

        /** Enables VK_LAYER_KHRONOS_validation when available (a warning is emitted otherwise). */
        bool EnableValidation = false;

        /** Enables VK_EXT_debug_utils when available (object names, command labels). */
        bool EnableDebugUtils = true;

        /** Minimum severity forwarded to the debug callback. */
        DebugSeverity MinimumSeverity = {DebugSeverity::Warning};

        /** Defaults to printing to stderr. */
        DebugMessageCallback OnDebugMessage = {};

        /** Required instance extensions (e.g. from glfwGetRequiredInstanceExtensions). */
        std::vector<std::string> InstanceExtensions;

        /**
         * Optional: creates the window surface once the instance exists, so the device is chosen for a queue that
         * can present to it. Leave empty for headless contexts. The Context owns the surface until a presenter takes
         * it with Context::ReleaseSurface (the Renderer does).
         */
        SurfaceFactory CreateSurface = {};

        /** Modules whose requirements and lifecycle hooks are applied in order. */
        std::vector<std::shared_ptr<Module>> Modules;

        /** GPU selection policy when PhysicalDeviceIndex is not set. */
        DevicePreference Preference = {DevicePreference::HighPerformance};

        /** When set, selects this physical device index instead of scoring devices. */
        std::optional<std::uint32_t> PhysicalDeviceIndex;

        /** When set, the pipeline cache is loaded from/saved to this file (validated against the device). */
        std::filesystem::path PipelineCachePath;

        /** Bindless heap sizes. */
        BindlessLimits Bindless = {};
    };

    /**
     * Owns the whole Vulkan device stack, in a deterministic order:
     * instance -> debug messenger -> surface -> device -> queues -> allocator -> caches/heaps.
     *
     * Requires Vulkan 1.3 and enables, at minimum: dynamic rendering, synchronization2, timeline semaphores,
     * buffer device address and descriptor indexing (bindless). Only the features that are requested are
     * enabled - never "everything the GPU supports".
     *
     * Resources (Buffer, Image, Pipeline...) keep a reference to their Context and must be destroyed before it;
     * the Context counts live resources and reports any that outlive it.
     *
     * volk uses global dispatch, so only one Context may exist at a time.
     */
    class LUVK_API Context
    {
    public:
        /** Constructor: builds the whole Vulkan device stack from Desc. Throws on failure. */
        explicit Context(const ContextDesc& Desc);

        /** Destructor. */
        ~Context();

        /** Deleted copy constructor. */
        Context(const Context&)            = delete;

        /** Deleted copy assignment. */
        Context& operator=(const Context&) = delete;

        /** Deleted move constructor. */
        Context(Context&&)                 = delete;

        /** Deleted move assignment. */
        Context& operator=(Context&&)      = delete;

        /** Instance created by this Context. */
        [[nodiscard]] VkInstance       Instance() const noexcept { return m_Instance; }

        /** Selected physical device. */
        [[nodiscard]] VkPhysicalDevice PhysicalDevice() const noexcept { return m_PhysicalDevice; }

        /** Logical device created from the physical device. */
        [[nodiscard]] VkDevice         Device() const noexcept { return m_Device; }

        /** Window surface still owned by this Context (VK_NULL_HANDLE for headless contexts or once released). */
        [[nodiscard]] VkSurfaceKHR     Surface() const noexcept { return m_Surface; }

        /** Transfers ownership of the window surface to its presenter; Surface() returns VK_NULL_HANDLE afterwards. */
        [[nodiscard]] VkSurfaceKHR     ReleaseSurface() noexcept { return std::exchange(m_Surface, VK_NULL_HANDLE); }

        /** VMA allocator (created with buffer device address support). */
        [[nodiscard]] VmaAllocator     Allocator() const noexcept { return m_Allocator; }

        /** Pipeline cache (empty unless ContextDesc::PipelineCachePath was set). */
        [[nodiscard]] VkPipelineCache  PipelineCache() const noexcept { return m_PipelineCache; }

        /** Device API version as a packed VK_API_VERSION_* value (1.3 or 1.4). */
        [[nodiscard]] std::uint32_t    ApiVersion() const noexcept { return m_ApiVersion; }

        /** Basic device properties (queried without the pNext chain). */
        [[nodiscard]] const VkPhysicalDeviceProperties&       Properties() const noexcept { return m_Properties.properties; }

        /** Vulkan 1.2 core properties. */
        [[nodiscard]] const VkPhysicalDeviceVulkan12Properties& Properties12() const noexcept { return m_Properties12; }

        /** Vulkan 1.3 core properties. */
        [[nodiscard]] const VkPhysicalDeviceVulkan13Properties& Properties13() const noexcept { return m_Properties13; }

        /** Feature chain actually enabled on the device. */
        [[nodiscard]] const FeatureChain&                     EnabledFeatures() const noexcept { return m_EnabledFeatures; }

        /** Device extensions enabled on the device. */
        [[nodiscard]] const std::vector<std::string>&         EnabledDeviceExtensions() const noexcept { return m_DeviceExtensions; }

        /** True when Name is among the enabled device extensions. */
        [[nodiscard]] bool                                    IsDeviceExtensionEnabled(std::string_view Name) const noexcept;

        /** True when VK_LAYER_KHRONOS_validation was enabled. */
        [[nodiscard]] bool                                    IsValidationEnabled() const noexcept { return m_ValidationEnabled; }

        /** True when VK_EXT_debug_utils was enabled (SetDebugName is functional). */
        [[nodiscard]] bool                                    IsDebugUtilsEnabled() const noexcept { return m_DebugUtilsEnabled; }

        /** True when Name is among the enabled instance extensions. */
        [[nodiscard]] bool                                    IsInstanceExtensionEnabled(std::string_view Name) const noexcept;

        /**
         * True when presentation can signal fences (VK_KHR/EXT_swapchain_maintenance1), which lets the
         * swapchain know exactly when present semaphores and old swapchains are no longer in use.
         */
        [[nodiscard]] bool SupportsPresentFences() const noexcept { return m_PresentFences; }

        /** Graphics/compute/present queue. */
        [[nodiscard]] Queue& GraphicsQueue() noexcept { return m_GraphicsQueue; }

        /** Transfer queue (may wrap the same VkQueue as the graphics one). */
        [[nodiscard]] Queue& TransferQueue() noexcept { return m_TransferQueue; }

        /** True when transfers run on a different queue family than graphics. */
        [[nodiscard]] bool   HasDedicatedTransferQueue() const noexcept { return m_GraphicsQueue.Family() != m_TransferQueue.Family(); }

        /** Queue families that resources shared between graphics and transfer must list (1 or 2 entries). */
        [[nodiscard]] std::span<const std::uint32_t> SharedQueueFamilies() const noexcept;

        /** Global bindless heap. */
        [[nodiscard]] BindlessHeap&  Bindless() noexcept { return *m_Bindless; }

        /** Upload batcher for CPU -> GPU copies. */
        [[nodiscard]] UploadManager& Uploads() noexcept { return *m_Uploads; }

        /**
         * Defers destruction of Object until the graphics work submitted so far (and any frame being
         * recorded now) has completed. Thread-safe; producers briefly lock the queue.
         */
        void Retire(RetiredObject&& Object);

        /** Same, with an explicit point (e.g. {0, TransferValue} for objects only used by the transfer queue). */
        void Retire(RetiredObject&& Object, const TimelinePoint& Point);

        /** Retires an arbitrary action (e.g. releasing user data tied to GPU work). */
        void RetireCallback(std::function<void()> Callback);

        /** Destroys every retired object whose GPU work completed. Called automatically every frame. */
        std::size_t CollectGarbage();

        /**
         * Flushes uploads, blocks until all queues are idle and flushes the deletion queue.
         * Must not be called while command buffers referencing retired resources are still being recorded.
         */
        void WaitIdle();

        /**
         * {graphics value of the next submission, 0}: the transfer component is only set for objects that
         * were actually used by an upload, so an idle transfer queue never blocks garbage collection.
         * While a frame is being recorded the graphics component is a placeholder resolved by
         * SealFrameRetirement, because other graphics submits (acceleration structure builds, ...) may happen
         * before the frame's own submission.
         */
        [[nodiscard]] TimelinePoint CurrentRetirePoint() const noexcept;

        /** Renderer hook: objects retired from now on wait for the frame being recorded, whatever is submitted first. */
        void BeginFrameRetirement() noexcept;

        /** Renderer hook: the recorded frame was submitted as GraphicsValue; releases what was retired while recording. */
        void SealFrameRetirement(std::uint64_t GraphicsValue);

        /** Number of retired objects not destroyed yet. */
        [[nodiscard]] std::size_t   PendingDeletions() const noexcept { return m_DeletionQueue.Size(); }

        /** Names a Vulkan object for debugging tools (no-op without VK_EXT_debug_utils). */
        void SetDebugName(VkObjectType Type, std::uint64_t Handle, std::string_view Name) const;

        /** Overload taking any Vulkan handle type directly (converted with HandleToU64). */
        template <typename HandleType>
        void SetDebugName(const HandleType Handle, const VkObjectType Type, const std::string_view Name) const
        {
            SetDebugName(Type, HandleToU64(Handle), Name);
        }

        /** Number of validation-layer error messages seen so far (0 when validation is off). */
        [[nodiscard]] std::uint32_t ValidationErrorCount() const noexcept { return m_ValidationErrors.load(std::memory_order_relaxed); }

        /** Live resource counter maintained by TrackResource (should reach 0 before the Context is destroyed). */
        [[nodiscard]] std::int64_t  LiveResourceCount() const noexcept { return m_LiveResources.load(std::memory_order_relaxed); }

        /** Used by resource types to report creation/destruction. */
        void TrackResource(std::int64_t Delta) noexcept;

        /** Forwards Message to the user callback (or stderr) when Severity passes the configured minimum. */
        void EmitMessage(DebugSeverity Severity, std::string_view Message) const;

    private:
        /** Runs the full creation sequence described in the class comment. */
        void Initialize(const ContextDesc& Desc);

        /** Destroys everything in reverse creation order (assumes an idle device where possible). */
        void Shutdown() noexcept;

        /** Creates the instance, validation layer and debug messenger from Requirements. */
        void CreateInstance(const ContextDesc& Desc, const InstanceRequirements& Requirements);

        /** Scores physical devices against Requirements, honouring Desc preference/index. */
        void SelectPhysicalDevice(const ContextDesc& Desc, DeviceRequirements& Requirements);

        /** Creates the logical device with the enabled extensions/features and initializes both queues. */
        void CreateDevice();

        /** Creates the VMA allocator (buffer device address, optional memory budget/priority). */
        void CreateAllocator();

        /** Loads and validates ContextDesc::PipelineCachePath, then creates the pipeline cache. */
        void CreatePipelineCache();

        /** Writes the pipeline cache to PipelineCachePath atomically (through a .tmp file). */
        void SavePipelineCache() const noexcept;

        /** Dispatches to the correct Vulkan destroy function for Object's kind. */
        void DestroyRetired(RetiredObject& Object) noexcept;

        /** Static trampoline registered with the debug messenger (forwards to EmitMessage). */
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      Severity,
                                                            VkDebugUtilsMessageTypeFlagsEXT             Types,
                                                            const VkDebugUtilsMessengerCallbackDataEXT* Data,
                                                            void*                                       UserData);

        /** User debug callback (empty = print to stderr). */
        DebugMessageCallback m_OnDebugMessage = {};

        /** Messages below this severity are dropped. */
        DebugSeverity m_MinimumSeverity = {DebugSeverity::Warning};

        /** Number of leading modules whose OnContextCreated ran (only those get OnContextDestroying). */
        std::size_t m_ModulesCreated = 0U;

        /** Placeholder graphics value of objects retired while a frame is recorded (see CurrentRetirePoint). */
        static constexpr std::uint64_t PendingFrameValue = UINT64_MAX;

        /** Guards m_FrameRecording and m_FrameRetired. */
        mutable std::mutex m_FrameRetireMutex;

        /** True between BeginFrameRetirement and SealFrameRetirement. */
        bool m_FrameRecording = false;

        /** Objects retired while the current frame is recorded, with their transfer component. */
        std::vector<std::pair<RetiredObject, std::uint64_t>> m_FrameRetired;

        /** Pipeline cache file (empty = no persistence). */
        std::filesystem::path m_PipelineCachePath;

        /** Modules applied at creation and notified at shutdown (in reverse order). */
        std::vector<std::shared_ptr<Module>> m_Modules;

        /** VkInstance handle. */
        VkInstance m_Instance = VK_NULL_HANDLE;

        /** Debug utils messenger (VK_NULL_HANDLE without debug utils). */
        VkDebugUtilsMessengerEXT m_Messenger = VK_NULL_HANDLE;

        /** Window surface owned by the Context (VK_NULL_HANDLE when headless). */
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

        /** Instance API version clamped to 1.3/1.4 (packed VK_API_VERSION_*). */
        std::uint32_t m_InstanceApiVersion = 0U;

        /** True when the validation layer was enabled. */
        bool m_ValidationEnabled = false;

        /** True when VK_EXT_debug_utils was enabled. */
        bool m_DebugUtilsEnabled = false;

        /** True when swapchain maintenance1 present fences are available. */
        bool m_PresentFences = false;

        /** Instance extensions actually enabled. */
        std::vector<std::string> m_InstanceExtensions;

        /** True while this Context holds the process-wide single-instance slot. */
        bool m_OwnsContextSlot = false;

        /** Selected physical device. */
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

        /** Basic device properties (pNext chain set temporarily while querying). */
        VkPhysicalDeviceProperties2        m_Properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};

        /** Vulkan 1.2 core properties. */
        VkPhysicalDeviceVulkan12Properties m_Properties12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};

        /** Vulkan 1.3 core properties. */
        VkPhysicalDeviceVulkan13Properties m_Properties13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES};

        /** Device API version (packed, 1.3 or 1.4). */
        std::uint32_t m_ApiVersion = 0U;

        /** Device extensions actually enabled. */
        std::vector<std::string> m_DeviceExtensions;

        /** Feature chain enabled at device creation. */
        FeatureChain m_EnabledFeatures = {};

        /** Graphics queue family index. */
        std::uint32_t m_GraphicsFamily = 0U;

        /** Transfer queue family index (may equal m_GraphicsFamily). */
        std::uint32_t m_TransferFamily = 0U;

        /** Queue index within the transfer family (1 when sharing the graphics family). */
        std::uint32_t m_TransferQueueIndex = 0U;

        /** Queue families for concurrent sharing (1 or 2 entries). */
        std::vector<std::uint32_t> m_SharedFamilies;

        /** Logical device handle. */
        VkDevice m_Device = VK_NULL_HANDLE;

        /** Serialises graphics queue access (shared with the transfer queue when both wrap one VkQueue). */
        std::mutex m_GraphicsQueueMutex;

        /** Serialises transfer queue access. */
        std::mutex m_TransferQueueMutex;

        /** Graphics queue wrapper. */
        Queue m_GraphicsQueue = {};

        /** Transfer queue wrapper. */
        Queue m_TransferQueue = {};

        /** VMA allocator handle. */
        VmaAllocator m_Allocator = VK_NULL_HANDLE;

        /** Pipeline cache handle. */
        VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;

        /** Timeline-tagged deferred destruction queue. */
        DeletionQueue m_DeletionQueue = {};

        /** Bindless heap (created last, destroyed first). */
        std::unique_ptr<BindlessHeap> m_Bindless;

        /** Upload manager (created after the device). */
        std::unique_ptr<UploadManager> m_Uploads;

        /** Count of validation error messages (relaxed atomic). */
        std::atomic<std::uint32_t> m_ValidationErrors = 0U;

        /** Live resource counter incremented/decremented by TrackResource. */
        std::atomic<std::int64_t> m_LiveResources = 0;
    };
}
