// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <glm/vec2.hpp>
#include <luvk/Core/Common.hpp>
#include <luvk/Render/FrameAllocator.hpp>
#include <luvk/Render/RenderGraph.hpp>
#include "luvk/Frame/Blackboard.hpp"

namespace luvk
{
    class Context;
}

namespace luvk::frame
{
    /** Names of the default phases, in frame order. Features of one phase are ordered by their After/Before names. */
    namespace Phases
    {
        inline constexpr std::string_view Prepare     = "Prepare";
        inline constexpr std::string_view Compute     = "Compute";
        inline constexpr std::string_view Opaque      = "Opaque";
        inline constexpr std::string_view Transparent = "Transparent";
        inline constexpr std::string_view Debug       = "Debug";
        inline constexpr std::string_view PostProcess = "PostProcess";
        inline constexpr std::string_view Upscale     = "Upscale";
        inline constexpr std::string_view Overlay     = "Overlay";

        /** Writes the final image into the output (the composite, a tonemapper writing the backbuffer, ...). */
        inline constexpr std::string_view Output = "Output";
    }

    /** One phase of the frame: features declare the phase they belong to, phases are ordered by After/Before. */
    struct PhaseDesc
    {
        /** Unique name. */
        std::string Name;

        /** Phases that must run before this one. */
        std::vector<std::string> After;

        /** Phases that must run after this one. */
        std::vector<std::string> Before;
    };

    /** Resolution and format settings describing a single frame. */
    struct FrameSettings
    {
        /** Resolution the scene is rendered at (scene color / depth). */
        Extent2D RenderExtent = {};

        /** Resolution of the output (backbuffer); differs from RenderExtent when the scene is scaled or upscaled. */
        Extent2D OutputExtent = {};

        /** Pixel rectangle of the output where the scene is composited. An empty extent means the full OutputExtent. The
         *  camera aspect and RenderExtent follow this rectangle. */
        Rect2D Viewport = {};

        /** Format of the scene color (HDR by default). */
        Format ColorFormat = Format::R16G16B16A16Sfloat;

        /** Subpixel camera jitter in render pixels (zero when no temporal upscaler is active). */
        glm::vec2 ProjectionJitter{0.F, 0.F};

        /** Format of the scene depth. */
        Format DepthFormat = Format::D32Sfloat;

        /** Format of the output image. */
        Format OutputFormat = Format::Undefined;

        /** Wireframe rasterization requested for the frame (only ever true when the device supports fillModeNonSolid). */
        bool Wireframe = false;

        /** Reversed depth: near = 1, far = 0 (much better precision for large view distances). SceneDepth is cleared to
         *  ClearDepth(), the camera projection follows it, and depth-tested features must compare with DepthCompare(). */
        bool ReverseDepth = false;

        /** Fill mode matching Wireframe, ready for GraphicsPipelineDesc::PolygonMode. */
        [[nodiscard]] constexpr luvk::PolygonMode PolygonMode() const noexcept { return Wireframe ? luvk::PolygonMode::Line : luvk::PolygonMode::Fill; }

        /** Depth value of the far plane, used to clear SceneDepth. */
        [[nodiscard]] constexpr float ClearDepth() const noexcept { return ReverseDepth ? 0.F : 1.F; }

        /** Depth comparison matching ReverseDepth, ready for GraphicsPipelineDesc::DepthCompare. */
        [[nodiscard]] constexpr CompareOp DepthCompare() const noexcept { return ReverseDepth ? CompareOp::GreaterOrEqual : CompareOp::LessOrEqual; }
    };

    /** Everything a feature needs to add its passes for the current frame. */
    struct FeatureContext
    {
        /** Rendering context. */
        Context& Ctx;

        /** Frame graph the feature adds its passes to. */
        RenderGraph& Graph;

        /** Async compute graph of the frame (compute and transfer passes only): submitted before Graph, which waits for it.
         *  It is Graph itself when the frame has no async graph. */
        RenderGraph& AsyncGraph = Graph;

        /** Per-frame memory for data written by the CPU every frame (vertices, instances, constants). */
        FrameAllocator& Allocator;

        /** Shared per-frame resource registry. */
        Blackboard& Board;

        /** Resolution and formats of the current frame. */
        const FrameSettings& Settings;

        /** Frame delta time in seconds. */
        float DeltaTime = 0.F;

        /** Total elapsed time in seconds. */
        double Time = 0.0;

        /** Number of the current frame. */
        std::uint64_t FrameNumber = 0U;
    };

    /**
     * A self-contained piece of rendering (sky, particles, debug draw, upscaler...). Features create their GPU resources in
     * OnAttach, add graph passes every frame and may expose a debug UI through OnGui. They never reference each other: they
     * meet through blackboard slots, and order themselves through a phase plus optional After/Before feature names.
     */
    class RenderFeature
    {
        /** Enabled features participate in Update and AddPasses. */
        bool m_Enabled = true;

        /** Set when a callback of the feature threw: the feature is then skipped until re-enabled. */
        std::string m_Failure;

        friend class FeatureStack;

    public:
        /** Destructor. */
        virtual ~RenderFeature();

        /** Unique name of the feature (ordering, debugging and lookups). */
        [[nodiscard]] virtual std::string_view Name() const = 0;

        /** Phase the feature's passes belong to (see Phases). */
        [[nodiscard]] virtual std::string_view Phase() const = 0;

        /** Features of the same phase that must add their passes before this one. */
        [[nodiscard]] virtual std::span<const std::string_view> After() const;

