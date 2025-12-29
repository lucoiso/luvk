/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <string_view>
#include <volk.h>
#include "luvk/Interfaces/IModule.hpp"
#include "luvk/Managers/Extensions.hpp"

namespace luvk
{
    /**
     * Arguments used to create the Vulkan instance.
     */
    struct LUVK_API InstanceCreationArguments
    {
        /** Name of the application. */
        std::string_view ApplicationName = "luvk";

        /** Name of the engine. */
        std::string_view EngineName = "luvk";

        /** Version of the application (VK_MAKE_VERSION). */
        std::uint32_t ApplicationVersion = VK_MAKE_VERSION(1U, 0U, 0U);

        /** Version of the engine (VK_MAKE_VERSION). */
        std::uint32_t EngineVersion = VK_MAKE_VERSION(1U, 0U, 0U);
    };

    /**
     * Manages the Vulkan Instance.
     */
    class LUVK_API Instance : public IModule
    {
    protected:
        /** The Vulkan instance handle. */
        VkInstance m_Instance{VK_NULL_HANDLE};

        /** Manager for instance-level extensions and layers. */
        InstanceExtensions m_Extensions{};

        /** The arguments used to create the instance. */
        InstanceCreationArguments m_Arguments{};

    public:
        /** Default constructor. */
        Instance();

        /** Default destructor. */
        ~Instance() override = default;

        /** Called upon module shutdown (destroys the instance). */
        void OnShutdown() override;

        /**
         * Creates the Vulkan instance using the provided arguments and registered extensions.
         * @param Arguments Configuration for the instance creation.
         * @return True if initialization was successful, false otherwise.
         */
        [[nodiscard]] bool Initialize(const InstanceCreationArguments& Arguments);

        /** Get the underlying VkInstance handle. */
        [[nodiscard]] constexpr VkInstance GetHandle() const noexcept
        {
            return m_Instance;
        }

        /** Get the instance extensions manager. */
        [[nodiscard]] constexpr InstanceExtensions& GetExtensions() noexcept
        {
            return m_Extensions;
        }

        /** Get the arguments used for instance creation. */
        [[nodiscard]] constexpr const InstanceCreationArguments& GetCreationArguments() const noexcept
        {
            return m_Arguments;
        }
    };
}
