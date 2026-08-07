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
//     +0x40 mConstrain[4]        the drag constrain rect L/T/R/B (StartDragMovie
//                                  @0x82B03B40.. seeds -9999 into all four)
//     +0x50 mGrabOffset[2]        the drag cursor->clip grab offset X/Y (seeded 0)
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
class AptActionQueueC;    // +0x0C -- the deferred-action queue (AptActionQueue.h; held by ptr)

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

// ---------------------------------------------------------------------------
// AptAnimationTargetParams -- the small sizing struct AptTarget::ctor hands the
// director's ctor (X360: a contiguous dword array; the ctor reads words 0/1/2/3/5).
// Modelled as a named struct so the ctor reads by name. Word 4 is not consumed by
// the director ctor (FLAG: role TBD -- read by another AptTarget setup step).
// ---------------------------------------------------------------------------
struct AptAnimationTargetParams
{
    // The console block is a contiguous DWORD array -- each param occupies a full
    // word (the set helpers consume only the low u16 OF their word). The prior
    // u16+u16 packing of [2]/[3] collapsed them into one word and shifted every
    // later field down one: mnActionQueueCap read word[4] (== 0) instead of
    // word[5] (== 512) -> a ZERO-CAPACITY action queue whose enqueue walked off
    // its 0-slot ring allocation (heap corruption). Fixed 2026-07-01.
    u32 mnNumIntervalTimers;  // [0] -> mnNumIntervalTimers
    u32 mnQueuedInputsCap;    // [1] -> mnQueuedInputsCap
    u32 mnListenerSetSize;    // [2] -> mListenerSet capacity (helper reads the low u16)
    u32 mnInputSetSize;       // [3] -> mInputSet capacity   (helper reads the low u16)
    u32 mnReserved4;          // [4]  FLAG: not read by the director ctor
    u32 mnActionQueueCap;     // [5] -> AptActionQueueC capacity
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
    void*                 mpTopMostSprite;      // +0x38  (ctor sentinel) XB1-NAMED: GetTopMostSprite
                                                //        (?GetTopMostSprite@AptAnimationTarget reads x64 +0x68,
                                                //        the widened twin of this slot; was "role TBD")
    void*                 mpDragMC;             // +0x3C  (Get/SetDragMC; ctor sentinel)
    f32                   mConstrain[4];        // x64 +0x78  drag constrain L/T/R/B (was mDragPos+maTail,
    f32                   mGrabOffset[2];       // x64 +0x88  drag grab offset X/Y     roles resolved 2026-07-02)

    // Layout pinned against the x64 XB1 export (never called; member body gives
    // complete-class offsetof context). All 16 members land on the x64 accessor
    // offsets; sizeof 0x90 (pool-alloc 144 at sub_140827500/sub_140827670).
    static void _AssertLayout()
    {
        static_assert(offsetof(AptAnimationTarget, mpActionQueue)       == 0x10, "x64 ?GetActionPool: mov rax,[rcx+10h]");
        static_assert(offsetof(AptAnimationTarget, mListenerSet)        == 0x18, "x64 ?GetListenerSet: lea rax,[rcx+18h]");
        static_assert(offsetof(AptAnimationTarget, mInputSet)           == 0x28, "x64 ?GetInputSet: lea rax,[rcx+28h]");
        static_assert(offsetof(AptAnimationTarget, mDisplayList)        == 0x38, "x64 ?GetDisplayList: lea rax,[rcx+38h]");
        static_assert(offsetof(AptAnimationTarget, mpIntervalTimers)    == 0x40, "x64 ?GetIntervalTimers: mov rax,[rcx+40h]");
        static_assert(offsetof(AptAnimationTarget, mnQueuedInputsCount) == 0x48, "x64 ?GetQueuedInputsSize: mov eax,[rcx+48h]");
        static_assert(offsetof(AptAnimationTarget, mpOnPressObject)     == 0x58, "x64 ?GetOnPressObject: [rcx+58h]");
        static_assert(offsetof(AptAnimationTarget, mpDragMC)            == 0x70, "x64 ?GetDragMC: [rcx+70h]");
        static_assert(offsetof(AptAnimationTarget, mConstrain)          == 0x78, "x64 ?GetDragPos: lea rax,[rcx+78h]");
        static_assert(offsetof(AptAnimationTarget, mGrabOffset)         == 0x88, "x64 StartDragMovie 0x14081D7E0: 88h/8Ch");
        static_assert(sizeof(AptAnimationTarget) == 0x90, "x64: pool-alloc 144 before the ctor");
        static_assert(offsetof(AptAnimationTargetSet, mppSlots) == 0x08, "x64 ctor: slot ptr 8 past the count");
        static_assert(sizeof(AptAnimationTargetSet) == 0x10, "x64: the set pair spans 0x18..0x38");
    }

