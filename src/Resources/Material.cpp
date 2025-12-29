/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/Material.hpp"
#include <algorithm>
#include "luvk/Resources/Texture.hpp"

using namespace luvk;

void Material::SetPipeline(const std::shared_ptr<Pipeline>& PipelineObj)
{
    m_Pipeline = PipelineObj;
}

void Material::SetTexture(const std::uint32_t Slot, const std::shared_ptr<Texture>& TextureObj)
{
    if (Slot >= std::size(m_Textures))
    {
        m_Textures.resize(Slot + 1);
    }
    m_Textures[Slot] = TextureObj;
}

void Material::SetDescriptorSet(const std::uint32_t Slot, const std::shared_ptr<DescriptorSet>& SetObj)
{
    if (Slot >= std::size(m_DescriptorSets))
    {
        m_DescriptorSets.resize(Slot + 1);
    }
    m_DescriptorSets[Slot] = SetObj;
}

void Material::SetData(std::span<const std::byte> Data)
{
    m_MaterialData.assign(std::begin(Data), std::end(Data));
}

void Material::Bind(const VkCommandBuffer CommandBuffer) const
{
    if (!m_Pipeline)
    {
        return;
    }

    vkCmdBindPipeline(CommandBuffer, m_Pipeline->GetBindPoint(), m_Pipeline->GetHandle());

    if (!std::empty(m_MaterialData))
    {
        vkCmdPushConstants(CommandBuffer,
                           m_Pipeline->GetLayout(),
                           m_Pipeline->GetPushConstantStages(),
                           0,
                           static_cast<std::uint32_t>(std::size(m_MaterialData)),
                           std::data(m_MaterialData));
    }
}
