/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <functional>
#include <vector>
#include <volk.h>
#include "luvk/Interfaces/IModule.hpp"

namespace luvk
{
    /**
     * Callback signature for rendering.
     * @param CommandBuffer The buffer inside a vkCmdBeginRendering block.
     * @param FrameIndex Index of the current frame in flight.
     */
    using DrawCallback = std::function<void(VkCommandBuffer, std::uint32_t)>;

    /**
     * Module responsible for managing the main rendering loop and execution of draw callbacks.
     */
    class LUVK_API Draw : public IModule
    {
        /** List of callbacks executed before the main draw commands (e.g., transitions). */
        std::vector<DrawCallback> m_BeginFrameCallbacks{};

        /** List of callbacks executed inside the vkCmdBeginRendering block. */
        std::vector<DrawCallback> m_DrawCallbacks{};

        /** Pointer to the central service locator. */
        IServiceLocator* m_ServiceLocator{nullptr};

    public:
        /** Default destructor. */
        ~Draw() override = default;

        /** Called upon module initialization. */
        void OnInitialize(IServiceLocator* ServiceLocator) override;

        /**
         * Register a function to be called at the beginning of frame recording.
         * @param Callback The function to be added.
         */
        void AddBeginFrameCallback(DrawCallback&& Callback);

        /**
         * Clears all registered begin frame callbacks.
         */
        void ClearBeginFrameCallbacks();

        /**
         * Register a function to be called during frame recording.
         * @param Callback The function to be added.
         */
        void AddDrawCallback(DrawCallback&& Callback);

        /**
         * Clears all registered draw callbacks.
         */
        void ClearDrawCallbacks();

        /**
         * Execute the render loop for the current frame.
         * Handles Dynamic Rendering setup (Begin/End Rendering).
         */
        void RenderFrame() const;
    };
}
