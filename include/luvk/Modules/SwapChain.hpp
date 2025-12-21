// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <volk.h>
#include "luvk/Module.hpp"
#include "luvk/Interfaces/IEventModule.hpp"
#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"
#include "luvk/Types/Vector.hpp"

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
        Vector<std::uint32_t>         QueueIndices{};
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

        VkSwapchainKHR                 m_SwapChain{VK_NULL_HANDLE};
        VkSwapchainKHR                 m_PreviousSwapChain{VK_NULL_HANDLE};
        Vector<VkImage>                m_Images{};
        Vector<VkImageView>            m_ImageViews{};
        Vector<VkFramebuffer>          m_Framebuffers{};
        Vector<std::shared_ptr<Image>> m_DepthImages{};
        VkFormat                       m_DepthFormat{VK_FORMAT_UNDEFINED};
        VkRenderPass                   m_RenderPass{VK_NULL_HANDLE};
        VkExtent2D                     m_Extent{0U, 0U};
        std::shared_ptr<Device>        m_DeviceModule{};
        std::shared_ptr<Memory>        m_MemoryModule{};
        CreationArguments              m_Arguments{};

    public:
        SwapChain() = delete;
        explicit SwapChain(const std::shared_ptr<Device>& DeviceModule,
                           const std::shared_ptr<Memory>& MemoryModule);

        ~SwapChain() override
        {
            SwapChain::ClearResources();
        }

        [[nodiscard]] constexpr ExtensionsMap GetDeviceExtensions() const override
        {
            return ToExtensionMap("", {VK_KHR_SWAPCHAIN_EXTENSION_NAME});
        }

        [[nodiscard]] constexpr const VkSwapchainKHR& GetHandle() const
        {
            return m_SwapChain;
        }

        [[nodiscard]] constexpr const Vector<VkImage>& GetImages() const
        {
            return m_Images;
        }

        [[nodiscard]] constexpr const Vector<VkImageView>& GetImageViews() const
        {
            return m_ImageViews;
        }

        [[nodiscard]] constexpr const VkFramebuffer& GetFramebuffer(const std::size_t Index) const
        {
            return m_Framebuffers.at(Index);
        }

        [[nodiscard]] constexpr const std::shared_ptr<Image>& GetDepthImage(const std::size_t Index) const
        {
            return m_DepthImages.at(Index);
        }

        [[nodiscard]] constexpr const Vector<std::shared_ptr<Image>>& GetDepthImages() const
        {
            return m_DepthImages;
        }

        [[nodiscard]] constexpr VkFormat GetDepthFormat() const
        {
            return m_DepthFormat;
        }

        [[nodiscard]] constexpr const VkRenderPass& GetRenderPass() const
        {
            return m_RenderPass;
        }

        [[nodiscard]] constexpr const VkExtent2D& GetExtent() const
        {
            return m_Extent;
        }

        [[nodiscard]] constexpr const CreationArguments& GetCreationArguments() const
        {
            return m_Arguments;
        }

        virtual void CreateSwapChain(CreationArguments&& Arguments, void* const& pNext);
        void Recreate(VkExtent2D NewExtent, void* const& pNext);

    protected:
        void ClearResources() override;

    private:
        void                   CreateSwapChainImages(const VkDevice& LogicalDevice);
        void                   DestroySwapChainImages(const VkDevice& LogicalDevice);
        void                   CreateRenderPass(const VkDevice& LogicalDevice);
        void                   DestroyRenderPass(const VkDevice& LogicalDevice);
        void                   CreateFramebuffers(const VkDevice& LogicalDevice);
        void                   DestroyFramebuffers(const VkDevice& LogicalDevice);
        void                   CreateDepthResources();
        void                   DestroyDepthResources();
        [[nodiscard]] VkFormat SelectDepthFormat(const VkPhysicalDevice& PhysicalDevice) const;
    };
} // namespace luvk
