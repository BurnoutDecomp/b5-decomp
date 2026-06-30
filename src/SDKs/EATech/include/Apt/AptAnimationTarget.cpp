// ===========================================================================
// EATech Apt -- AptAnimationTarget class-static data layer.
//
// A small set of FILE-STATIC tables the Apt input/update layer shares across all
// animation targets (X360 globals dword_8324E534..dword_8324E550), plus the
// static accessors over them. Reconstructed from the X360 ARTIST pseudocode/asm
// (SetupStaticData @0x82AE41F0 builds them, CleanupStaticData @0x82AE42A8 frees
// them) via the decompile->verify workflow.
//
// (The per-player analog-stick tables saAStickLeft/Right @unk_8324D750/unk_8324E2D8
// are a follow-on -- their fixed element bound is not yet determined.)
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"

#include "SDKs/EATech/include/Apt/AptActionQueue.h"          // AptActionQueueC enqueue/clear/GC
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"        // AptValue (GC tag bits + virtuals)
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"  // AptIntervalTimer::mParams item access
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // AptCIH (queued action / event target)
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"      // mDisplayList head GC mark walk
#include "SDKs/EATech/Apt/DogmaAllocator.h"   // DOGMA_PoolManager::Allocate/Deallocate

#include <cstring>   // memset
#include <cstdint>   // intptr_t (the X360 fastcall `return r3` / array-cookie math)
#include <new>       // placement new (inline-construct the queue / timers / display list)

// The shared Apt fixed-size pool (X360 off_8324D808); defined by the Apt pseudo-data
// layer (same handle AptActionQueue / AptCharacterSpriteInstBase allocate from).
extern DOGMA_PoolManager* gpAptPseudoDataPool;   // off_8324D808

// ---------------------------------------------------------------------------
// FLAG (deferred input-recorder subsystem): the Apt input recorder the X360 feeds
// every accepted input/analog event so a session can be replayed. dword_8324E518 is
// the "recording enabled" flag, dword_8324E830 the record-sink callback invoked as
// fn(pBigEndianRecord, nBytes), and dword_8324D820 the record-tag word stamped into
// the leading dword. The byte/halfword reversal AddInput performs is the X360's
// big-endian record serialization (reproduced verbatim); the sink itself is a
// not-yet-homed debug subsystem, declared here as opaque externs.
// ---------------------------------------------------------------------------
extern int  gAptInputRecorderEnabled;                    // dword_8324E518
extern int  (*gpAptInputRecorderSink)(void*, int);       // dword_8324E830
extern u32  gAptInputRecorderTag;                         // dword_8324D820

namespace
{
    // X360 globals dword_8324E534..dword_8324E550 (the shared static-data layer).
    int   ssMousePosX             = 0;        // dword_8324E534 -- cursor X fed to AS
    int   ssMousePosY             = 0;        // dword_8324E538 -- cursor Y fed to AS
    void* spStaticBlock           = nullptr;  // dword_8324E53C -- 80-byte scratch block
    int   snMaxNewMovieClips      = 0;        // dword_8324E540 -- table element count
    void* spNewInsts              = nullptr;  // off_8324E544   -- new-instance table (4*count)
    int   snNewInstSize           = 0;        // dword_8324E548 -- new-instance fill counter
    void* spDelayedReleaseList    = nullptr;  // off_8324E54C   -- delayed-release table (4*count)
    int   snDelayedReleaseListSize = 0;       // dword_8324E550 -- delayed-release fill counter
}

// ---- class-static accessors (X360 one-liners over the globals above) -----------
int   AptAnimationTarget::GetXMousePos()             { return ssMousePosX; }              // @0x82AD5F98
int   AptAnimationTarget::GetYMousePos()             { return ssMousePosY; }              // @0x82AD5FA8
int   AptAnimationTarget::GetMaxNewMovieClips()      { return snMaxNewMovieClips; }       // @0x82AD5E90
void* AptAnimationTarget::GetNewInsts()              { return spNewInsts; }               // @0x82AD5EA0
int   AptAnimationTarget::GetNewInstSize()           { return snNewInstSize; }            // @0x82AD5EB0
void* AptAnimationTarget::GetDelayedReleaseList()    { return spDelayedReleaseList; }     // @0x82AD5ED8
int   AptAnimationTarget::GetDelayedReleaseListSize(){ return snDelayedReleaseListSize; } // @0x82AD5EE8

// DecNewInstSize @0x82AD5EC0 -- despite the name, hands out the next new-instance
// index and POST-INCREMENTS the fill counter (returns the pre-increment value).
int   AptAnimationTarget::DecNewInstSize()           { return snNewInstSize++; }

// SetupStaticData @0x82AE41F0 -- allocate the shared static tables, sized to the
// max-new-movie-clips count. (The X360 reads the count from its params struct at
// +0x18; taken here as the explicit count so no opaque param layout is needed.)
void AptAnimationTarget::SetupStaticData(int nMaxNewMovieClips)
{
    spStaticBlock = gpAptPseudoDataPool->Allocate(80);
    memset(spStaticBlock, 0, 80);

    snMaxNewMovieClips   = nMaxNewMovieClips;
    spNewInsts           = gpAptPseudoDataPool->Allocate(4 * nMaxNewMovieClips);
    spDelayedReleaseList = gpAptPseudoDataPool->Allocate(4 * nMaxNewMovieClips);
    memset(spDelayedReleaseList, 0, 4 * nMaxNewMovieClips);

    snNewInstSize            = 0;
    snDelayedReleaseListSize = 0;
}

// CleanupStaticData @0x82AE42A8 -- free the three tables SetupStaticData built.
void AptAnimationTarget::CleanupStaticData()
{
    gpAptPseudoDataPool->Deallocate(spNewInsts, 4 * snMaxNewMovieClips);
    gpAptPseudoDataPool->Deallocate(spDelayedReleaseList, 4 * snMaxNewMovieClips);
    gpAptPseudoDataPool->Deallocate(spStaticBlock, 80);
}

// ===========================================================================
// Per-frame input pump (the queued-input ring at mnQueuedInputsCount/mpQueuedInputs).
// ===========================================================================

// AddInput @0x82AD93B0 -- append one packed input event to the queued-input ring.
// No-op (returns 0) when the ring is at capacity; otherwise stores the event, bumps
// the fill counter, optionally streams it to the input recorder, and returns 1.
int AptAnimationTarget::AddInput(int nPackedInput)
{
    if (mnQueuedInputsCount >= mnQueuedInputsCap)   // ring full (count >= cap) -> reject
        return 0;

    mpQueuedInputs[mnQueuedInputsCount] = (u32)nPackedInput;   // *(4*count + buf) = event
    ++mnQueuedInputsCount;

    if (gAptInputRecorderEnabled)
    {
        // FLAG (deferred recorder): serialise the event big-endian and hand it to the
        // record sink. The X360 builds an 8-byte record { tag, byteReversed(event) }
        // via the inline XOR byte-swap + halfword-swap (net: a full 32-bit byte
        // reversal); reproduced here as that reversal.
        const u32 ev = (u32)nPackedInput;
        const u32 evBE = ((ev & 0x000000FFu) << 24)
                       | ((ev & 0x0000FF00u) << 8)
                       | ((ev & 0x00FF0000u) >> 8)
                       | ((ev & 0xFF000000u) >> 24);
        u32 record[2];
        record[0] = gAptInputRecorderTag;
        record[1] = evBE;
        gpAptInputRecorderSink(record, 8);
    }
    return 1;
}

