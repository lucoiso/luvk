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
#include <array>
#include "luvk/Core/Common.hpp"
#include "luvk/Core/DeletionQueue.hpp"
#include "luvk/Core/Queue.hpp"

struct VkPhysicalDeviceProperties;
struct VkPhysicalDeviceVulkan12Properties;
struct VkPhysicalDeviceVulkan13Properties;

namespace luvk
{
    class Module;
    class FeatureChain;
    struct InstanceRequirements;
    struct DeviceRequirements;
    struct DeviceInfo;
    class BindlessHeap;
    class UploadManager;
    class DiagnosticsModule;
    struct DeviceLostReport;

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

    /**
     * Features of VK_LAYER_KHRONOS_validation, applied only when ContextDesc::EnableValidation is set. They are passed
     * through VK_EXT_layer_settings (validate_gpu_based/gpuav_enable, validate_sync, validate_best_practices,
     * printf_enable), or VkValidationFeaturesEXT with older layers.
     */
    struct ValidationOptions
    {
        /** GPU-assisted validation: instruments shaders to catch out-of-bounds descriptor indexing, invalid buffer device
         *  addresses, bad indirect arguments... (slow; the best tool against VK_ERROR_DEVICE_LOST). */
        bool GpuAssisted = false;

        /** Synchronization validation: reports missing or wrong barriers (hazards) between commands and submissions. */
        bool Synchronization = false;

        /** Best practices warnings (performance and portability). */
        bool BestPractices = false;

        /** Shader debugPrintfEXT / Slang printf output (written to stdout by the layer). */
        bool DebugPrintf = false;
    };

    /** Queues created besides the graphics and transfer ones. */
    struct ExtraQueueDesc
    {
        /** Creates a dedicated compute queue (a family with compute and without graphics) when the device has one; it runs
         *  the Renderer's async compute graph. Without it Context::ComputeQueue() is the graphics queue. */
        bool AsyncCompute = true;

        /** Creates one more present-capable queue for third-party code (e.g. FidelityFX frame generation's presentQueue):
         *  a queue of a non-graphics family when it can present, else another queue of the graphics family. luvk never
         *  submits to it. Requires a surface. See Context::GetQueueInfo(QueueRole::Present). */
        bool Present = false;

        /** Creates one more queue for third-party code (e.g. FidelityFX frame generation's imageAcquireQueue); luvk never
         *  submits to it. See Context::GetQueueInfo(QueueRole::ImageAcquire). */
        bool ImageAcquire = false;
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

        /** Application version (major, minor, patch) reported to the driver. */
        std::array<std::uint32_t, 3> ApplicationVersion = {1U, 0U, 0U};

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

        /** Validation layer features (only applied when EnableValidation is set). */
        ValidationOptions Validation = {};

        /** Additional queues (async compute, third-party present / image acquire queues). */
        ExtraQueueDesc ExtraQueues = {};

        /** When set, device-lost reports are also written to this directory as gpu-crash-YYYYMMDD-HHMMSS.txt. */
        std::filesystem::path CrashReportDirectory;
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
     *
     * Whenever luvk observes VK_ERROR_DEVICE_LOST (any checked Vulkan call: submits, waits, present, acquire...), the
     * Context produces one DeviceLostReport (see ReportDeviceLost); from then on every wait returns at once, so teardown
     * after a device loss never blocks.
     */
    class LUVK_API Context
    {
        friend class DiagnosticsModule;

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

        /** Window surface still owned by this Context (nullptr for headless contexts or once released). */
        [[nodiscard]] VkSurfaceKHR     Surface() const noexcept { return m_Surface; }

        /** Transfers ownership of the window surface to its presenter; Surface() returns nullptr afterwards. */
        [[nodiscard]] VkSurfaceKHR     ReleaseSurface() noexcept { return std::exchange(m_Surface, nullptr); }

        /** VMA allocator (created with buffer device address support). */
        [[nodiscard]] VmaAllocator     Allocator() const noexcept { return m_Allocator; }

        /** Pipeline cache (empty unless ContextDesc::PipelineCachePath was set). */
        [[nodiscard]] VkPipelineCache  PipelineCache() const noexcept { return m_PipelineCache; }

