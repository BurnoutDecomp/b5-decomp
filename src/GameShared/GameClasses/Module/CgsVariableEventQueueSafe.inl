#pragma once

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "rw/core/debug/DebugCriticalSection.h"

namespace CgsModule
{
    namespace detail
    {
        // FAKEMATCH: ARTIST inlines the locked message build into the queue methods.
        __forceinline void AssertQueueNotConstructed(const char* lpcFile, s32 liLine)
        {
            using namespace CgsDev::Assert;
            gAssertMutex.Enter();
            CgsDev::StrStream lStrStream(gpcMessageBuffer, giMessageBufferSize, CgsDev::E_PRINTMODE_DECIMAL);
            lStrStream << "Not Constructed\n";
            FireAssert(gpcMessageBuffer, lpcFile, liLine);
            gAssertMutex.Leave();
        }
    }

    // -------- AddEventSafe<EventT> (typed convenience, safe) --------
    // X360: 0x82828868 (EventT = CgsDev::Internal::CInEventDrawAxis) and the sibling
    // CInEventDraw* debug-draw records. Asserts constructed (inline at
    // CgsVariableEventQueue.h:711) then forwards to the three-arg AddEventSafe with the
    // compile-time event size. Mirror of AddEvent<EventT> above but over the non-asserting
    // overflow-guarded AddEventSafe.
    template <s32 BUFSIZE, s32 ALIGN>
    template <typename EventT>
    bool VariableEventQueue<BUFSIZE, ALIGN>::AddEventSafe(const EventT& lrEvent, s32 liType)
    {
        if (!mbIsConstructed)
            detail::AssertQueueNotConstructed(CGS_ASSERT_LOCATION(detail::KAC_VEQ_FILE, 711));

        return AddEventSafe(reinterpret_cast<const Event*>(&lrEvent), liType, static_cast<s32>(sizeof(EventT)));
    }

}
