// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include "luvk/Core/Common.hpp"
#include "luvk/Core/Handles.hpp"
#include "luvk/Resources/Buffer.hpp"
#include "luvk/Resources/Image.hpp"
#include "luvk/Resources/ResourceState.hpp"

namespace luvk
{
    class Context;
    class CommandList;
    class GpuProfiler;
    class DiagnosticsModule;
    enum class BreadcrumbStream : std::uint8_t;

    /** Handles are only valid for the frame (graph build) that created them; handles from another frame or another
     *  graph are detected (process-wide generations). */
    struct TextureHandle
    {
        /** Index into the graph resource array. */
        std::uint32_t Index = 0xFFFFFFFFU;

        /** Generation counter for stale-handle detection. */
        std::uint32_t Generation = 0U;

        /** Mip level the handle refers to, or AllSubresources. */
        std::uint32_t MipLevel = AllSubresources;

        /** Array layer the handle refers to, or AllSubresources. */
        std::uint32_t ArrayLayer = AllSubresources;

        /** True when the handle refers to a resource. */
        [[nodiscard]] constexpr bool IsValid() const noexcept { return Index != 0xFFFFFFFFU; }

        /** Same texture restricted to one mip level: passes can then read one mip and write another (barriers are
         *  tracked per subresource), attachments render into it and Resolve returns its own view and bindless slots. */
        [[nodiscard]] constexpr TextureHandle Mip(const std::uint32_t Level) const noexcept
        {
            TextureHandle Result = *this;
            Result.MipLevel      = Level;
            return Result;
        }

        /** Same texture restricted to one array layer (e.g. a shadow cascade or a cube face). */
        [[nodiscard]] constexpr TextureHandle Layer(const std::uint32_t LayerIndex) const noexcept
        {
            TextureHandle Result = *this;
            Result.ArrayLayer    = LayerIndex;
            return Result;
        }

        /** True when the handle covers every mip level and array layer. */
        [[nodiscard]] constexpr bool IsWhole() const noexcept { return MipLevel == AllSubresources && ArrayLayer == AllSubresources; }
    };

    /** Handle to a buffer registered in the graph. */
    struct BufferHandle
    {
        /** Index into the graph resource array. */
        std::uint32_t Index = 0xFFFFFFFFU;

        /** Generation counter for stale-handle detection. */
        std::uint32_t Generation = 0U;

        /** True when the handle refers to a resource. */
        [[nodiscard]] constexpr bool IsValid() const noexcept { return Index != 0xFFFFFFFFU; }
    };

    /**
     * How a pass uses a resource. The graph derives pipeline stages, access masks, image layouts and
     * (for transient resources) usage flags from this.
     */
    enum class Access : std::uint8_t
    {
        ColorAttachment,
        DepthAttachment,
        DepthRead,
        ShaderSampled,
        ShaderStorageRead,
        ShaderStorageWrite,
        UniformRead,
        VertexBuffer,
        IndexBuffer,
        IndirectBuffer,
        TransferSrc,
        TransferDst,
        Present,
        HostRead,
    };

    /** Load operation applied to an attachment at the start of a pass. */
    enum class LoadOp : std::uint8_t
    {
        Load,
        Clear,
        DontCare,
    };

    /** Queue type a pass executes on (drives the pipeline bind point and the barrier stages). */
    enum class PassType : std::uint8_t
    {
        Graphics,
        Compute,
        Transfer,
    };

    /** Queue a graph is executed on. */
    enum class GraphQueue : std::uint8_t
    {
        /** The graphics queue (every pass type). */
        Graphics,

        /**
         * The async compute queue (Context::ComputeQueue): only PassType::Compute and PassType::Transfer passes (no blits).
         * On a dedicated compute family its transients are created with concurrent sharing, imported images must not be
         * GraphicsQueueOnly and external images (swapchains) cannot be imported; barriers only use stages that queue supports.
         */
        AsyncCompute,
    };

    /**
     * Description of a transient texture: the graph binds it to a pooled image when it is created (so its bindless
     * indices are known while the frame is built) and recycles the image across frames.
     */
    struct TransientTextureDesc
    {
        /** Size in pixels. */
        Extent2D Extent = {};

        /** Depth in texels: more than 1 creates a 3D texture. */
        std::uint32_t Depth = 1U;

        /** Image format. */
        luvk::Format Format = luvk::Format::Undefined;

        /** Number of mip levels. */
        std::uint32_t MipLevels = 1U;

