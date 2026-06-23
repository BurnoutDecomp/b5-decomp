// ============================================================================
// b5-decomp/src/GameSource/Network/Managers/BrnNetworkStatsRequestEventQueue.cpp
// ============================================================================
// BrnNetwork::EventQueue<StatsRequestEvent, 32>::Push @ 0x8254DDA0
// BrnNetwork::EventQueue<StatsRequestEvent, 32>::Pop  @ 0x8254DE18
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (no leak / no DWARF). Both are simple
// fixed-capacity ring-buffer ops over the 20-byte StatsRequestEvent element:
//
//   Push: if (miLength >= 32) return 0;                          // full -> reject
//         copy 5 words a2 -> &maEvents[miWriteIndex];
//         ++miWriteIndex; ++miLength;
//         if (miWriteIndex >= 32) miWriteIndex = 0;              // wrap
//         return 1;
//
//   Pop:  if (miLength <= 0) return 0;                           // empty -> reject
//         copy 5 words &maEvents[miReadIndex] -> a2;
//         ++miReadIndex; --miLength;
//         if (miReadIndex >= 32) miReadIndex = 0;                // wrap
//         return 1;
//
// The X360 element index math (`index*5` via slwi+add then `*4` via slwi) is the
// 20-byte stride; modelled here by indexing the typed maEvents[] array. The 5-word
// copy loop (mtctr 5; lwz/stw) is the whole-element copy, reproduced via the element
// type's assignment.

#include "GameSource/Network/Managers/BrnNetworkStatsRequestEventQueue.h"

namespace BrnNetwork
{

template <typename T, int MAX>
int EventQueue<T, MAX>::Push(const T* lpEvent)
{
    if (miLength >= MAX)
    {
        return 0;
    }

    maEvents[miWriteIndex] = *lpEvent;   // 5-word whole-element copy a2 -> tail

    ++miWriteIndex;
    ++miLength;
    if (miWriteIndex >= MAX)
    {
        miWriteIndex = 0;
    }
    return 1;
}

template <typename T, int MAX>
int EventQueue<T, MAX>::Pop(T* lpEvent)
{
    if (miLength <= 0)
    {
        return 0;
    }

    *lpEvent = maEvents[miReadIndex];    // 5-word whole-element copy head -> a2

    ++miReadIndex;
    --miLength;
    if (miReadIndex >= MAX)
    {
        miReadIndex = 0;
    }
    return 1;
}

// Explicit instantiation of the X360-attested instance (the manager's mEventQueue).
template int EventQueue<StatsRequestEvent, 32>::Push(const StatsRequestEvent*);
template int EventQueue<StatsRequestEvent, 32>::Pop(StatsRequestEvent*);

}
