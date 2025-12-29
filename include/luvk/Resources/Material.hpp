/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <memory>
#include <vector>
#include <volk.h>
#include "luvk/Resources/Pipeline.hpp"

namespace luvk
{
    class Texture;
    class Buffer;
    class Device;
    class DescriptorSet;

    /**
     * Material system supporting Bindless Rendering and Push Descriptors.
     * Stores references to textures and pipelines but relies on shader indices/BDA.
     */
    class LUVK_API Material
    {
    protected:
        /** The rendering pipeline used by this material. */
        std::shared_ptr<Pipeline> m_Pipeline{};

        /** References to textures used by this material (for lifetime management). */
        std::vector<std::shared_ptr<Texture>> m_Textures{};

        /** References to descriptor sets used by this material (for lifetime management). */
        std::vector<std::shared_ptr<DescriptorSet>> m_DescriptorSets{};

        /** Raw data for push constants or other material-specific uniforms. */
        std::vector<std::byte> m_MaterialData{};

    public:
        /** Default constructor. */
        constexpr Material() = default;

        /** Virtual destructor. */
        virtual ~Material() = default;

        /**
         * Sets the rendering pipeline for this material.
         * @param PipelineObj Shared pointer to the Pipeline object.
         */
        void SetPipeline(const std::shared_ptr<Pipeline>& PipelineObj);

        /**
         * Get the pipeline object
         * @return Pointer to the pipeline.
         */
        [[nodiscard]] std::shared_ptr<Pipeline> GetPipeline() const noexcept
        {
            return m_Pipeline;
        }

        /**
         * Bind a texture to a specific slot/index for this material.
         * In Bindless, this just stores the reference to keep it alive.
         * @param Slot The conceptual texture slot/index.
         * @param TextureObj Shared pointer to the Texture object.
         */
        void SetTexture(std::uint32_t Slot, const std::shared_ptr<Texture>& TextureObj);

        /**
         * Get the texture in the specified slot
         * @param Slot The conceptual texture slot/index.
         * @return Pointer to the texture.
         */
        [[nodiscard]] std::shared_ptr<Texture> GetTexture(const std::uint32_t Slot) const noexcept
        {
            return m_Textures[Slot];
        }

        /**
         * Store a reference to a DescriptorSet to manage its lifetime.
         * @param Slot The conceptual descriptor set slot/index.
         * @param SetObj Shared pointer to the DescriptorSet object.
         */
        void SetDescriptorSet(std::uint32_t Slot, const std::shared_ptr<DescriptorSet>& SetObj);

        /**
         * Get the descriptor in the specified slot
         * @param Slot The conceptual descriptor set slot/index.
         * @return Pointer to the descriptor set.
         */
        [[nodiscard]] std::shared_ptr<DescriptorSet> GetDescriptorSet(const std::uint32_t Slot) const noexcept
        {
            return m_DescriptorSets[Slot];
        }

        /**
         * Update internal material data (Push Constants).
         * @param Data Span of raw bytes to copy into the material data.
         */
        void SetData(std::span<const std::byte> Data);

        /**
         * Bind pipeline and push constants/descriptors.
         * @param CommandBuffer The buffer to record commands to.
         */
        void Bind(VkCommandBuffer CommandBuffer) const;
    };
}
