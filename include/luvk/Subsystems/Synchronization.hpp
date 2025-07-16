// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Subsystems/Commands/CommandBufferPool.hpp"
#include <volk/volk.h>
#include <memory>
#include <vector>
#include <string_view>
#include <iterator>

namespace luvk
{
    /** Frame synchronization primitives */
    class LUVKMODULE_API Synchronization : public IRenderModule
    {
    public:
        struct FrameData
        {
            VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
            std::vector<VkCommandBuffer> SecondaryBuffers{};
            VkSemaphore ImageAvailable{VK_NULL_HANDLE};
            VkFence InFlight{VK_NULL_HANDLE};
            bool Submitted{false};
        };

    private: /** Per-frame data used by the renderer */
        std::vector<FrameData> m_Frames{};

        /** Semaphores signaled when rendering is finished */
        std::vector<VkSemaphore> m_RenderFinished{};

        /** Pool providing secondary command buffers */
        CommandBufferPool m_SecondaryPool{};

        /** Index of the frame currently being processed */
        std::size_t m_CurrentFrame{0};

        /** Number of rendering threads available */
        std::size_t m_ThreadCount{1};

        /** Device module used for resource creation */
        std::shared_ptr<IRenderModule> m_DeviceModule{};

    public: /** Default constructor */
        constexpr Synchronization() = default;

        //~ Begin of IRenderModule interface
        /** Virtual destructor */
        ~Synchronization() override = default;

        /** Initialize internal synchronization objects */
        void Initialize(std::shared_ptr<IRenderModule> const& DeviceModule, std::size_t FrameCount);

        /** Setup per-frame resources */
        void SetupFrames(std::shared_ptr<IRenderModule> const& DeviceModule,
                         std::shared_ptr<IRenderModule> const& SwapChainModule,
                         std::shared_ptr<IRenderModule> const& CommandPoolModule);

        [[nodiscard]] std::size_t GetFrameCount() const
        {
            return std::size(m_Frames);
        }

        /** Retrieve frame data at the specified index */
        [[nodiscard]] FrameData& GetFrame(std::size_t Index);

        [[nodiscard]] VkSemaphore& GetRenderFinished(const std::size_t Index)
        {
            return m_RenderFinished.at(Index);
        }

        [[nodiscard]] std::size_t GetCurrentFrame() const
        {
            return m_CurrentFrame;
        }

        [[nodiscard]] std::size_t GetThreadCount() const
        {
            return m_ThreadCount;
        }

        [[nodiscard]] CommandBufferPool& GetSecondaryPool()
        {
            return m_SecondaryPool;
        }

        /** Advance to the next frame index */
        void AdvanceFrame();

        /** Retrieve required device extensions for this module */
        [[nodiscard]] std::unordered_map<std::string_view, std::vector<std::string_view>> GetRequiredDeviceExtensions() const override
        {
            return {};
        }

        /** Get the feature chain for device creation */
        [[nodiscard]] void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const& DeviceModule) const noexcept override
        {
            return nullptr;
        }

        /** Get the feature chain for instance creation */
        [[nodiscard]] void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept override
        {
            return nullptr;
        }

    private: /** Synchronization module has no dependencies to set up */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const&) override {}

        /** Release allocated Vulkan objects */
        void ClearResources() override;
        //~ End of IRenderModule interface
    };
} // namespace luvk
