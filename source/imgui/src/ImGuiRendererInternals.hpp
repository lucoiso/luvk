// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <imgui.h>
#include <luvk/Core/Common.hpp>

namespace luvk
{
    class Image;
}

namespace luvk::imgui
{
    class ImGuiRenderer;

    /** Internal helpers shared by the ImGui renderer translation units. */
    namespace detail
    {
        /** Renderer backend of the current ImGui context (stored in ImGuiIO::BackendRendererUserData). */
        [[nodiscard]] ImGuiRenderer* CurrentBackend() noexcept;

        /** Encodes a bindless sampled-image index as an ImTextureID (0 is invalid). */
        [[nodiscard]] ImTextureID ToTextureId(std::uint32_t SampledIndex) noexcept;

        /** Decodes an ImTextureID back to a bindless sampled-image index. */
        [[nodiscard]] std::uint32_t ToSampledIndex(ImTextureID Id) noexcept;

        /** Uploads the full pixel buffer of one backend texture. */
        void UploadTexture(Image& Target, ImTextureData& Data);

        /** Uploads only the rectangles of one backend texture listed in ImTextureData::Updates. */
        void UploadTextureUpdates(Image& Target, ImTextureData& Data);

        /** Renderer callbacks registered in ImGuiPlatformIO (secondary viewports are rendered by ImGuiLayer). */
        void ImGuiCreateViewportWindow(ImGuiViewport* Viewport);
        void ImGuiDestroyViewportWindow(ImGuiViewport* Viewport);
    }
}
