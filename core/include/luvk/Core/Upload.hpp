// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <vector>
#include "luvk/Core/Common.hpp"
#include "luvk/Resources/Buffer.hpp"

namespace luvk
{
    class Context;
    class CommandList;
    class Image;
    struct ImageRegion;

    /**
     * Batches CPU -> GPU copies on the transfer queue (a dedicated DMA queue when the GPU has one).
     *
     * Data is copied into a staging ring owned by each batch (reused once the batch's previous submission completed),
     * so steady-state uploads allocate nothing. Work recorded here is submitted by Flush(); the renderer flushes automatically and
     * makes the graphics queue wait on the transfer timeline, so uploaded data is visible to the next frame.
     * A batch waits for the last graphics submission that used any of its destinations (the render graph stamps it on
     * every persistent resource), so overwriting a resource that in-flight frames still read is safe while uploads into
     * idle resources overlap rendering. Record() waits for all previous graphics work. All methods are thread-safe; this
     * class is the only producer of
     * transfer-timeline signals, which keeps batch values exact.
     */
    class LUVK_API UploadManager
    {
        /** One command pool/buffer pair recycled round-robin while its previous submit is in flight. */
        struct Batch
        {
            /** Transient command pool for this batch (created on the transfer queue family). */
            VkCommandPool Pool = nullptr;

            /** Primary command buffer allocated from Pool. */
            VkCommandBuffer Commands = nullptr;

            /** Transfer timeline value this batch was last submitted with (0 until the first Flush()). */
            std::uint64_t Value = 0U;

            /** Staging memory of the batch (grown to fit the largest single upload). */
            Buffer Staging;

            /** Bytes of Staging used by the batch being recorded. */
            DeviceSize StagingUsed = 0U;
        };

        /** Staged copy source. */
        struct StagedRange
        {
            /** Staging buffer holding the data. */
            VkBuffer Buffer = nullptr;

            /** Byte offset of the data inside Buffer. */
            DeviceSize Offset = 0U;
        };

        /** Initial size of a batch's staging ring. */
        static constexpr DeviceSize StagingBlockSize = 4ULL * 1024ULL * 1024ULL;

        /** Owning context (device, queues). */
        Context&                   m_Context;

        /** Ring of batches; a batch is reused only after its previous timeline value completed. */
        std::vector<Batch> m_Batches;

        /** Index of the batch currently being recorded (round-robin). */
        std::size_t m_Current = 0U;

        /** True while the current batch has an open command buffer. */
        bool m_Recording = false;

        /** True when a transfer write was recorded and a barrier is needed before the next one. */
        bool m_PendingCopy = false;

        /** True when the batch being recorded holds work that must wait for previously submitted graphics work. */
        std::uint64_t m_WaitGraphicsValue = 0U;

        /** Serialises the recording state (every public method is thread-safe). */
        std::mutex m_Mutex;

        /** Latest transfer timeline value submitted by Flush(). */
        std::atomic<std::uint64_t> m_LastSubmitted = 0U;

    public:
        /** Constructor: creates BatchCount command pools/buffers on the transfer queue. */
        explicit UploadManager(Context& Ctx, std::uint32_t BatchCount = 3U);

        /** Destructor. */
        ~UploadManager();

        /** Deleted copy constructor. */
        UploadManager(const UploadManager&)            = delete;

        /** Deleted copy assignment. */
        UploadManager& operator=(const UploadManager&) = delete;

        /** Copies Data into Destination at the given byte offset through a staging buffer, ordered as Order says. */
        void Upload(Buffer& Destination, std::span<const std::byte> Data, DeviceSize Offset = 0U);

        /** Copies Data into a region of one mip level/array layer of Destination (validates the expected byte size). */
        void Upload(Image& Destination, std::span<const std::byte> Data, const ImageRegion& Region);

        /**
         * Records custom commands on the transfer timeline (they run before the next frame that waits on it).
         * Returns the batch value: call NoteTransferUse(Value) on every Buffer/Image the commands touch.
         * Do not call other UploadManager methods (e.g. Buffer::Write) from inside Commands.
         */
        [[nodiscard]] std::uint64_t Record(const std::function<void(CommandList&)>& Commands);

        /** Submits the pending batch (if any). Returns the transfer timeline value covering every upload so far. */
        [[nodiscard]] std::uint64_t Flush();

        /** Flush() and block until the uploads complete. */
        void FlushAndWait();

        /** Transfer timeline value covering every upload submitted so far. */
        [[nodiscard]] std::uint64_t LastSubmittedValue() const noexcept
        {
            return m_LastSubmitted.load(std::memory_order_acquire);
        }

    private:
        /** Returns the current batch's command buffer, starting a new recording when needed (mutex held). */
        VkCommandBuffer BeginLocked();

        /** Destroys every batch command pool. */
        void            DestroyBatches() noexcept;

        /** Copies Data into the current batch's staging ring (or a one-off buffer when it is full) (mutex held). */
        [[nodiscard]] StagedRange StageLocked(std::span<const std::byte> Data, DeviceSize Alignment);

        /** Inserts a transfer-to-transfer memory barrier when a previous copy is pending (mutex held). */
        void            SerializeCopiesLocked(VkCommandBuffer Commands);
    };
}