// ProcessAptInput @0x82B01F78 -- decode one packed input word and run it through the
// input set then the listener set. The word packs: event-id in bits [31..17], a 7-bit
// code in bits [16..10] and an 8-bit sub field in bits [9..2].
int AptAnimationTarget::ProcessAptInput(unsigned int nPackedInput, int bFirstThisFrame)
{
    const int nEventId = (int)(nPackedInput >> 17);          // srwi 17
    const int nCode    = (int)((nPackedInput >> 10) & 0x7F); // extrwi 7,15
    const int nSub     = (int)((nPackedInput >> 2) & 0xFF);  // extrwi 8,22

    ProcessInputSet(nEventId, nCode, nPackedInput, nSub, bFirstThisFrame);
    return ProcessListenerEvents(nEventId, (unsigned int)nCode, (int)nPackedInput, nSub);
}

// ProcessInputs @0x82B01FD0 -- drain the queued-input ring this frame: dispatch each
// packed event (the first event of the frame flagged), then reset the fill counter.
int AptAnimationTarget::ProcessInputs()
{
    int result = 0;
    int nIndex = 0;   // v2/r30: 0-based event ordinal (drives the "first this frame" flag)
    if ((int)mnQueuedInputsCount > 0)
    {
        do
        {
            // bFirstThisFrame == (v2++ == 0); the X360 forms it via cntlzw/extrwi.
            result = ProcessAptInput(mpQueuedInputs[nIndex], nIndex == 0 ? 1 : 0);
            ++nIndex;
        }
        while (nIndex < (int)mnQueuedInputsCount);
    }
    mnQueuedInputsCount = 0;
    return result;
}

// ===========================================================================
//  The timer / listener / GC / queue machinery (ctor/dtor + per-frame drains +
//  GC mark passes + event dispatch). All bodies below are reconstructed from
//  BURNOUT_X360_ARTIST.XEX (no DWARF / Feb-2007 source for this TU). Where a
//  callee is itself un-homed (the interpreter execution core, the GC pool helper,
//  the input-set Set sub-object ctor/dtor helpers, a few module statics) it is
//  declared as an extern WITH a // FLAG, per the project's un-homed-callee rule.
// ===========================================================================

// The None/undefined sentinel value the input-event slots + interval-timer
// "no explicit context" fields are seeded to (X360 off_8324D814). Shared, GC-owned.
extern AptValue* gpAptNoneValue;   // off_8324D814

// ---------------------------------------------------------------------------
// FLAG (sibling-owned: the AptAnimationTargetSet ctor/dtor helpers). The two inline
// listener/input "set" sub-objects are built + torn down by these unnamed X360 subs
// (sub_82AE16xx build the table, sub_82AE17xx free it). They take the set sub-object
// + a u16 capacity. Declared as externs so the director ctor/dtor wire them by name;
// bodied when that small Set TU is homed.
// ---------------------------------------------------------------------------
extern void AptAnimationTargetSet_Construct(AptAnimationTargetSet* pSet, u16 nCapacity);   // sub_82AE16xx (build)
extern void AptAnimationTargetSet_Destruct (AptAnimationTargetSet* pSet);                  // sub_82AE1670 (listener free)
extern void AptAnimationTargetSet_Destruct2(AptAnimationTargetSet* pSet);                  // sub_82AE1780 (input free)

// ---------------------------------------------------------------------------
// FLAG (un-homed GC / interpreter cluster): the module-static interpreter state +
// helper subsystems the per-frame drains thread through. Each is declared with its
// X360 symbol so the bodies below stay faithful; they are owned by the (not-yet-
// homed) AptActionInterpreter / AptGC TUs.
// ---------------------------------------------------------------------------
extern int   AptReplaceReferences(AptValue* pOld, AptValue* pNew,
                                  AptValue** ppTable, int nCount);                  // ReplaceReferences
extern AptValue* AptUpdateZombieVector(char bClear);                               // AptUpdateZombieVector
extern void  AptValue_setGCRoot(AptValue* pValue, int bRoot);                      // AptValue::setGCRoot (free-fn form)

// The GC value pool: GetAllAllocatedAptValues snapshots the live-value table the
// remove-list flush remaps references against; the table's element count lives at
// +0x28 of the pool object. Declared opaque (the pool layout is its own TU).
extern void** AptValueGC_PoolManager_GetAllAllocatedAptValues(void* pPool);        // ...::GetAllAllocatedAptValues
extern void*  gpAptValueGCPool;                                                    // off_8324D834
extern int    AptValueGCPool_GetAllocatedCount(void* pPool);                       // *(pool + 0x28)
extern void (*gpAptGCTableFree)(void* p, unsigned nBytes);                         // dword_8324E820 (frees the snapshot)

// FLAG (un-homed AptCIH behavioural TU): per-frame tick of a freshly-created node.
// AptCIH::tick @0x... is behavioural surface owned by AptCIHBehaviour.cpp; declared
// as a free-function shim (the X360 calls it with the CIH in r3) so TickNewInsts links.
extern void AptCIH_tick(AptCIH* pCIH);

// FLAG (un-homed AptCIH behavioural TU): queue a clip-event against a CIH. The X360
// calls AptCIH::queueClipEvents(pCIH, nEventMask, nPacked, bDeferred) -> AptValue*;
// declared as a free-function shim so the two input dispatchers (ProcessInputSet /
// AddListenerToQueue path) link. Its body belongs with AptCIH's event machinery.
extern AptValue* AptCIH_queueClipEvents(AptValue* pCIH, int nEventMask,
                                        unsigned int nPacked, int bDeferred);

// ---------------------------------------------------------------------------
// FLAG (un-homed AS event-descriptor rodata + name table). AddListenerToQueue walks
// a table of {eventMask, nameIndex} pairs (X360 unk_82F7334C..dword_82F73380): for
// each descriptor whose mask intersects the dispatched event, it looks up the named
// handler child via the AS-name table (dword_8324E580 -- an EAStringC array indexed
// by nameIndex). Both arrays live in the interpreter's static-data TU; declared here
// as opaque externs so the by-name walk links. The descriptor pair stride is 8 bytes
// (mask at +0, nameIndex at +4) -- the X360 `v15 += 2` (two dwords) step.
// ---------------------------------------------------------------------------
struct AptListenerEventDescriptor
{
    int nEventMask;    // +0x00 (the X360 `*(v15 - 1)` mask, read one dword before the index)
    int nNameIndex;    // +0x04 (the AS-name table index `*v15`)
};
extern const AptListenerEventDescriptor gAptListenerEventDescriptors[];   // unk_82F7334C.. (mask,index pairs)
extern const int gAptListenerEventDescriptorCount;                        // (82F73380 - 82F73350)/8 + 1
extern const class EAStringC gAptASNameTable[];                           // dword_8324E580

