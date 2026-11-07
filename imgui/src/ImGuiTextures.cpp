// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "ImGuiRenderer.hpp"
#include "ImGuiRendererInternals.hpp"
#include <cstdio>
#include <stdexcept>
#include <luvk/Core/Context.hpp>
#include <luvk/Resources/Image.hpp>

namespace luvk::imgui::detail
{
    ImGuiRenderer* CurrentBackend() noexcept
    {
        return ImGui::GetCurrentContext() != nullptr ? static_cast<ImGuiRenderer*>(ImGui::GetIO().BackendRendererUserData) : nullptr;
    }

    ImTextureID ToTextureId(const std::uint32_t SampledIndex) noexcept
    {
        return static_cast<ImTextureID>(static_cast<ImU64>(SampledIndex) + 1U);
    }

    std::uint32_t ToSampledIndex(const ImTextureID Id) noexcept
    {
        if (Id == ImTextureID_Invalid)
        {
            return InvalidBindlessIndex;
        }
        return static_cast<std::uint32_t>(static_cast<ImU64>(Id) - 1U);
    }

    void UploadTexture(Image& Target, ImTextureData& Data)
    {
        const void* const Pixels = Data.GetPixels();
        const int         Size   = Data.GetSizeInBytes();
        if (Pixels == nullptr || Size <= 0)
        {
            return;
        }

        Target.Upload(std::as_bytes(std::span<const std::byte>(static_cast<const std::byte*>(Pixels), static_cast<std::size_t>(Size))));
    }

    void UploadTextureUpdates(Image& Target, ImTextureData& Data)
    {
        // Only the rectangles Dear ImGui changed (e.g. glyphs rasterized on demand) go to the GPU, read straight from
        // the full-width CPU copy of the texture.
        for (const ImTextureRect& Rect : Data.Updates)
        {
            if (Rect.w == 0U || Rect.h == 0U)
            {
                continue;
            }

            const std::size_t Bytes = (static_cast<std::size_t>(Rect.h - 1U) * static_cast<std::size_t>(Data.Width) + Rect.w) *
                                      static_cast<std::size_t>(Data.BytesPerPixel);
            const auto* const First = static_cast<const std::byte*>(Data.GetPixelsAt(Rect.x, Rect.y));

            Target.Upload(std::span<const std::byte>(First, Bytes),
                          ImageRegion{.Offset    = {static_cast<std::int32_t>(Rect.x), static_cast<std::int32_t>(Rect.y), 0},
                                      .Extent    = {Rect.w, Rect.h, 1U},
                                      .RowLength = static_cast<std::uint32_t>(Data.Width)});
        }
    }
}
void luvk::imgui::ImGuiRenderer::RefreshLiveImages()
{
    m_LiveImages.clear();
    m_ValidIndices.clear();
    m_ValidIndices.insert(m_WhiteIndex);

    for (const auto& [Data, Record] : m_BackendTextures)
    {
        if (Record.Texture != nullptr && *Record.Texture)
        {
            m_LiveImages.push_back(Record.Texture.get());
            m_ValidIndices.insert(Record.Texture->SampledIndex());
        }
    }

    for (Image* const Texture : m_UserTextures)
    {
        if (Texture != nullptr && *Texture)
        {
            m_LiveImages.push_back(Texture);
            m_ValidIndices.insert(Texture->SampledIndex());
        }
    }
}

void luvk::imgui::ImGuiRenderer::UpdateTextures()
{
    ImVector<ImTextureData*>* Textures = &ImGui::GetPlatformIO().Textures;
    if (const ImDrawData* const DrawData = ImGui::GetDrawData(); DrawData != nullptr && DrawData->Textures != nullptr)
    {
        Textures = DrawData->Textures;
    }

    bool Changed = false;
    for (ImTextureData* const Data : *Textures)
    {
        if (Data == nullptr || Data->Status == ImTextureStatus_OK || Data->Status == ImTextureStatus_Destroyed)
        {
            continue;
        }

        switch (Data->Status)
        {
        case ImTextureStatus_WantCreate:
        {
            if (Data->Format != ImTextureFormat_RGBA32)
            {
                std::fprintf(stderr, "[luvk-imgui] Unsupported ImGui texture format %d\n", static_cast<int>(Data->Format));
                Data->SetStatus(ImTextureStatus_Destroyed);
                break;
            }

            auto Texture = std::make_unique<Image>(m_Context,
                                                   ImageDesc{.Extent = {static_cast<std::uint32_t>(Data->Width), static_cast<std::uint32_t>(Data->Height), 1U},
                                                             .Format = VK_FORMAT_R8G8B8A8_UNORM,
                                                             .Usage  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                             .Name   = "luvk ImGui texture"});
            detail::UploadTexture(*Texture, *Data);

            const std::uint32_t Sampled = Texture->SampledIndex();
            m_BackendTextures.insert_or_assign(Data, TextureRecord{.Texture = std::move(Texture)});

            Data->SetTexID(detail::ToTextureId(Sampled));
            Data->SetStatus(ImTextureStatus_OK);
            Changed = true;
            break;
        }
        case ImTextureStatus_WantUpdates:
        {
            const auto It = m_BackendTextures.find(Data);
            if (It == std::end(m_BackendTextures) || It->second.Texture == nullptr)
            {
                Data->SetStatus(ImTextureStatus_Destroyed);
                break;
            }

            detail::UploadTextureUpdates(*It->second.Texture, *Data);
            Data->SetStatus(ImTextureStatus_OK);
            break;
        }
        case ImTextureStatus_WantDestroy:
        {
            m_BackendTextures.erase(Data);
            Data->SetTexID(ImTextureID_Invalid);
            Data->SetStatus(ImTextureStatus_Destroyed);
            Changed = true;
            break;
        }
        default:
            break;
        }
    }

    if (Changed)
    {
        RefreshLiveImages();
    }
}

ImTextureID luvk::imgui::ImGuiRenderer::AddTexture(Image& Texture)
{
    if (!Texture)
    {
        throw std::invalid_argument("[luvk-imgui] UiTexture requires a valid Image.");
    }

    if ((Texture.Usage() & VK_IMAGE_USAGE_SAMPLED_BIT) == 0U || Texture.SampledIndex() == InvalidBindlessIndex)
    {
        throw std::logic_error("[luvk-imgui] UiTexture requires an image with VK_IMAGE_USAGE_SAMPLED_BIT.");
    }

    // Registering an image every frame (e.g. right before ImGui::Image) is cheap: only new images rebuild the list.
    if (m_UserTextures.insert(&Texture).second)
    {
        RefreshLiveImages();
    }
    return detail::ToTextureId(Texture.SampledIndex());
}

void luvk::imgui::ImGuiRenderer::RemoveTexture(const Image& Texture)
{
    if (m_UserTextures.erase(const_cast<Image*>(&Texture)) != 0U)
    {
        RefreshLiveImages();
    }
}