        /** Device API version as a packed VK_API_VERSION_* value (1.3 or 1.4). */
        [[nodiscard]] std::uint32_t    ApiVersion() const noexcept { return m_ApiVersion; }

        /** Device identity and common limits. */
        [[nodiscard]] const DeviceLimits& Limits() const noexcept;

        /** Usage and budget of every memory heap (VK_EXT_memory_budget through VMA). */
        [[nodiscard]] std::vector<HeapBudget> MemoryBudget() const;

        /** Usage an image of Value gets: Requested when set (plus transfer), else every usage the format supports. */
        [[nodiscard]] ImageUsage ResolveImageUsage(Format Value, ImageUsage Requested) const noexcept;

        /** VkBufferUsageFlags every buffer is created with (storage, uniform, index, vertex, indirect, transfer, address). */
        [[nodiscard]] std::uint32_t DefaultBufferUsage() const noexcept { return m_DefaultBufferUsage; }

        /** Number of the frame being recorded by the Renderer (0 outside a frame). */
        [[nodiscard]] std::uint64_t RecordingFrame() const noexcept { return m_RecordingFrame.load(std::memory_order_relaxed); }

        /** Renderer hook: the frame being recorded (0 once it is submitted). */
        void SetRecordingFrame(std::uint64_t Frame) noexcept { m_RecordingFrame.store(Frame, std::memory_order_relaxed); }

        /** Interop: basic device properties (luvk/Core/Vulkan.hpp defines the type). */
        [[nodiscard]] const VkPhysicalDeviceProperties&         Properties() const noexcept;

        /** Interop: Vulkan 1.2 core properties. */
        [[nodiscard]] const VkPhysicalDeviceVulkan12Properties& Properties12() const noexcept;

        /** Interop: Vulkan 1.3 core properties. */
        [[nodiscard]] const VkPhysicalDeviceVulkan13Properties& Properties13() const noexcept;

        /** Interop: feature chain actually enabled on the device (luvk/Core/FeatureChain.hpp defines the type). */
        [[nodiscard]] const FeatureChain&                       EnabledFeatures() const noexcept;

        /** Device extensions enabled on the device. */
        [[nodiscard]] const std::vector<std::string>&         EnabledDeviceExtensions() const noexcept { return m_DeviceExtensions; }

        /** True when Name is among the enabled device extensions. */
        [[nodiscard]] bool                                    IsDeviceExtensionEnabled(std::string_view Name) const noexcept;

        /** True when VK_LAYER_KHRONOS_validation was enabled. */
        [[nodiscard]] bool                                    IsValidationEnabled() const noexcept { return m_ValidationEnabled; }

        /** Validation layer features actually requested from the layer (all false when validation is off). */
        [[nodiscard]] const ValidationOptions&                EnabledValidation() const noexcept { return m_Validation; }

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

        /** Async compute queue (ExtraQueueDesc::AsyncCompute), or the graphics queue when the device has none. */
        [[nodiscard]] Queue& ComputeQueue() noexcept { return m_HasAsyncCompute ? m_ComputeQueue : m_GraphicsQueue; }

        /** True when a dedicated compute queue (a family without graphics) exists. */
        [[nodiscard]] bool   HasAsyncCompute() const noexcept { return m_HasAsyncCompute; }

        /** True when transfers run on a different queue family than graphics. */
        [[nodiscard]] bool   HasDedicatedTransferQueue() const noexcept { return m_GraphicsQueue.Family() != m_TransferQueue.Family(); }

        /**
         * Queue of the given role. Present and ImageAcquire are only distinct queues (UsedByLuvk = false) when they were
         * requested in ContextDesc::ExtraQueues and the device has a free queue; otherwise they fall back to the graphics
         * queue (UsedByLuvk = true: use Queue::WithLock). Compute falls back to the graphics queue without async compute.
         */
        [[nodiscard]] QueueInfo GetQueueInfo(QueueRole Role) const noexcept;

        /** Every distinct queue family luvk submits to (graphics, transfer, async compute): resources not restricted to the
         *  graphics queue are created with VK_SHARING_MODE_CONCURRENT across them (1 to 3 entries). */
        [[nodiscard]] std::span<const std::uint32_t> SharedQueueFamilies() const noexcept;

