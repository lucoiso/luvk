// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Types/Material.hpp"
#include "luvk/Resources/Buffer.hpp"
#include "luvk/Resources/DescriptorSet.hpp"
#include "luvk/Resources/Image.hpp"
#include "luvk/Resources/Pipeline.hpp"
#include "luvk/Resources/Sampler.hpp"
#include "luvk/Types/Texture.hpp"

using namespace luvk;

void Material::Initialize(const std::shared_ptr<Device>&         Device,
                          const std::shared_ptr<DescriptorPool>& Pool,
                          const std::shared_ptr<Memory>&         Memory,
                          const std::shared_ptr<Pipeline>&       PipelineObj)
{
    m_Pipeline = PipelineObj;

    if (m_Pipeline)
    {
        m_DescriptorSet = std::make_shared<DescriptorSet>(Device, Pool, Memory);
    }
}

void Material::AllocateDescriptorSet(const std::span<const VkDescriptorSetLayoutBinding> Bindings) const
{
    if (m_DescriptorSet)
    {
        m_DescriptorSet->CreateLayout({.Bindings = Bindings});
        m_DescriptorSet->Allocate();
    }
}

void Material::Bind(const VkCommandBuffer CommandBuffer) const
{
    if (!m_Pipeline)
    {
        return;
    }

    const VkPipelineBindPoint BindPoint = m_Pipeline->GetBindPoint();
    vkCmdBindPipeline(CommandBuffer, BindPoint, m_Pipeline->GetPipeline());

    const VkDescriptorSet SetHandle = m_DescriptorSet ? m_DescriptorSet->GetHandle() : VK_NULL_HANDLE;

    if (SetHandle != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(CommandBuffer, BindPoint, m_Pipeline->GetPipelineLayout(), 0, 1, &SetHandle, 0, nullptr);
    }
}

void Material::SetPipeline(const std::shared_ptr<Pipeline>& PipelineObj)
{
    m_Pipeline = PipelineObj;
}

void Material::SetDescriptorSet(const std::shared_ptr<DescriptorSet>& DescriptorSetObj)
{
    m_DescriptorSet = DescriptorSetObj;
}

void Material::SetTexture(const std::shared_ptr<Texture>& TextureObj)
{
    m_Texture = TextureObj;

    if (m_DescriptorSet && m_Texture)
    {
        m_DescriptorSet->UpdateImage(m_Texture->GetImage()->GetView(),
                                     m_Texture->GetSampler()->GetHandle(),
                                     0,
                                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
}

void Material::SetUniformBuffer(const std::shared_ptr<Buffer>& BufferObj, const std::uint32_t Binding) const
{
    if (m_DescriptorSet && BufferObj)
    {
        m_DescriptorSet->UpdateBuffer(BufferObj->GetHandle(), BufferObj->GetSize(), Binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }
}
