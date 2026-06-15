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

        // Appends a copy of lEvent (asserting on overflow) and bumps miLength.
        // Declared here; each instantiation's body is emitted out-of-line by the
        // X360 build, so it is its own ledger TU resolved at link time.
        bool AddEvent(const T& lEvent);

        s32 GetMaxLength() const { return miMaxLength; }
        s32 GetLength() const { return miLength; }

    protected:
        T*  mpEvents;
        s32 miMaxLength;
        s32 miLength;
    };
}
