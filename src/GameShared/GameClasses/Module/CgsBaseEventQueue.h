#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // std::memcpy (BaseEventQueue<T>::Append, models the Xbox XMemCpy intrinsic)

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

        // Checked element accessor (X360 BaseEventQueue<T>::GetEvent(int), non-const overload,
        // CgsBaseEventQueue.h:290). The X360 build emits a per-instantiation out-of-line body;
        // modelled here as the generic inline template body. Hex-Rays renders the return as int
        // because a T& is ABI-returned as a 32-bit pointer; the DWARF gives the real T&.
        T& GetEvent(s32 liIndex)
        {
            CGS_ASSERT(mpEvents != nullptr, "mpEvents != NULL");
            CGS_ASSERT(liIndex < GetLength(), "liIndex < GetLength()");
            CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
            return mpEvents[liIndex];
        }

        // Asserted read of the queue's backing buffer (X360 CgsBaseEventQueue.h:486; called by Append).
        const T* GetQueueStartPointer() const
        {
            CGS_ASSERT(mpEvents != nullptr, "mpEvents != NULL");
            return mpEvents;
        }

        // Merges all live events from lSource onto the tail of this queue (X360
        // BaseEventQueue<T>::Append @ 0x82369DD0). Asserts (non-gating tripwires) that this
        // queue owns a buffer and the combined length will not overflow, then block-copies
        // lSource's events and advances miLength. Returns true.
        bool Append(const BaseEventQueue<T>& lSource)
        {
            CGS_ASSERT(mpEvents != nullptr, "mpEvents != NULL");                          // CgsBaseEventQueue.h:413
            CGS_ASSERT(lSource.miLength + miLength <= miMaxLength, "Base event queue overflow"); // :414

            s32 liSourceLength = lSource.miLength;
            const T* lpSourceEvents = lSource.GetQueueStartPointer();

            // XMemCpy (Xbox block-copy intrinsic), modelled as std::memcpy.
            std::memcpy(mpEvents + miLength, lpSourceEvents, sizeof(T) * liSourceLength);
            miLength += lSource.miLength;
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
