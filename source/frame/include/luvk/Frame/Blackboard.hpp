// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <vector>
#include <luvk/Render/RenderGraph.hpp>

namespace luvk::frame
{
    /** 64-bit FNV-1a of a slot name, evaluated at compile time for constant keys. */
    [[nodiscard]] constexpr std::uint64_t HashSlotName(const std::string_view Name) noexcept
    {
        std::uint64_t Hash = 14695981039346656037ULL;
        for (const char Character : Name)
        {
            Hash = (Hash ^ static_cast<std::uint8_t>(Character)) * 1099511628211ULL;
        }
        return Hash;
    }

    /**
     * Typed name of a blackboard slot: the value type is part of the key, so a slot published as a texture cannot be read
     * as anything else. Keys are constants (`inline constexpr SlotKey<TextureHandle> SceneColor{"SceneColor"}`).
     */
    template <typename T>
    struct SlotKey
    {
        /** Human-readable name (debugging, error messages). */
        std::string_view Name;

        /** Hash of Name. */
        std::uint64_t Hash = 0U;

        /** Constructor. */
        constexpr explicit SlotKey(const std::string_view SlotName) noexcept
            : Name(SlotName),
              Hash(HashSlotName(SlotName))
        {
        }
    };

    /**
     * Per-frame registry of graph resources and small values, so features communicate without knowing each other (e.g. an
     * upscaler reads the scene color and publishes a new one under the same key). Values are trivially copyable and live in
     * stable blocks: a pointer returned by Find stays valid until Clear. Lookups are a short linear scan of hashes; nothing
     * is allocated once the blocks are warm.
     */
    class Blackboard
    {
        /** One published value. */
        struct Entry
        {
            /** Hash of the slot name. */
            std::uint64_t Hash = 0U;

            /** Address unique to the value type. */
            const void* Type = nullptr;

            /** Storage of the value. */
            std::byte* Data = nullptr;
        };

        /** Size of one storage block. */
        static constexpr std::size_t BlockSize = 4096U;

        /** Published values of the frame. */
        std::vector<Entry> m_Entries;

        /** Value storage; blocks are kept across frames. */
        std::vector<std::unique_ptr<std::byte[]>> m_Blocks;

        /** Block being filled. */
        std::size_t m_Block = 0U;

        /** Bytes used in the block being filled. */
        std::size_t m_Used = 0U;

        /** Unique address per type. */
        template <typename T>
        static constexpr char TypeTag = 0;

        /** Storage for one value of Size bytes aligned to Alignment (Size must not exceed BlockSize). */
        [[nodiscard]] std::byte* Allocate(std::size_t Size, std::size_t Alignment);

        /** The entry of (Hash, Type), or nullptr. */
        [[nodiscard]] const Entry* Locate(std::uint64_t Hash, const void* Type) const noexcept;

    public:
        /** Removes every value (the storage blocks are kept). */
        void Clear() noexcept;

        /** Publishes (or replaces) Value under Key. */
        template <typename T>
            requires std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>
        void Set(const SlotKey<T>& Key, const T& Value)
        {
            static_assert(sizeof(T) <= BlockSize, "Blackboard values are small: publish large data through a buffer");
            if (const Entry* Existing = Locate(Key.Hash, &TypeTag<T>))
            {
                std::memcpy(Existing->Data, &Value, sizeof(T));
                return;
            }

            std::byte* const Storage = Allocate(sizeof(T), alignof(T));
            std::memcpy(Storage, &Value, sizeof(T));
            m_Entries.push_back({.Hash = Key.Hash, .Type = &TypeTag<T>, .Data = Storage});
        }

        /** The value published under Key, or nullptr. */
        template <typename T>
        [[nodiscard]] const T* Find(const SlotKey<T>& Key) const noexcept
        {
            const Entry* const Found = Locate(Key.Hash, &TypeTag<T>);
            return Found != nullptr ? std::launder(reinterpret_cast<const T*>(Found->Data)) : nullptr;
        }

        /** The texture published under Key, or an invalid handle. */
        [[nodiscard]] TextureHandle FindTexture(const SlotKey<TextureHandle>& Key) const noexcept
        {
            const TextureHandle* const Found = Find(Key);
            return Found != nullptr ? *Found : TextureHandle{};
        }

        /** The buffer published under Key, or an invalid handle. */
        [[nodiscard]] BufferHandle FindBuffer(const SlotKey<BufferHandle>& Key) const noexcept
        {
            const BufferHandle* const Found = Find(Key);
            return Found != nullptr ? *Found : BufferHandle{};
        }
    };
}
