// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <span>
#include <volk.h>
#include "luvk/Module.hpp"
#include "luvk/Constants/Rendering.hpp"
#include "luvk/Interfaces/IEventModule.hpp"
#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class Device;
    class Memory;
    class Image;

    struct SwapChainCreationArguments
    {
        bool                          Clip{true};
        VkSwapchainCreateFlagsKHR     Flags{};
        VkPresentModeKHR              PresentMode{VK_PRESENT_MODE_FIFO_KHR};
        VkSurfaceTransformFlagBitsKHR TransformFlags{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR};
        VkFormat                      Format{VK_FORMAT_R8G8B8A8_UNORM};
        VkColorSpaceKHR               ColorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        VkImageUsageFlags             UsageFlags{VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
        VkCompositeAlphaFlagBitsKHR   CompositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
        VkExtent2D                    Extent{.width = 0U, .height = 0U};
        VkSurfaceKHR                  Surface{VK_NULL_HANDLE};
        std::vector<std::uint32_t>    QueueIndices{};
    };

    enum class SwapChainEvents : std::uint8_t
    {
        OnCreated
    };

    class LUVKMODULE_API SwapChain : public IRenderModule,
                                     public IEventModule,
                                     public IExtensionsModule
    {
    protected:
        using CreationArguments = SwapChainCreationArguments;

        VkSwapchainKHR m_SwapChain{VK_NULL_HANDLE};
        VkSwapchainKHR m_PreviousSwapChain{VK_NULL_HANDLE};

        VkFormat     m_DepthFormat{VK_FORMAT_UNDEFINED};
        VkRenderPass m_RenderPass{VK_NULL_HANDLE};

        std::array<VkImage, Constants::ImageCount>                m_Images{};
        std::array<VkImageView, Constants::ImageCount>            m_ImageViews{};
        std::array<VkFramebuffer, Constants::ImageCount>          m_Framebuffers{};
        std::array<std::shared_ptr<Image>, Constants::ImageCount> m_DepthImages{};

        std::shared_ptr<Device> m_DeviceModule{};
        std::shared_ptr<Memory> m_MemoryModule{};

        CreationArguments m_Arguments{};

    public:
        SwapChain() = delete;
        explicit SwapChain(const std::shared_ptr<Device>& DeviceModule,
                           const std::shared_ptr<Memory>& MemoryModule);

        ~SwapChain() override
        {
            SwapChain::ClearResources();
        }

        [[nodiscard]] ExtensionMap GetDeviceExtensions() const noexcept override
        {
            return {{"", {VK_KHR_SWAPCHAIN_EXTENSION_NAME}}};
        }

        [[nodiscard]] constexpr VkSwapchainKHR GetHandle() const noexcept
        {
            return m_SwapChain;
        }

        [[nodiscard]] constexpr std::span<const VkImage> GetImages() const noexcept
        {
            return m_Images;
        }

        [[nodiscard]] constexpr std::span<const VkImageView> GetImageViews() const noexcept
        {
            return m_ImageViews;
        }

        [[nodiscard]] constexpr VkFramebuffer GetFramebuffer(const std::size_t Index) const noexcept
        {
            return m_Framebuffers.at(Index);
        }

        [[nodiscard]] std::shared_ptr<Image> GetDepthImage(const std::size_t Index) const noexcept
        {
            return m_DepthImages.at(Index);
        }

        [[nodiscard]] constexpr std::span<const std::shared_ptr<Image>> GetDepthImages() const noexcept
        {
            return m_DepthImages;
        }

        [[nodiscard]] constexpr VkFormat GetDepthFormat() const noexcept
        {
            return m_DepthFormat;
        }

        [[nodiscard]] constexpr VkRenderPass GetRenderPass() const noexcept
        {
            return m_RenderPass;
        }

        [[nodiscard]] constexpr VkExtent2D GetExtent() const noexcept
        {
            return m_Arguments.Extent;
        }

        [[nodiscard]] constexpr const CreationArguments& GetCreationArguments() const noexcept
        {
            return m_Arguments;
        }

        virtual void CreateSwapChain(CreationArguments&& Arguments, void* const& pNext);
        void         Recreate(const VkExtent2D& NewExtent, void* const& pNext);

    protected:
        void ClearResources() override;

    private:
        void                   CreateSwapChainImages(VkDevice LogicalDevice);
        void                   DestroySwapChainImages(VkDevice LogicalDevice);
        void                   CreateRenderPass(VkDevice LogicalDevice);
        void                   DestroyRenderPass(VkDevice LogicalDevice);
        void                   CreateFramebuffers(VkDevice LogicalDevice);
        void                   DestroyFramebuffers(VkDevice LogicalDevice);
        void                   CreateDepthResources();
        void                   DestroyDepthResources();
        [[nodiscard]] VkFormat SelectDepthFormat(VkPhysicalDevice PhysicalDevice) const;
    };
} // namespace luvk
