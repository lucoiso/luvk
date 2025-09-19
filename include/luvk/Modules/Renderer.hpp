// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <functional>
#include <memory>
#include <span>
#include <typeindex>
#include "luvk/Module.hpp"
#include "luvk/Modules/Memory.hpp"
#include "luvk/Modules/Synchronization.hpp"
#include "luvk/Resources/Extensions.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Types/UnorderedMap.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    enum class RendererEvents : std::uint8_t
    {
        OnPostInitialized, OnRenderLoopInitialized
    };

    class LUVKMODULE_API Renderer : public IRenderModule,
                                    public std::enable_shared_from_this<Renderer>
    {
        VkInstance m_Instance{VK_NULL_HANDLE};
        InstanceExtensions m_Extensions{};
        Vector<std::function<void(VkCommandBuffer)>> m_PostRenderCommands{};
        Vector<std::function<void()>> m_ExternalDestructors{};
        UnorderedMap<std::type_index, std::shared_ptr<IRenderModule>> m_ModuleMap{};
        bool m_Paused{false};

    public:
        constexpr Renderer() = default;

        ~Renderer() override;

        [[nodiscard]] const VkInstance& GetInstance() const
        {
            return m_Instance;
        }

        [[nodiscard]] InstanceExtensions& GetExtensions()
        {
            return m_Extensions;
        }

        [[nodiscard]] const auto& GetModules() const
        {
            return m_ModuleMap;
        }

        template <typename Type>
        [[nodiscard]] constexpr std::shared_ptr<Type> FindModule() const
        {
            if (const auto It = m_ModuleMap.find(std::type_index(typeid(Type)));
                It != std::end(m_ModuleMap))
            {
                return std::static_pointer_cast<Type>(It->Second);
            }
            return nullptr;
        }

        void PreInitializeRenderer();
        void RegisterModules(Vector<std::shared_ptr<IRenderModule>>&& Modules);

        struct InstanceCreationArguments
        {
            std::string_view ApplicationName;
            std::string_view EngineName;
            std::uint32_t ApplicationVersion;
            std::uint32_t EngineVersion;
        };

        [[nodiscard]] bool InitializeRenderer(const InstanceCreationArguments& Arguments, const void* pNext);

        void PostInitializeRenderer();
        void InitializeRenderLoop();

        void DrawFrame();
        void SetPaused(bool Paused);
        void Refresh(const VkExtent2D& Extent) const;

        void EnqueueCommand(std::function<void(VkCommandBuffer)>&& Cmd);
        void EnqueueDestructor(std::function<void()>&& Destructor);

        [[nodiscard]] VkDescriptorPool CreateExternalDescriptorPool(std::uint32_t MaxSets,
                                                                    std::span<const VkDescriptorPoolSize> PoolSizes,
                                                                    VkDescriptorPoolCreateFlags Flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

        void DestroyExternalDescriptorPool(VkDescriptorPool& Pool) const;

        void BeginExternalFrame();
        void EndExternalFrame();

    private:
        void InitializeDependencies(const std::shared_ptr<IRenderModule>& MainRenderer) override;
        void ClearResources() override;
        void SetupFrames() const;
        void RecordComputePass(const VkCommandBuffer& Cmd) const;
        void RecordGraphicsPass(Synchronization::FrameData& Frame, std::uint32_t ImageIndex);
        void RecordCommands(Synchronization::FrameData& Frame, std::uint32_t ImageIndex);
        void SubmitFrame(Synchronization::FrameData& Frame, std::uint32_t ImageIndex) const;
    };
} // namespace luvk