// ---------------------------------------------------------------------------
// FLAG (un-homed per-player analog tables, X360 rodata/bss). The analog-input axis
// values + the raw stick snapshots are kept in fixed per-player tables (stride 16
// bytes / player). flt_8324E200/flt_8324E204 hold the two axis floats; unk_8324D750/
// unk_8324E2D8 hold the left/right 16-byte stick samples. Their fixed element bound
// is not yet recovered (noted in AptAnimationTarget.h), so they are declared as
// opaque byte tables indexed by 16*player, matching the X360 stride.
// ---------------------------------------------------------------------------
extern f32 gAptAnalogAxis0[];    // flt_8324E200 (player stride 16 bytes)
extern f32 gAptAnalogAxis1[];    // flt_8324E204 (player stride 16 bytes)
extern u8  gAptAStickLeft[];     // unk_8324D750 (player stride 16 bytes)
extern u8  gAptAStickRight[];    // unk_8324E2D8 (player stride 16 bytes)

// ---------------------------------------------------------------------------
// ctor @ 0x82AFF648
// ---------------------------------------------------------------------------
AptAnimationTarget::AptAnimationTarget(const AptAnimationTargetParams* pParams)
{
    mnNumIntervalTimers = pParams->mnNumIntervalTimers;   // a1[1] = *a2
    mnQueuedInputsCap   = pParams->mnQueuedInputsCap;     // a1[2] = a2[1]

    AptAnimationTargetSet_Construct(&mListenerSet, pParams->mnListenerSetSize);  // __(a1+4, a2[2])
    AptAnimationTargetSet_Construct(&mInputSet,    pParams->mnInputSetSize);     // sub_82AE1708(a1+6, a2[3])

    // The root display list (a1+0x20) is the inline member mDisplayList; its ctor
    // (AptDisplayList::AptDisplayList) is invoked automatically as a member
    // construction here, matching the X360's explicit call at this sequence point.

    // Action queue: pool-allocate the 20-byte control block then placement-construct
    // it with the requested capacity (null on allocation failure).
    void* lpQueueMem = gpAptPseudoDataPool->Allocate(sizeof(AptActionQueueC));   // Allocate(off_8324D808, 20)
    mpActionQueue = lpQueueMem
                  ? new (lpQueueMem) AptActionQueueC(pParams->mnActionQueueCap)  // a2[5]
                  : nullptr;

    // Interval-timer array: one pool block with a leading count cookie (a (36*N + 4)-
    // byte alloc, clamped on overflow), each element placement-ctor'd. The block
    // leads with the byte size and the element count, then the N timers.
    // FLAG (x64 widening): the X360 stride is the console 36; here the genuine PC
    // element stride is sizeof(AptIntervalTimer). The block geometry (size dword then
    // count dword then the elements) and the overflow clamp are reproduced faithfully.
    const u32 lnCount = mnNumIntervalTimers;
    s32 liBlockSize = static_cast<s32>(sizeof(AptIntervalTimer) * lnCount);
    if (lnCount > 0x71C71C7u)                       // 36 * lnCount would overflow
    {
        liBlockSize = -1;
    }
    // X360: v8 = 36*N (payload) [c:console 36 -> x64 sizeof]; v7 = v8 + 4 (size cookie);
    // alloc = v7 + 4 = payload + 8 (room for BOTH the size + count header dwords).
    s32* lpBlock = static_cast<s32*>(
        gpAptPseudoDataPool->Allocate(static_cast<u32>(liBlockSize) + 8u));
    *lpBlock = liBlockSize + 4;                      // size cookie = v7 = payload + 4
    if (reinterpret_cast<intptr_t>(lpBlock) == -4)   // Allocate failed (addic. r3,4 == 0)
    {
        mpIntervalTimers = nullptr;
    }
    else
    {
        s32* lpCountSlot = lpBlock + 1;
        *lpCountSlot = static_cast<s32>(lnCount);    // new[] count cookie
        AptIntervalTimer* lpTimers =
            reinterpret_cast<AptIntervalTimer*>(lpCountSlot + 1);
        for (u32 liIndex = 0; liIndex < lnCount; ++liIndex)
        {
            new (&lpTimers[liIndex]) AptIntervalTimer();
        }
        mpIntervalTimers = lpTimers;
    }

    // Queued-input ring (4 * cap), the fill counter and the input mask.
    mpQueuedInputs      = static_cast<u32*>(
        gpAptPseudoDataPool->Allocate(4 * mnQueuedInputsCap));   // a1[11]
    mnQueuedInputsCount = 0;                                     // a1[10]
    mpInputMask         = nullptr;                               // *a1 = 0

    // Seed the four input-event object slots to the None sentinel (a1[15/12/13/14]).
    mpDragMC             = gpAptNoneValue;   // +0x3C (a1[15])
    mpOnPressObject      = gpAptNoneValue;   // +0x30 (a1[12])
    mpOnRollOverObject   = gpAptNoneValue;   // +0x34 (a1[13])
    mpInputEventObject38 = gpAptNoneValue;   // +0x38 (a1[14])
}

// ---------------------------------------------------------------------------
// dtor @ 0x82AFF790
// ---------------------------------------------------------------------------
AptAnimationTarget::~AptAnimationTarget()
{
    // Queued-input ring (4 * cap).
    gpAptPseudoDataPool->Deallocate(mpQueuedInputs, 4 * mnQueuedInputsCap);

    // Interval-timer array (the compiler array deleting destructor, array + free).
    if (mpIntervalTimers != nullptr)
    {
        AptIntervalTimer::_vector_deleting_destructor_(mpIntervalTimers, 3);
    }

    // Action queue: free its ring block (the leading-dword-prefixed allocation the
    // queue ctor made: *mpBegin's preceding dword holds the byte size) then the
    // 20-byte control block itself.
    AptActionQueueC* lpQueue = mpActionQueue;
    if (lpQueue != nullptr)
    {
        // X360: *v3 == mpBegin; Deallocate(pool, *v3 - 4, *(*v3 - 4) + 4).
        s32* lpRingHeader = reinterpret_cast<s32*>(lpQueue->mpBegin) - 1;
        gpAptPseudoDataPool->Deallocate(lpRingHeader,
                                        static_cast<u32>(*lpRingHeader) + 4u);
        gpAptPseudoDataPool->Deallocate(lpQueue, sizeof(AptActionQueueC));   // 20
    }

    // The X360 dtor calls AptDisplayList::~AptDisplayList(a1+0x20) here; mDisplayList
    // is an inline member with a user dtor, so its destruction is emitted
    // automatically at scope exit (a benign reorder relative to the set-destructs,
    // which do not touch the display list).
    AptAnimationTargetSet_Destruct2(&mInputSet);     // sub_82AE1780(a1+0x18)
    AptAnimationTargetSet_Destruct (&mListenerSet);  // sub_82AE1670(a1+0x10)
}

