// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "RenderGraphInternals.hpp"
#include "luvk/Render/RenderGraph.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Diagnostics.hpp"
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

void luvk::RenderGraph::SetBreadcrumbs(DiagnosticsModule* const Module, const std::uint32_t Slot, const BreadcrumbStream Stream, const std::uint64_t Frame) noexcept
{
    m_Breadcrumbs      = Module;
    m_BreadcrumbSlot   = Slot;
    m_BreadcrumbStream = Stream;
    m_BreadcrumbFrame  = Frame;
}

bool luvk::RenderGraph::IsOnComputeFamily() const noexcept
{
    return m_Queue == GraphQueue::AsyncCompute && m_Context->HasAsyncCompute() &&
           m_Context->ComputeQueue().Family() != m_Context->GraphicsQueue().Family();
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

luvk::PassBuilder& luvk::PassBuilder::ColorAttachment(const TextureHandle Texture, const LoadOp Load, const ClearColorValue Clear, const bool Store)
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

luvk::PassBinder::PassBinder(RenderGraph& Graph, const std::uint32_t Pass)
    : PassBuilder(Graph, Pass)
{
}

luvk::Texture2D luvk::PassBinder::Sampled(const TextureHandle Texture)
{
    Use(Texture, Access::ShaderSampled);
    return m_Graph->ResolveTexture(Texture).Sampled();
}

luvk::Texture2D luvk::PassBinder::Sampled(Image& Texture)
{
    return Sampled(m_Graph->Import(Texture));
}

luvk::Texture3D luvk::PassBinder::Sampled3D(const TextureHandle Texture)
{
    Use(Texture, Access::ShaderSampled);
    return m_Graph->ResolveTexture(Texture).Sampled3D();
}

luvk::Texture3D luvk::PassBinder::Sampled3D(Image& Texture)
{
    return Sampled3D(m_Graph->Import(Texture));
}

luvk::TextureCube luvk::PassBinder::SampledCube(Image& Texture)
{
    Use(m_Graph->Import(Texture), Access::ShaderSampled);
    return Texture.SampledCube();
}

luvk::StorageImage luvk::PassBinder::Read(const TextureHandle Texture)
{
    Use(Texture, Access::ShaderStorageRead);
    return m_Graph->ResolveTexture(Texture).Storage();
}

luvk::StorageImage luvk::PassBinder::Read(Image& Texture)
{
    return Read(m_Graph->Import(Texture));
}

luvk::StorageImage luvk::PassBinder::Write(const TextureHandle Texture)
{
    Use(Texture, Access::ShaderStorageWrite);
    return m_Graph->ResolveTexture(Texture).Storage();
}

luvk::StorageImage luvk::PassBinder::Write(Image& Texture)
{
    return Write(m_Graph->Import(Texture));
}

luvk::DeviceAddress luvk::PassBinder::Read(const BufferHandle Buffer)
{
    Use(Buffer, Access::ShaderStorageRead);
    return m_Graph->ResolveBuffer(Buffer).Address;
}

luvk::DeviceAddress luvk::PassBinder::Read(Buffer& Data)
{
    return Read(m_Graph->Import(Data));
}

luvk::DeviceAddress luvk::PassBinder::Write(const BufferHandle Buffer)
{
    Use(Buffer, Access::ShaderStorageWrite);
    return m_Graph->ResolveBuffer(Buffer).Address;
}

luvk::DeviceAddress luvk::PassBinder::Write(Buffer& Data)
{
    return Write(m_Graph->Import(Data));
}

luvk::BufferHandle luvk::PassBinder::Indirect(const BufferHandle Buffer)
{
    Use(Buffer, Access::IndirectBuffer);
    return Buffer;
}

luvk::BufferHandle luvk::PassBinder::Indirect(Buffer& Data)
{
    return Indirect(m_Graph->Import(Data));
}

luvk::DeviceSize luvk::PassBinder::BufferSize(const BufferHandle Buffer) const
{
    return m_Graph->ResolveBuffer(Buffer).Size;
}

luvk::RenderGraph::RenderGraph(Context& Ctx, const GraphQueue Queue)
    : m_Context(&Ctx),
      m_Impl(std::make_unique<Implementation>()),
      m_Queue(Queue)
{
    if (IsOnComputeFamily())
    {
        // Transients of the async graph live on the compute family: concurrent sharing, compute-queue barriers only.
        m_Impl->GraphicsOnlyTransients = false;
        m_Impl->ComputeQueueOnly       = true;
    }
    if (m_Queue == GraphQueue::AsyncCompute)
    {
        m_Impl->FinalAccessType = PassType::Compute;
    }
}

luvk::RenderGraph::~RenderGraph() = default;

luvk::RenderGraph::RenderGraph(RenderGraph&&) noexcept = default;

luvk::RenderGraph& luvk::RenderGraph::operator=(RenderGraph&&) noexcept = default;

void luvk::RenderGraph::Reset()
{
    m_Impl->Resources.clear();
    // The pass records stay for the next frame (their storage is reused); the callbacks and what they capture go now.
    for (Implementation::Pass& Old : m_Impl->ActivePasses())
    {
        Old.Execute = nullptr;
    }
    m_Impl->PassCount = 0U;
    m_Impl->Generation = NextGraphGeneration();
    m_ExecutedPasses   = 0U;

    // A frame that was abandoned before Execute still holds its pooled transients.
    for (const auto& Entry : m_Impl->TexturePool)
    {
        Entry->InUse = false;
    }
    for (const auto& Entry : m_Impl->BufferPool)
    {
        Entry->InUse = false;
    }
}

void luvk::RenderGraph::MarkSubmitted(const std::uint64_t GraphicsValue) noexcept
{
    for (ResourceState* const State : m_Impl->UsedStates)
    {
        State->LastGraphicsUse = std::max(State->LastGraphicsUse, GraphicsValue);
    }
    m_Impl->UsedStates.clear();
}

void luvk::RenderGraph::SetFrameNumber(const std::uint64_t Frame) noexcept
{
    m_Impl->FrameNumber = Frame;
}

void luvk::RenderGraph::SetExecute(const std::uint32_t Pass, ExecuteFunction Execute)
{
    m_Impl->Passes.at(Pass).Execute = std::move(Execute);
}

luvk::TextureHandle luvk::RenderGraph::Import(Image& Texture)
{
    if (!Texture)
    {
        throw std::invalid_argument("[luvk] Cannot import an empty Image.");
    }

    if (IsOnComputeFamily() && !Texture.IsSharedWithComputeQueue())
    {
        throw std::logic_error("[luvk] The async compute graph cannot import an image created with ImageDesc::GraphicsQueueOnly "
                               "(it is exclusive to the graphics queue family).");
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
    Texture.State().ImportedInFrame = m_Impl->FrameNumber;

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
    Data.State().ImportedInFrame = m_Impl->FrameNumber;

    m_Impl->Resources.push_back(std::move(Res));
    return BufferHandle{static_cast<std::uint32_t>(std::size(m_Impl->Resources) - 1U), m_Impl->Generation};
}

luvk::TextureHandle luvk::RenderGraph::ImportExternal(const ExternalImage& Texture)
{
    if (Texture.Image == VK_NULL_HANDLE || Texture.State == nullptr)
    {
        throw std::invalid_argument("[luvk] ImportExternal requires an image and a state.");
    }

    if (IsOnComputeFamily())
    {
        throw std::logic_error("[luvk] The async compute graph cannot import external images (e.g. swapchain images are exclusive to the graphics queue).");
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
    if (Desc.Extent.Width == 0U || Desc.Extent.Height == 0U || Desc.Format == Format::Undefined)
    {
        throw std::invalid_argument("[luvk] Transient textures need a non-zero extent and a format.");
    }

    Implementation::Resource Res{.IsTexture = true, .Imported = false, .Name = std::string(Desc.Name)};
    Res.TextureDesc             = Desc;
    Res.TextureDesc.MipLevels   = std::max(1U, Desc.MipLevels);
    Res.TextureDesc.ArrayLayers = std::max(1U, Desc.ArrayLayers);
    Res.TextureDesc.Depth       = std::max(1U, Desc.Depth);
    m_Impl->AcquireTexture(*m_Context, Res);
    Res.TextureDesc.Name = {};

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
    m_Impl->AcquireBuffer(*m_Context, Res);
    Res.BufferDesc.Name = {};

    m_Impl->Resources.push_back(std::move(Res));
    return BufferHandle{static_cast<std::uint32_t>(std::size(m_Impl->Resources) - 1U), m_Impl->Generation};
}

luvk::PassBuilder luvk::RenderGraph::BeginPass(const std::string_view Name, const PassType Type, ExecuteFunction Execute)
{
    if (m_Queue == GraphQueue::AsyncCompute && Type == PassType::Graphics)
    {
        throw std::logic_error("[luvk] Pass '" + std::string(Name) +
                               "': the async compute graph only accepts PassType::Compute and PassType::Transfer passes (add graphics passes to Frame::Graph).");
    }

    if (m_Impl->PassCount < std::size(m_Impl->Passes))
    {
        Implementation::Pass& Reused = m_Impl->Passes[m_Impl->PassCount];
        Reused.Name.assign(Name);
        Reused.Type    = Type;
        Reused.Execute = std::move(Execute);
        Reused.Accesses.clear();
        Reused.Colors.clear();
        Reused.Depth.reset();
        Reused.SideEffect = false;
        Reused.Live       = false;
    }
    else
    {
        m_Impl->Passes.push_back(Implementation::Pass{.Name = std::string(Name), .Type = Type, .Execute = std::move(Execute)});
    }
    ++m_Impl->PassCount;
    return PassBuilder(*this, static_cast<std::uint32_t>(m_Impl->PassCount - 1U));
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
    Impl.BindTransientStates();
    Impl.CollectUsedStates();
    Impl.SeedSubresources();

    const bool Labels = m_Context->IsDebugUtilsEnabled();
    m_ExecutedPasses  = 0U;

    if (m_Breadcrumbs != nullptr)
    {
        m_Breadcrumbs->BeginBreadcrumbs(m_BreadcrumbSlot, m_BreadcrumbStream, m_BreadcrumbFrame);
    }

    for (Implementation::Pass& Current : Impl.ActivePasses())
    {
        if (!Current.Live)
        {
            continue;
        }

        // Breadcrumbs bracket the pass together with its barriers, so a stall on a barrier is attributed to the pass.
        const auto PassIndex = static_cast<std::uint32_t>(m_ExecutedPasses);
        if (m_Breadcrumbs != nullptr)
        {
            m_Breadcrumbs->MarkPass(Commands, m_BreadcrumbSlot, m_BreadcrumbStream, m_BreadcrumbFrame, PassIndex, Current.Name, false);
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
        Extent2D RenderExtent = {UINT32_MAX, UINT32_MAX};

        if (HasAttachments)
        {
            std::vector<VkRenderingAttachmentInfo> ColorInfos;
            ColorInfos.reserve(std::size(Current.Colors));

            for (const Implementation::ColorAttachmentInfo& Color : Current.Colors)
            {
                const GraphTexture Target = Implementation::SubresourceTexture(Impl.Resources[Color.Resource], Color.MipLevel, Color.ArrayLayer);
                RenderExtent.Width         = std::min(RenderExtent.Width, Target.Extent.Width);
                RenderExtent.Height        = std::min(RenderExtent.Height, Target.Extent.Height);

                ColorInfos.push_back(VkRenderingAttachmentInfo{.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                               .imageView   = Target.View,
                                                               .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                               .loadOp      = ToVulkan(Color.Load),
                                                               .storeOp     = Color.Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                               .clearValue  = {.color = ToVk(Color.Clear)}});
            }

            VkRenderingAttachmentInfo DepthInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            bool                      HasDepth   = false;
            bool                      HasStencil = false;

            if (Current.Depth.has_value())
            {
                const GraphTexture Target = Implementation::SubresourceTexture(Impl.Resources[Current.Depth->Resource],
                                                                               Current.Depth->MipLevel,
                                                                               Current.Depth->ArrayLayer);
                RenderExtent.Width         = std::min(RenderExtent.Width, Target.Extent.Width);
                RenderExtent.Height        = std::min(RenderExtent.Height, Target.Extent.Height);

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
                                                .renderArea           = {{0, 0}, ToVk(RenderExtent)},
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
            Cmd.SetScissor(Rect2D{{0, 0}, RenderExtent});
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

        if (m_Breadcrumbs != nullptr)
        {
            m_Breadcrumbs->MarkPass(Commands, m_BreadcrumbSlot, m_BreadcrumbStream, m_BreadcrumbFrame, PassIndex, Current.Name, true);
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

        const AccessInfo Info = Describe(*Res.FinalAccess, Impl.FinalAccessType);
        Impl.Transition(Res, Info.Stage, Info.Mask, Res.IsTexture ? Info.Layout : VK_IMAGE_LAYOUT_UNDEFINED, Info.Write);
        if (Res.Imported && std::ranges::find(Impl.UsedStates, Res.State) == std::end(Impl.UsedStates))
        {
            Impl.UsedStates.push_back(Res.State);
        }
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

std::uint64_t luvk::RenderGraph::FirstUseStage(const TextureHandle Texture) const
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
    return m_Impl->PassCount;
}

std::size_t luvk::RenderGraph::PooledTextureCount() const noexcept
{
    return std::size(m_Impl->TexturePool);
}

std::size_t luvk::RenderGraph::PooledBufferCount() const noexcept
{
    return std::size(m_Impl->BufferPool);
}
