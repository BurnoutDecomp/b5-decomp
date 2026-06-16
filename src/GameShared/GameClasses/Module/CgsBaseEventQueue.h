#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsModule
{
    // A non-owning ring of events stored in an externally supplied buffer. The
    // derived EventQueue<T, N> supplies an inline maEvents[N] buffer; other users
    // can point it at storage they manage. Recovered from the X360 spine with
    // member names/types from the DecFIGS DWARF (CgsBaseEventQueue.h).
    template <typename T>
    class BaseEventQueue
    {
    public:
        void Construct(T* lpEventBuffer, s32 liMaxLength)
        {
            CGS_ASSERT(lpEventBuffer != nullptr, "lpEventBuffer != NULL");
            mpEvents = lpEventBuffer;
            miMaxLength = liMaxLength;
            miLength = 0;
        }

        // Appends a copy of lEvent (asserting on overflow) and bumps miLength. The X360 build
        // emits a per-instantiation out-of-line body; modelled here as the generic inline
        // template body (each instantiation's ledger TU is an explicit-instantiation .cpp).
        // Returns false (without appending) when the queue is already full.
        bool AddEvent(const T& lEvent)
        {
            // X360 AddEvent appends UNCONDITIONALLY -- the two asserts are non-gating tripwires
            // (the bounds-gated "return false on full" variant is the separate AddEventSafe).
            CGS_ASSERT(mpEvents != nullptr, "mpEvents != NULL");
            CGS_ASSERT(miLength < miMaxLength, "EventQueue::AddEvent - Reached Max length");
            mpEvents[miLength] = lEvent;
            ++miLength;
            return true;
        }

        s32 GetMaxLength() const { return miMaxLength; }
        s32 GetLength() const { return miLength; }

    protected:
        T*  mpEvents;
        s32 miMaxLength;
        s32 miLength;
    };
}