// ---------------------------------------------------------------------------
// PreDestroy @ 0x82AFE420
//   Release each armed interval timer's callback + context values, drain its param
//   stack, null the slot; release each listener-set entry; PreDestroy the root list.
// ---------------------------------------------------------------------------
void AptAnimationTarget::PreDestroy()
{
    for (u32 liTimer = 0; liTimer < mnNumIntervalTimers; ++liTimer)
    {
        AptIntervalTimer& lrTimer = mpIntervalTimers[liTimer];
        if (lrTimer.mpActiveValue != nullptr)        // *(v3 + mpIntervalTimers) != 0
        {
            lrTimer.mpCBFunction->Release();         // (*(*v4 + 4))(v4)
            if (lrTimer.mpContext != nullptr)        // *(v3 + ... + 16)
            {
                lrTimer.mpContext->Release();
            }
            // Drain the param stack (CleanParams once per remaining value -- the X360
            // re-reads the count each pass, which pop() decrements).
            for (s32 liParam = 0; liParam < lrTimer.mParams.mnTop; ++liParam)
            {
                lrTimer.CleanParams();
            }
            lrTimer.mpActiveValue = nullptr;         // *(v3 + mpIntervalTimers) = 0
        }
    }

    if (mListenerSet.mnCapacity != 0)                // *(a1 + 18) (mListenerSet.mnCapacity)
    {
        for (u32 liSlot = 0; liSlot < mListenerSet.mnCapacity; ++liSlot)
        {
            if (mListenerSet.mppSlots[liSlot] != nullptr)
            {
                mListenerSet.mppSlots[liSlot]->Release();   // (*(**(...) + 4))(...)
                mListenerSet.mppSlots[liSlot] = nullptr;
            }
        }
    }

    mDisplayList.PreDestroy();   // AptDisplayList::PreDestroy(a1 + 32)
}

// ===========================================================================
//  Deferred-action queue thunks (each tail-jumps into mpActionQueue) @0x82ADC608..
// ===========================================================================
AptValue* AptAnimationTarget::AddActionBack(s32 iEventId, AptCIH* pCIH, s32 iContext)
{
    return mpActionQueue->AddActionBack(iEventId, pCIH, iContext);
}

AptValue* AptAnimationTarget::AddActionFront(s32 iEventId, AptCIH* pCIH, s32 iContext)
{
    return mpActionQueue->AddActionFront(iEventId, pCIH, iContext);
}

AptValue* AptAnimationTarget::AddFunctionBack(AptValue* pContext, AptValue* pFuncDef,
                                              s32 iReturnReg, s32 iArgCount)
{
    return mpActionQueue->AddFunctionBack(pContext, pFuncDef, iReturnReg, iArgCount);
}

AptValue* AptAnimationTarget::AddFunctionFront(AptValue* pContext, AptValue* pFuncDef,
                                               s32 iReturnReg, s32 iArgCount)
{
    return mpActionQueue->AddFunctionFront(pContext, pFuncDef, iReturnReg, iArgCount);
}

// ---------------------------------------------------------------------------
// RemoveTimerFunctions @ 0x82AE4320
//   Tear down every armed interval timer whose callback function is a script
//   function (type tags 34..36, defined) owned by -- or unowned but to be claimed by
//   -- the given CIH context.
// ---------------------------------------------------------------------------
int AptAnimationTarget::RemoveTimerFunctions(AptCIH* pContext)
{
    for (u32 liTimer = 0; liTimer < mnNumIntervalTimers; ++liTimer)
    {
        AptIntervalTimer& lrTimer = mpIntervalTimers[liTimer];
        if (lrTimer.mpActiveValue == nullptr)
        {
            continue;                                  // *v6 == 0 -> free slot
        }
        // X360: r8 = *(pContext + 0x20) (the CIH's character-instance); the body is
        // gated on it, and the function's owner is matched against it (NOT against
        // pContext itself). FLAG: read through the recovered +0x20 offset.
        void* lpGate = *reinterpret_cast<void**>(reinterpret_cast<char*>(pContext) + 0x20);
        if (lpGate == nullptr)
        {
            continue;
        }

        AptValue* lpFunc = lrTimer.mpCBFunction;       // v8 = v6[1]
        // The callback is a defined script function (tag in {34,35,36}, mbIsDefined).
        // X360: ((type - 34) <= 2) && ((bitfield >> 27) & 1).
        const AptVirtualFunctionTable_Indices leType = lpFunc->getVtblIndex();
        const bool lbIsScriptFn =
            (static_cast<u32>(leType) - AptVFT_ScriptFunction1) <= 2u
            && lpFunc->getIsDefined();
        if (!lbIsScriptFn)
        {
            continue;
        }

        // The function's owning CIH context (its [+0x20] value's [+0x20]); when the
        // function is unowned (0) or owned by pContext, this timer is torn down.
        // FLAG (un-homed value layout): the script-function value's owner pointer at
        // word +0x20 of the AptValue is interpreter-private; read through the recovered
        // offset (the AptScriptFunction value type is not yet modelled by name).
        void* lpFuncOwner =
            *reinterpret_cast<void**>(
                reinterpret_cast<char*>(
                    *reinterpret_cast<void**>(reinterpret_cast<char*>(lpFunc) + 0x20))
                + 0x20);
        if (lpFuncOwner != nullptr && lpFuncOwner != lpGate)   // funcOwner vs *(pContext+0x20)
        {
            continue;
        }

        lrTimer.mpCBFunction->Release();               // (*(*v13 + 4))(v13)
        if (lrTimer.mpContext != nullptr)
        {
            lrTimer.mpContext->Release();
        }
        lrTimer.CleanParams();
        lrTimer.mpActiveValue = nullptr;
    }
    return reinterpret_cast<intptr_t>(this);           // X360 returns r3 (this) unchanged
}

// ---------------------------------------------------------------------------
// TickNewInsts @ 0x82B0C8E0
//   Tick + release every entry on the shared "new instance" table (CIHs created this
//   frame), then clear the table. Static: operates entirely on the module statics.
// ---------------------------------------------------------------------------
void AptAnimationTarget::TickNewInsts()
{
    AptValue** lpInsts = static_cast<AptValue**>(spNewInsts);   // off_8324E544
    for (int liIndex = 0; liIndex < snNewInstSize; ++liIndex)   // dword_8324E548
    {
        AptCIH* lpCIH = reinterpret_cast<AptCIH*>(lpInsts[liIndex]);
        if (lpCIH == nullptr)
        {
            continue;
        }

        // Tick the node only when it is a sprite(5) / custom-control(16) character
        // instance still at the "freshly created" depth sentinel (-1).
        // FLAG (un-homed AptCharacterInst layout): the type tag (charInst[+8] >> 26)
        // and the create-depth (charInst[+16]) are read through the recovered offsets
        // off the CIH's character instance; AptCharacterInst is not modelled by name.
        void* lpCharInst =
            *reinterpret_cast<void**>(reinterpret_cast<char*>(lpCIH) + 0x20);  // mpCharacterInst (CIH word 8)
        const int liTypeTag =
            *reinterpret_cast<int*>(reinterpret_cast<char*>(lpCharInst) + 8) >> 26;
        if ((liTypeTag == 5 || liTypeTag == 16)
            && *reinterpret_cast<int*>(reinterpret_cast<char*>(lpCharInst) + 16) == -1)
        {
            AptCIH_tick(lpCIH);                                 // AptCIH::tick
            lpInsts = static_cast<AptValue**>(spNewInsts);     // reload (tick may realloc)
        }

        if (lpInsts[liIndex] != nullptr)
        {
            lpInsts[liIndex]->Release();                       // (*(*v9 + 4))(v9)
            lpInsts = static_cast<AptValue**>(spNewInsts);
        }
        lpInsts[liIndex] = nullptr;
    }
    snNewInstSize = 0;
}

