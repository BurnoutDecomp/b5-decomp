// ============================================================================
// b5-decomp/src/GameSource/Network/Managers/BrnNetworkStatsRequestEventQueue.h
// ============================================================================
// BrnNetwork::StatsRequestEvent + the fixed-capacity ring queue the
// NetworkPlayerStatsManager embeds for it. The X360 demangled TU name
// `BrnNetwork::StatsRequestEvent,32>` is the truncated spelling of the template
// instance BrnNetwork::EventQueue<StatsRequestEvent, 32> (the manager's `mEventQueue`,
// per the X360 BrnNetworkPlayerStatsManager.cpp assert text
// "mEventQueue.GetLength() + 1 < mEventQueue.GetMaxLength()").
//
// No source or DWARF is available for this TU, so SHAPE + bodies are recovered from the
// X360 asm of the two exported instance methods:
//   Push @ 0x8254DDA0  (BrnNetwork::NetworkPlayerStatsManager::AddEvent)
//   Pop  @ 0x8254DE18  (BrnNetwork::NetworkPlayerStatsManager::CopyNextValidEventOutOfQueue)
//
// LAYOUT (from the asm; offsets are from the queue's `this`):
//   +0x000  maEvents[32]  -- the element ring; element stride 20 bytes (5 dwords:
//                            the asm computes `head*5*4 + this` -> slwi*2 then *4).
//   +0x280  miReadIndex   -- Pop cursor  (read from head, wraps at 32)
//   +0x284  miWriteIndex  -- Push cursor (write at tail, wraps at 32)
//   +0x288  miLength      -- live element count (>=32 rejects Push; <=0 rejects Pop)
// sizeof == 640 + 12 == 652 bytes.
//
// StatsRequestEvent is a 20-byte (5 x s32-word) value the manager copies whole into /
// out of the queue. Its finer field types are not recoverable from the pure word-copy
// loop, so it is modelled as five offset-pinned s32 words (a later finer
// reconstruction can retype them without moving anything).
#pragma once

#include "types.hpp"

namespace BrnNetwork
{
    // ------------------------------------------------------------------------
    // StatsRequestEvent -- a 20-byte stats-request record (5 words). Copied whole
    // word-for-word by the queue's Push/Pop. Modelled as five pinned words.
    // ------------------------------------------------------------------------
    struct StatsRequestEvent
    {
        s32 maWords[5];   // 5 x s32 (20-byte element stride proven by the X360 index math)
    };
    static_assert(sizeof(StatsRequestEvent) == 20, "StatsRequestEvent element stride (20 bytes)");

    // ------------------------------------------------------------------------
    // EventQueue<StatsRequestEvent, 32> -- the fixed-capacity ring the manager embeds.
    // Push appends at miWriteIndex; Pop removes from miReadIndex; both wrap at MAX
    // (32) and keep miLength in sync. Push rejects when full (length >= 32), Pop
    // rejects when empty (length <= 0). The X360 returns int (1 = ok, 0 = rejected).
    // ------------------------------------------------------------------------
    template <typename T, int MAX>
    struct EventQueue
    {
        T   maEvents[MAX];   // +0x000  ring storage
        s32 miReadIndex;     // +0x280  (== MAX*sizeof(T) for MAX=32, sizeof(T)=20)
        s32 miWriteIndex;    // +0x284
        s32 miLength;        // +0x288  live element count

        // @ 0x8254DDA0 -- append lpEvent at miWriteIndex (wraps at MAX), bump length.
        //                 Returns 0 if the queue is full, 1 otherwise.
        int Push(const T* lpEvent);

        // @ 0x8254DE18 -- copy the front element to lpEvent, advance miReadIndex
        //                 (wraps at MAX), drop the length. Returns 0 if empty, 1 otherwise.
        int Pop(T* lpEvent);

        // Inline accessors named by the X360 assert text (GetLength / GetMaxLength).
        // No standalone X360 export -- inlined at the manager's assert sites.
        s32 GetLength() const { return miLength; }
        s32 GetMaxLength() const { return MAX; }
    };

    typedef EventQueue<StatsRequestEvent, 32> StatsRequestEventQueue;
}
