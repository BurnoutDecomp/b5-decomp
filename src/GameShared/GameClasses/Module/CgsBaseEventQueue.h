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
        // Appends UNCONDITIONALLY -- the two asserts are non-gating tripwires; the bounds-gated
        // "return false when already full" variant is the separate AddEventSafe below.
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

        // No-arg "reserve next slot" variant (X360 BaseEventQueue<T>::AddEvent(), DWARF
        // CgsBaseEventQueue.h:358; asserts at :360/:361). Reserves the slot at the current tail,
        // bumps miLength, and returns a reference to the freshly-reserved (uninitialised) element
        // so the caller can fill it in place -- the X360 body computes &mpEvents[miLength], stores
        // miLength+1 back, and returns the slot pointer. Appends UNCONDITIONALLY; the two asserts
        // are non-gating tripwires. Used by VehicleOutputInterface::AddTrafficState (816-byte
        // PhysicalTrafficState element). ADDITIVE GROW (flagged): new no-arg overload, no change to
        // existing members/behaviour.
        T& AddEvent()
        {
            CGS_ASSERT(mpEvents != nullptr, "mpEvents != NULL");
            CGS_ASSERT(GetLength() < GetMaxLength(), "GetLength() < GetMaxLength()");
            s32 liIndex = miLength;
            ++miLength;
            return mpEvents[liIndex];
        }

        // Bounds-gated append (X360 BaseEventQueue<T>::AddEventSafe, DWARF CgsBaseEventQueue.h:329):
        // asserts the queue owns a buffer, then returns false WITHOUT appending when already full,
        // otherwise appends a copy of lEvent, bumps miLength and returns true. The X360 build emits
        // a per-instantiation out-of-line body; modelled here as the generic inline template body.
        bool AddEventSafe(const T& lEvent)
        {
            CGS_ASSERT(mpEvents != nullptr, "mpEvents != NULL");
            if (miLength >= miMaxLength) return false;
            mpEvents[miLength] = lEvent;
            ++miLength;
            return true;
        }

        // Checked element accessor, const overload (X360 BaseEventQueue<T>::GetEvent(int) const,
        // CgsBaseEventQueue.h:270). The X360 build emits a per-instantiation out-of-line body
        // (e.g. 0x823ABCD0 for BaseInputEvent stride 8, 0x823ABD78 for BindResult stride 12);
        // modelled here as the generic inline template body. Hex-Rays renders the asserts at
        // source lines 272/274/275 and the return as `8*a2 + *a1` (mpEvents + liIndex*sizeof(T)),
        // i.e. &mpEvents[liIndex]. The DWARF gives the real `const T&` return.
        const T& GetEvent(s32 liIndex) const
        {
            CGS_ASSERT(mpEvents != nullptr, "mpEvents != NULL");
            CGS_ASSERT(liIndex < GetLength(), "liIndex < GetLength()");
            CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
            return mpEvents[liIndex];
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

        // Drop the HEAD event, shifting the remaining ones down. ADDITIVE GROW (flagged):
        // no layout change, no effect on existing users. Added for the PC bundle loader's
        // one-request-per-Update pacing (CgsResourceBundleLoaderModule.cpp), which has to
        // leave the un-serviced tail queued for the next frame the way the console's async
        // loader does.
        void PopFront()
        {
            if (miLength <= 0) return;
            for (s32 liIndex = 1; liIndex < miLength; ++liIndex)
            {
                mpEvents[liIndex - 1] = mpEvents[liIndex];
            }
            --miLength;
        }

        // Drop all live events without touching the backing buffer (X360 sets miLength = 0
        // directly; e.g. inlined into NetworkInputInterface::operator= before each per-car
        // Append). ADDITIVE GROW (flagged): pure miLength reset, no layout/behaviour change to
        // existing users.
        void Clear() { miLength = 0; }

        s32 GetMaxLength() const { return miMaxLength; }
        s32 GetLength() const { return miLength; }

    protected:
        T*  mpEvents;
        s32 miMaxLength;
        s32 miLength;
    };
}
