#pragma once

// ===========================================================================
// XAUDIO::CEngine -- THE XAudio engine singleton in BURNOUT_X360_ARTIST.XEX. It
// is the object stored at the module global `off_832BB944` that the whole XAudio
// voice cluster (CVoice / CRoutedVoice / CSourceVoice / CSubmixVoice /
// CMasteringVoice / CReverbEffect) dispatches through -- the owner of the
// per-category effect managers, the default-output (mastering) voice, the
// master + active voice lists, the processing-thread pool, and the engine
// callback groups. This header is the canonical OWNING home for the
// class:XAUDIO::CEngine TU (18 ledger functions):
//
//     XAUDIO::CEngine::CEngine                                     @ 0x82C2EA28
//     XAUDIO::CEngine::~CEngine                                    @ 0x82C2E2D0
//     XAUDIO::CEngine::`vector deleting destructor'                @ 0x82C2EAF8
//     XAUDIO::CEngine::CreateObject                                @ 0x82C2EEC8
//     XAUDIO::CEngine::Initialize                                  @ 0x82C2EB28
//     XAUDIO::CEngine::GetObjectAdditionalSize                     @ 0x82C2DD08
//     XAUDIO::CEngine::AssignThreadUsage                           @ 0x82C2D598
//     XAUDIO::CEngine::GetVoiceList                                @ 0x82C30BD8
//     XAUDIO::CEngine::Process                                     @ 0x82C2E910
//     XAUDIO::CEngine::ProcessFrame                                @ 0x82C2E088
//     XAUDIO::CEngine::ProcessVoiceList                            @ 0x82C2DDE8
//     XAUDIO::CEngine::ProcessEngineCallbacks                      @ 0x82C2D8B0
//     XAUDIO::CEngine::ProcessingThreadSynchronizationCheckpoint   @ 0x82C2D6B0
//     XAUDIO::CEngine::SwapActiveLists                             @ 0x82C2DFA0
//     XAUDIO::CEngine::BlockOnActiveVoice                          @ 0x82C2DA30
//     XAUDIO::CEngine::PeonProcessingThreadEntryPoint              @ 0x82C2E860
//     XAUDIO::CEngine::SupervisorProcessingThreadEntryPoint        @ 0x82C2E798
//     XAUDIO::CEngine::TitleTerminateCallback                      @ 0x82C2DB70
//
// There is no reference source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 asm (chiefly the ctor @0x82C2EA28, the
// Initialize @0x82C2EB28 and the destructor @0x82C2E2D0 load/store
// displacements). `XAUDIO` is an external middleware boundary, so its API
// identifiers are preserved verbatim per the naming convention; reconstructed
// members use the project Hungarian.
//
// SIZE: the object is allocated as 372 (0x174) bytes -- both CreateObject
// (`(*(*allocator + 20))(allocator, 372)`) and the Initialize scratch confirm
// it. The X360 byte offsets in the comments below tile that 372 exactly:
//   +0x00 vptr .. +0x0C effect managers[12] .. +0x3C effect mgr .. +0x40 default
//   output .. +0x44 list-D .. +0x50 master list(44) .. +0x7C active-list array
//   .. +0x80 count .. +0x84/+0x88 category volumes .. +0x8C change mask ..
//   +0xA8 callback groups[2*64] .. +0x128 cur-cb .. +0x12C thread marker ..
//   +0x130 thread count .. +0x134 thread handles[6] .. +0x14C list markers[6] ..
//   +0x164 sync checkpoints[2] .. +0x174 == 372.
// Pointer members are widened to x64 (the byte offsets are an X360 compiler
// artifact; semantic parity is by named member), so `sizeof` is NOT asserted.
//
// BODIES: every one of the 18 methods is BLOCKED in this wave -- unlike the
// self-contained accessors CVoice landed, CEngine has none. Each body drives one
// or more of: the Xbox kernel spinlock/IRQL/threading/semaphore primitives
// (KeRaiseIrqlToDpcLevel / KeAcquireSpinLockAtRaisedIrql /
// KeReleaseSpinLockFromRaisedIrql / KfLowerIrql / KeWaitForSingleObject /
// KeWaitForMultipleObjects / KeSetEvent / KeInitializeSemaphore /
// KeReleaseSemaphore / KeSetBasePriorityThread / KeResumeThread /
// KeGetCurrentProcessType / ExCreateThread / ExRegisterTitleTerminateNotification
// / ObReferenceObjectByHandle / ObDereferenceObject / WaitForSingleObject /
// CloseHandle / Rtl{Enter,Leave}CriticalSection), the per-hardware-thread block
// read through `r13` (the KPCR: `lbz 0x10C(r13)` hw-thread index,
// `lwz 0x100(r13)` thread ptr), the un-recovered module rodata/state globals
// (the shared spinlock `unk_83222C28` + `dword_83222C2C`/`dword_83222C30`/
// `byte_83222C34`, the module state `byte_832BB900..dword_832BB944`, the class
// vtable rodata `off_821B5514`/`off_821B5528`/`off_82108FF4`, the default effect
// context `unk_82108F38`), or a still-todo foreign vtable slot (the CVoice /
// mastering-voice / batch-allocator dispatches). They are declared here (with
// asm-grounded signatures) so the class interface and vptr are correct and so
// the downstream can bind against CEngine; the ledger marks them blocked.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XAudio/XAudioPacketCtx.h"       // XAUDIO::XAUDIOPACKETCTX_ (list heads)
#include "SDKs/XAudio/XAudioMasterVoiceList.h" // XAUDIO::CMasterVoiceList (embedded)