// ---------------------------------------------------------------------------
// CleanRemList @ 0x82AEAB08
//   Flush the shared delayed-release list: for each queued GC value (a defined CIH /
//   CIH-none not already in the deferred vector and not a None), run its GC teardown
//   (PreDestroy/DestroyGCPointers) and either ForceDelete it now or, if it survived
//   (still has a non-"to-be-deleted" GC tag), re-queue it for a reference-remap pass.
// ---------------------------------------------------------------------------
void AptAnimationTarget::CleanRemList()
{
    AptValue** lpList = static_cast<AptValue**>(spDelayedReleaseList);   // off_8324E54C
    int liSurvivors = 0;

    for (int liIndex = 0; liIndex < snDelayedReleaseListSize; ++liIndex)  // dword_8324E550
    {
        AptValue* lpValue = lpList[liIndex];
        lpList[liIndex] = nullptr;
        if (lpValue == nullptr)
        {
            continue;
        }

        // Only act on a defined CIH / CIH-none value that is not a "None" (type 3) and
        // whose CIH state (mFlagsA bits 29-30) is not the protected 0x20000000 form.
        const AptVirtualFunctionTable_Indices leType = lpValue->getVtblIndex();
        const bool lbIsCIH =
            (leType == AptVFT_CharacterInstHandle || leType == AptVFT_CIHNone);
        if (!lbIsCIH
            || (reinterpret_cast<AptCIH*>(lpValue)->mFlagsA & 0x60000000u) == 0x20000000u  // *(v4+12)
            || leType == AptVFT_None)
        {
            continue;
        }

        // The packed "delayed-deletion / refcount-class" field (bits [25..14]); 0 or 1
        // are handled by a straight ForceDelete, anything higher runs the full teardown.
        const u32 lnDelClass = (lpValue->mnValueData >> 14) & 0xFFFu;
        if (lnDelClass <= 1u)
        {
            if (lnDelClass == 1u)
            {
                lpValue->Release();   // LABEL_18: (*(*v4 + 4))(v4)
            }
            continue;
        }

        lpValue->PreDestroy();        // (*(*v4 + 36))(v4)
        lpValue->DestroyGCPointers(); // (*(*v4 + 40))(v4)

        // Re-test after teardown: still a CIH-family value?
        const AptVirtualFunctionTable_Indices leType2 = lpValue->getVtblIndex();
        if (leType2 != AptVFT_CharacterInstHandle && leType2 != AptVFT_CIHNone)
        {
            continue;
        }

        if ((lpValue->mnValueData & 0x3FFC000u) != 0x4000u)   // GC-root tag != survives
        {
            ++liSurvivors;
            lpList[liSurvivors - 1] = lpValue;                // compact survivors to the front
        }
        else
        {
            lpValue->Release();   // LABEL_18 fallthrough: (*(*v4 + 4))(v4)
        }
    }

    // Second pass: remap each survivor's references against the live-value snapshot,
    // then free or delete it.
    if (liSurvivors > 0)
    {
        AptValue** lpAllValues =
            reinterpret_cast<AptValue**>(
                AptValueGC_PoolManager_GetAllAllocatedAptValues(gpAptValueGCPool));
        const int liAllCount = AptValueGCPool_GetAllocatedCount(gpAptValueGCPool);  // *(pool + 0x28)

        for (int liIndex = 0; liIndex < liSurvivors; ++liIndex)
        {
            AptValue* lpValue = lpList[liIndex];
            lpList[liIndex] = nullptr;
            AptReplaceReferences(lpValue, nullptr, lpAllValues, liAllCount);

            if ((lpValue->mnValueData & 0x3FFC000u) == 0x4000u)
            {
                lpValue->Release();      // (*(vtbl + 4))(v16)
            }
            else
            {
                lpValue->ForceDelete();  // (*(vtbl + 44))(v16)
            }
        }

        if (lpAllValues != nullptr)
        {
            gpAptGCTableFree(lpAllValues, 4 * liAllCount);   // dword_8324E820
        }
    }

    snDelayedReleaseListSize = 0;
}

// ---------------------------------------------------------------------------
// RegisterReferences @ 0x82ADEC00
//   GC mark walk: register every AptValue* this director (and the shared new-inst +
//   delayed-release tables) holds with the collector under its debug name, so the
//   held value graph survives a collection.
// ---------------------------------------------------------------------------
void AptAnimationTarget::RegisterReferences()
{
    AptValue::ReferenceRegistrationCb lpCb = AptValue::sReferenceRegistrationCb;

    if (mpInputMask != nullptr)
    {
        lpCb(nullptr, &mpInputMask, "AptAnimationTarget::mpInputMask", 1);
    }

    AptValue** lpNewInsts = static_cast<AptValue**>(spNewInsts);   // off_8324E544
    for (int liIndex = 0; liIndex < snNewInstSize; ++liIndex)      // dword_8324E548
    {
        if (lpNewInsts[liIndex] != nullptr)
        {
            lpCb(nullptr, &lpNewInsts[liIndex], "AptAnimationTarget::mapNewInsts", 1);
            lpNewInsts = static_cast<AptValue**>(spNewInsts);      // reload (cb may move)
        }
    }

    // mListenerSet entries (count == mnCapacity, the +0x12 halfword).
    for (u32 liSlot = 0; liSlot < mListenerSet.mnCapacity; ++liSlot)
    {
        if (mListenerSet.mppSlots[liSlot] != nullptr)
        {
            lpCb(nullptr, &mListenerSet.mppSlots[liSlot],
                 "AptAnimationTarget::mListenerSet.aElements", 0);
        }
    }

    // mInputSet entries (count == mnCapacity, the +0x1A halfword).
    for (u32 liSlot = 0; liSlot < mInputSet.mnCapacity; ++liSlot)
    {
        if (mInputSet.mppSlots[liSlot] != nullptr)
        {
            lpCb(nullptr, &mInputSet.mppSlots[liSlot],
                 "AptAnimationTarget::inputSet.aElements", 0);
        }
    }

    // Root display list (mark each listed CIH) + the deferred-action queue.
    if (mDisplayList.mpHead != nullptr)
    {
        mDisplayList.AsState()->RegisterReferences(nullptr);   // (*(a1+32), 0)
    }
    mpActionQueue->RegisterReferences();

    // Interval timers: each armed timer's callback + context + every saved param.
    for (u32 liTimer = 0; liTimer < mnNumIntervalTimers; ++liTimer)
    {
        AptIntervalTimer& lrTimer = mpIntervalTimers[liTimer];
        if (lrTimer.mpActiveValue == nullptr)
        {
            continue;
        }
        lpCb(nullptr, &lrTimer.mpCBFunction,
             "AptAnimationTarget::maIntervalTimers[i].pCBFunction", 0);
        lpCb(nullptr, &lrTimer.mpContext,
             "AptAnimationTarget::maIntervalTimers[i].pContext", 0);

        const s32 liParamCount = lrTimer.mParams.mnTop;
        for (s32 liParam = 0; liParam < liParamCount; ++liParam)
        {
            // The X360 registers the params top-down through a stack-local copy of
            // mppItems[count-1-i] (it registers the local's address, not the live
            // slot's -- the param values are re-fetched fresh each pass).
            AptValue* lpParam = lrTimer.mParams.mppItems[liParamCount - liParam - 1];
            lpCb(nullptr, &lpParam,
                 "AptAnimationTarget::maIntervalTimers[i].pParams", 0);
        }
    }

    // Delayed-release list.
    AptValue** lpRem = static_cast<AptValue**>(spDelayedReleaseList);   // off_8324E54C
    for (int liIndex = 0; liIndex < snDelayedReleaseListSize; ++liIndex)
    {
        AptValue* lpValue = lpRem[liIndex];
        if (lpValue != nullptr)
        {
            lpCb(nullptr, &lpValue, "apDelayedReleaseList[i]", 0);
            lpRem = static_cast<AptValue**>(spDelayedReleaseList);
        }
    }
}

