/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <typeindex>

namespace luvk
{
    /**
     * Interface for the Service Locator pattern.
     * Allows modules to retrieve other dependencies without coupling to the Renderer.
     */
    class LUVK_API IServiceLocator
    {
    public:
        /** Virtual destructor. */
        virtual ~IServiceLocator() = default;

        /**
         * Retrieve a registered module by its type.
         * @tparam T The module type to retrieve.
         * @return Pointer to the module or nullptr if not found.
         */
        template <typename T>
        [[nodiscard]] T* GetModule() const
        {
            return static_cast<T*>(GetModuleInternal(typeid(T).hash_code()));
        }

        /**
         * Internal method to retrieve module by type hash.
         * @param TypeHash Hash code of the requested type.
         * @return Void pointer to the module.
         */
        [[nodiscard]] virtual void* GetModuleInternal(std::size_t TypeHash) const = 0;
    };
}