        /** Number of array layers. */
        std::uint32_t ArrayLayers = 1U;

        /** Sample count. */
        SampleCount Samples = SampleCount::X1;

        /** Debug name. */
        std::string_view Name;
    };

    /** Description of a transient buffer that the graph binds to a pooled buffer when it is created. */
    struct TransientBufferDesc
    {
        /** Size in bytes. */
        DeviceSize Size = 0U;

        /** Debug name. */
        std::string_view Name;
    };

    /** An image the graph does not own (e.g. a swapchain image). State must outlive the frame. */
    struct ExternalImage
    {
        /** Vulkan image handle. */
        VkImage Image = nullptr;

        /** Image view used when the image is accessed as a render target. */
        VkImageView View = nullptr;

        /** Image format. */
        luvk::Format Format = luvk::Format::Undefined;

        /** Size in pixels (depth is 1 for 2D images). */
        Extent3D Extent = {};

        /** Caller-owned state tracking; must outlive the frame. */
        ResourceState* State = nullptr;

        /** First barrier chains with a semaphore wait (acquire). */
        bool WaitsOnSemaphore = false;

        /** Debug name. */
        std::string_view Name;
    };

    /** Resolved Vulkan objects of a texture owned or imported by the graph. */
    struct GraphTexture
    {
        /** Vulkan image handle. */
        VkImage Image = nullptr;

        /** Image view covering the handle's subresources (the whole resource for whole handles). */
        VkImageView View = nullptr;

        /** Image format. */
        luvk::Format Format = luvk::Format::Undefined;

        /** Size in pixels of the first mip level covered (depth is 1 for 2D images). */
        Extent3D Extent = {};

        /** VkImageAspectFlags derived from the format. */
        std::uint32_t Aspect = 0U;

        /** Bindless sampled-image index (InvalidBindlessIndex when not available). */
        std::uint32_t SampledIndex = InvalidBindlessIndex;

        /** Bindless storage-image index (InvalidBindlessIndex when not available). */
        std::uint32_t StorageIndex = InvalidBindlessIndex;

        /** First mip level covered. */
        std::uint32_t BaseMipLevel = 0U;

        /** Number of mip levels covered. */
        std::uint32_t MipLevels = 1U;

        /** First array layer covered. */
        std::uint32_t BaseArrayLayer = 0U;

        /** Number of array layers covered. */
        std::uint32_t ArrayLayers = 1U;

        /** Sampled handle (2D view). */
        [[nodiscard]] constexpr Texture2D Sampled() const noexcept { return {SampledIndex}; }

        /** Sampled handle (3D view). */
        [[nodiscard]] constexpr Texture3D Sampled3D() const noexcept { return {SampledIndex}; }

        /** Storage handle. */
        [[nodiscard]] constexpr StorageImage Storage() const noexcept { return {StorageIndex}; }
    };

    /** Resolved Vulkan objects of a buffer owned or imported by the graph. */
    struct GraphBuffer
    {
        /** Vulkan buffer handle. */
        VkBuffer Buffer = nullptr;

        /** Size in bytes. */
        DeviceSize Size = 0U;

        /** Device address of the buffer (for shader access). */
        DeviceAddress Address = 0U;
    };

    class RenderGraph;

    /** Declares the resources used by one pass. Only valid inside the setup callback of AddPass. */
    class LUVK_API PassBuilder
    {
        friend class RenderGraph;

    protected:
        /** Graph that owns the pass being declared. */
        RenderGraph*  m_Graph;

        /** Index of the pass being declared. */
        std::uint32_t m_Pass;

        /** Constructor; only RenderGraph creates builders. */
        PassBuilder(RenderGraph& Graph, std::uint32_t Pass);

    public:
        /** Declares a non-attachment texture access (attachments have dedicated methods). */
        PassBuilder& Use(TextureHandle Texture, Access Usage);

        /** Declares a buffer access. */
        PassBuilder& Use(BufferHandle Buffer, Access Usage);

        /** Declares a color attachment with its load operation, clear value and store behaviour. */
        PassBuilder& ColorAttachment(TextureHandle Texture, LoadOp Load = LoadOp::Clear, ClearColorValue Clear = {}, bool Store = true);

