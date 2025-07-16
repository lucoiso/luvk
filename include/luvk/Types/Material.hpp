// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Modules/Module.hpp"

#include <memory>

namespace luvk
{
    class Texture;
    class DescriptorSet;
    class Pipeline;

    /** Basic material holding pipeline and texture references */
    class LUVKMODULE_API Material
    {
        std::shared_ptr<Texture> m_Texture{};
        std::shared_ptr<DescriptorSet> m_Descriptor{};
        std::shared_ptr<Pipeline> m_Pipeline{};

    public:
        constexpr Material() = default;

        void SetTexture(std::shared_ptr<Texture> tex)
        {
            m_Texture = std::move(tex);
        }

        void SetDescriptor(std::shared_ptr<DescriptorSet> desc)
        {
            m_Descriptor = std::move(desc);
        }

        void SetPipeline(std::shared_ptr<Pipeline> pipe)
        {
            m_Pipeline = std::move(pipe);
        }

        [[nodiscard]] constexpr std::shared_ptr<Texture> const& GetTexture() const noexcept
        {
            return m_Texture;
        }

        [[nodiscard]] constexpr std::shared_ptr<DescriptorSet> const& GetDescriptor() const noexcept
        {
            return m_Descriptor;
        }

        [[nodiscard]] constexpr std::shared_ptr<Pipeline> const& GetPipeline() const noexcept
        {
            return m_Pipeline;
        }
    };
} // namespace luvk