// ---------------------------------------------------------------------------
// RemoveCIHReferences @ 0x82ADEEA0
//   The ReplaceReferences callback for a director: re-register (remap) every held
//   AptValue* under its short debug name, and for each armed interval timer, after
//   the param values are remapped, scrub any param slot that still points at a value
//   being replaced back to the None sentinel.
// ---------------------------------------------------------------------------
void* AptAnimationTarget::RemoveCIHReferences()
{
    AptValue::ReferenceRegistrationCb lpCb = AptValue::sReferenceRegistrationCb;
    void* lpResult = this;

    if (mpInputMask != nullptr)
    {
        lpResult = lpCb(nullptr, &mpInputMask, "AptAnimationTarget::mpInputMask", 1);
    }

    AptValue** lpNewInsts = static_cast<AptValue**>(spNewInsts);
    for (int liIndex = 0; liIndex < snNewInstSize; ++liIndex)
    {
        if (lpNewInsts[liIndex] != nullptr)
        {
            lpResult = lpCb(nullptr, &lpNewInsts[liIndex], "mapNewInsts Element", 0);
            lpNewInsts = static_cast<AptValue**>(spNewInsts);
        }
    }

    for (u32 liSlot = 0; liSlot < mListenerSet.mnCapacity; ++liSlot)
    {
        if (mListenerSet.mppSlots[liSlot] != nullptr)
        {
            lpResult = lpCb(nullptr, &mListenerSet.mppSlots[liSlot], "mListenerSet Element", 0);
        }
    }

    for (u32 liSlot = 0; liSlot < mInputSet.mnCapacity; ++liSlot)
    {
        if (mInputSet.mppSlots[liSlot] != nullptr)
        {
            lpResult = lpCb(nullptr, &mInputSet.mppSlots[liSlot], "inputSet Element", 0);
        }
    }

    for (u32 liTimer = 0; liTimer < mnNumIntervalTimers; ++liTimer)
    {
        AptIntervalTimer& lrTimer = mpIntervalTimers[liTimer];
        if (lrTimer.mpActiveValue == nullptr)
        {
            continue;
        }
        // The X360 invokes vtbl[13] (slot +52) on the callback + context here -- the
        // value-type "remap this reference" virtual. AptValue does not model a +52
        // virtual by name yet; reproduce the indirect call through the recovered slot.
        // FLAG (un-homed virtual): vtbl[13] == the GC reference-remap virtual.
        {
            using RemapFn = void* (*)(void*);
            char* lpVtbl = *reinterpret_cast<char**>(lrTimer.mpCBFunction);
            (*reinterpret_cast<RemapFn*>(lpVtbl + 52))(lrTimer.mpCBFunction);
            lpVtbl = *reinterpret_cast<char**>(lrTimer.mpContext);
            lpResult = (*reinterpret_cast<RemapFn*>(lpVtbl + 52))(lrTimer.mpContext);
        }

        const s32 liParamCount = lrTimer.mParams.mnTop;
        for (s32 liParam = 0; liParam < liParamCount; ++liParam)
        {
            AptValue** lppSlot = &lrTimer.mParams.mppItems[liParamCount - liParam - 1];
            AptValue*  lpParam = *lppSlot;
            if (lpParam != nullptr)
            {
                AptValue* lpLocal = lpParam;
                lpResult = lpCb(nullptr, &lpLocal, "IntervalTimerParam", 0);
                lpParam = lpLocal;
            }
            // If the cb produced a different value, the live slot is repointed to the
            // None sentinel (unless it already holds it).
            if (lpParam != *lppSlot)
            {
                if (gpAptNoneValue != *lppSlot)
                {
                    *lppSlot = gpAptNoneValue;
                }
            }
        }
    }

    return lpResult;
}