        /** Declares the (single) depth attachment. Write = false declares a read-only access: Load must be LoadOp::Load
         *  and nothing is stored (Store is ignored). Attachments of images with several mip levels or layers must select
         *  one with TextureHandle::Mip / Layer. Formats with a stencil aspect use the same load/store operations for
         *  stencil, cleared to ClearStencil. */
        PassBuilder& DepthAttachment(TextureHandle Texture, LoadOp Load = LoadOp::Clear, float ClearDepth = 1.F, bool Write = true, bool Store = true,
                                     std::uint32_t ClearStencil = 0U);

        /** The pass has effects the graph cannot see (e.g. writes through raw handles): never culled. */
        PassBuilder& SideEffect();
    };

    /**
     * PassBuilder of AddPass<Push>: each method declares one access and returns what the shader receives for it (a typed
     * bindless handle or a device address), so filling the push struct and declaring the pass are the same line.
     * Persistent Image and Buffer objects are accepted directly and imported once per frame.
     */
    class LUVK_API PassBinder : public PassBuilder
    {
        friend class RenderGraph;

        /** Constructor; only RenderGraph creates binders. */
        PassBinder(RenderGraph& Graph, std::uint32_t Pass);

    public:
        /** Sampled 2D read (Access::ShaderSampled). */
        Texture2D Sampled(TextureHandle Texture);

        /** Sampled 2D read of a persistent image. */
        Texture2D Sampled(Image& Texture);

        /** Sampled 3D read (Access::ShaderSampled). */
        Texture3D Sampled3D(TextureHandle Texture);

        /** Sampled 3D read of a persistent image. */
        Texture3D Sampled3D(Image& Texture);

        /** Sampled cube read of a persistent image. */
        TextureCube SampledCube(Image& Texture);

        /** Storage image read (Access::ShaderStorageRead). */
        StorageImage Read(TextureHandle Texture);

        /** Storage image read of a persistent image. */
        StorageImage Read(Image& Texture);

        /** Storage image write or read-write (Access::ShaderStorageWrite). */
        StorageImage Write(TextureHandle Texture);

        /** Storage image write or read-write of a persistent image. */
        StorageImage Write(Image& Texture);

        /** Storage buffer read (Access::ShaderStorageRead). */
        DeviceAddress Read(BufferHandle Buffer);

        /** Storage buffer read of a persistent buffer. */
        DeviceAddress Read(Buffer& Data);

        /** Storage buffer write or read-write (Access::ShaderStorageWrite). */
        DeviceAddress Write(BufferHandle Buffer);

        /** Storage buffer write or read-write of a persistent buffer. */
        DeviceAddress Write(Buffer& Data);

        /** Indirect arguments read (Access::IndirectBuffer): returns the handle the execute callback passes to the draw. */
        BufferHandle Indirect(BufferHandle Buffer);

        /** Indirect arguments read of a persistent buffer. */
        BufferHandle Indirect(Buffer& Data);

        /** Read access as a bounds-checked span of T. */
        template <typename T>
        GpuSpan<T> ReadSpan(const BufferHandle Buffer)
        {
            return MakeSpan<T>(Buffer, Read(Buffer));
        }

        /** Read access of a persistent buffer as a span of T. */
        template <typename T>
        GpuSpan<T> ReadSpan(Buffer& Data)
        {
            return {.Address = Read(Data), .Count = static_cast<std::uint32_t>(Data.Size() / sizeof(T))};
        }

        /** Write access as a bounds-checked span of T. */
        template <typename T>
        GpuSpan<T> WriteSpan(const BufferHandle Buffer)
        {
            return MakeSpan<T>(Buffer, Write(Buffer));
        }

        /** Write access of a persistent buffer as a span of T. */
        template <typename T>
        GpuSpan<T> WriteSpan(Buffer& Data)
        {
            return {.Address = Write(Data), .Count = static_cast<std::uint32_t>(Data.Size() / sizeof(T))};
        }

    private:
        /** Span of T over the resolved size of Buffer. */
        template <typename T>
        GpuSpan<T> MakeSpan(BufferHandle Buffer, DeviceAddress Address) const;

        /** Size in bytes of a graph buffer. */
        [[nodiscard]] DeviceSize BufferSize(BufferHandle Buffer) const;
    };

