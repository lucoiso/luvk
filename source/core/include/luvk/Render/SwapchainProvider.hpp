// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Core/Common.hpp"
#include "luvk/Core/Vulkan.hpp"

namespace luvk
{
    class Context;

    /** Replacement swapchain entry points of a SwapchainProvider; null fields fall back to the core Vulkan functions. */
    struct SwapchainFunctions
    {
        /** Replaces vkGetSwapchainImagesKHR. */
        PFN_vkGetSwapchainImagesKHR GetImages = nullptr;

        /** Replaces vkAcquireNextImageKHR. */
        PFN_vkAcquireNextImageKHR Acquire = nullptr;

        /** Replaces vkQueuePresentKHR (called on the graphics queue with its luvk lock held). */
        PFN_vkQueuePresentKHR Present = nullptr;
    };

    /**
     * Hook that lets third-party code own the swapchain of a PresentTarget, e.g. AMD FidelityFX frame generation, whose
     * Vulkan backend replaces the swapchain and its create/destroy/get-images/acquire/present functions.
     *
     * Lifetime rules:
     *  - Create receives the swapchain to replace in InOutSwapchain (nullptr when there is none, Info.oldSwapchain
     *    is the same handle) and returns the swapchain to use in it: either the same handle recreated in place (FidelityFX's
     *    create-swapchain replacement) or a new one, in which case the provider must have destroyed the old one
     *    (FidelityFX's context creation does). luvk never touches the old handle again. It may have been created by the
     *    core Vulkan functions (when a provider is installed on a running target).
     *  - Destroy is called exactly once for every swapchain Create returned that luvk still owns (on recreation through
     *    another provider, on SetProvider(nullptr) and when the target is destroyed), with the GPU idle.
     *  - The PresentTarget keeps the provider alive (shared ownership) until its last swapchain was destroyed.
     */
    class LUVK_API SwapchainProvider
    {
    public:
        /** Destructor. */
        virtual ~SwapchainProvider();

        /** Creates (or recreates in place, when InOutSwapchain is not null) the swapchain. */
        virtual VkResult Create(Context& Ctx, const VkSwapchainCreateInfoKHR& Info, VkSwapchainKHR& InOutSwapchain) = 0;

        /** Destroys a swapchain returned by Create. */
        virtual void Destroy(Context& Ctx, VkSwapchainKHR Swapchain) noexcept = 0;

        /** Replacement entry points (null fields use the core functions). */
        [[nodiscard]] virtual SwapchainFunctions Functions() const = 0;

        /**
         * True when the replacement present honours VkSwapchainPresentFenceInfoKHR (swapchain_maintenance1). Defaults to
         * false: luvk then relies on one render-finished semaphore per swapchain image and idle waits on recreation.
         */
        [[nodiscard]] virtual bool SupportsPresentFences() const noexcept;
    };
}
