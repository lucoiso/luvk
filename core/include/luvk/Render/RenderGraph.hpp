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
#include <utility>
#include <vector>
#include "luvk/Core/Common.hpp"
#include "luvk/Resources/Buffer.hpp"
#include "luvk/Resources/Image.hpp"
#include "luvk/Resources/ResourceState.hpp"

namespace luvk
{
    class Context;
    class CommandList;
    class GpuProfiler;

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

    /** Description of a transient texture that the graph allocates on demand and pools for reuse. */
    struct TransientTextureDesc
    {
        /** Size in pixels (depth is 1). */
        VkExtent2D Extent = {};

        /** Image format. */
        VkFormat Format = VK_FORMAT_UNDEFINED;

        /** Number of mip levels. */
        std::uint32_t MipLevels = 1U;

        /** Number of array layers. */
        std::uint32_t ArrayLayers = 1U;

        /** Sample count. */
        VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;

        /** Usage beyond what the declared accesses imply. */
        VkImageUsageFlags ExtraUsage = 0U;

        /** Debug name. */
        std::string_view Name;
    };

    /** Description of a transient buffer that the graph allocates on demand and pools for reuse. */
    struct TransientBufferDesc
    {
        /** Size in bytes. */
        VkDeviceSize Size = 0U;

        /** Usage beyond what the declared accesses imply. */
        VkBufferUsageFlags ExtraUsage = 0U;

        /** Debug name. */
        std::string_view Name;
    };

    /** An image the graph does not own (e.g. a swapchain image). State must outlive the frame. */
    struct ExternalImage
    {
        /** Vulkan image handle. */
        VkImage Image = VK_NULL_HANDLE;

        /** Image view used when the image is accessed as a render target. */
        VkImageView View = VK_NULL_HANDLE;

        /** Image format. */
        VkFormat Format = VK_FORMAT_UNDEFINED;

        /** Size in pixels (depth is 1 for 2D images). */
        VkExtent3D Extent = {};

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
        VkImage Image = VK_NULL_HANDLE;

        /** Image view covering the handle's subresources (the whole resource for whole handles). */
        VkImageView View = VK_NULL_HANDLE;

        /** Image format. */
        VkFormat Format = VK_FORMAT_UNDEFINED;

        /** Size in pixels of the first mip level covered (depth is 1 for 2D images). */
        VkExtent3D Extent = {};

        /** Image aspects derived from the format. */
        VkImageAspectFlags Aspect = 0U;

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
    };

    /** Resolved Vulkan objects of a buffer owned or imported by the graph. */
    struct GraphBuffer
    {
        /** Vulkan buffer handle. */
        VkBuffer Buffer = VK_NULL_HANDLE;

        /** Size in bytes. */
        VkDeviceSize Size = 0U;

        /** Device address of the buffer (for shader access). */
        VkDeviceAddress Address = 0U;
    };

    class RenderGraph;

    /** Declares the resources used by one pass. Only valid inside the setup callback of AddPass. */
    class LUVK_API PassBuilder
    {
        friend class RenderGraph;

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
        PassBuilder& ColorAttachment(TextureHandle Texture, LoadOp Load = LoadOp::Clear, VkClearColorValue Clear = {}, bool Store = true);

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
     * Frame graph: passes declare what they read and write, the graph culls unused passes, allocates and
     * recycles transient resources and records every barrier and layout transition (synchronization2),
     * plus dynamic rendering begin/end for passes with attachments.
     *
     * Typical frame:
     *     auto Color = Graph.CreateTexture({.Extent = Size, .Format = VK_FORMAT_R16G16B16A16_SFLOAT});
     *     Graph.AddPass("Scene", PassType::Graphics,
     *                   [&](PassBuilder& Pass) { Pass.ColorAttachment(Color, LoadOp::Clear); },
     *                   [&](CommandList& Cmd) { Cmd.BindPipeline(ScenePipeline); Cmd.Draw(3); });
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

        /** Constructor. */
        explicit RenderGraph(Context& Ctx);

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

        template <typename SetupFunction>
        /** Adds a pass: Setup declares its resource accesses, Execute records its commands. */
        void AddPass(const std::string_view Name, const PassType Type, SetupFunction&& Setup, ExecuteFunction Execute)
        {
            PassBuilder Builder = BeginPass(Name, Type, std::move(Execute));
            std::forward<SetupFunction>(Setup)(Builder);
        }

        /** Access the resource must be left in after the graph (e.g. Access::Present, Access::HostRead). */
        void SetFinalAccess(TextureHandle Texture, Access Usage);

        /** Final access a buffer must be left in after Execute (e.g. Access::HostRead). */
        void SetFinalAccess(BufferHandle Buffer, Access Usage);

        /** Culls, allocates transients and records everything into Commands. */
        void Execute(VkCommandBuffer Commands);

        /** Times every executed pass with GPU timestamps (nullptr disables). */
        void SetProfiler(GpuProfiler* Profiler) noexcept;

        /** Fills every mip level after the first by successive linear blits (the texture needs no special usage: the
         *  transfer accesses are declared here). Typically run after rendering or uploading mip 0. */
        void GenerateMips(TextureHandle Texture);

        /** Resolved Vulkan objects of a graph texture (the view and bindless slots of the selected subresources). */
        [[nodiscard]] GraphTexture ResolveTexture(TextureHandle Texture) const;

        /** Resolved Vulkan objects of a graph buffer. */
        [[nodiscard]] const GraphBuffer&  ResolveBuffer(BufferHandle Buffer) const;

        /** Stage at which an ExternalImage with WaitsOnSemaphore is first used (valid after Execute()). */
        [[nodiscard]] VkPipelineStageFlags2 FirstUseStage(TextureHandle Texture) const;

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

        /** Owning context. */
        Context* m_Context = nullptr;

        /** Implementation holding the passes, resources and transient pools. */
        std::unique_ptr<Implementation> m_Impl;

        /** Optional profiler that timestamps each executed pass. */
        GpuProfiler* m_Profiler = nullptr;

        /** Passes recorded by the last Execute. */
        std::size_t m_ExecutedPasses = 0U;
    };
}
