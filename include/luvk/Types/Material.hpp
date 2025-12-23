// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <span>
#include <volk.h>
#include "luvk/Module.hpp"

namespace luvk
{
    class Texture;
    class DescriptorSet;
    class Pipeline;
    class Buffer;
    class Device;
    class DescriptorPool;
    class Memory;

    class LUVKMODULE_API Material
    {
    protected:
        std::shared_ptr<Pipeline>      m_Pipeline{};
        std::shared_ptr<DescriptorSet> m_DescriptorSet{};
        std::shared_ptr<Texture>       m_Texture{};

    public:
        constexpr Material() = default;

        void Initialize(const std::shared_ptr<Device>&         Device,
                        const std::shared_ptr<DescriptorPool>& Pool,
                        const std::shared_ptr<Memory>&         Memory,
                        const std::shared_ptr<Pipeline>&       PipelineObj);

        void AllocateDescriptorSet(std::span<const VkDescriptorSetLayoutBinding> Bindings) const;

        void Bind(VkCommandBuffer CommandBuffer) const;

        void SetPipeline(const std::shared_ptr<Pipeline>& PipelineObj);
        void SetDescriptorSet(const std::shared_ptr<DescriptorSet>& DescriptorSetObj);
        void SetTexture(const std::shared_ptr<Texture>& TextureObj);
        void SetUniformBuffer(const std::shared_ptr<Buffer>& BufferObj, std::uint32_t Binding = 0) const;

        [[nodiscard]] std::shared_ptr<Pipeline> GetPipeline() const noexcept
        {
            return m_Pipeline;
        }

        [[nodiscard]] std::shared_ptr<DescriptorSet> GetDescriptor() const noexcept
        {
            return m_DescriptorSet;
        }
    };
} // namespace luvk
