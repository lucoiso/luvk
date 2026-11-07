// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <any>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <luvk/Core/Common.hpp>
#include <luvk/Render/FrameAllocator.hpp>
#include <luvk/Render/RenderGraph.hpp>

namespace luvk
{
    class Context;
}

namespace luvk::app
{
    /**
     * Order in which features add their passes to the frame graph. Features of the same stage run in
     * registration order. The graph inserts every barrier, so a stage is only a logical ordering.
     */
    enum class FeatureStage : std::uint8_t
    {
        Prepare,
        Compute,
        Opaque,
        Transparent,
        Debug,
        PostProcess,
        Upscale,
        Overlay,

        /** Writes the final image into the output (e.g. the app layer's tonemap/composite into the backbuffer). */
        Output,
    };

    /**
     * Per-frame registry of named graph resources and arbitrary data, so features can communicate without
     * knowing each other (e.g. an upscaler reads the scene color and publishes a new one under the same name).
     * The names themselves are a convention of whoever builds the frame (see luvk::app::Slots).
     */
    class Blackboard
    {
        /** Stable storage for the registered names, so lookups can use string_view without allocating. */
        std::unordered_set<std::string> m_Names;

        /** Registered texture handles by name. */
        std::unordered_map<std::string_view, TextureHandle> m_Textures;

        /** Registered buffer handles by name. */
        std::unordered_map<std::string_view, BufferHandle> m_Buffers;

        /** Type-erased values by name. */
        std::unordered_map<std::string_view, std::any> m_Data;

        /** Interns Name and returns a view valid until Clear(). */
        std::string_view Intern(std::string_view Name);

    public:
        /** Removes every registered texture, buffer and value. */
        void Clear();

        /** Registers (or replaces) a texture handle under Name. */
        void SetTexture(std::string_view Name, TextureHandle Texture);

        /** Registers (or replaces) a buffer handle under Name. */
        void SetBuffer(std::string_view Name, BufferHandle Buffer);

        /** Returns an invalid handle when the name is not present. */
        [[nodiscard]] TextureHandle FindTexture(std::string_view Name) const;

        /** Returns the buffer registered under Name, or an invalid handle. */
        [[nodiscard]] BufferHandle  FindBuffer(std::string_view Name) const;

        template <typename T>
        /** Stores an arbitrary value under Name, replacing any previous one. */
        void Set(const std::string_view Name, T Value)
        {
            m_Data.insert_or_assign(Intern(Name), std::any(std::move(Value)));
        }

        /** Returns nullptr when the name is missing or holds another type. */
        template <typename T>
        [[nodiscard]] const T* Find(const std::string_view Name) const
        {
            const auto It = m_Data.find(Name);
            return It != std::end(m_Data) ? std::any_cast<T>(&It->second) : nullptr;
        }
    };

    /** Resolution and format settings describing a single frame. */
    struct FrameSettings
    {
        /** Resolution the scene is rendered at (scene color / depth). */
        VkExtent2D RenderExtent = {};

        /** Resolution of the output (backbuffer); differs from RenderExtent when the scene is scaled or upscaled. */
        VkExtent2D OutputExtent = {};

        /** Format of the scene color (HDR by default). */
        VkFormat ColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

        /** Format of the scene depth. */
        VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;

        /** Format of the output image. */
        VkFormat OutputFormat = VK_FORMAT_UNDEFINED;

        /** Wireframe rasterization requested for the frame (only ever true when the device supports fillModeNonSolid). */
        bool Wireframe = false;

        /** Reversed depth: near = 1, far = 0 (much better precision for large view distances). SceneDepth is cleared to
         *  ClearDepth(), the camera projection follows it, and depth-tested features must compare with DepthCompare(). */
        bool ReverseDepth = false;

        /** Fill mode matching Wireframe, ready for GraphicsPipelineDesc::PolygonMode. */
        [[nodiscard]] constexpr VkPolygonMode PolygonMode() const noexcept { return Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL; }

        /** Depth value of the far plane, used to clear SceneDepth. */
        [[nodiscard]] constexpr float ClearDepth() const noexcept { return ReverseDepth ? 0.F : 1.F; }

        /** Depth comparison matching ReverseDepth, ready for GraphicsPipelineDesc::DepthCompare. */
        [[nodiscard]] constexpr VkCompareOp DepthCompare() const noexcept
        {
            return ReverseDepth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
        }
    };

    /** Everything a feature needs to add its passes for the current frame. */
    struct FeatureContext
    {
        /** Rendering context. */
        Context&             Ctx;

        /** Frame graph the feature adds its passes to. */
        RenderGraph&         Graph;

        /** Per-frame memory for data written by the CPU every frame (vertices, instances, constants). */
        FrameAllocator&      Allocator;

        /** Shared per-frame resource registry. */
        Blackboard&          Board;

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
     * A self-contained piece of rendering (skybox, particles, debug draw, upscaler...). Features create their
     * GPU resources in OnAttach, add graph passes every frame and may expose a debug UI through OnGui.
     */
    class RenderFeature
    {
        /** Enabled features participate in Update and AddPasses. */
        bool m_Enabled = true;

    public:
        /** Destructor. */
        virtual ~RenderFeature();

        /** Unique name of the feature (for debugging and lookups). */
        [[nodiscard]] virtual std::string_view Name() const  = 0;

        /** Stage that orders the feature's passes in the frame graph. */
        [[nodiscard]] virtual FeatureStage     Stage() const = 0;

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
        [[nodiscard]] bool IsEnabled() const noexcept { return m_Enabled; }

        /** Enables or disables the feature. */
        void               SetEnabled(bool Enabled) noexcept;
    };

    /** Ordered collection of features. Adding attaches (creates GPU resources), removing detaches. */
    class FeatureStack
    {
        /** Context forwarded to the features' callbacks. */
        Context&                                    m_Context;

        /** Features sorted by stage, then by insertion order. */
        std::vector<std::shared_ptr<RenderFeature>> m_Features;

        /** Last settings passed to Resize, forwarded to features added later. */
        std::optional<FrameSettings>                m_Settings;

    public:
        /** Constructor. */
        explicit FeatureStack(Context& Ctx);

        /** Destructor; detaches every remaining feature. */
        ~FeatureStack();

        /** Non-copyable. */
        FeatureStack(const FeatureStack&)            = delete;

        /** Non-copyable. */
        FeatureStack& operator=(const FeatureStack&) = delete;

        /** Attaches the feature and inserts it sorted by FeatureStage. */
        void Add(std::shared_ptr<RenderFeature> Feature);

        template <typename T, typename... Arguments>
            requires std::is_base_of_v<RenderFeature, T>

        /** Creates a feature of type T, adds it and returns the owning shared_ptr. */
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

        template <typename T>
        /** Returns the first feature of type T, or nullptr when absent. */
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

        /** Calls AddPasses on every enabled feature whose stage lies in [First, Last], in stack order. */
        void AddPasses(FeatureContext& Frame, FeatureStage First, FeatureStage Last) const;

        /** Read-only view of the features in execution order. */
        [[nodiscard]] std::span<const std::shared_ptr<RenderFeature>> All() const noexcept { return m_Features; }
    };
}
