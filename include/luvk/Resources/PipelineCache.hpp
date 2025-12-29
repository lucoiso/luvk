/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <string_view>
#include <volk.h>

namespace luvk
{
    class Device;

    /**
     * Manages VkPipelineCache for faster pipeline creation.
     */
    class LUVK_API PipelineCache
    {
        /** The Vulkan pipeline cache handle. */
        VkPipelineCache m_Cache{VK_NULL_HANDLE};

        /** Pointer to the Device module for logical device handle. */
        Device* m_DeviceModule{nullptr};

    public:
        /** Pipeline caches cannot be default constructed. */
        PipelineCache() = delete;

        /**
         * Constructor.
         * @param DeviceModule Pointer to the Device module.
         */
        explicit PipelineCache(Device* DeviceModule);

        /** Destructor (destroys the pipeline cache). */
        ~PipelineCache();

        /**
         * Initializes the pipeline cache (creates or loads if not loaded).
         */
        void Initialize();

        /**
         * Saves the pipeline cache data to a file.
         * @param Path The file path to save to.
         */
        void Save(std::string_view Path) const;

        /**
         * Loads pipeline cache data from a file to initialize the cache.
         * @param Path The file path to load from.
         */
        void Load(std::string_view Path);

        /** Get the underlying VkPipelineCache handle. */
        [[nodiscard]] constexpr VkPipelineCache GetHandle() const noexcept
        {
            return m_Cache;
        }
    };
}