        /** Features of the same phase that must add their passes after this one. */
        [[nodiscard]] virtual std::span<const std::string_view> Before() const;

        /** Called when the feature is added to a FeatureStack; create GPU resources here. */
        virtual void OnAttach(Context& Ctx);

        /** Called when the feature is removed or the stack is destroyed; release resources here. */
        virtual void OnDetach(Context& Ctx);

        /** Called once the frame settings are known (also right after OnAttach) and whenever they change. */
        virtual void OnResize(Context& Ctx, const FrameSettings& Settings);

        /** Per-frame CPU-side update, called before passes are added (skipped when disabled). */
        virtual void Update(float DeltaTime);

        /** Declares this feature's passes in the frame graph (skipped when disabled). */
        virtual void AddPasses(FeatureContext& Frame) = 0;

        /** Debug UI hook, called inside the application's UI frame (implementations typically use ImGui). */
        virtual void OnGui();

        /** True when the feature participates in Update and AddPasses. */
        [[nodiscard]] bool IsEnabled() const noexcept { return m_Enabled && m_Failure.empty(); }

        /** Enables or disables the feature; enabling clears a recorded failure. */
        void SetEnabled(bool Enabled) noexcept;

        /** Message of the exception that disabled the feature (empty when it runs). */
        [[nodiscard]] std::string_view Failure() const noexcept { return m_Failure; }
    };

    /**
     * Ordered collection of features. Adding attaches (creates GPU resources), removing detaches. The order is computed once
     * whenever the set changes: phases by their After/Before names, then the features of each phase by theirs (insertion
     * order otherwise); a cycle is an error naming the members involved. An exception thrown by one feature's Update or
     * AddPasses disables that feature and is reported once; the frame goes on without it.
     */
    class FeatureStack
    {
        /** Context forwarded to the features' callbacks. */
        Context& m_Context;

        /** Features in insertion order. */
        std::vector<std::shared_ptr<RenderFeature>> m_Added;

        /** Features in execution order (rebuilt when the set or the phases change). */
        std::vector<std::shared_ptr<RenderFeature>> m_Features;

        /** Phase index of every entry of m_Features. */
        std::vector<std::uint32_t> m_FeaturePhase;

        /** Declared phases. */
        std::vector<PhaseDesc> m_Phases;

        /** Phase names in execution order. */
        std::vector<std::string> m_PhaseOrder;

        /** Last settings passed to Resize, forwarded to features added later. */
        std::optional<FrameSettings> m_Settings;

        /** Recomputes m_PhaseOrder and m_Features (throws std::logic_error on cycles or unknown phases). */
        void Sort();

        /** Index of Name in m_PhaseOrder (throws for unknown phases). */
        [[nodiscard]] std::uint32_t PhaseIndex(std::string_view Name) const;

        /** Runs Callback for Feature and disables the feature when it throws. */
        template <typename Callback>
        void Guard(RenderFeature& Feature, std::string_view Where, Callback&& Function) const;

    public:
        /** Constructor: the default phases (Phases::Prepare ... Phases::Output, in that order). */
        explicit FeatureStack(Context& Ctx);

        /** Destructor; detaches every remaining feature. */
        ~FeatureStack();

        /** Non-copyable. */
        FeatureStack(const FeatureStack&) = delete;

        /** Non-copyable. */
        FeatureStack& operator=(const FeatureStack&) = delete;

        /** Declares a phase (e.g. {"Shadows", {"Prepare"}, {"Opaque"}}); throws when the order becomes cyclic. */
        void AddPhase(PhaseDesc Phase);

        /** Phase names in execution order. */
        [[nodiscard]] std::span<const std::string> PhaseOrder() const noexcept { return m_PhaseOrder; }

        /** Attaches the feature and inserts it in phase order. If attaching or resizing throws, the feature is detached and
         *  the stack is unchanged. */
        void Add(std::shared_ptr<RenderFeature> Feature);

        /** Creates a feature of type T, adds it and returns the owning shared_ptr. */
        template <typename T, typename... Arguments>
            requires std::is_base_of_v<RenderFeature, T>
        std::shared_ptr<T> Add(Arguments&&... Args)
        {
            auto Feature = std::make_shared<T>(std::forward<Arguments>(Args)...);
            Add(Feature);
            return Feature;
        }

        /** Detaches and removes the given feature (no-op when not found). */
        void Remove(const RenderFeature& Feature);

        /** Detaches every feature in reverse order and empties the stack. */
        void Clear();

        /** Returns the first feature of type T, or nullptr when absent. */
        template <typename T>
        [[nodiscard]] std::shared_ptr<T> Find() const
        {
            for (const auto& Feature : m_Features)
            {
                if (auto Typed = std::dynamic_pointer_cast<T>(Feature))
                {
                    return Typed;
                }
            }
            return nullptr;
        }

        /** Calls Update on every enabled feature, in stack order. */
        void Update(float DeltaTime) const;

        /** Stores Settings and calls OnResize on every feature, in stack order. */
        void Resize(const FrameSettings& Settings);

        /** Calls AddPasses on every enabled feature, in stack order. */
        void AddPasses(FeatureContext& Frame) const;

        /** Calls AddPasses on every enabled feature whose phase lies between First and Last (inclusive), in stack order. */
        void AddPasses(FeatureContext& Frame, std::string_view First, std::string_view Last) const;

        /** Read-only view of the features in execution order. */
        [[nodiscard]] std::span<const std::shared_ptr<RenderFeature>> All() const noexcept { return m_Features; }
    };
}