namespace XAUDIO
{

// Forward decls: these members are held by pointer only, so the full definitions
// are not needed to lay out CEngine (keeps the CEngine include surface small).
class CVoice;          // the default-output / mastering voice at +0x40
class CEffectManager;  // the XAudio effect-manager handle at +0x3C

// ---------------------------------------------------------------------------
// The engine creation descriptor handed to CreateObject / GetObjectAdditional-
// Size / Initialize (Hex-Rays `unsigned __int8* a1`). Only the fields this TU
// reads are modelled, at the X360 byte offsets they were read at; the rest of
// the descriptor is opaque. FLAG: reconstructed field set, not a complete
// descriptor.
//   +0x00 muMaxVoices    -- `lbz 0(cfg)`, clamped to a minimum of 6 when sizing
//                           the frame buffer / per-voice pool.
//   +0x01 muNumActiveLists-- `lbz 1(cfg)`, the number of 44-byte active voice
//                           lists Initialize allocates (stored at +0x80).
//   +0x02 muThreadUsage   -- `lbz 2(cfg)`, the processing-thread usage bitfield
//                           fed to AssignThreadUsage.
//   +0x04 mpEffectConfig  -- `lwz 4(cfg)`, the effect-registration table pointer
//                           passed to XAudioQueryEffectManagerSize /
//                           XAudioCreateEffectManager (0 => &unk_82108F38).
// ---------------------------------------------------------------------------
struct SEngineConfig
{
    u8    muMaxVoices;      // +0x00
    u8    muNumActiveLists; // +0x01
    u8    muThreadUsage;    // +0x02
    u8    mPad03;           // +0x03 alignment
    void* mpEffectConfig;   // +0x04  (SEffectRegistrationTable*, opaque here)
};

// The 8-byte thread-usage plan AssignThreadUsage fills from the config's
// thread-usage bits (`_BYTE* a2`, zeroed then written): a running count pair and
// six per-thread role slots (`a2[2+i]`), with `a2[6]` doubling as the
// supervisor/mode slot the algorithm resolves last. FLAG: byte roles inferred
// from the store pattern; exact enumerals not recovered.
struct SThreadUsagePlan
{
    u8 muAssignedCount; // +0x00  (`++a2[0]` per assigned peon role)
    u8 muRoleCount;     // +0x01  (`++a2[1]`)
    u8 muThreadRole[6]; // +0x02  per-hardware-thread role byte (2/1/4 seen)
};

// One engine callback slot (8 bytes): a function pointer + its argument. Eight of
// these form a group; ProcessEngineCallbacks selects a group by `(index<<6)` and
// walks its eight entries, calling `entry.mpfn(entry.mpArg)` for each non-null.
struct SEngineCallback
{
    s32 (*mpfnCallback)(void* apArg); // +0x00
    void* mpArg;                      // +0x04
};

// ---------------------------------------------------------------------------
// One engine voice list (X360 stride 0x2C == 44 bytes). The ctor self-seats the
// master head and its two active sub-lists; SwapActiveLists drives each entry
// through CEngine::CEngineVoiceList::SwapActiveLists. It is embedded once at
// +0x50 (the master list) and once per element of the +0x7C active-list array.
//   +0x00 mMaster    -- CMasterVoiceList head (next/prev/base, size seed 16).
//   +0x0C mActiveA   -- active sub-list head (the un-homed CActiveVoiceList; its
//                       layout is the shared XAUDIOPACKETCTX_ head, so it is
//                       modelled by that type here, size seed 24).
//   +0x18 mActiveB   -- second active sub-list head (size seed 24).
//   +0x24 mpActiveA  -- pointer seated to &mActiveA by the ctor.
//   +0x28 mpActiveB  -- pointer seated to &mActiveB by the ctor.
// ---------------------------------------------------------------------------
struct SEngineVoiceList
{
    // @ 0x82C2DC50 -- X360 symbol XAUDIO::CEngine::CEngineVoiceList::SwapActiveLists.
    // Flip this list's front/back active sub-list pointers (mpActiveA <-> mpActiveB)
    // under the XAudio module-shared recursive spin-lock. Driven once per list by
    // CEngine::SwapActiveLists. Bodied in XAudioEngineVoiceList.cpp (the ledger TU
    // class:XAUDIO::CEngine::CEngineVoiceList); the recursive-lock primitives it
    // needs live in XAudioEngineVoiceList.h.
    s32 SwapActiveLists();

