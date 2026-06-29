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
    // +0x10 / +0x18 -- two inline 8-byte sub-objects the ctor builds from params[2]/
    // params[3]. GetListenerSet/GetInputSet hand back their addresses. FLAG: exact
    // element types TBD; kept as opaque 8-byte stores (accessed by address only).
    u32               mListenerSet[2];      // +0x10  (ctor __(this+0x10, params[2]))
    u32               mInputSet[2];         // +0x18  (ctor sub_82AE1708(this+0x18, params[3]))
    AptDisplayList    mDisplayList;         // +0x20  (inline root display list)
    AptIntervalTimer* mpIntervalTimers;     // +0x24  (array of mnNumIntervalTimers)
    u32               mnQueuedInputsSize;   // +0x28  (SetQueuedInputsSize; zeroed at ctor)
    void*             mpListenerSlots;      // +0x2C  (4*mnNumListenerSlots alloc)
    // +0x30..+0x3C -- four input-event object slots, ALL seeded to the None sentinel
    // (off_8324D814) by the ctor; the input layer's Set*Object accessors store the
    // currently-acting movie clip / object here. (Earlier guessed "queue heads" --
    // the ctor's None-seeding + the Set* accessors prove they are object slots.)
    void*             mpOnPressObject;      // +0x30  (SetOnPressObject)
    void*             mpOnRollOverObject;   // +0x34  (SetOnRollOverObject)
    void*             mpInputEventObject38; // +0x38  FLAG: 3rd input-event slot, role TBD
    void*             mpDragMC;             // +0x3C  (Get/SetDragMC -- the dragged clip)
    u32               mDragPos[2];          // +0x40  (GetDragPos returns &mDragPos; inline pos)
    u32               maTail[4];            // +0x48..+0x54  FLAG: roles TBD

    // GetRootDisplayList -- the root display list the context walks (mDisplayList).
    AptDisplayList* GetRootDisplayList() { return &mDisplayList; }

    // ---- instance accessors (X360 ARTIST one-liners; offsets above) ------------
    AptDisplayList* GetDisplayList()              { return &mDisplayList; }            // @0x82AD5F18
    void*           GetListenerSet()              { return &mListenerSet; }            // @0x82AD5F08
    void*           GetInputSet()                 { return &mInputSet; }               // @0x82AD5F10
    void*           GetDragPos()                  { return &mDragPos; }                // @0x82AD5F38
    void*           GetDragMC()                   { return mpDragMC; }                 // @0x82AD5F30
    void            SetDragMC(void* pClip)        { mpDragMC = pClip; }                // @0x82AD5F28
    void            SetOnPressObject(void* p)     { mpOnPressObject = p; }             // @0x82AD5F40
    void            SetOnRollOverObject(void* p)  { mpOnRollOverObject = p; }          // @0x82AD5F48
    void            SetQueuedInputsSize(int nSize){ mnQueuedInputsSize = (u32)nSize; } // @0x82AD4FC0

    // ---- class-static data layer (shared tables; bodies in AptAnimationTarget.cpp) ----
    static void  SetupStaticData(int nMaxNewMovieClips);  // @0x82AE41F0
    static void  CleanupStaticData();                     // @0x82AE42A8
    static int   GetXMousePos();                          // @0x82AD5F98
    static int   GetYMousePos();                          // @0x82AD5FA8
    static int   GetMaxNewMovieClips();                   // @0x82AD5E90
    static void* GetNewInsts();                           // @0x82AD5EA0
    static int   GetNewInstSize();                        // @0x82AD5EB0
    static int   DecNewInstSize();                        // @0x82AD5EC0 (post-increments)
    static void* GetDelayedReleaseList();                 // @0x82AD5ED8
    static int   GetDelayedReleaseListSize();             // @0x82AD5EE8
};
