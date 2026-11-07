// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "luvk/Render/RenderGraph.hpp"

namespace luvk
{
    /** Access masks that count as writes for the barrier tracker. */
    inline constexpr VkAccessFlags2 WriteAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT |
                                               VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                                               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                                               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                               VK_ACCESS_2_TRANSFER_WRITE_BIT |
                                               VK_ACCESS_2_HOST_WRITE_BIT |
                                               VK_ACCESS_2_MEMORY_WRITE_BIT;

    /** Executions after which an unused pooled transient is released. */
    inline constexpr std::uint64_t TransientLifetime = 8U;

    /** Process-wide handle generation: every graph build gets a fresh value, so a handle from another frame slot
     *  or another graph instance never validates by accident. Zero is skipped on wrap-around. */
    [[nodiscard]] inline std::uint32_t NextGraphGeneration() noexcept
    {
        static std::atomic<std::uint32_t> Counter{0U};
        std::uint32_t Value = 0U;
        while (Value == 0U)
        {
            Value = Counter.fetch_add(1U, std::memory_order_relaxed) + 1U;
        }
        return Value;
    }

    /** Vulkan description derived from one Access: pipeline stages, access masks, image layout and usage flags. */
    struct AccessInfo
    {
        /** Pipeline stages that perform the access. */
        VkPipelineStageFlags2 Stage = VK_PIPELINE_STAGE_2_NONE;

        /** Access mask of the operation (read, write or both). */
        VkAccessFlags2 Mask = VK_ACCESS_2_NONE;

        /** Image layout required during the access (ignored for buffers). */
        VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;

        /** True when the access writes the resource. */
        bool Write = false;

        /** True when the access applies to textures. */
        bool Images = false;

        /** True when the access applies to buffers. */
        bool Buffers = false;

        /** VkImageUsageFlags a texture needs for the access. */
        VkImageUsageFlags ImageUsage = 0U;

        /** VkBufferUsageFlags a buffer needs for the access. */
        VkBufferUsageFlags BufferUsage = 0U;
    };

    /** Human-readable name of an Access (used in error messages). */
    inline const char* AccessName(const luvk::Access Usage)
    {
        switch (Usage)
        {
        case luvk::Access::ColorAttachment: return "ColorAttachment";
        case luvk::Access::DepthAttachment: return "DepthAttachment";
        case luvk::Access::DepthRead: return "DepthRead";
        case luvk::Access::ShaderSampled: return "ShaderSampled";
        case luvk::Access::ShaderStorageRead: return "ShaderStorageRead";
        case luvk::Access::ShaderStorageWrite: return "ShaderStorageWrite";
        case luvk::Access::UniformRead: return "UniformRead";
        case luvk::Access::VertexBuffer: return "VertexBuffer";
        case luvk::Access::IndexBuffer: return "IndexBuffer";
        case luvk::Access::IndirectBuffer: return "IndirectBuffer";
        case luvk::Access::TransferSrc: return "TransferSrc";
        case luvk::Access::TransferDst: return "TransferDst";
        case luvk::Access::Present: return "Present";
        case luvk::Access::HostRead: return "HostRead";
        }
        return "?";
    }