// ---------------------------------------------------------------------------
// AddListenerToQueue @ 0x82B01C88
//   For an event-bearing listener (a defined CIH / CIH-none), for each AS event
//   handler whose mask intersects nEventMask, resolve the named handler child; when
//   it is a script function, (re)bind its "this" to the listener (a GC-root handoff
//   via the zombie-vector accounting) and enqueue a deferred function call for it on
//   the action queue. Returns the last enqueue / lookup result.
//
//   FLAG (un-homed value-graph mechanics): the X360 reaches through several AptValue
//   virtual slots (vtbl index 2 = "resolve listener object", 15 = "clone+rebind to
//   target", 17 = "get callable"), the script-function bound-"this" slot (value word
//   +0x24), and the bound-this's packed zombie-count field (bits 7..22 of its flags
//   word +0x0C). Those value subtypes (AptScriptFunction / the listener-object value)
//   are not yet modelled by name, so these accesses are reproduced verbatim through
//   the recovered console slot/word offsets, each flagged here.
// ---------------------------------------------------------------------------
AptValue* AptAnimationTarget::AddListenerToQueue(AptValue* pListener, int nEventMask,
                                                 int nPacked)
{
    typedef AptValue* (*VFn0)(AptValue*);              // vtbl(self)
    typedef AptValue* (*VFn1)(AptValue*, AptValue*);   // vtbl(self, arg)

    AptValue* lpResult = pListener;

    // Listener must be a defined CIH (type 12) or a CIH-none (type 37).
    const AptVirtualFunctionTable_Indices leType = pListener->getVtblIndex();
    const bool lbEventBearing =
        (leType == AptVFT_CharacterInstHandle && pListener->getIsDefined())
        || leType == AptVFT_CIHNone;

    // Proceed when it is NOT event-bearing-only, OR its character's own clip-event
    // mask already intersects nEventMask, OR it has a matching __proto__ event member.
    // (X360: !v10 || ((*(a2[8]+20)>>8)&a3) || HasEventMember(a2,a3).)
    bool lbProceed = !lbEventBearing;
    if (!lbProceed)
    {
        // The character instance's clip-event mask (charInst[+0x14] >> 8) & nEventMask.
        // FLAG (un-homed AptCharacterInst): read through the recovered offset.
        void* lpCharInst =
            *reinterpret_cast<void**>(reinterpret_cast<char*>(pListener) + 0x20);  // a2[8]
        const int liClipMask =
            *reinterpret_cast<int*>(reinterpret_cast<char*>(lpCharInst) + 0x14) >> 8;
        if ((liClipMask & nEventMask) != 0)
        {
            lbProceed = true;
        }
        else
        {
            lpResult = reinterpret_cast<AptValue*>(
                static_cast<intptr_t>(
                    reinterpret_cast<AptCIH*>(pListener)->HasEventMember(nEventMask)));
            lbProceed = (lpResult != nullptr);
        }
    }

    if (!lbProceed)
    {
        return lpResult;
    }

    // Resolve the listener object (vtbl index 2) and decide whether to walk handlers.
    AptValue* lpResolved =
        (*reinterpret_cast<VFn0*>(*reinterpret_cast<char**>(pListener) + 8))(pListener);
    lpResult = lpResolved;

    const AptVirtualFunctionTable_Indices leType2 =
        lpResolved ? lpResolved->getVtblIndex() : AptVFT_xxx;
    const bool lbResolvedEventBearing =
        (leType2 == AptVFT_CharacterInstHandle && lpResolved->getIsDefined())
        || leType2 == AptVFT_CIHNone;

    // X360: dispatch when the resolved object is event-bearing, OR its own event mask
    // (resolved[4] == word +0x10) intersects nEventMask, OR (resolved=resolved[2]) is
    // null, OR its further-resolved form's mask intersects nEventMask.
    bool lbDispatch = lbResolvedEventBearing;
    if (!lbDispatch)
    {
        const int liResolvedMask =
            *reinterpret_cast<int*>(reinterpret_cast<char*>(lpResolved) + 0x10);   // result[4]
        if ((liResolvedMask & nEventMask) != 0)
        {
            lbDispatch = true;
        }
        else
        {
            AptValue* lpNext =
                *reinterpret_cast<AptValue**>(reinterpret_cast<char*>(lpResolved) + 8);  // result[2]
            lpResult = lpNext;
            if (lpNext == nullptr)
            {
                lbDispatch = true;
            }
            else
            {
                lpResult =
                    (*reinterpret_cast<VFn0*>(*reinterpret_cast<char**>(lpNext) + 8))(lpNext);
                const int liNextMask =
                    *reinterpret_cast<int*>(reinterpret_cast<char*>(lpResult) + 0x10);
                if ((liNextMask & nEventMask) != 0)
                {
                    lbDispatch = true;
                }
            }
        }
    }

    if (!lbDispatch)
    {
        return lpResult;
    }

    for (int liDesc = 0; liDesc < gAptListenerEventDescriptorCount; ++liDesc)
    {
        const AptListenerEventDescriptor& lrDesc = gAptListenerEventDescriptors[liDesc];
        if ((lrDesc.nEventMask & nEventMask) == 0)
        {
            continue;
        }

        AptValue* lpChild = pListener->findChild(&gAptASNameTable[lrDesc.nNameIndex], nullptr);
        if (lpChild == nullptr)
        {
            continue;
        }

        // The handler must be a defined script function (type 34..36, mbIsDefined).
        const u32 liChildBits = lpChild->mnValueData;
        const bool lbScriptFn =
            ((liChildBits >> 27) & 1u) != 0
            && (static_cast<u32>((liChildBits << 25) >> 25) - 34u) <= 2u;
        if (!lbScriptFn)
        {
            continue;
        }

        // The script function's currently-bound "this" (value word +0x20). When it is
        // not already the listener, clone+rebind the function to the listener (vtbl
        // index 15) and hand the GC root over through the zombie-vector accounting.
        AptValue* lpBoundThis =
            *reinterpret_cast<AptValue**>(reinterpret_cast<char*>(lpChild) + 0x20);  // child[8]
        AptValue* lpTarget = lpChild;

        if (lpBoundThis != pListener)
        {
            lpTarget =
                (*reinterpret_cast<VFn1*>(*reinterpret_cast<char**>(lpChild) + 60))(lpChild, pListener);

            // The clone's bound-this slot (target word +0x24): decrement the OLD
            // bound-this's packed zombie count (flags word +0x0C, bits [7..22]) and,
            // when it hits zero, flush the zombie vector; Release it; install the new
            // bound-this; increment ITS zombie count; AddRef it; mark the target a GC root.
            AptValue** lppBoundSlot =
                reinterpret_cast<AptValue**>(reinterpret_cast<char*>(lpTarget) + 0x24);  // v18[9]
            AptValue* lpOldBound = *lppBoundSlot;

            u32* lpOldFlags =
                reinterpret_cast<u32*>(reinterpret_cast<char*>(lpOldBound) + 0x0C);     // *(v19+12)
            const u32 liOldZombie = ((((*lpOldFlags << 9) & 0xFFFF0000u) >> 9) - 1u) & 0x7FFF80u;
            *lpOldFlags = (*lpOldFlags & 0xFF80007Fu) | liOldZombie;
            if ((*lpOldFlags & 0x7FFF80u) == 0u)
            {
                AptUpdateZombieVector(0);
            }
            (*reinterpret_cast<VFn0*>(*reinterpret_cast<char**>(lpOldBound) + 4))(lpOldBound);  // Release

            *lppBoundSlot = lpBoundThis;
            u32* lpNewFlags =
                reinterpret_cast<u32*>(reinterpret_cast<char*>(lpBoundThis) + 0x0C);
            const u32 liNewZombie = ((((*lpNewFlags << 9) & 0xFFFF0000u) >> 9) + 128u) & 0x7FFF80u;
            *lpNewFlags = (*lpNewFlags & 0xFF80007Fu) | liNewZombie;
            (*reinterpret_cast<VFn0*>(*reinterpret_cast<char**>(lpBoundThis)))(lpBoundThis);  // vtbl[0] AddRef

            AptValue_setGCRoot(lpTarget, 1);
        }

        // Get the callable (vtbl index 17) and enqueue the deferred function call.
        const s32 liReturnReg = static_cast<s32>(reinterpret_cast<intptr_t>(
            (*reinterpret_cast<VFn0*>(*reinterpret_cast<char**>(lpTarget) + 68))(lpTarget)));
        lpResult = mpActionQueue->AddFunctionBack(pListener, lpTarget, liReturnReg, nPacked);
    }

    return lpResult;
}