    /**
     * Frame graph: passes declare what they read and write, the graph culls unused passes, allocates and
     * recycles transient resources and records every barrier and layout transition (synchronization2),
     * plus dynamic rendering begin/end for passes with attachments.
     *
     * Typical frame:
     *     auto Color = Graph.CreateTexture({.Extent = Size, .Format = Format::R16G16B16A16Sfloat});
     *     Graph.AddPass("Scene", PassType::Graphics,
     *                   [&](PassBuilder& Pass) { Pass.ColorAttachment(Color, LoadOp::Clear); },
     *                   [&](CommandList& Cmd) { Cmd.BindPipeline(ScenePipeline); Cmd.Draw(3); });
     *
     * A compute pass whose resources live in its push struct:
     *     Graph.AddPass<BlurPush>("Blur", PassType::Compute,
     *                             [&](PassBinder& Bind) { return BlurPush{.Source = Bind.Sampled(Color), .Target = Bind.Write(Out)}; },
     *                             [&](CommandList& Cmd, const BlurPush& Push) { Cmd.BindPipeline(Blur); Cmd.PushConstants(Push); Cmd.DispatchThreads(W, H); });
     *
     * A graph instance must not be executed again before its previous execution completed on the GPU
     * (the Renderer keeps one graph per frame in flight).
     */
    class LUVK_API RenderGraph
    {
        friend class PassBuilder;
        friend class CommandList;

    public:
        /** Callback that records a pass's commands into the command list. */
        using ExecuteFunction = std::function<void(CommandList&)>;

        /** Constructor; Queue selects the queue the graph is executed on (see GraphQueue). */
        explicit RenderGraph(Context& Ctx, GraphQueue Queue = GraphQueue::Graphics);

        /** Destructor. */
        ~RenderGraph();

        /** Non-copyable. */
        RenderGraph(const RenderGraph&)            = delete;

        /** Non-copyable. */
        RenderGraph& operator=(const RenderGraph&) = delete;

        /** Move constructor. */
        RenderGraph(RenderGraph&&) noexcept;

        /** Move assignment operator. */
        RenderGraph& operator=(RenderGraph&&) noexcept;

        /** Starts a new frame: drops passes and resource handles (pooled transients are kept for reuse). */
        void Reset();

        /** Imports an existing Image (its own state is tracked); returns its handle. */
        TextureHandle Import(Image& Texture);

        /** Imports an existing Buffer (its own state is tracked); returns its handle. */
        BufferHandle  Import(Buffer& Data);

        /** Imports a raw image with caller-owned state (e.g. the swapchain backbuffer); returns its handle. */
        TextureHandle ImportExternal(const ExternalImage& Texture);

        /** Declares a transient texture (allocated on demand, pooled across frames); returns its handle. */
        TextureHandle CreateTexture(const TransientTextureDesc& Desc);

        /** Declares a transient buffer (allocated on demand, pooled across frames); returns its handle. */
        BufferHandle  CreateBuffer(const TransientBufferDesc& Desc);

        /** Adds a pass: Setup declares its resource accesses, Execute records its commands. */
        template <typename SetupFunction>
        void AddPass(const std::string_view Name, const PassType Type, SetupFunction&& Setup, ExecuteFunction Execute)
        {
            PassBuilder Builder = BeginPass(Name, Type, std::move(Execute));
            std::forward<SetupFunction>(Setup)(Builder);
        }

        /**
         * Adds a pass declared from its push struct: Bind receives a PassBinder, declares every access while filling the
         * push struct and returns it; Execute receives the command list and that struct.
         */
        template <typename Push, typename BindFunction, typename PushExecuteFunction>
            requires std::is_trivially_copyable_v<Push>
        void AddPass(const std::string_view Name, const PassType Type, BindFunction&& Bind, PushExecuteFunction&& Execute)
        {
            static_assert(sizeof(Push) <= PushConstantSize, "Push constants are limited to luvk::PushConstantSize bytes");
            PassBinder Binder(*this, BeginPass(Name, Type, {}).m_Pass);
            const Push Value = std::forward<BindFunction>(Bind)(static_cast<PassBinder&>(Binder));
            SetExecute(Binder.m_Pass, [Value, Function = std::forward<PushExecuteFunction>(Execute)](CommandList& Cmd) { Function(Cmd, Value); });
        }

        /** Access the resource must be left in after the graph (e.g. Access::Present, Access::HostRead). */
        void SetFinalAccess(TextureHandle Texture, Access Usage);

        /** Final access a buffer must be left in after Execute (e.g. Access::HostRead). */
        void SetFinalAccess(BufferHandle Buffer, Access Usage);

        /** Culls and records everything into Commands. */
        void Execute(VkCommandBuffer Commands);