    // ctor @0x82AFF648 -- build the director from the sizing params: copy the timer
    // count + input cap, build the two listener/input sets, pool-allocate the action
    // queue + the interval-timer array (each AptIntervalTimer ctor'd) + the queued-
    // input ring, and seed the four input-event object slots to the None sentinel.
    AptAnimationTarget(const AptAnimationTargetParams* pParams);

    // dtor @0x82AFF790 -- free the queued-input ring, tear down the interval-timer
    // array, free the action queue (ring block + control block), destroy the root
    // display list, then the input + listener sets.
    ~AptAnimationTarget();

    // GetRootDisplayList -- the root display list the context walks (mDisplayList).
    AptDisplayList* GetRootDisplayList() { return &mDisplayList; }

    // ---- instance accessors (X360 ARTIST one-liners; offsets above) ------------
    AptDisplayList* GetDisplayList()              { return &mDisplayList; }            // @0x82AD5F18
    void*           GetListenerSet()              { return &mListenerSet; }            // @0x82AD5F08
    void*           GetInputSet()                 { return &mInputSet; }               // @0x82AD5F10
    void*           GetDragPos()                  { return &mConstrain[0]; }           // @0x82AD5F38 (the +0x40 slot)
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
    // ProcessAptInput dispatches: each walks the live AptValue* entries of
    // mInputSet / mListenerSet and routes the input event to the listed CIHs. The
    // asm only reads nCode (the event phase: 0 == rollover, 1 == press) and nPacked
    // (the packed event word) of the args ProcessAptInput passes; nEventId / nSub /
    // bFirstThisFrame are forwarded for signature parity but not re-read here.
    int  ProcessInputSet(int nEventId, int nCode, unsigned int nPacked, int nSub, int bFirstThisFrame);
    int  ProcessListenerEvents(int nEventId, unsigned int nCode, int nPacked, int nSub);

    // AddListenerToQueue @0x82B01C88 -- if pListener is an event-bearing value (an
    // AS object/CIH-none whose __proto__ chain or own hash carries one of the input
    // event handlers in nEventMask), resolve each matching handler child, (re)bind
    // its "this" to pListener (a GC-root handoff), and enqueue a deferred function
    // call for it on the action queue. Returns the last enqueue/lookup result.
    AptValue* AddListenerToQueue(AptValue* pListener, int nEventMask, int nPacked);

    // ---- deferred-action queue thunks (each tail-jumps into mpActionQueue) ------
    // The four X360 one-liners at 0x82ADC608..0x82ADC620 load mpActionQueue (+0x0C)
    // and tail-call the matching AptActionQueueC enqueue method on it.
    AptValue* AddActionBack  (const void* pEventStreamSlot, AptCIH* pCIH, s32 iContext);   // @0x82ADC608
    AptValue* AddActionFront (const void* pEventStreamSlot, AptCIH* pCIH, s32 iContext);   // @0x82ADC610
    AptValue* AddFunctionBack (AptValue* pContext, AptValue* pFuncDef,
                               s32 iReturnReg, s32 iArgCount);                  // @0x82ADC618
    AptValue* AddFunctionFront(AptValue* pContext, AptValue* pFuncDef,
                               s32 iReturnReg, s32 iArgCount);                  // @0x82ADC620

    // ---- analog-stick input (bodies in AptAnimationTarget.cpp) -----------------
    // AptAnalogInputEvent -- the 16-byte by-value analog sample AddAnalogInput takes
    // (X360 passes it in the r4:r5 register pair). One of the two axis floats is live
    // per event id; the player index + event id select the destination table.
    struct AptAnalogInputEvent
    {
        f32 mfAxis0;     // +0x00  axis-0 value (event 0x134)
        f32 mfAxis1;     // +0x04  axis-1 value (event 0x135)
        u8  mnPlayer;    // +0x08  player index (table stride 16 bytes)
        u8  mauPad[3];   // +0x09
        u32 mnEventId;   // +0x0C  0x134/0x135 analog axes; 0x1F5/0x1F6 stick snapshots
    };

