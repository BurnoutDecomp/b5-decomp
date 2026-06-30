#pragma once

// ===========================================================================
// SDKs/EATech/include/Apt/AptAnimationTarget.h
//
// AptAnimationTarget -- the Apt animation DIRECTOR an AptTarget owns at +0x18: it
// holds the root display list, the deferred action/function queue, the interval
// timers, the per-frame queued-input buffer and the event-listener / input
// "sets" that drive one Flash movie's playback. The large majority of the Apt
// runtime reaches it as gpAptTarget->GetAnimationTarget() (X360
// off_8324E574->[+0x18]).
//
// LAYOUT proven from the ctor @0x82AFF648 (88-byte pool alloc) + the dtor
// @0x82AFF790, plus the input/listener/register passes (AddInput @0x82AD93B0,
// ProcessInputs @0x82B01FD0, ProcessInputSet @0x82AF4478, RegisterReferences
// @0x82ADEC00, RemoveCIHReferences @0x82ADEEA0, PreDestroy @0x82AFE420), which
// jointly fix the offsets:
//     +0x00 mpInputMask          AptValue* (GC-ref; ctor 0; "AptAnimationTarget::mpInputMask")
//     +0x04 mnNumIntervalTimers  (= params[0]; the mpIntervalTimers count)
//     +0x08 mnQueuedInputsCap    (= params[1]; AddInput's full-test cap; 4*cap alloc @+0x2C)
//     +0x0C mpActionQueue        AptActionQueueC*  (20-byte deferred-action queue; held by ptr)
//     +0x10 mListenerSet  Set  (count@0x10, capacity@0x12, slots@0x14; built from params[2])
//     +0x18 mInputSet     Set  (count@0x18, capacity@0x1A, slots@0x1C; built from params[3])
//     +0x20 mDisplayList         AptDisplayList    (INLINE; the root display list)
//     +0x24 mpIntervalTimers     AptIntervalTimer* (array of mnNumIntervalTimers, 36B each)
//     +0x28 mnQueuedInputsCount  (AddInput fill counter; ProcessInputs drains then 0s)
//     +0x2C mpQueuedInputs       u32* (4*mnQueuedInputsCap alloc; the per-frame input ring)
//     +0x30 mpOnPressObject      ) four input-event object slots, ALL seeded to the
//     +0x34 mpOnRollOverObject   ) None/undefined sentinel (off_8324D814) by the ctor;
//     +0x38 mpInputEventObject38 ) the input layer's Set* accessors store the currently
//     +0x3C mpDragMC             ) acting movie clip / object here.
//     +0x40 mDragPos[2]          (GetDragPos returns &mDragPos; inline pos)
//     +0x48 maTail[4]            FLAG: roles TBD (not written by the ctor)
//
// Member access is BY NAME; the X360 byte offsets are documentation only (on the
// x64 PC gate the inline AptDisplayList + the Set slot pointers widen, so later
// offsets shift -- correct per the semantic-parity-by-named-members rule, no
// size assert).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptDisplayList.h"     // mDisplayList (inline, +0x20)
#include "SDKs/EATech/include/Apt/AptIntervalTimer.h"   // mpIntervalTimers element type

class AptValue;           // GC value (slots / input-event objects held as AptValue*)
struct AptActionQueueC;   // +0x0C -- the 20-byte deferred-action queue (own TU; held by ptr)

// ---------------------------------------------------------------------------
// AptAnimationTargetSet -- one of the two inline 8-byte (console) "set" sub-objects
// the ctor builds from params[2]/params[3] (sub_82AE16xx / sub_82AE17xx). Each is a
// small fixed table of AptValue* listeners/inputs: a live count, a slot capacity and
// the slot array. Proven from the count16/count16/slots access pattern the
// register/process passes use (lhz +0/+2, lwz +4). GetListenerSet/GetInputSet hand
// back the address of one of these. FLAG: the ctor/dtor helper bodies that fill the
// table live in a sibling TU (sub_82AE16xx/17xx) -- modelled here by NAMED members.
// ---------------------------------------------------------------------------
struct AptAnimationTargetSet
{
    u16        mnCount;     // +0x00  live entries used this frame
    u16        mnCapacity;  // +0x02  slot count
    AptValue** mppSlots;    // +0x04  slot array (4*mnCapacity on console)
};

struct AptAnimationTarget
{
    AptValue*             mpInputMask;          // +0x00  (ctor 0; a GC-registered handle)
    u32                   mnNumIntervalTimers;  // +0x04  (= params[0])
    u32                   mnQueuedInputsCap;    // +0x08  (= params[1]; AddInput cap)
    AptActionQueueC*      mpActionQueue;        // +0x0C
    AptAnimationTargetSet mListenerSet;         // +0x10  (built from params[2])
    AptAnimationTargetSet mInputSet;            // +0x18  (built from params[3])
    AptDisplayList        mDisplayList;         // +0x20  (inline root display list)
    AptIntervalTimer*     mpIntervalTimers;     // +0x24  (array of mnNumIntervalTimers)
    u32                   mnQueuedInputsCount;  // +0x28  (AddInput fill counter)
    u32*                  mpQueuedInputs;       // +0x2C  (4*mnQueuedInputsCap alloc)
    void*                 mpOnPressObject;      // +0x30  (SetOnPressObject; ctor sentinel)
    void*                 mpOnRollOverObject;   // +0x34  (SetOnRollOverObject; ctor sentinel)
    void*                 mpInputEventObject38; // +0x38  (ctor sentinel) FLAG: role TBD
    void*                 mpDragMC;             // +0x3C  (Get/SetDragMC; ctor sentinel)
    u32                   mDragPos[2];          // +0x40  (GetDragPos returns &mDragPos)
    u32                   maTail[4];            // +0x48..+0x54  FLAG: roles TBD

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
    void            SetQueuedInputsSize(int nSize){ mnQueuedInputsCount = (u32)nSize; }// @0x82AD4FC0

    // ---- per-frame input pump (bodies in AptAnimationTarget.cpp) ---------------
    // AddInput @0x82AD93B0 -- append one packed input event to the queued-input ring
    // (no-op when the ring is full). Returns 1 on success, 0 if full.
    int  AddInput(int nPackedInput);

    // ProcessInputs @0x82B01FD0 -- drain the queued-input ring this frame, decoding
    // each packed event and dispatching it, then reset the fill counter.
    int  ProcessInputs();

    // ProcessAptInput @0x82B01F78 -- decode one packed input word into its
    // (event-id, code, raw) fields and run it through the input + listener sets.
    int  ProcessAptInput(unsigned int nPackedInput, int bFirstThisFrame);

    // ProcessInputSet @0x82AF4478 / ProcessListenerEvents @0x82B01ED0 -- the two
    // ProcessAptInput dispatches. FLAG: bodies pending -- they walk the AptValue GC
    // tag bits of mInputSet/mListenerSet entries and call AptCIH::queueClipEvents /
    // AddListenerToQueue, which need the (sibling-owned) AptValue/AptCIH layouts
    // modelled before a faithful by-name decompile is possible. Declared so the
    // ProcessAptInput dispatcher links into the per-frame pump.
    int  ProcessInputSet(int nEventId, int nCode, unsigned int nPacked, int nSub, int bFirstThisFrame);
    int  ProcessListenerEvents(int nEventId, unsigned int nCode, int nPacked, int nSub);

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
