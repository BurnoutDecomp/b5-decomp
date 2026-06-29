#pragma once

// ===========================================================================
// SDKs/EATech/include/Apt/AptAnimationTarget.h
//
// AptAnimationTarget -- the Apt animation DIRECTOR an AptTarget owns at +0x18: it
// holds the root display list, the deferred action/function queues, the interval
// timers and the event-listener slots that drive one Flash movie's playback. The
// large majority of the Apt runtime reaches it as
// gpAptTarget->GetAnimationTarget() (X360 off_8324E574->[+0x18]).
//
// LAYOUT (88 bytes / 22 dwords), proven from the ctor @0x82AFF648 (88-byte pool
// alloc) + the dtor @0x82AFF790 (which frees each owned sub-object, fixing the
// pointer members + their element sizes):
//     +0x00 mnField00            (zeroed at ctor)                       FLAG: role TBD
//     +0x04 mnNumIntervalTimers  (= params[0]; the mpIntervalTimers count)
//     +0x08 mnNumListenerSlots   (= params[1]; the mpListenerSlots count, freed 4*count)
//     +0x0C mpActionQueue        AptActionQueueC*  (20-byte; dtor frees 20)
//     +0x10 mInlineSubA[2]       inline 8-byte sub-object (ctor/dtor sub_82AE16xx) FLAG: type TBD
//     +0x18 mInlineSubB[2]       inline 8-byte sub-object (ctor/dtor sub_82AE17xx) FLAG: type TBD
//     +0x20 mDisplayList         AptDisplayList    (INLINE; the root display list)
//     +0x24 mpIntervalTimers     AptIntervalTimer* (array of mnNumIntervalTimers, 36B each)
//     +0x28 mnField28            (zeroed at ctor)                       FLAG: role TBD
//     +0x2C mpListenerSlots      void*  (4*mnNumListenerSlots alloc; the listener table)
//     +0x30 mpQueueHeadA         void*  ) four deferred-callback list heads, each seeded
//     +0x34 mpQueueHeadB         void*  ) to the off_8324D814 empty-list sentinel; these
//     +0x38 mpQueueHeadC         void*  ) back AddAction/AddFunction Front/Back. FLAG:
//     +0x3C mpQueueHeadD         void*  ) the exact A/B/C/D->front/back mapping is TBD.
//     +0x40 maTail[6]            (six dwords not written by the ctor)   FLAG: roles TBD
//
// Member access is BY NAME; the X360 byte offsets are documentation only (on the
// x64 PC gate the inline AptDisplayList pointer widens, so later offsets shift --
// correct per the semantic-parity-by-named-members rule, no size assert).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptDisplayList.h"     // mDisplayList (inline, +0x20)
#include "SDKs/EATech/include/Apt/AptIntervalTimer.h"   // mpIntervalTimers element type

struct AptActionQueueC;   // +0x0C -- the 20-byte deferred-action queue (own TU; held by ptr)

struct AptAnimationTarget
{
    u32               mnField00;            // +0x00  FLAG: role TBD
    u32               mnNumIntervalTimers;  // +0x04  (= params[0])
    u32               mnNumListenerSlots;   // +0x08  (= params[1])
    AptActionQueueC*  mpActionQueue;        // +0x0C
    u32               mInlineSubA[2];       // +0x10  FLAG: inline 8-byte sub-object, type TBD
    u32               mInlineSubB[2];       // +0x18  FLAG: inline 8-byte sub-object, type TBD
    AptDisplayList    mDisplayList;         // +0x20  (inline root display list)
    AptIntervalTimer* mpIntervalTimers;     // +0x24  (array of mnNumIntervalTimers)
    u32               mnField28;            // +0x28  FLAG: role TBD
    void*             mpListenerSlots;      // +0x2C  (4*mnNumListenerSlots alloc)
    void*             mpQueueHeadA;         // +0x30  FLAG: deferred-callback list head
    void*             mpQueueHeadB;         // +0x34  FLAG: deferred-callback list head
    void*             mpQueueHeadC;         // +0x38  FLAG: deferred-callback list head
    void*             mpQueueHeadD;         // +0x3C  FLAG: deferred-callback list head
    u32               maTail[6];            // +0x40..+0x54  FLAG: roles TBD

    // GetRootDisplayList -- the root display list the context walks (mDisplayList).
    AptDisplayList* GetRootDisplayList() { return &mDisplayList; }
};
