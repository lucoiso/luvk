/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include "luvk/Interfaces/IModule.hpp"
#include "luvk/Interfaces/IServiceLocator.hpp"

namespace luvk
{
    /**
     * The main renderer class, acting as the central Service Locator and manager for all modules.
     */
    class LUVK_API Renderer : public IServiceLocator
    {
        /** Map of module type index to the unique pointer of the module instance. */
        std::unordered_map<std::type_index, std::unique_ptr<IModule>> m_Modules{};

        /** Ordered list of modules to ensure correct initialization and shutdown sequence. */
        std::vector<IModule*> m_InitializationOrder{};

        /** Flag indicating if the renderer is paused (i.e., DrawFrame should do nothing). */
        bool m_Paused{false};

        /** The parent locator of this renderer if applicable */
        IServiceLocator* m_ParentLocator{nullptr};

    public:
        /**
         * Constructor of this renderer
         * @tparam Parent The parent locator of this renderer.
         */
        explicit Renderer(IServiceLocator* Parent = nullptr);

        /** Destructor (calls Shutdown). */
        ~Renderer() override;

        /**
         * Register and store a module but do not call OnInitialize yet.
         * @tparam T The type of the module (must derive from IModule).
         * @return Pointer to the newly registered module.
         */
        template <typename T>
        T* RegisterModule()
        {
            static_assert(std::is_base_of_v<IModule, T>, "T must derive from IModule");
            const std::type_index Index(typeid(T));

            if (!m_Modules.contains(Index))
            {
                m_Modules[Index] = std::make_unique<T>();
                m_InitializationOrder.push_back(m_Modules[Index].get());
            }

            return static_cast<T*>(m_Modules[Index].get());
        }

        /** Initialize all registered modules in order */
        void InitializeModules();

        /** Shuts down all registered modules in reverse order and clears the module list. */
        void Shutdown();

        /** Executes the main draw loop for one frame. */
        void DrawFrame() const;

        /** Sets the paused state of the renderer. */
        void SetPaused(bool Paused);

        /** Check if the renderer is currently paused. */
        [[nodiscard]] constexpr bool IsPaused() const
        {
            return m_Paused;
        }

        /** Get the list of registered modules in their initialization order. */
        [[nodiscard]] const std::vector<IModule*>& GetRegisteredModules() const;

    protected:
        /** Internal method to retrieve module by type hash (used by GetModule<T>). */
        [[nodiscard]] void* GetModuleInternal(std::size_t TypeHash) const override;
    };
}