    /** Derives the Vulkan description of one Access used in a pass of the given type (throws when the access is not allowed there). */
    inline AccessInfo Describe(const luvk::Access Usage, const luvk::PassType Type)
    {
        using luvk::Access;
        using luvk::PassType;

        const VkPipelineStageFlags2 ShaderStages = Type == PassType::Compute
                                                       ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                                                       : VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

        auto RequireType = [&](const bool Allowed)
        {
            if (!Allowed)
            {
                throw std::logic_error(std::string("[luvk] Access::") + AccessName(Usage) + " is not allowed in this pass type.");
            }
        };

        switch (Usage)
        {
        case Access::ColorAttachment:
            RequireType(Type == PassType::Graphics);
            return {.Stage      = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .Mask       = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .Layout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                    .Write      = true,
                    .Images     = true,
                    .ImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
        case Access::DepthAttachment:
            RequireType(Type == PassType::Graphics);
            return {.Stage      = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    .Mask       = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .Layout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                    .Write      = true,
                    .Images     = true,
                    .ImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};
        case Access::DepthRead:
            RequireType(Type == PassType::Graphics);
            return {.Stage      = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    .Mask       = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                    .Layout     = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    .Images     = true,
                    .ImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};
        case Access::ShaderSampled:
            RequireType(Type != PassType::Transfer);
            return {.Stage      = ShaderStages,
                    .Mask       = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .Layout     = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    .Images     = true,
                    .ImageUsage = VK_IMAGE_USAGE_SAMPLED_BIT};
        case Access::ShaderStorageRead:
            RequireType(Type != PassType::Transfer);
            return {.Stage       = ShaderStages,
                    .Mask        = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                    .Layout      = VK_IMAGE_LAYOUT_GENERAL,
                    .Images      = true,
                    .Buffers     = true,
                    .ImageUsage  = VK_IMAGE_USAGE_STORAGE_BIT,
                    .BufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
        case Access::ShaderStorageWrite:
            RequireType(Type != PassType::Transfer);
            return {.Stage       = ShaderStages,
                    .Mask        = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    .Layout      = VK_IMAGE_LAYOUT_GENERAL,
                    .Write       = true,
                    .Images      = true,
                    .Buffers     = true,
                    .ImageUsage  = VK_IMAGE_USAGE_STORAGE_BIT,
                    .BufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
        case Access::UniformRead:
            RequireType(Type != PassType::Transfer);
            return {.Stage = ShaderStages, .Mask = VK_ACCESS_2_UNIFORM_READ_BIT, .Buffers = true, .BufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};
        case Access::VertexBuffer:
            RequireType(Type == PassType::Graphics);
            return {.Stage       = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
                    .Mask        = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
                    .Buffers     = true,
                    .BufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
        case Access::IndexBuffer:
            RequireType(Type == PassType::Graphics);
            return {.Stage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, .Mask = VK_ACCESS_2_INDEX_READ_BIT, .Buffers = true, .BufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
        case Access::IndirectBuffer:
            RequireType(Type != PassType::Transfer);
            return {.Stage       = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                    .Mask        = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                    .Buffers     = true,
                    .BufferUsage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT};
        case Access::TransferSrc:
            return {.Stage       = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .Mask        = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .Layout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .Images      = true,
                    .Buffers     = true,
                    .ImageUsage  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    .BufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        case Access::TransferDst:
            return {.Stage       = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .Mask        = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .Layout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .Write       = true,
                    .Images      = true,
                    .Buffers     = true,
                    .ImageUsage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    .BufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT};
        case Access::Present:
            return {.Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, .Images = true};
        case Access::HostRead:
            return {.Stage = VK_PIPELINE_STAGE_2_HOST_BIT, .Mask = VK_ACCESS_2_HOST_READ_BIT, .Buffers = true};
        }

        throw std::logic_error("[luvk] Unknown access.");
    }
}

/** State of the render graph for one frame: resources, passes, transient pools and barrier recording (private implementation of luvk::RenderGraph). */
struct luvk::RenderGraph::Implementation
{
    /** One resource tracked by the graph: imported or transient, texture or buffer. */
    struct Resource
    {
        /** True when the resource is a texture, false when it is a buffer. */
        bool IsTexture = true;

        /** True when the resource wraps an externally owned texture or buffer. */
        bool Imported = false;

        /** Debug name of the resource (used in error messages). */
        std::string Name;

        /** Texture view data resolved during allocation (textures only). */
        GraphTexture Texture = {};

        /** Creation parameters of a transient texture (extent, format, samples). */
        TransientTextureDesc TextureDesc = {};

        /** Accumulated VkImageUsageFlags required by all accesses. */
        VkImageUsageFlags ImageUsage = 0U;

        /** Buffer handle data resolved during allocation (buffers only). */
        GraphBuffer Data = {};

        /** Creation parameters of a transient buffer (size, extra usage). */
        TransientBufferDesc BufferDesc = {};

        /** Accumulated VkBufferUsageFlags required by all accesses. */
        VkBufferUsageFlags BufferUsage = 0U;

        /** Active barrier state (points to LocalState for transients or to external storage for imported resources). */
        ResourceState* State = {nullptr};

        /** Barrier state storage for transient resources. */
        ResourceState LocalState = {};

        /** True when the first use follows an acquisition semaphore instead of graph work. */
        bool WaitsOnSemaphore = false;

        /** True after the first barrier of the resource was emitted in this frame. */
        bool FirstBarrierDone = false;

        /** Pipeline stage of the first use (kept for the semaphore-wait barrier). */
        VkPipelineStageFlags2 FirstUseStage = VK_PIPELINE_STAGE_2_NONE;

        /** Image the texture lives in (imported or pooled) for subresource views; null for external images. */
        Image* Owner = nullptr;

        /** True when a pass of this frame accesses single mip levels / layers (state is then tracked per subresource). */
        bool Split = false;

        /** Per mip/layer states of a split texture (mip-major), seeded from *State when the frame executes. */
        std::vector<ResourceState> Subresources;

        /** Layout of the most recent access: a split texture is left uniformly in it after the frame. */
        VkImageLayout LastLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        /** Final access performed on the resource after the graph (e.g. Present or HostRead). */
        std::optional<Access> FinalAccess;
    };

    /** One use of one resource by one pass. */
    struct PassAccess
    {
        /** Index into Resources. */
        std::uint32_t Resource = 0U;

        /** Mip level used, or AllSubresources. */
        std::uint32_t MipLevel = AllSubresources;

        /** Array layer used, or AllSubresources. */
        std::uint32_t ArrayLayer = AllSubresources;

        /** Pipeline stages that execute the use. */
        VkPipelineStageFlags2 Stage = VK_PIPELINE_STAGE_2_NONE;

        /** Access mask of the use (merged when a pass uses the resource several times). */
        VkAccessFlags2 Mask = VK_ACCESS_2_NONE;

        /** Image layout required by the use (undefined for buffers). */
        VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;

        /** True when the use writes the resource. */
        bool Write = false;

        /** False for write-only uses that must not keep the resource alive during culling. */
        bool ReadsContent = true;
    };

    /** Color attachment binding of one pass. */
    struct ColorAttachmentInfo
    {
        /** Index into Resources. */
        std::uint32_t Resource = 0U;

        /** Mip level rendered into, or AllSubresources. */
        std::uint32_t MipLevel = AllSubresources;

        /** Array layer rendered into, or AllSubresources. */
        std::uint32_t ArrayLayer = AllSubresources;

        /** Load operation applied when the pass begins. */
        LoadOp Load = {LoadOp::Clear};

        /** Clear value used when Load is LoadOp::Clear. */
        VkClearColorValue Clear = {};

        /** True when the attachment contents are stored when the pass ends. */
        bool Store = true;
    };

    /** Depth attachment binding of one pass. */
    struct DepthAttachmentInfo
    {
        /** Index into Resources. */
        std::uint32_t Resource = 0U;

        /** Mip level rendered into, or AllSubresources. */
        std::uint32_t MipLevel = AllSubresources;

        /** Array layer rendered into, or AllSubresources. */
        std::uint32_t ArrayLayer = AllSubresources;

        /** Load operation applied when the pass begins. */
        LoadOp Load = {LoadOp::Clear};

        /** Depth value used when Load is LoadOp::Clear. */
        float ClearDepth = 1.F;

        /** Stencil value used when Load is LoadOp::Clear (formats with a stencil aspect). */
        std::uint32_t ClearStencil = 0U;

        /** False when the depth buffer is only read (DepthRead access). */
        bool Write = true;

        /** True when the depth contents are stored when the pass ends. */
        bool Store = true;
    };

    /** One pass recorded in the graph, with its resource accesses and attachments. */
    struct Pass
    {
        /** Debug name of the pass (used in error messages). */
        std::string Name;

        /** Graphics or compute (decides which accesses are allowed). */
        PassType Type = {PassType::Graphics};

        /** Callback that records the pass commands. */
        ExecuteFunction Execute = {};

        /** All resource uses of the pass. */
        std::vector<PassAccess> Accesses;

        /** Color attachments bound by the pass. */
        std::vector<ColorAttachmentInfo> Colors;

        /** Depth attachment bound by the pass (empty when the pass has none). */
        std::optional<DepthAttachmentInfo> Depth;

        /** True when the pass has effects beyond the tracked resources and is never culled. */
        bool SideEffect = false;

        /** True when culling kept the pass. */
        bool Live = false;
    };

    /** Reusable pooled transient texture. */
    struct PooledTexture
    {
        /** Backing image of the pool entry. */
        Image Resource = {};

        /** Extent of the pooled image in texels. */
        VkExtent2D Extent = {};

        /** Format of the pooled image. */
        VkFormat Format = VK_FORMAT_UNDEFINED;

        /** Mip level count of the pooled image. */
        std::uint32_t MipLevels = 1U;

        /** Array layer count of the pooled image. */
        std::uint32_t ArrayLayers = 1U;

        /** Sample count of the pooled image. */
        VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;

        /** Superset of the image usages requested so far. */
        VkImageUsageFlags Usage = 0U;

        /** Execution index that last used the entry (drives the lifetime based release). */
        std::uint64_t LastUsed = 0U;

        /** True while the entry is bound to a resource of the current compile. */
        bool InUse = false;
    };

    /** Reusable pooled transient buffer. */
    struct PooledBuffer
    {
        /** Backing buffer of the pool entry. */
        luvk::Buffer Resource = {};

        /** Superset of the buffer usages requested so far. */
        VkBufferUsageFlags Usage = 0U;

        /** Execution index that last used the entry (drives the lifetime based release). */
        std::uint64_t LastUsed = 0U;

        /** True while the entry is bound to a resource of the current compile. */
        bool InUse = false;
    };

    /** All resources declared for the current frame. */
    std::vector<Resource> Resources;

    /** All passes in declaration order. */
    std::vector<Pass> Passes;

    /** Pool of transient textures reused across executions. */
    std::vector<std::unique_ptr<PooledTexture>> TexturePool;

    /** Pool of transient buffers reused across executions. */
    std::vector<std::unique_ptr<PooledBuffer>> BufferPool;

    /** Per-resource flag telling whether a live pass uses the resource. */
    std::vector<bool> Referenced;

    /** Fresh process-wide value on every Reset (handles from previous frames or other graphs become stale). */
    std::uint32_t Generation = NextGraphGeneration();

    /** Number of completed executions (drives pooled transient lifetimes). */
    std::uint64_t ExecutionIndex = 0U;

    /** Total number of barriers emitted so far (statistics). */
    std::size_t EmittedBarriers = 0U;

    /** Image barriers accumulated for the next flush. */
    std::vector<VkImageMemoryBarrier2> ImageBarriers;

    /** Buffer barriers accumulated for the next flush. */
    std::vector<VkBufferMemoryBarrier2> BufferBarriers;

    /** Index of the imported resource tracking State, if the graph already imported it this frame. */
    [[nodiscard]] std::optional<std::uint32_t> FindImported(const ResourceState* const State, const bool Texture) const noexcept
    {
        for (std::uint32_t Index = 0U; Index < std::size(Resources); ++Index)
        {
            if (Resources[Index].Imported && Resources[Index].IsTexture == Texture && Resources[Index].State == State)
            {
                return Index;
            }
        }
        return std::nullopt;
    }

    /** An attachment view covers one mip level and one layer: textures with several must select them with Mip / Layer. */
    static void RequireAttachmentSubresource(const Resource& Res, const TextureHandle Texture, const char* const Kind)
    {
        const bool MipMissing   = Res.Texture.MipLevels > 1U && Texture.MipLevel == AllSubresources;
        const bool LayerMissing = Res.Texture.ArrayLayers > 1U && Texture.ArrayLayer == AllSubresources;
        if (MipMissing || LayerMissing)
        {
            throw std::logic_error(std::string("[luvk] '") + Res.Name + "' has several mip levels or layers: select one for the " + Kind +
                                   " attachment with TextureHandle::Mip / Layer.");
        }
    }

    /** Throws when a subresource selection is out of the texture's range. */
    static void RequireSubresourceInRange(const Resource& Res, const std::uint32_t MipLevel, const std::uint32_t ArrayLayer)
    {
        if ((MipLevel != AllSubresources && MipLevel >= Res.Texture.MipLevels) || (ArrayLayer != AllSubresources && ArrayLayer >= Res.Texture.ArrayLayers))
        {
            throw std::out_of_range("[luvk] Texture '" + Res.Name + "': mip level or array layer out of range.");
        }
    }

    /** Graph texture describing the selected subresources of Res (its own view, bindless slots and mip extent). */
    static GraphTexture SubresourceTexture(const Resource& Res, const std::uint32_t MipLevel, const std::uint32_t ArrayLayer)
    {
        const bool WholeMips   = MipLevel == AllSubresources || Res.Texture.MipLevels == 1U;
        const bool WholeLayers = ArrayLayer == AllSubresources || Res.Texture.ArrayLayers == 1U;
        if (WholeMips && WholeLayers)
        {
            return Res.Texture;
        }

        if (Res.Owner == nullptr)
        {
            throw std::logic_error("[luvk] Subresources of '" + Res.Name + "' are not available (external image).");
        }

        const ImageSubview& Subview = Res.Owner->Subview(WholeMips ? AllSubresources : MipLevel, WholeLayers ? AllSubresources : ArrayLayer);

        GraphTexture Result = Res.Texture;
        Result.View         = Subview.View;
        Result.SampledIndex = Subview.SampledIndex;
        Result.StorageIndex = Subview.StorageIndex;
        if (!WholeMips)
        {
            Result.BaseMipLevel = MipLevel;
            Result.MipLevels    = 1U;
            Result.Extent       = {std::max(1U, Res.Texture.Extent.width >> MipLevel),
                                   std::max(1U, Res.Texture.Extent.height >> MipLevel),
                                   std::max(1U, Res.Texture.Extent.depth >> MipLevel)};
        }
        if (!WholeLayers)
        {
            Result.BaseArrayLayer = ArrayLayer;
            Result.ArrayLayers    = 1U;
        }
        return Result;
    }

    /** Validates a render graph handle against the current generation and returns the matching resource (throws when the handle is stale or mistyped). */
    Resource& Resolve(const std::uint32_t Index, const std::uint32_t HandleGeneration, const bool Texture)
    {
        if (HandleGeneration != Generation || Index >= std::size(Resources))
        {
            throw std::logic_error("[luvk] Stale or invalid render graph handle (handles are only valid for the frame that created them).");
        }

        Resource& Found = Resources[Index];
        if (Found.IsTexture != Texture)
        {
            throw std::logic_error("[luvk] Render graph handle type mismatch.");
        }
        return Found;
    }

    /** Registers one access of a resource by a pass and merges it with existing uses of the same resource (throws on invalid combinations). */
    void AddAccess(const std::uint32_t PassIndex,
                   const std::uint32_t ResourceIndex,
                   const Access        Usage,
                   const bool          ReadsContent = true,
                   const std::uint32_t MipLevel     = AllSubresources,
                   const std::uint32_t ArrayLayer   = AllSubresources)
    {
        Pass&            Target   = Passes.at(PassIndex);
        Resource&        Res      = Resources[ResourceIndex];
        const AccessInfo Info     = Describe(Usage, Target.Type);

        if (Usage == Access::Present || Usage == Access::HostRead)
        {
            throw std::logic_error("[luvk] Access::Present / Access::HostRead are final accesses: use RenderGraph::SetFinalAccess.");
        }

        if ((Res.IsTexture && !Info.Images) || (!Res.IsTexture && !Info.Buffers))
        {
            throw std::logic_error(std::string("[luvk] Access::") + AccessName(Usage) + " is not valid for " + (Res.IsTexture ? "textures" : "buffers") +
                                   " (resource '" + Res.Name + "', pass '" + Target.Name + "').");
        }

        Res.ImageUsage |= Info.ImageUsage;
        Res.BufferUsage |= Info.BufferUsage;

        if (Res.IsTexture)
        {
            RequireSubresourceInRange(Res, MipLevel, ArrayLayer);
            Res.Split = Res.Split || MipLevel != AllSubresources || ArrayLayer != AllSubresources;

            // Different subresources of one texture may be used differently in one pass (read mip N, write mip N + 1),
            // but overlapping uses must agree on the layout.
            const auto Overlaps = [&](const PassAccess& Other)
            {
                const bool MipsOverlap   = Other.MipLevel == AllSubresources || MipLevel == AllSubresources || Other.MipLevel == MipLevel;
                const bool LayersOverlap = Other.ArrayLayer == AllSubresources || ArrayLayer == AllSubresources || Other.ArrayLayer == ArrayLayer;
                return Other.Resource == ResourceIndex && MipsOverlap && LayersOverlap &&
                       (Other.MipLevel != MipLevel || Other.ArrayLayer != ArrayLayer) && Other.Layout != Info.Layout;
            };
            if (std::ranges::any_of(Target.Accesses, Overlaps))
            {
                throw std::logic_error("[luvk] Overlapping subresources of '" + Res.Name + "' are used with conflicting layouts in pass '" +
                                       Target.Name + "'.");
            }
        }

        const auto Existing = std::ranges::find_if(Target.Accesses,
                                                   [&](const PassAccess& Other)
                                                   {
                                                       return Other.Resource == ResourceIndex && Other.MipLevel == MipLevel && Other.ArrayLayer == ArrayLayer;
                                                   });
        if (Existing == std::end(Target.Accesses))
        {
            Target.Accesses.push_back(PassAccess{.Resource     = ResourceIndex,
                                                 .MipLevel     = MipLevel,
                                                 .ArrayLayer   = ArrayLayer,
                                                 .Stage        = Info.Stage,
                                                 .Mask         = Info.Mask,
                                                 .Layout       = Res.IsTexture ? Info.Layout : VK_IMAGE_LAYOUT_UNDEFINED,
                                                 .Write        = Info.Write,
                                                 .ReadsContent = ReadsContent});
            return;
        }

        if (Res.IsTexture && Existing->Layout != Info.Layout)
        {
            throw std::logic_error("[luvk] Resource '" + Res.Name + "' is used with conflicting layouts in pass '" + Target.Name +
                                   "' (e.g. sampled while bound as an attachment).");
        }

        Existing->Stage |= Info.Stage;
        Existing->Mask |= Info.Mask;
        Existing->Write = Existing->Write || Info.Write;
        Existing->ReadsContent = Existing->ReadsContent || ReadsContent;
    }

    /** Marks live passes by walking backwards from the last pass and recomputes the Referenced flags. */
    void Cull()
    {
        std::vector<bool> Needed(std::size(Resources), false);

        for (auto It = std::rbegin(Passes); It != std::rend(Passes); ++It)
        {
            Pass& Current = *It;
            Current.Live  = Current.SideEffect;

            for (const PassAccess& Use : Current.Accesses)
            {
                const Resource& Res = Resources[Use.Resource];
                if (Use.Write && (Res.Imported || Needed[Use.Resource]))
                {
                    Current.Live = true;
                }
            }

            if (Current.Live)
            {
                for (const PassAccess& Use : Current.Accesses)
                {
                    if (Use.ReadsContent)
                    {
                        Needed[Use.Resource] = true;
                    }
                }
            }
        }

        Referenced.assign(std::size(Resources), false);
        for (const Pass& Current : Passes)
        {
            if (Current.Live)
            {
                for (const PassAccess& Use : Current.Accesses)
                {
                    Referenced[Use.Resource] = true;
                }
            }
        }
    }

    /** Binds pooled textures and buffers to every referenced transient resource of the frame. */
    void AllocateTransients(Context& Ctx)
    {
        for (std::size_t Index = 0U; Index < std::size(Resources); ++Index)
        {
            Resource& Res = Resources[Index];
            if (Res.Imported)
            {
                continue;
            }

            Res.LocalState = {};
            Res.State      = &Res.LocalState;

            if (!Referenced[Index])
            {
                continue;
            }

            if (Res.IsTexture)
            {
                const TransientTextureDesc& Desc  = Res.TextureDesc;
                const VkImageUsageFlags     Usage = Res.ImageUsage | Desc.ExtraUsage;

                PooledTexture* Match = nullptr;
                for (const std::unique_ptr<PooledTexture>& Entry : TexturePool)
                {
                    if (!Entry->InUse && Entry->Extent.width == Desc.Extent.width && Entry->Extent.height == Desc.Extent.height &&
                        Entry->Format == Desc.Format && Entry->MipLevels == Desc.MipLevels && Entry->ArrayLayers == Desc.ArrayLayers &&
                        Entry->Samples == Desc.Samples && (Entry->Usage & Usage) == Usage)
                    {
                        Match = Entry.get();
                        break;
                    }
                }

                if (Match == nullptr)
                {
                    auto Entry      = std::make_unique<PooledTexture>();
                    Entry->Resource = Image(Ctx,
                                            ImageDesc{.Extent            = {Desc.Extent.width, Desc.Extent.height, 1U},
                                                      .Format            = Desc.Format,
                                                      .Usage             = Usage,
                                                      .MipLevels         = Desc.MipLevels,
                                                      .ArrayLayers       = Desc.ArrayLayers,
                                                      .Samples           = Desc.Samples,
                                                      .GraphicsQueueOnly = true,
                                                      .Name              = Res.Name});
                    Entry->Extent      = Desc.Extent;
                    Entry->Format      = Desc.Format;
                    Entry->MipLevels   = Desc.MipLevels;
                    Entry->ArrayLayers = Desc.ArrayLayers;
                    Entry->Samples     = Desc.Samples;
                    Entry->Usage       = Usage;
                    Match              = TexturePool.emplace_back(std::move(Entry)).get();
                }

                Match->InUse    = true;
                Match->LastUsed = ExecutionIndex;

                Image& Img  = Match->Resource;
                Res.Owner   = &Img;
                Res.Texture = GraphTexture{.Image        = Img.Handle(),
                                           .View         = Img.View(),
                                           .Format       = Img.Format(),
                                           .Extent       = Img.Extent(),
                                           .Aspect       = Img.Aspect(),
                                           .SampledIndex = Img.SampledIndex(),
                                           .StorageIndex = Img.StorageIndex(),
                                           .MipLevels    = Img.MipLevels(),
                                           .ArrayLayers  = Img.ArrayLayers()};
            }
            else
            {
                const VkBufferUsageFlags Usage = Res.BufferUsage | Res.BufferDesc.ExtraUsage;

                PooledBuffer* Match = nullptr;
                for (const std::unique_ptr<PooledBuffer>& Entry : BufferPool)
                {
                    // Bounded reuse: a small request must not pin a much larger pooled buffer indefinitely.
                    if (!Entry->InUse && Entry->Resource.Size() >= Res.BufferDesc.Size && Entry->Resource.Size() <= Res.BufferDesc.Size * 2U &&
                        (Entry->Usage & Usage) == Usage)
                    {
                        Match = Entry.get();
                        break;
                    }
                }

                if (Match == nullptr)
                {
                    auto Entry      = std::make_unique<PooledBuffer>();
                    Entry->Resource = luvk::Buffer(Ctx, BufferDesc{.Size = Res.BufferDesc.Size, .Usage = Usage, .Name = Res.Name});
                    Entry->Usage    = Usage;
                    Match           = BufferPool.emplace_back(std::move(Entry)).get();
                }

                Match->InUse    = true;
                Match->LastUsed = ExecutionIndex;
                Res.Data        = GraphBuffer{.Buffer = Match->Resource.Handle(), .Size = Res.BufferDesc.Size, .Address = Match->Resource.Address()};
            }
        }
    }

    /** Marks pooled entries as free and destroys the ones unused for more than TransientLifetime executions. */
    void ReleaseTransients()
    {
        for (const std::unique_ptr<PooledTexture>& Entry : TexturePool)
        {
            Entry->InUse = false;
        }
        for (const std::unique_ptr<PooledBuffer>& Entry : BufferPool)
        {
            Entry->InUse = false;
        }

        // An idle entry whose format is now used at another extent was superseded by a resize: drop it right away
        // instead of keeping full-screen targets of every intermediate size alive for TransientLifetime frames.
        const auto Superseded = [this](const PooledTexture& Idle)
        {
            return std::ranges::any_of(TexturePool,
                                       [&](const std::unique_ptr<PooledTexture>& Used)
                                       {
                                           return Used->LastUsed == ExecutionIndex && Used->Format == Idle.Format &&
                                                  (Used->Extent.width != Idle.Extent.width || Used->Extent.height != Idle.Extent.height);
                                       });
        };

        std::erase_if(TexturePool,
                      [&](const std::unique_ptr<PooledTexture>& Entry)
                      {
                          return ExecutionIndex - Entry->LastUsed > TransientLifetime || (Entry->LastUsed != ExecutionIndex && Superseded(*Entry));
                      });
        std::erase_if(BufferPool,
                      [this](const std::unique_ptr<PooledBuffer>& Entry)
                      {
                          return ExecutionIndex - Entry->LastUsed > TransientLifetime;
                      });
    }

    /** Records a write in the tracked state and invalidates the previously visible reads. */
    static void RecordWrite(ResourceState& State, const VkPipelineStageFlags2 Stage, const VkAccessFlags2 Mask)
    {
        State.WriteStages   = Stage != VK_PIPELINE_STAGE_2_NONE ? Stage : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        State.WriteAccess   = Mask & WriteAccessMask;
        State.VisibleStages = VK_PIPELINE_STAGE_2_NONE;
        State.VisibleAccess = VK_ACCESS_2_NONE;
        State.ReadStages    = VK_PIPELINE_STAGE_2_NONE;
    }

    /** Starts per-subresource tracking for every split texture, seeded from its whole-resource state. */
    void SeedSubresources()
    {
        for (Resource& Res : Resources)
        {
            if (Res.IsTexture && Res.Split && Res.State != nullptr)
            {
                Res.Subresources.assign(static_cast<std::size_t>(Res.Texture.MipLevels) * Res.Texture.ArrayLayers, *Res.State);
                Res.LastLayout = Res.State->Layout;
            }
        }
    }

    /** Emits the barriers required by one access (skipped when already satisfied) and updates the tracked state. */
    void Transition(Resource&                   Res,
                    const VkPipelineStageFlags2 Stage,
                    const VkAccessFlags2        Mask,
                    const VkImageLayout         Layout,
                    const bool                  Write,
                    const std::uint32_t         MipLevel   = AllSubresources,
                    const std::uint32_t         ArrayLayer = AllSubresources)
    {
        if (std::empty(Res.Subresources))
        {
            TransitionState(Res, *Res.State, {Res.Texture.Aspect, 0U, VK_REMAINING_MIP_LEVELS, 0U, VK_REMAINING_ARRAY_LAYERS}, Stage, Mask, Layout, Write);
            return;
        }

        const std::uint32_t FirstMip   = MipLevel == AllSubresources ? 0U : MipLevel;
        const std::uint32_t LastMip    = MipLevel == AllSubresources ? Res.Texture.MipLevels : MipLevel + 1U;
        const std::uint32_t FirstLayer = ArrayLayer == AllSubresources ? 0U : ArrayLayer;
        const std::uint32_t LastLayer  = ArrayLayer == AllSubresources ? Res.Texture.ArrayLayers : ArrayLayer + 1U;

        for (std::uint32_t Mip = FirstMip; Mip < LastMip; ++Mip)
        {
            for (std::uint32_t Layer = FirstLayer; Layer < LastLayer; ++Layer)
            {
                TransitionState(Res,
                                Res.Subresources[static_cast<std::size_t>(Mip) * Res.Texture.ArrayLayers + Layer],
                                {Res.Texture.Aspect, Mip, 1U, Layer, 1U},
                                Stage,
                                Mask,
                                Layout,
                                Write);
            }
        }
        Res.LastLayout = Layout;
    }

    /**
     * Leaves every split texture in one layout (the final access's, else the last one used) with a single tracked
     * state again, so the next frame, uploads and SetFinalAccess see a uniform image. The barriers only make the
     * writes available; the next access adds the visibility it needs.
     */
    void MergeSubresources()
    {
        for (Resource& Res : Resources)
        {
            if (std::empty(Res.Subresources))
            {
                continue;
            }

            const VkImageLayout Target = Res.FinalAccess.has_value() ? Describe(*Res.FinalAccess, PassType::Graphics).Layout : Res.LastLayout;
            bool                Pending = false;

            for (std::uint32_t Mip = 0U; Mip < Res.Texture.MipLevels; ++Mip)
            {
                for (std::uint32_t Layer = 0U; Layer < Res.Texture.ArrayLayers; ++Layer)
                {
                    const ResourceState& State  = Res.Subresources[static_cast<std::size_t>(Mip) * Res.Texture.ArrayLayers + Layer];
                    const VkPipelineStageFlags2 Stages = State.WriteStages | State.ReadStages;
                    if (State.Layout == Target && Stages == VK_PIPELINE_STAGE_2_NONE)
                    {
                        continue;
                    }

                    if (Res.Texture.Image != VK_NULL_HANDLE)
                    {
                        ImageBarriers.push_back(VkImageMemoryBarrier2{.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                                      .srcStageMask        = Stages,
                                                                      .srcAccessMask       = State.WriteAccess,
                                                                      .dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                                                      .dstAccessMask       = VK_ACCESS_2_NONE,
                                                                      .oldLayout           = State.Layout,
                                                                      .newLayout           = Target,
                                                                      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                                      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                                      .image               = Res.Texture.Image,
                                                                      .subresourceRange    = {Res.Texture.Aspect, Mip, 1U, Layer, 1U}});
                    }
                    Pending = true;
                }
            }

            *Res.State = ResourceState{.Layout      = Target,
                                       .WriteStages = Pending ? VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_2_NONE};
            Res.Subresources.clear();
            Res.Split = false;
        }
    }

    /** Emits the barrier required by one access of one tracked state (skipped when already satisfied) and updates it. */
    void TransitionState(Resource&                     Res,
                         ResourceState&                State,
                         const VkImageSubresourceRange Range,
                         const VkPipelineStageFlags2   Stage,
                         const VkAccessFlags2          Mask,
                         const VkImageLayout           Layout,
                         const bool                    Write)
    {
        const bool LayoutChange = Res.IsTexture && State.Layout != Layout;

        bool Needed = false;
        if (LayoutChange)
        {
            Needed = true;
        }
        else if (Write)
        {
            Needed = (State.WriteStages | State.ReadStages) != VK_PIPELINE_STAGE_2_NONE;
        }
        else
        {
            Needed = State.WriteStages != VK_PIPELINE_STAGE_2_NONE && ((Stage & ~State.VisibleStages) != 0U || (Mask & ~State.VisibleAccess) != 0U);
        }

        if (!Needed)
        {
            if (Write)
            {

                RecordWrite(State, Stage, Mask);
            }
            else
            {
                State.ReadStages |= Stage;
            }
            return;
        }

        VkPipelineStageFlags2 SrcStage  = State.WriteStages | State.ReadStages;
        VkAccessFlags2        SrcAccess = State.WriteAccess;

        if (Res.WaitsOnSemaphore && !Res.FirstBarrierDone)
        {

            SrcStage          = Stage != VK_PIPELINE_STAGE_2_NONE ? Stage : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            SrcAccess         = VK_ACCESS_2_NONE;
            Res.FirstUseStage = SrcStage;
        }
        Res.FirstBarrierDone = true;

        if (Res.IsTexture)
        {
            ImageBarriers.push_back(VkImageMemoryBarrier2{.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                          .srcStageMask        = SrcStage,
                                                          .srcAccessMask       = SrcAccess,
                                                          .dstStageMask        = Stage,
                                                          .dstAccessMask       = Mask,
                                                          .oldLayout           = State.Layout,
                                                          .newLayout           = Layout,
                                                          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                          .image               = Res.Texture.Image,
                                                          .subresourceRange    = Range});
        }
        else
        {
            BufferBarriers.push_back(VkBufferMemoryBarrier2{.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                                            .srcStageMask        = SrcStage,
                                                            .srcAccessMask       = SrcAccess,
                                                            .dstStageMask        = Stage,
                                                            .dstAccessMask       = Mask,
                                                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                            .buffer              = Res.Data.Buffer,
                                                            .offset              = 0U,
                                                            .size                = VK_WHOLE_SIZE});
        }

        if (Write)
        {
            RecordWrite(State, Stage, Mask);
        }
        else if (LayoutChange)
        {

            State.WriteStages   = Stage != VK_PIPELINE_STAGE_2_NONE ? Stage : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            State.WriteAccess   = VK_ACCESS_2_NONE;
            State.VisibleStages = Stage;
            State.VisibleAccess = Mask;
            State.ReadStages    = Stage;
        }
        else
        {
            State.VisibleStages |= Stage;
            State.VisibleAccess |= Mask;
            State.ReadStages |= Stage;
        }

        if (Res.IsTexture)
        {
            State.Layout = Layout;
        }
    }

    /** Records all accumulated barriers into the command buffer and clears the batches. */
    void FlushBarriers(const VkCommandBuffer Commands)
    {
        if (std::empty(ImageBarriers) && std::empty(BufferBarriers))
        {
            return;
        }

        const VkDependencyInfo Dependency{.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                          .bufferMemoryBarrierCount = static_cast<std::uint32_t>(std::size(BufferBarriers)),
                                          .pBufferMemoryBarriers    = std::data(BufferBarriers),
                                          .imageMemoryBarrierCount  = static_cast<std::uint32_t>(std::size(ImageBarriers)),
                                          .pImageMemoryBarriers     = std::data(ImageBarriers)};
        vkCmdPipelineBarrier2(Commands, &Dependency);

        EmittedBarriers += std::size(ImageBarriers) + std::size(BufferBarriers);
        ImageBarriers.clear();
        BufferBarriers.clear();
    }
};
