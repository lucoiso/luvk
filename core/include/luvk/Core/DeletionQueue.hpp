// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <mutex>
#include <utility>
#include <vector>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    /**
     * A point on the GPU timelines. An object retired at point P can only be destroyed once every
     * queue timeline has reached the corresponding value (i.e. all work that could reference it finished).
     */
    struct TimelinePoint
    {
        /** Timeline value the graphics queue must reach. */
        std::uint64_t Graphics = 0U;

        /** Timeline value the transfer queue must reach (0 when the object never touched the transfer queue). */
        std::uint64_t Transfer = 0U;

        /** True when every timeline of this point has been reached by Completed. */
        [[nodiscard]] constexpr bool ReachedBy(const TimelinePoint& Completed) const noexcept
        {
            return Graphics <= Completed.Graphics && Transfer <= Completed.Transfer;
        }
    };

    /** Kind of retired object; selects the destroy path used by Context::DestroyRetired. */
    enum class RetiredKind : std::uint8_t
    {
        Buffer,
        Image,
        ImageView,
        Sampler,
        Pipeline,
        CommandPool,
        AccelerationStructure,
        BindlessSlot,
        Callback,
    };

    /**
     * Plain description of something that must be destroyed later. Kept POD-like (except the optional
     * callback) so that retiring the common Vulkan objects never allocates beyond the queue entry.
     */
    struct RetiredObject
    {
        /** Which destroy path Context::DestroyRetired must use. */
        RetiredKind Kind = {RetiredKind::Callback};

        /** Vulkan handle cast to 64 bits (HandleToU64); meaning depends on Kind. */
        std::uint64_t Handle = 0U;

        /** VMA allocation to release together with buffers/images (nullptr otherwise). */
        VmaAllocation Allocation = nullptr;

        /** BindlessSlot enumerator value (only for RetiredKind::BindlessSlot). */
        std::uint32_t Slot = 0U;

        /** Bindless index to free (only for RetiredKind::BindlessSlot). */
        std::uint32_t Index = 0U;

        /** Function to invoke (only for RetiredKind::Callback). */
        std::function<void()> Callback;
    };

    /**
     * Deferred destruction queue ("GPU garbage collector").
     *
     * - Producers (any thread) call Push(); a small mutex protects the incoming vector. Retirement is a
     *   cold/medium path (pipelines, buffers, bindless slots), so this is cheaper and safer than a lock-free
     *   stack that would allocate/deallocate nodes and rely on ABA-prone head exchanges.
     * - One consumer at a time calls Collect(): it takes the incoming vector, appends it in FIFO order to the
     *   pending list and destroys every object whose timeline point has been reached. Concurrent Collect()
     *   calls never block: the losers return 0.
     * - Entries are stored inline in vectors (no per-object node allocation) and the pending vector is
     *   compacted in place, so steady-state retirement does not fragment the heap.
     */
    class LUVK_API DeletionQueue
    {
        /** One retired object and the point that must be reached before it can be destroyed. */
        struct Entry
        {
            /** The retired object. */
            RetiredObject Object = {};

            /** Timeline point that must be reached before the object is destroyed. */
            TimelinePoint Point = {};
        };

        /** Protects m_Incoming and m_Size against concurrent producers. */
        mutable std::mutex m_Mutex;

        /** Objects published by producers and not yet taken by a collector, in push order. */
        std::vector<Entry> m_Incoming;

        /** Atomic flag ensuring a single Collect() runs at a time; concurrent callers return 0. */
        std::atomic_flag m_Collecting = ATOMIC_FLAG_INIT;

        /** Entries whose timeline points have not been reached yet, kept in FIFO order. */
        std::vector<Entry> m_Pending;

        /** Number of objects in the queue (incoming + pending), reported by Size(). */
        std::atomic<std::size_t> m_Size = 0U;

    public:
        /** Constructor. */
        DeletionQueue() = default;

        /** Destructor: frees every remaining entry (leaked objects are reported to stderr). */
        ~DeletionQueue();

        /** Deleted copy constructor. */
        DeletionQueue(const DeletionQueue&)            = delete;

        /** Deleted copy assignment. */
        DeletionQueue& operator=(const DeletionQueue&) = delete;

        /** Deleted move constructor. */
        DeletionQueue(DeletionQueue&&)                 = delete;

        /** Deleted move assignment. */
        DeletionQueue& operator=(DeletionQueue&&)      = delete;

        /** Thread-safe. Publishes Object for destruction once Point is reached. */
        void Push(RetiredObject&& Object, const TimelinePoint& Point);

        /** Destroys every pending object whose point was reached. Returns the number of destroyed objects. */
        template <typename Destroyer>
        std::size_t Collect(const TimelinePoint& Completed, Destroyer&& Destroy)
        {
            if (m_Collecting.test_and_set(std::memory_order_acquire))
            {
                return 0U;
            }

            struct ReleaseGuard
            {
                std::atomic_flag& Flag;

                ~ReleaseGuard()
                {
                    Flag.clear(std::memory_order_release);
                }
            } Guard{m_Collecting};

            std::vector<Entry> Incoming;
            {
                const std::scoped_lock Lock(m_Mutex);
                Incoming.swap(m_Incoming);
            }

            m_Pending.reserve(std::size(m_Pending) + std::size(Incoming));
            for (Entry& Item : Incoming)
            {
                m_Pending.push_back(std::move(Item));
            }

            std::size_t Destroyed = 0U;
            std::size_t Kept      = 0U;
            for (std::size_t Index = 0U; Index < std::size(m_Pending); ++Index)
            {
                Entry& Current = m_Pending[Index];
                if (Current.Point.ReachedBy(Completed))
                {
                    Destroy(Current.Object);
                    ++Destroyed;
                }
                else
                {
                    if (Kept != Index)
                    {
                        m_Pending[Kept] = std::move(Current);
                    }
                    ++Kept;
                }
            }
            m_Pending.resize(Kept);

            m_Size.fetch_sub(Destroyed, std::memory_order_relaxed);
            return Destroyed;
        }

        /** Destroys everything regardless of the timelines. Only valid once the device is idle. */
        template <typename Destroyer>
        std::size_t CollectAll(Destroyer&& Destroy)
        {
            std::size_t Total = 0U;
            /** Destroyers may retire more objects (e.g. callbacks), so loop until the queue is drained. */
            while (Size() > 0U)
            {
                const std::size_t Count = Collect(TimelinePoint{UINT64_MAX, UINT64_MAX}, Destroy);
                if (Count == 0U)
                {
                    break;
                }
                Total += Count;
            }
            return Total;
        }

        /** Number of objects waiting for destruction (approximate under concurrent pushes). */
        [[nodiscard]] std::size_t Size() const noexcept
        {
            return m_Size.load(std::memory_order_relaxed);
        }
    };
}
