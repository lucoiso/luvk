// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Frame/Blackboard.hpp"

std::byte* luvk::frame::Blackboard::Allocate(const std::size_t Size, const std::size_t Alignment)
{
    const auto Aligned = [Alignment](const std::size_t Offset) { return (Offset + Alignment - 1U) / Alignment * Alignment; };

    if (!std::empty(m_Blocks) && Aligned(m_Used) + Size > BlockSize)
    {
        ++m_Block;
        m_Used = 0U;
    }

    if (m_Block >= std::size(m_Blocks))
    {
        // operator new[] returns storage aligned for any fundamental type, which covers every value a slot holds.
        m_Blocks.push_back(std::make_unique<std::byte[]>(BlockSize));
        m_Block = std::size(m_Blocks) - 1U;
        m_Used  = 0U;
    }

    const std::size_t Offset = Aligned(m_Used);
    m_Used                   = Offset + Size;
    return m_Blocks[m_Block].get() + Offset;
}

const luvk::frame::Blackboard::Entry* luvk::frame::Blackboard::Locate(const std::uint64_t Hash, const void* const Type) const noexcept
{
    for (const Entry& Candidate : m_Entries)
    {
        if (Candidate.Hash == Hash && Candidate.Type == Type)
        {
            return &Candidate;
        }
    }
    return nullptr;
}

void luvk::frame::Blackboard::Clear() noexcept
{
    m_Entries.clear();
    m_Block = 0U;
    m_Used  = 0U;
}
