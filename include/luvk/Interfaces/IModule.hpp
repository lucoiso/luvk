/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include "luvk/Interfaces/IEventModule.hpp"

namespace luvk
{
    class IServiceLocator;

    enum class ModuleEvents : std::uint8_t
    {
        OnInitialize,
        OnShutdown,
        Count
    };

    /**
     * Base interface for all luvk modules
     */
    class LUVK_API IModule : public IEventModule
    {
    public:
        /** Virtual destructor. */
        ~IModule() override = default;

        /**
         * Called when the module is initialized.
         * @param ServiceLocator Pointer to the central service locator.
         */
        virtual void OnInitialize(IServiceLocator* ServiceLocator)
        {
            GetEventSystem().Execute(ModuleEvents::OnInitialize);
        }

        /**
         * Called before the module is destroyed or unregistered.
         */
        virtual void OnShutdown()
        {
            GetEventSystem().Execute(ModuleEvents::OnShutdown);
        }
    };
}