    CMasterVoiceList  mMaster;   // +0x00  (12B X360 head)
    XAUDIOPACKETCTX_  mActiveA;  // +0x0C  (concrete type: XAUDIO::CActiveVoiceList)
    XAUDIOPACKETCTX_  mActiveB;  // +0x18  (concrete type: XAUDIO::CActiveVoiceList)
    XAUDIOPACKETCTX_* mpActiveA; // +0x24
    XAUDIOPACKETCTX_* mpActiveB; // +0x28
};

class CEngine
{
public:
    // @ 0x82C2EA28 -- seat the base + engine vtables, miRefCount = 1, store the
    // allocator (AddRef'd via its vtable[0]), self-seat the +0x44 list-D and the
    // +0x50 master voice list, the two category volumes (1.0f) and publish
    // `this` into the module singleton (off_832BB944).
    // BLOCKED: seats the un-recovered class vtable rodata (off_821B5514 base image
    // then off_821B5528) and writes the module singleton global; body deferred.
    explicit CEngine(void* apAllocator);

    // @ 0x82C2E2D0 -- teardown: stop the mastering voice, join + close the six
    // processing threads, unregister the title-terminate callback, drain every
    // voice list, release the per-category + main effect managers, run the
    // embedded list dtors and release the allocator, clearing the singleton.
    // BLOCKED: Xbox kernel threading/spinlock primitives + module-state globals +
    // foreign vtable Release slots; body deferred.
    virtual ~CEngine();

    // @ 0x82C2EEC8 -- static factory: size the object (372 + effect-manager
    // additional size), allocate it through a prepared batch allocator, placement-
    // construct the CEngine, Initialize it, and hand it back through `*appOut`.
    // BLOCKED: batch-allocator vtable protocol + the blocked ctor/Initialize;
    // body deferred. (Hex-Rays spills a4..a14 as ABI stack args; the load-bearing
    // parameters are the config, the reserved word stored into the allocator
    // request, and the out-pointer.)
    static s32 CreateObject(const SEngineConfig* apConfig, void* aReserved,
                            CEngine** appOut);