    // AddAnalogInput @0x82ADEA28 -- record one analog sample. For an axis event
    // (0x134/0x135) the live float is stored into the per-player axis table (the other
    // axis zeroed) and, when it is non-zero + the recorder is on, the event is logged;
    // for a stick-snapshot event (0x1F5/0x1F6) the whole 16-byte sample is copied into
    // the per-player analog-stick table. When any of those wrote, a packed analog input
    // (event id + player) is appended to the queued-input ring (then logged). Returns
    // the AddInput result, or 0 when nothing was queued.
    int  AddAnalogInput(const AptAnalogInputEvent& rEvent);

    // ---- per-frame action / timer drains (bodies in AptAnimationTarget.cpp) ----
    // RunActions @0x82B0C9B0 -- drain the deferred action queue: clean the remove
    // list, then for each queued slot run the clip-event action stream / function
    // call through the AptActionInterpreter, ticking newly-created instances; finish
    // by ticking new insts, clearing the queue and cleaning the remove list again.
    int  RunActions();

    // TickIntervalTimers @0x82AEAD00 -- advance every armed interval timer by the
    // frame delta; when one elapses, fire its callback (pushing the saved param
    // values) and either reschedule (setInterval) or tear it down (setTimeout).
    int  TickIntervalTimers(int nDeltaMs);

    // TickNewInsts @0x82B0C8E0 -- tick + release every entry on the shared
    // "new instance" table (CIHs created this frame), then clear the table.
    static void TickNewInsts();

    // RemoveTimerFunctions @0x82AE4320 -- tear down every interval timer whose
    // callback is bound to (or owned by) the given CIH context (used when a CIH that
    // owns timers is cleared / its character instance swapped).
    int  RemoveTimerFunctions(AptCIH* pContext);

    // CleanRemList @0x82AEAB08 -- flush the shared delayed-release list: for each
    // queued value run its GC teardown (PreDestroy/DestroyGCPointers) and either free
    // it now or, when it survives, replace its references and delete it.
    static void CleanRemList();

    // ---- GC mark / reference passes (bodies in AptAnimationTarget.cpp) ---------
    // RegisterReferences @0x82ADEC00 -- register every AptValue* this director (and
    // the shared new-inst / delayed-release tables) holds with the GC collector so
    // the held graph survives a collection.
    void RegisterReferences();

    // RemoveCIHReferences @0x82ADEEA0 -- the ReplaceReferences callback: re-register
    // (remap) every held AptValue* and scrub the interval-timer param slots that
    // point at the value being replaced.
    void* RemoveCIHReferences();

    // PreDestroy @0x82AFE420 -- shutdown teardown: release every armed interval
    // timer's callback/context values + drain their param stacks, release every
    // listener-set entry, then PreDestroy the root display list.
    void PreDestroy();

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

    // ---- per-player analog-stick table accessors (X360 one-liners) -------------
    // GetAStickLeft @0x82AD5F50 / GetAStickRight @0x82AD5F68 -- the address of player
    // nPlayer's 16-byte left/right analog-stick snapshot (the tables AddAnalogInput's
    // 0x1F5/0x1F6 cases write; stride 16 bytes / player). Static: the tables are the
    // shared per-player arrays gAptAStickLeft/gAptAStickRight.
    static char* GetAStickLeft (int nPlayer);             // @0x82AD5F50
    static char* GetAStickRight(int nPlayer);             // @0x82AD5F68
};

// ---------------------------------------------------------------------------
// MakeAptAnimationTarget -- the AptAnimationTarget::AptAnimationTarget ctor @0x82AFF648
// wrapped as the AptTarget::AptTarget factory: placement-construct the 88-byte director
// over a pool-allocated block from the raw AptUpdateParams dword array (reinterpreted to
// AptAnimationTargetParams). Returns the constructed director (the ctor's `this`). Body
// in AptAnimationTarget.cpp (next to the ctor it wraps).
// ---------------------------------------------------------------------------
AptAnimationTarget* MakeAptAnimationTarget(void* pMem, const u32* pParams);