        /**
         * Renderer hook, right after the submission that contains the last Execute: stamps GraphicsValue as the last
         * graphics use of every persistent resource the frame touched, which orders later uploads into them.
         */
        void MarkSubmitted(std::uint64_t GraphicsValue) noexcept;

        /** Renderer hook: the frame number stamped into imported resources (write-while-in-use checks of validation builds). */
        void SetFrameNumber(std::uint64_t Frame) noexcept;

        /** Times every executed pass with GPU timestamps (nullptr disables). */
        void SetProfiler(GpuProfiler* Profiler) noexcept;

        /**
         * Writes a GPU breadcrumb before and after every executed pass through Module (nullptr disables), tagged with the
         * frame slot, stream and frame number. The Renderer sets it when a DiagnosticsModule with breadcrumbs is attached.
         */
        void SetBreadcrumbs(DiagnosticsModule* Module, std::uint32_t Slot, BreadcrumbStream Stream, std::uint64_t Frame) noexcept;

        /** Queue this graph is executed on. */
        [[nodiscard]] GraphQueue Queue() const noexcept { return m_Queue; }

        /** Fills every mip level after the first by successive linear blits (the texture needs no special usage: the
         *  transfer accesses are declared here). Typically run after rendering or uploading mip 0. */
        void GenerateMips(TextureHandle Texture);

        /** Resolved Vulkan objects of a graph texture (the view and bindless slots of the selected subresources). */
        [[nodiscard]] GraphTexture ResolveTexture(TextureHandle Texture) const;

        /** Resolved Vulkan objects of a graph buffer. */
        [[nodiscard]] const GraphBuffer&  ResolveBuffer(BufferHandle Buffer) const;

        /** VkPipelineStageFlags2 at which an ExternalImage with WaitsOnSemaphore is first used (valid after Execute()). */
        [[nodiscard]] std::uint64_t FirstUseStage(TextureHandle Texture) const;

        /** Number of passes declared since the last Reset (including culled ones). */
        [[nodiscard]] std::size_t PassCount() const noexcept;

        /** Number of passes recorded by the last Execute (after culling). */
        [[nodiscard]] std::size_t ExecutedPassCount() const noexcept { return m_ExecutedPasses; }

        /** Number of barriers emitted by the last Execute. */
        [[nodiscard]] std::size_t BarrierCount() const noexcept;

        /** Number of transient textures kept in the pool for reuse. */
        [[nodiscard]] std::size_t PooledTextureCount() const noexcept;

        /** Number of transient buffers kept in the pool for reuse. */
        [[nodiscard]] std::size_t PooledBufferCount() const noexcept;

    private:
        struct Implementation;

        /** Registers a pass and returns its builder; called by AddPass. */
        PassBuilder BeginPass(std::string_view Name, PassType Type, ExecuteFunction Execute);

        /** Replaces the execute callback of a pass (AddPass<Push> sets it once the push struct is known). */
        void SetExecute(std::uint32_t Pass, ExecuteFunction Execute);

        /** True for the async compute graph on a dedicated compute family (images must then be concurrent, barriers compute-only). */
        [[nodiscard]] bool IsOnComputeFamily() const noexcept;

        /** Owning context. */
        Context* m_Context = nullptr;

        /** Implementation holding the passes, resources and transient pools. */
        std::unique_ptr<Implementation> m_Impl;

        /** Optional profiler that timestamps each executed pass. */
        GpuProfiler* m_Profiler = nullptr;

        /** Queue the graph is executed on. */
        GraphQueue m_Queue = {GraphQueue::Graphics};

        /** Optional breadcrumb writer (DiagnosticsModule). */
        DiagnosticsModule* m_Breadcrumbs = nullptr;

        /** Frame slot of the breadcrumbs. */
        std::uint32_t m_BreadcrumbSlot = 0U;

        /** Stream of the breadcrumbs. */
        BreadcrumbStream m_BreadcrumbStream = {};

        /** Frame number of the breadcrumbs. */
        std::uint64_t m_BreadcrumbFrame = 0U;

        /** Passes recorded by the last Execute. */
        std::size_t m_ExecutedPasses = 0U;
    };

    template <typename T>
    GpuSpan<T> PassBinder::MakeSpan(const BufferHandle Buffer, const DeviceAddress Address) const
    {
        return {.Address = Address, .Count = static_cast<std::uint32_t>(BufferSize(Buffer) / sizeof(T))};
    }
}