    // @ 0x82C2EB28 -- allocate the active voice-list array, create the effect
    // manager, initialise the module lock/semaphore/title-terminate state, spin
    // up the processing threads per the thread-usage plan, create the per-thread
    // frame buffers and finally the mastering (default-output) voice.
    // BLOCKED: Xbox kernel threading/semaphore/handle primitives + module-state
    // globals + XAudioCreateEffectManager/XAudioCreateFrameBuffer + the
    // CMasteringVoice::CreateObject foreign dispatch; body deferred.
    s32 Initialize(const SEngineConfig* apConfig);

    // @ 0x82C2DD08 -- static: the extra bytes an engine built from `apConfig`
    // needs beyond the 372-byte object = the effect-manager additional size + the
    // frame-buffer size * 2 * threads-in-use + (44 * numActiveLists + 4) when the
    // config asks for active lists.
    // BLOCKED: XAudioQueryEffectManagerSize / XAudioQueryFrameBufferSize helpers +
    // AssignThreadUsage; body deferred.
    static s32 GetObjectAdditionalSize(const SEngineConfig* apConfig, u32* apSize);

    // @ 0x82C2D598 -- expand the config's 6-bit thread-usage mask into the 8-byte
    // SThreadUsagePlan (per-hardware-thread role bytes + counts), selecting the
    // supervisor thread last.
    // BLOCKED: KeGetCurrentProcessType (Xbox kernel) gates the whole assignment;
    // body deferred.
    s32 AssignThreadUsage(u8 auThreadUsage, SThreadUsagePlan* apPlan);

    // @ 0x82C30BD8 -- resolve the voice list a voice belongs to: query the voice
    // for its category (its vtable slot at +0x34), then return &mMasterVoiceList
    // (category 0), the addressed active-list entry (category 1) or null.
    // BLOCKED: dispatches the still-un-recovered CVoice vtable slot at +0x34; body
    // deferred. (See the observed-dispatch note appended to XAudioVoice.h.)
    void* GetVoiceList(CVoice* apVoice);

    // @ 0x82C2E910 -- one engine tick under the module critical section: either
    // signal the worker threads and wait, or (single-threaded) swap the active
    // lists and process a frame inline; then run the mastering voice's render.
    // BLOCKED: Rtl critical section + Ke event/multi-wait + the per-thread `r13`
    // read + mastering-voice vtable dispatch; body deferred.
    s32 Process();

    // @ 0x82C2E088 -- process one audio frame: engine callbacks, per-category
    // volume change resolution, the master voice list, then each active voice
    // list with a sync checkpoint between them.
    // BLOCKED: KeReleaseSemaphore + the module singleton read + the blocked
    // ProcessEngineCallbacks/ProcessVoiceList/checkpoint helpers; body deferred.
    s32 ProcessFrame(u8 abRunCallbacks);

    // @ 0x82C2DDE8 -- drain one voice list under the module spinlock, dispatching
    // each pending voice's Process (its vtable slot at +0x44).
    // BLOCKED: Xbox spinlock/IRQL primitives + the CVoice Process vtable slot;
    // body deferred.
    s32 ProcessVoiceList(int aListSelector);

    // @ 0x82C2D8B0 -- run the eight callbacks of the selected engine callback
    // group, taking/releasing the module spinlock around each dispatch.
    // BLOCKED: Xbox spinlock/IRQL primitives + module lock state; body deferred.
    s32 ProcessEngineCallbacks(int aGroupIndex);

    // @ 0x82C2D6B0 -- lock-free processing-thread rendezvous: pack the six
    // ready-flag member dwords into a byte signature and spin on the caller's
    // 64-bit checkpoint word until every thread has arrived.
    // BLOCKED: reads the per-hardware-thread index from the `r13` KPCR
    // (`lbz 0x10C(r13)`); body deferred.
    s32 ProcessingThreadSynchronizationCheckpoint(__int64* apCheckpoint);

