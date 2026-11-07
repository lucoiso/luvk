// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "RenderGraphInternals.hpp"
#include "luvk/Render/RenderGraph.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include "luvk/Core/Context.hpp"
#include "luvk/Render/CommandList.hpp"
#include "luvk/Render/GpuProfiler.hpp"

namespace
{

    VkAttachmentLoadOp ToVulkan(const luvk::LoadOp Load)
    {
        switch (Load)
        {
        case luvk::LoadOp::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
        case luvk::LoadOp::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case luvk::LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

void luvk::RenderGraph::SetProfiler(GpuProfiler* Profiler) noexcept
{
    m_Profiler = Profiler;
}

luvk::PassBuilder::PassBuilder(RenderGraph& Graph, const std::uint32_t Pass)
    : m_Graph(&Graph),
      m_Pass(Pass)
{
}

luvk::PassBuilder& luvk::PassBuilder::Use(const TextureHandle Texture, const Access Usage)
{
    if (Usage == Access::ColorAttachment || Usage == Access::DepthAttachment)
    {
        throw std::logic_error("[luvk] Use PassBuilder::ColorAttachment / DepthAttachment for attachments.");
    }

    auto& Impl = *m_Graph->m_Impl;
    (void)Impl.Resolve(Texture.Index, Texture.Generation, true);
    // Like buffers, a transfer destination overwrites the content instead of reading it (relevant for culling).
    Impl.AddAccess(m_Pass, Texture.Index, Usage, Usage != Access::TransferDst, Texture.MipLevel, Texture.ArrayLayer);
    return *this;
}

luvk::PassBuilder& luvk::PassBuilder::Use(const BufferHandle Buffer, const Access Usage)
{
    auto& Impl = *m_Graph->m_Impl;
    (void)Impl.Resolve(Buffer.Index, Buffer.Generation, false);
    Impl.AddAccess(m_Pass, Buffer.Index, Usage, Usage != Access::TransferDst);
    return *this;
}

luvk::PassBuilder& luvk::PassBuilder::ColorAttachment(const TextureHandle Texture, const LoadOp Load, const VkClearColorValue Clear, const bool Store)
{
    auto& Impl = *m_Graph->m_Impl;
    Impl.RequireAttachmentSubresource(Impl.Resolve(Texture.Index, Texture.Generation, true), Texture, "color");
    Impl.AddAccess(m_Pass, Texture.Index, Access::ColorAttachment, Load == LoadOp::Load, Texture.MipLevel, Texture.ArrayLayer);
    Impl.Passes.at(m_Pass).Colors.push_back(
        {.Resource = Texture.Index, .MipLevel = Texture.MipLevel, .ArrayLayer = Texture.ArrayLayer, .Load = Load, .Clear = Clear, .Store = Store});
    return *this;
}

luvk::PassBuilder& luvk::PassBuilder::DepthAttachment(const TextureHandle Texture, const LoadOp Load, const float ClearDepth, const bool Write, const bool Store,
                                                     const std::uint32_t ClearStencil)
{
    auto& Impl = *m_Graph->m_Impl;
    auto& Pass = Impl.Passes.at(m_Pass);
    if (Pass.Depth.has_value())
    {
        throw std::logic_error("[luvk] A pass can only have one depth attachment.");
    }

    if (!Write && Load == LoadOp::Clear)
    {
        throw std::logic_error("[luvk] A read-only depth attachment cannot be cleared (use LoadOp::Load).");
    }

    Impl.RequireAttachmentSubresource(Impl.Resolve(Texture.Index, Texture.Generation, true), Texture, "depth");
    Impl.AddAccess(m_Pass, Texture.Index, Write ? Access::DepthAttachment : Access::DepthRead, Load == LoadOp::Load || !Write, Texture.MipLevel,
                   Texture.ArrayLayer);
    Pass.Depth = RenderGraph::Implementation::DepthAttachmentInfo{.Resource     = Texture.Index,
                                                                  .MipLevel     = Texture.MipLevel,
                                                                  .ArrayLayer   = Texture.ArrayLayer,
                                                                  .Load         = Load,
                                                                  .ClearDepth   = ClearDepth,
                                                                  .ClearStencil = ClearStencil,
                                                                  .Write        = Write,
                                                                  .Store        = Store};
    return *this;
}

luvk::PassBuilder& luvk::PassBuilder::SideEffect()
{
    m_Graph->m_Impl->Passes.at(m_Pass).SideEffect = true;
    return *this;
}

luvk::RenderGraph::RenderGraph(Context& Ctx)
    : m_Context(&Ctx),
      m_Impl(std::make_unique<Implementation>()) {}

luvk::RenderGraph::~RenderGraph() = default;

luvk::RenderGraph::RenderGraph(RenderGraph&&) noexcept = default;

luvk::RenderGraph& luvk::RenderGraph::operator=(RenderGraph&&) noexcept = default;

void luvk::RenderGraph::Reset()
{
    m_Impl->Resources.clear();
    m_Impl->Passes.clear();
    m_Impl->Generation = NextGraphGeneration();
    m_ExecutedPasses = 0U;
}

luvk::TextureHandle luvk::RenderGraph::Import(Image& Texture)
{
    if (!Texture)
    {
        throw std::invalid_argument("[luvk] Cannot import an empty Image.");
    }

    // Importing the same image twice in one graph would track its state through two resources: reuse the first one.
    if (const std::optional<std::uint32_t> Existing = m_Impl->FindImported(&Texture.State(), true))
    {
        return TextureHandle{*Existing, m_Impl->Generation};
    }

    Implementation::Resource Res{.IsTexture = true, .Imported = true, .Name = "imported image"};
    Res.Texture = GraphTexture{.Image        = Texture.Handle(),
                               .View         = Texture.View(),
                               .Format       = Texture.Format(),
                               .Extent       = Texture.Extent(),
                               .Aspect       = Texture.Aspect(),
                               .SampledIndex = Texture.SampledIndex(),
                               .StorageIndex = Texture.StorageIndex(),
                               .MipLevels    = Texture.MipLevels(),
                               .ArrayLayers  = Texture.ArrayLayers()};
    Res.State   = &Texture.State();
    Res.Owner   = &Texture;

    m_Impl->Resources.push_back(std::move(Res));
    return TextureHandle{static_cast<std::uint32_t>(std::size(m_Impl->Resources) - 1U), m_Impl->Generation};
}

luvk::BufferHandle luvk::RenderGraph::Import(luvk::Buffer& Data)
{
    if (!Data)
    {
        throw std::invalid_argument("[luvk] Cannot import an empty Buffer.");
    }

    if (const std::optional<std::uint32_t> Existing = m_Impl->FindImported(&Data.State(), false))
    {
        return BufferHandle{*Existing, m_Impl->Generation};
    }

    Implementation::Resource Res{.IsTexture = false, .Imported = true, .Name = "imported buffer"};
    Res.Data  = GraphBuffer{.Buffer = Data.Handle(), .Size = Data.Size(), .Address = Data.Address()};
    Res.State = &Data.State();

    m_Impl->Resources.push_back(std::move(Res));
    return BufferHandle{static_cast<std::uint32_t>(std::size(m_Impl->Resources) - 1U), m_Impl->Generation};
}

luvk::TextureHandle luvk::RenderGraph::ImportExternal(const ExternalImage& Texture)
{
    if (Texture.Image == VK_NULL_HANDLE || Texture.State == nullptr)
    {
        throw std::invalid_argument("[luvk] ImportExternal requires an image and a state.");
    }

    Implementation::Resource Res{.IsTexture = true, .Imported = true, .Name = std::string(Texture.Name)};
    Res.Texture          = GraphTexture{.Image = Texture.Image, .View = Texture.View, .Format = Texture.Format, .Extent = Texture.Extent, .Aspect = AspectFromFormat(Texture.Format)};
    Res.State            = Texture.State;
    Res.WaitsOnSemaphore = Texture.WaitsOnSemaphore;

    m_Impl->Resources.push_back(std::move(Res));
    return TextureHandle{static_cast<std::uint32_t>(std::size(m_Impl->Resources) - 1U), m_Impl->Generation};
}

luvk::TextureHandle luvk::RenderGraph::CreateTexture(const TransientTextureDesc& Desc)
{
    if (Desc.Extent.width == 0U || Desc.Extent.height == 0U || Desc.Format == VK_FORMAT_UNDEFINED)
    {
        throw std::invalid_argument("[luvk] Transient textures need a non-zero extent and a format.");
    }

    Implementation::Resource Res{.IsTexture = true, .Imported = false, .Name = std::string(Desc.Name)};
    Res.TextureDesc = Desc;
    Res.Texture     = GraphTexture{.Format      = Desc.Format,
                                   .Extent      = {Desc.Extent.width, Desc.Extent.height, 1U},
                                   .Aspect      = AspectFromFormat(Desc.Format),
                                   .MipLevels   = std::max(1U, Desc.MipLevels),
                                   .ArrayLayers = std::max(1U, Desc.ArrayLayers)};

    m_Impl->Resources.push_back(std::move(Res));
    return TextureHandle{static_cast<std::uint32_t>(std::size(m_Impl->Resources) - 1U), m_Impl->Generation};
}

luvk::BufferHandle luvk::RenderGraph::CreateBuffer(const TransientBufferDesc& Desc)
{
    if (Desc.Size == 0U)
    {
        throw std::invalid_argument("[luvk] Transient buffers need a non-zero size.");
    }

    Implementation::Resource Res{.IsTexture = false, .Imported = false, .Name = std::string(Desc.Name)};
    Res.BufferDesc = Desc;

    m_Impl->Resources.push_back(std::move(Res));
    return BufferHandle{static_cast<std::uint32_t>(std::size(m_Impl->Resources) - 1U), m_Impl->Generation};
}

luvk::PassBuilder luvk::RenderGraph::BeginPass(const std::string_view Name, const PassType Type, ExecuteFunction Execute)
{
    m_Impl->Passes.push_back(Implementation::Pass{.Name = std::string(Name), .Type = Type, .Execute = std::move(Execute)});
    return PassBuilder(*this, static_cast<std::uint32_t>(std::size(m_Impl->Passes) - 1U));
}

void luvk::RenderGraph::SetFinalAccess(const TextureHandle Texture, const Access Usage)
{
    if (Usage == Access::ColorAttachment || Usage == Access::DepthAttachment || Usage == Access::HostRead)
    {
        throw std::invalid_argument("[luvk] Invalid final access for a texture.");
    }
    m_Impl->Resolve(Texture.Index, Texture.Generation, true).FinalAccess = Usage;
}

void luvk::RenderGraph::SetFinalAccess(const BufferHandle Buffer, const Access Usage)
{
    if (Usage == Access::Present)
    {
        throw std::invalid_argument("[luvk] Access::Present is only valid for textures.");
    }
    m_Impl->Resolve(Buffer.Index, Buffer.Generation, false).FinalAccess = Usage;
}

void luvk::RenderGraph::Execute(const VkCommandBuffer Commands)
{
    Implementation& Impl = *m_Impl;
    ++Impl.ExecutionIndex;
    Impl.EmittedBarriers = 0U;

    Impl.Cull();
    Impl.AllocateTransients(*m_Context);
    Impl.SeedSubresources();

    const bool Labels = m_Context->IsDebugUtilsEnabled();
    m_ExecutedPasses  = 0U;

    for (Implementation::Pass& Current : Impl.Passes)
    {
        if (!Current.Live)
        {
            continue;
        }

        for (const Implementation::PassAccess& Use : Current.Accesses)
        {
            Impl.Transition(Impl.Resources[Use.Resource], Use.Stage, Use.Mask, Use.Layout, Use.Write, Use.MipLevel, Use.ArrayLayer);
        }
        Impl.FlushBarriers(Commands);

        if (Labels)
        {
            const VkDebugUtilsLabelEXT Label{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pLabelName = Current.Name.c_str()};
            vkCmdBeginDebugUtilsLabelEXT(Commands, &Label);
        }

        const std::uint32_t TimingToken = m_Profiler != nullptr ? m_Profiler->BeginPass(Commands, Current.Name) : 0xFFFFFFFFU;

        const bool HasAttachments = !std::empty(Current.Colors) || Current.Depth.has_value();
        VkExtent2D RenderExtent = {UINT32_MAX, UINT32_MAX};

        if (HasAttachments)
        {
            std::vector<VkRenderingAttachmentInfo> ColorInfos;
            ColorInfos.reserve(std::size(Current.Colors));

            for (const Implementation::ColorAttachmentInfo& Color : Current.Colors)
            {
                const GraphTexture Target = Implementation::SubresourceTexture(Impl.Resources[Color.Resource], Color.MipLevel, Color.ArrayLayer);
                RenderExtent.width         = std::min(RenderExtent.width, Target.Extent.width);
                RenderExtent.height        = std::min(RenderExtent.height, Target.Extent.height);

                ColorInfos.push_back(VkRenderingAttachmentInfo{.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                               .imageView   = Target.View,
                                                               .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                               .loadOp      = ToVulkan(Color.Load),
                                                               .storeOp     = Color.Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                               .clearValue  = {.color = Color.Clear}});
            }

            VkRenderingAttachmentInfo DepthInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            bool                      HasDepth   = false;
            bool                      HasStencil = false;

            if (Current.Depth.has_value())
            {
                const GraphTexture Target = Implementation::SubresourceTexture(Impl.Resources[Current.Depth->Resource],
                                                                               Current.Depth->MipLevel,
                                                                               Current.Depth->ArrayLayer);
                RenderExtent.width         = std::min(RenderExtent.width, Target.Extent.width);
                RenderExtent.height        = std::min(RenderExtent.height, Target.Extent.height);

                DepthInfo = VkRenderingAttachmentInfo{.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                      .imageView   = Target.View,
                                                      .imageLayout = Current.Depth->Write ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                                      .loadOp      = ToVulkan(Current.Depth->Load),
                                                      // A read-only attachment must not be written back: STORE_OP_NONE (core in 1.3).
                                                      .storeOp     = !Current.Depth->Write ? VK_ATTACHMENT_STORE_OP_NONE
                                                                     : Current.Depth->Store ? VK_ATTACHMENT_STORE_OP_STORE
                                                                                            : VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                      .clearValue  = {.depthStencil = {Current.Depth->ClearDepth, Current.Depth->ClearStencil}}};

                HasDepth   = (Target.Aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0U;
                HasStencil = (Target.Aspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0U;
            }

            const VkRenderingInfo RenderingInfo{.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                                .renderArea           = {{0, 0}, RenderExtent},
                                                .layerCount           = 1U,
                                                .colorAttachmentCount = static_cast<std::uint32_t>(std::size(ColorInfos)),
                                                .pColorAttachments    = std::data(ColorInfos),
                                                .pDepthAttachment     = HasDepth ? &DepthInfo : nullptr,
                                                .pStencilAttachment   = HasStencil ? &DepthInfo : nullptr};
            vkCmdBeginRendering(Commands, &RenderingInfo);
        }

        CommandList Cmd(*m_Context, Commands, this);
        if (HasAttachments)
        {
            Cmd.SetViewport(RenderExtent);
            Cmd.SetScissor(VkRect2D{{0, 0}, RenderExtent});
        }

        if (Current.Execute)
        {
            Current.Execute(Cmd);
        }

        if (HasAttachments)
        {
            vkCmdEndRendering(Commands);
        }

        if (m_Profiler != nullptr)
        {
            m_Profiler->EndPass(Commands, TimingToken);
        }

        if (Labels)
        {
            vkCmdEndDebugUtilsLabelEXT(Commands);
        }

        ++m_ExecutedPasses;
    }

    Impl.MergeSubresources();

    for (Implementation::Resource& Res : Impl.Resources)
    {
        if (!Res.FinalAccess.has_value() || Res.State == nullptr)
        {
            continue;
        }

        if (!Res.IsTexture && Res.Data.Buffer == VK_NULL_HANDLE)
        {
            continue;
        }

        if (Res.IsTexture && Res.Texture.Image == VK_NULL_HANDLE)
        {
            continue;
        }

        const AccessInfo Info = Describe(*Res.FinalAccess, PassType::Graphics);
        Impl.Transition(Res, Info.Stage, Info.Mask, Res.IsTexture ? Info.Layout : VK_IMAGE_LAYOUT_UNDEFINED, Info.Write);
    }
    Impl.FlushBarriers(Commands);

    Impl.ReleaseTransients();
}

luvk::GraphTexture luvk::RenderGraph::ResolveTexture(const TextureHandle Texture) const
{
    return Implementation::SubresourceTexture(m_Impl->Resolve(Texture.Index, Texture.Generation, true), Texture.MipLevel, Texture.ArrayLayer);
}

void luvk::RenderGraph::GenerateMips(const TextureHandle Texture)
{
    const Implementation::Resource& Res    = m_Impl->Resolve(Texture.Index, Texture.Generation, true);
    const std::uint32_t             Mips   = Res.Texture.MipLevels;
    const std::uint32_t             Layers = Texture.ArrayLayer == AllSubresources ? Res.Texture.ArrayLayers : 1U;

    // One pass per level: it reads the previous level and overwrites the next, so barriers are per mip level.
    for (std::uint32_t Level = 1U; Level < Mips; ++Level)
    {
        AddPass("Generate mips",
                PassType::Transfer,
                [&](PassBuilder& Pass)
                {
                    Pass.Use(Texture.Mip(Level - 1U), Access::TransferSrc).Use(Texture.Mip(Level), Access::TransferDst);
                },
                [Texture, Level, Layers](CommandList& Cmd)
                {
                    for (std::uint32_t Layer = 0U; Layer < Layers; ++Layer)
                    {
                        const TextureHandle Slice = Layers == 1U ? Texture : Texture.Layer(Layer);
                        Cmd.BlitTexture(Slice.Mip(Level - 1U), Slice.Mip(Level));
                    }
                });
    }
}

const luvk::GraphBuffer& luvk::RenderGraph::ResolveBuffer(const BufferHandle Buffer) const
{
    return m_Impl->Resolve(Buffer.Index, Buffer.Generation, false).Data;
}

VkPipelineStageFlags2 luvk::RenderGraph::FirstUseStage(const TextureHandle Texture) const
{
    const auto& Res = m_Impl->Resolve(Texture.Index, Texture.Generation, true);
    return Res.FirstUseStage != VK_PIPELINE_STAGE_2_NONE ? Res.FirstUseStage : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
}

std::size_t luvk::RenderGraph::BarrierCount() const noexcept
{
    return m_Impl->EmittedBarriers;
}

std::size_t luvk::RenderGraph::PassCount() const noexcept
{
    return std::size(m_Impl->Passes);
}

std::size_t luvk::RenderGraph::PooledTextureCount() const noexcept
{
    return std::size(m_Impl->TexturePool);
}

std::size_t luvk::RenderGraph::PooledBufferCount() const noexcept
{
    return std::size(m_Impl->BufferPool);
}
