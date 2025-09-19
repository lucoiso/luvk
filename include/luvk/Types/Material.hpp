// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include "luvk/Module.hpp"

namespace luvk
{
    class Texture;
    class DescriptorSet;
    class Pipeline;

    class LUVKMODULE_API Material
    {
        std::shared_ptr<Texture> m_Texture{};
        std::shared_ptr<DescriptorSet> m_Descriptor{};
        std::shared_ptr<Pipeline> m_Pipeline{};

    public:
        constexpr Material() = default;

        void SetTexture(const std::shared_ptr<Texture>& Texture)
        {
            m_Texture = Texture;
        }

        void SetDescriptor(const std::shared_ptr<DescriptorSet>& Descriptor)
        {
            m_Descriptor = Descriptor;
        }

        void SetPipeline(const std::shared_ptr<Pipeline>& Pipeline)
        {
            m_Pipeline = Pipeline;
        }

        [[nodiscard]] constexpr const std::shared_ptr<Texture>& GetTexture() const noexcept
        {
            return m_Texture;
        }

        [[nodiscard]] constexpr const std::shared_ptr<DescriptorSet>& GetDescriptor() const noexcept
        {
            return m_Descriptor;
        }

        [[nodiscard]] constexpr const std::shared_ptr<Pipeline>& GetPipeline() const noexcept
        {
            return m_Pipeline;
        }
    };
} // namespace luvk