    // @ 0x82C2DFA0 -- swap the front/back buffers of the master voice list and
    // every active voice list under the module spinlock.
    // BLOCKED: Xbox spinlock/IRQL primitives + CEngine::CEngineVoiceList::
    // SwapActiveLists (todo); body deferred.
    s32 SwapActiveLists();

    // @ 0x82C2DA30 -- block until the given active voice is no longer owned by any
    // processing thread, releasing/re-taking the module spinlock + critical
    // section around the wait.
    // BLOCKED: Xbox spinlock/IRQL + Rtl critical section + `r13` reads; body
    // deferred.
    s32 BlockOnActiveVoice(void* apVoice);

    // @ 0x82C2E860 -- peon processing-thread entry point: publish this thread's
    // list marker, then loop waiting on the run semaphore and processing frames
    // until the engine stops.
    // BLOCKED: KeWaitForSingleObject + the module singleton/`r13` reads; body
    // deferred. (Static thread entry; no `this`.)
    static s32 PeonProcessingThreadEntryPoint(void* apArg);

    // @ 0x82C2E798 -- supervisor processing-thread entry point: like the peon
    // loop but also swaps the active lists and drives the sync events.
    // BLOCKED: KeWaitForSingleObject/KeSetEvent/KeReleaseSemaphore + module state;
    // body deferred. (Static thread entry; no `this`.)
    static s32 SupervisorProcessingThreadEntryPoint(void* apArg);

    // @ 0x82C2DB70 -- title-terminate notification callback: on our own
    // notification record, unregister and signal the shutdown event.
    // BLOCKED: compares the un-recovered module notification global
    // (dword_832BB8F0) and calls KeSetEvent; body deferred. (Static callback.)
    static void* TitleTerminateCallback(void* apNotification);

    // ----- layout (declared in X360 offset order; see the header banner) -----
    u32               miRefCount;             // +0x04  ctor seeds 1
    void*             mpAllocator;            // +0x08  ctor arg (AddRef/Release)
    CEffectManager*   mpEffectManagers[12];   // +0x0C  per-category effect mgrs
                                              //        (6 categories x 2; released
                                              //        by the dtor, populated
                                              //        outside this TU -- FLAG)
    CEffectManager*   mpEffectManager;        // +0x3C  main effect manager
    CVoice*           mpDefaultOutputVoice;   // +0x40  mastering / default output
    XAUDIOPACKETCTX_  mListD;                 // +0x44  CActiveVoiceList head (12B)
    SEngineVoiceList  mMasterVoiceList;       // +0x50  embedded master list (44B)
    SEngineVoiceList* mpActiveVoiceLists;     // +0x7C  active-list array
    u8                muNumActiveLists;       // +0x80
    u8                mPad81[3];              // +0x81  alignment
    float             mfCategoryVolume[2];    // +0x84  ctor seeds 1.0f each
    u32               muVolumeChangeMask;     // +0x8C  ProcessFrame change mask
    u8                mPad90[0xA8 - 0x90];    // +0x90  (not exercised by this TU)
    SEngineCallback   mCallbackGroups[2][8];  // +0xA8  two 8-entry callback groups
    SEngineCallback*  mpCurrentCallbackEntry; // +0x128 ProcessEngineCallbacks cursor
    void*             mpProcessingThread;     // +0x12C thread marker (Process/Block)
    u32               muNumProcessingThreads; // +0x130
    void*             mThreadHandles[6];      // +0x134 processing-thread handles
    void*             mThreadListMarker[6];   // +0x14C per-hw-thread list marker
    __int64           mSyncCheckpoints[2];    // +0x164 processing sync checkpoints
};

// The engine singleton the ctor publishes (`off_832BB944`) and the dtor clears.
// The whole XAudio voice cluster reaches the default output (+0x40) and effect
// manager (+0x3C) through it. Declared here as the canonical name; defined in
// XAudioEngine.cpp.
extern CEngine* gpEngine;

} // namespace XAUDIO
