/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/Renderer.hpp"
#include <mutex>
#include "luvk/Modules/Draw.hpp"

using namespace luvk;

Renderer::Renderer(IServiceLocator* Parent)
    : m_ParentLocator(Parent)
{
    static std::once_flag Flag;
    std::call_once(Flag,
                   []
                   {
                       volkInitialize();
                   });
}

Renderer::~Renderer()
{
    Shutdown();
}

void Renderer::InitializeModules()
{
    for (auto* Module : m_InitializationOrder)
    {
        Module->OnInitialize(this);
    }
}

void Renderer::Shutdown()
{
    for (auto It = m_InitializationOrder.rbegin(); It != m_InitializationOrder.rend(); ++It)
    {
        if (*It)
        {
            (*It)->OnShutdown();
        }
    }

    m_InitializationOrder.clear();
    m_Modules.clear();
}

void Renderer::DrawFrame() const
{
    if (m_Paused)
    {
        return;
    }

    if (const auto* DrawMod = GetModule<Draw>())
    {
        DrawMod->RenderFrame();
    }
}

void Renderer::SetPaused(const bool Paused)
{
    m_Paused = Paused;
}

const std::vector<IModule*>& Renderer::GetRegisteredModules() const
{
    return m_InitializationOrder;
}

void* Renderer::GetModuleInternal(const std::size_t TypeHash) const
{
    for (const auto& [Index, Module] : m_Modules)
    {
        if (Index.hash_code() == TypeHash)
        {
            return Module.get();
        }
    }

    if (m_ParentLocator)
    {
        return m_ParentLocator->GetModuleInternal(TypeHash);
    }

    return nullptr;
}