        /** Global bindless heap. */
        [[nodiscard]] BindlessHeap&  Bindless() noexcept { return *m_Bindless; }

        /** Upload batcher for CPU -> GPU copies. */
        [[nodiscard]] UploadManager& Uploads() noexcept { return *m_Uploads; }

        /**
         * Defers destruction of Object until the graphics work submitted so far (and any frame being
         * recorded now) has completed. Thread-safe; producers briefly lock the queue.
         */
        void Retire(RetiredObject&& Object) noexcept;

        /** Same, with an explicit point (e.g. {0, TransferValue} for objects only used by the transfer queue). */
        void Retire(RetiredObject&& Object, const TimelinePoint& Point) noexcept;

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

        /** Names a Vulkan object (Type is a VkObjectType value) for debugging tools (no-op without VK_EXT_debug_utils). */
        void SetDebugName(std::int32_t Type, std::uint64_t Handle, std::string_view Name) const;

        /** Overload taking any Vulkan handle type directly (converted with HandleToU64). */
        template <typename HandleType, typename ObjectType>
        void SetDebugName(const HandleType Handle, const ObjectType Type, const std::string_view Name) const
        {
            SetDebugName(static_cast<std::int32_t>(Type), HandleToU64(Handle), Name);
        }

        /** Number of validation-layer error messages seen so far (0 when validation is off). */
        [[nodiscard]] std::uint32_t ValidationErrorCount() const noexcept { return m_ValidationErrors.load(std::memory_order_relaxed); }

        /** True once VK_ERROR_DEVICE_LOST was observed: waits return at once and the device can only be destroyed. */
        [[nodiscard]] bool IsDeviceLost() const noexcept { return m_DeviceLost.load(std::memory_order_acquire); }

        /** Report of the device loss (nullptr until one was observed). */
        [[nodiscard]] const DeviceLostReport* LastDeviceLostReport() const noexcept;

        /**
         * Marks the device as lost and, the first time only, builds the DeviceLostReport (GPU and driver, VK_EXT_device_fault
         * information, breadcrumbs of the frames in flight), prints it to stderr and writes it to
         * ContextDesc::CrashReportDirectory. luvk calls it whenever a checked Vulkan call returns VK_ERROR_DEVICE_LOST; call it
         * yourself when third-party code (e.g. a vendor SDK) reports the loss. Where describes the failing operation.
         */
        void ReportDeviceLost(std::string_view Where) noexcept;

        /** Attached DiagnosticsModule (nullptr when none was added to ContextDesc::Modules). */
        [[nodiscard]] DiagnosticsModule* Diagnostics() const noexcept { return m_Diagnostics; }

        /** Live resource counter maintained by TrackResource (should reach 0 before the Context is destroyed). */
        [[nodiscard]] std::int64_t  LiveResourceCount() const noexcept { return m_LiveResources.load(std::memory_order_relaxed); }

        /** Used by resource types to report creation/destruction. */
        void TrackResource(std::int64_t Delta) noexcept;

        /** Forwards Message to the user callback (or stderr) when Severity passes the configured minimum. */
        void EmitMessage(DebugSeverity Severity, std::string_view Message) const;

        /** Debug messenger hook: counts a validation-layer error. */
        void NoteValidationError() noexcept { m_ValidationErrors.fetch_add(1U, std::memory_order_relaxed); }

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

        /** Builds the text of a DeviceLostReport (called once, under m_DeviceLostMutex). */
        [[nodiscard]] DeviceLostReport BuildDeviceLostReport(std::string_view Where) const;

        /** Writes Report to CrashReportDirectory; returns the file written (empty on failure or without a directory). */
        [[nodiscard]] std::filesystem::path WriteCrashReport(const DeviceLostReport& Report) const noexcept;

        /** Loads and validates ContextDesc::PipelineCachePath, then creates the pipeline cache. */
        void CreatePipelineCache();

        /** Writes the pipeline cache to PipelineCachePath atomically (through a .tmp file). */
        void SavePipelineCache() const noexcept;

        /** Dispatches to the correct Vulkan destroy function for Object's kind. */
        void DestroyRetired(RetiredObject& Object) noexcept;