// ---------------------------------------------------------------------------
// AddAnalogInput @ 0x82ADEA28
// ---------------------------------------------------------------------------
int AptAnimationTarget::AddAnalogInput(const AptAnalogInputEvent& rEvent)
{
    const u32 lnByteOffset = static_cast<u32>(rEvent.mnPlayer) << 4;   // 16 * player
    bool lbQueue = false;   // v14: did a stick-snapshot write (-> queue a packed input)

    switch (rEvent.mnEventId)
    {
        case 0x134:
        {
            if (rEvent.mfAxis0 == 0.0f)
            {
                break;
            }
            const bool lbRecord = (gAptInputRecorderEnabled != 0);
            *reinterpret_cast<f32*>(reinterpret_cast<char*>(gAptAnalogAxis0) + lnByteOffset) = rEvent.mfAxis0;
            *reinterpret_cast<f32*>(reinterpret_cast<char*>(gAptAnalogAxis1) + lnByteOffset) = 0.0f;
            if (lbRecord)
            {
                // FLAG (deferred recorder): log the 24-byte analog record { tag, type,
                // <the 16-byte event> } via the record sink. (X360 builds {dword_8324D820,
                // 0x0B000000, event...} and calls sink(rec, 24).)
                u32 lauRecord[6];
                lauRecord[0] = gAptInputRecorderTag;   // dword_8324D820
                lauRecord[1] = 0x0B000000u;            // 184549376
                std::memcpy(&lauRecord[2], &rEvent, sizeof(AptAnalogInputEvent));
                gpAptInputRecorderSink(lauRecord, 24);
            }
            break;
        }

        case 0x135:
        {
            const bool lbWasZero = (rEvent.mfAxis1 == 0.0f);
            *reinterpret_cast<f32*>(reinterpret_cast<char*>(gAptAnalogAxis1) + lnByteOffset) = rEvent.mfAxis1;
            *reinterpret_cast<f32*>(reinterpret_cast<char*>(gAptAnalogAxis0) + lnByteOffset) = 0.0f;
            if (lbWasZero || gAptInputRecorderEnabled == 0)
            {
                break;
            }
            u32 lauRecord[6];
            lauRecord[0] = gAptInputRecorderTag;
            lauRecord[1] = 0x0B000000u;
            std::memcpy(&lauRecord[2], &rEvent, sizeof(AptAnalogInputEvent));
            gpAptInputRecorderSink(lauRecord, 24);
            break;
        }

        case 0x1F5:
        {
            lbQueue = true;
            std::memcpy(gAptAStickLeft + lnByteOffset, &rEvent, sizeof(AptAnalogInputEvent));
            break;
        }

        case 0x1F6:
        {
            lbQueue = true;
            std::memcpy(gAptAStickRight + lnByteOffset, &rEvent, sizeof(AptAnalogInputEvent));
            break;
        }

        default:
            break;
    }

    int liResult = 0;
    if (lbQueue)
    {
        // Pack {eventId, player} into a queued input word: (((eventId<<15)|player)<<2)|1.
        const u32 lnPacked =
            ((((rEvent.mnEventId << 15) | rEvent.mnPlayer) << 2) | 1u);
        liResult = AddInput(static_cast<int>(lnPacked));
        if (gAptInputRecorderEnabled && liResult)
        {
            // FLAG (deferred recorder): log the raw 16-byte stick sample.
            liResult = gpAptInputRecorderSink(const_cast<AptAnalogInputEvent*>(&rEvent), 16);
        }
    }
    return liResult;
}

// ---------------------------------------------------------------------------
// ProcessInputSet @ 0x82AF4478
//   Route one packed input event to every live mInputSet CIH whose display-list
//   parent chain reaches the active input mask (mpInputMask). For a "down/press"
//   event (packed bits[1..0] == 1): a rollover-phase press (nCode 0) on a 501/502
//   event raises clip-event 64, on any other event raises 64 then (once per frame)
//   the 0x20000 input event; a press-phase press (nCode 1) raises clip-event 128.
//   Only nCode (the event phase) and nPacked (the packed word) of the forwarded args
//   are consumed; nEventId / nSub / bFirstThisFrame are passed for parity.
// ---------------------------------------------------------------------------
int AptAnimationTarget::ProcessInputSet(int nEventId, int nCode, unsigned int nPacked,
                                        int nSub, int bFirstThisFrame)
{
    (void)nEventId; (void)nSub; (void)bFirstThisFrame;

    int  liResult     = 0;
    bool lbInputFired = false;   // v7/v26: the once-per-frame 0x20000 input latch
    u32  liDispatched = 0;       // v8: live entries dispatched so far (vs mnCount)

    if (mInputSet.mnCapacity == 0)
    {
        return liResult;
    }

    for (u32 liSlot = 0; liSlot < mInputSet.mnCapacity; ++liSlot)
    {
        if (liDispatched == mInputSet.mnCount)
        {
            break;   // every live entry handled
        }

        AptValue* lpEntry = mInputSet.mppSlots[liSlot];
        if (lpEntry == nullptr)
        {
            continue;
        }

        // "Masked" test: walk the entry's display-list parent chain; the entry is
        // eligible (NOT masked out) when the chain reaches the active input mask, or
        // when there is no input mask at all.
        bool lbMasked;
        if (mpInputMask == nullptr)
        {
            lbMasked = false;
        }
        else
        {
            AptCIH* lpWalk = reinterpret_cast<AptCIH*>(lpEntry);
            for (;;)
            {
                if (reinterpret_cast<AptValue*>(lpWalk) == mpInputMask)
                {
                    lbMasked = false;   // reached the mask -> eligible
                    break;
                }
                lpWalk = lpWalk->GetDisplayListParent();   // value[+0x1C]
                if (lpWalk == nullptr)
                {
                    lbMasked = true;    // chain ended without the mask -> skip
                    break;
                }
            }
        }

        if (!lbMasked)
        {
            if ((nPacked & 3u) == 1u)   // a "down/press" packed event
            {
                if (nCode == 1)
                {
                    liResult = reinterpret_cast<intptr_t>(
                        AptCIH_queueClipEvents(lpEntry, 128, nPacked, 0));
                }
                else if (nCode == 0)
                {
                    const unsigned int liEvent = nPacked >> 17;
                    if (liEvent == 502u || liEvent == 501u)
                    {
                        liResult = reinterpret_cast<intptr_t>(
                            AptCIH_queueClipEvents(lpEntry, 64, nPacked, 0));
                    }
                    else
                    {
                        liResult = reinterpret_cast<intptr_t>(
                            AptCIH_queueClipEvents(lpEntry, 64, nPacked, 0));
                        if (!lbInputFired)
                        {
                            AptValue* lpRet =
                                AptCIH_queueClipEvents(lpEntry, 0x20000, nPacked, 1);
                            liResult     = reinterpret_cast<intptr_t>(lpRet);
                            // X360 tests only the low byte (clrlwi. r11,r26,24; IDA char v7).
                            lbInputFired = ((reinterpret_cast<uintptr_t>(lpRet) & 0xFFu) != 0);
                        }
                    }
                }
            }
            ++liDispatched;
        }
    }

    return liResult;
}

// ---------------------------------------------------------------------------
// ProcessListenerEvents @ 0x82B01ED0
//   For a rollover (nCode 0) or press (nCode 1) event, route it to every live
//   mListenerSet entry via AddListenerToQueue (clip-event mask 64 for rollover, 128
//   for press). Only nCode + nPacked of the forwarded args are consumed.
// ---------------------------------------------------------------------------
int AptAnimationTarget::ProcessListenerEvents(int nEventId, unsigned int nCode,
                                              int nPacked, int nSub)
{
    (void)nEventId; (void)nSub;

    int liResult = reinterpret_cast<intptr_t>(this);   // X360 returns this when no dispatch
    if (nCode > 1u)
    {
        return liResult;   // only rollover(0)/press(1) phases drive listeners
    }

    if (mListenerSet.mnCapacity == 0)
    {
        return liResult;
    }

    u32 liDispatched = 0;   // v7: live entries handled (vs mnCount)
    for (u32 liSlot = 0; liSlot < mListenerSet.mnCapacity; ++liSlot)
    {
        if (liDispatched == mListenerSet.mnCount)
        {
            break;
        }

        AptValue* lpEntry = mListenerSet.mppSlots[liSlot];
        if (lpEntry != nullptr)
        {
            const int liMask = (nCode == 1u) ? 128 : 64;
            liResult = reinterpret_cast<intptr_t>(
                AddListenerToQueue(lpEntry, liMask, nPacked));
            ++liDispatched;
        }
    }

    return liResult;
}