        /** Fills the limits, the default buffer usage and the format cache once the device exists. */
        void QueryDeviceInfo();

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
        VkInstance m_Instance = nullptr;

        /** Debug utils messenger (nullptr without debug utils). */
        VkDebugUtilsMessengerEXT m_Messenger = nullptr;

        /** Window surface owned by the Context (nullptr when headless). */
        VkSurfaceKHR m_Surface = nullptr;

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
        VkPhysicalDevice m_PhysicalDevice = nullptr;

        /** Device properties, enabled features, limits and the per-format usage cache (private Vulkan state). */
        std::unique_ptr<DeviceInfo> m_Info;

        /** VkBufferUsageFlags of every buffer. */
        std::uint32_t m_DefaultBufferUsage = 0U;

        /** Frame being recorded (0 outside a frame). */
        std::atomic<std::uint64_t> m_RecordingFrame = 0U;

        /** Device API version (packed, 1.3 or 1.4). */
        std::uint32_t m_ApiVersion = 0U;

        /** Device extensions actually enabled. */
        std::vector<std::string> m_DeviceExtensions;



        /** Graphics queue family index. */
        std::uint32_t m_GraphicsFamily = 0U;

        /** Transfer queue family index (may equal m_GraphicsFamily). */
        std::uint32_t m_TransferFamily = 0U;

        /** Queue index within the transfer family (1 when sharing the graphics family). */
        std::uint32_t m_TransferQueueIndex = 0U;

        /** Async compute queue family index (valid when m_HasAsyncCompute). */
        std::uint32_t m_ComputeFamily = 0U;

        /** Queue index within the async compute family. */
        std::uint32_t m_ComputeQueueIndex = 0U;

        /** True when a dedicated compute queue was created. */
        bool m_HasAsyncCompute = false;

        /** Extra present queue for third parties (Queue = nullptr when not created). */
        QueueInfo m_PresentQueue = {};

        /** Extra image-acquire queue for third parties (Queue = nullptr when not created). */
        QueueInfo m_ImageAcquireQueue = {};

        /** Extra queues requested by the ContextDesc. */
        ExtraQueueDesc m_ExtraQueues = {};

        /** Queue families for concurrent sharing (graphics, transfer and async compute; 1 to 3 entries). */
        std::vector<std::uint32_t> m_SharedFamilies;

        /** Logical device handle. */
        VkDevice m_Device = nullptr;

        /** Serialises graphics queue access (shared with the transfer queue when both wrap one VkQueue). */
        std::recursive_mutex m_GraphicsQueueMutex;

        /** Serialises transfer queue access. */
        std::recursive_mutex m_TransferQueueMutex;

        /** Serialises async compute queue access. */
        std::recursive_mutex m_ComputeQueueMutex;

        /** Graphics queue wrapper. */
        Queue m_GraphicsQueue = {};

        /** Transfer queue wrapper. */
        Queue m_TransferQueue = {};

        /** Async compute queue wrapper (initialized only when m_HasAsyncCompute). */
        Queue m_ComputeQueue = {};

        /** VMA allocator handle. */
        VmaAllocator m_Allocator = nullptr;

        /** Pipeline cache handle. */
        VkPipelineCache m_PipelineCache = nullptr;

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

        /** Validation layer features requested from the layer. */
        ValidationOptions m_Validation = {};

        /** Set once VK_ERROR_DEVICE_LOST was observed (read by the queues to stop waiting). */
        std::atomic<bool> m_DeviceLost = false;

        /** Serialises the creation of the device-lost report. */
        mutable std::mutex m_DeviceLostMutex;

        /** Device-lost report (null until the device is lost). */
        std::unique_ptr<DeviceLostReport> m_DeviceLostReport;

        /** Set once m_DeviceLostReport is complete (LastDeviceLostReport reads it without locking). */
        std::atomic<bool> m_DeviceLostReported = false;

        /** Directory receiving the crash reports (empty = not written). */
        std::filesystem::path m_CrashReportDirectory;

        /** Attached diagnostics module (set by DiagnosticsModule::OnContextCreated). */
        DiagnosticsModule* m_Diagnostics = nullptr;
    };
}
