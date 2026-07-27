// =====================================================================================
// rw::audio::core::System -- the owning EARenderWare "rwaudio" sub-system: the base/hub
// of the audio-core family (DelayLine / DelayFilter / Decoder / Gain / Send / Voice /
// TimerManager all hang off it). Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC);
// the asm is authoritative for every store. The reconstructed layout lives in PlugIn.h
// (the canonical home of the plug-in/System family). No Feb-2007 source, no DecFIGS DWARF,
// and no ProStreet08 PDB entry exist for these bodies.
//
// ONE ledger function remains to be reconstructed here (per-function guidance, the
// recovered rodata and the tail-carve recipe live in scratchpad/waveE/AudioSystem.spec.md):
//   ExecuteCommands @0x82B6F6D0 -- the audio-frame pump: tick the ITask timer list (now
//                                  homed in ITask.h), run the TimerManager, expel faded
//                                  voices, replay the deferred-command ring.
//                                  BLOCKED on a MISSING DECLARATION: the asm's final timer
//                                  phase calls rw::audio::core::TimerManager::Defragment
//                                  @0x82B6EC08 (r3 = &System::mTimerManager, immediately
//                                  after ExecuteTimers(&mTimerManager, 1)). That function is
//                                  NOT declared in TimerManager.h, has no per-address IDA
//                                  export and no ledger identity (an exporter gap, same
//                                  class as EaXmaDec::DeallocateResources). Omitting the call
//                                  would drop a side effect the binary has, and standing a
//                                  free function in for it would be an invented shim -- so
//                                  the body is withheld. It unblocks with ONE additive line
//                                  in TimerManager.h:
//                                      static void Defragment(TimerManager *self); // @0x82B6EC08
// =====================================================================================

#include "rw/audio/core/PlugIn.h"       // rw::audio::core::System (+ PlugInRegistry, hooks)
#include "rw/audio/core/Voice.h"        // Voice / VoiceListLink / VoiceActiveNode
#include "rw/audio/core/TimerManager.h" // rw::audio::core::TimerManager
#include "rw/audio/core/Profiler.h"     // rw::audio::core::Profiler / KPF_ProfilerVTable
#include "rw/audio/core/DecoderRegistry.h" // DecoderRegistry (Release frees it via its +0x0C back-pointer)
#include "rw/audio/core/EaXmaDec.h"        // EaXmaDec::DeallocateResources (nullary; the XMA contexts)
#include "SDKs/EATech/eathread/eathread_mutex.h" // EA::Thread::Mutex
#include "SDKs/EATech/eathread/BrnEAThreadX360.h" // EA::Thread::ThreadSleep(const u32 *)

#include <cstddef> // offsetof
#include <cstdint> // uintptr_t (the allocation-tail carve)
#include <cstring> // memset (the X360 XMemSet)

// -------------------------------------------------------------------------------------
// Xbox 360 physical-memory API (XAM/xtl). Platform primitives; declared here rather than
// pulled from the SDK so the default physical hooks link. XPhysicalAlloc's second argument
// is the physical-address hint (MAXULONG_PTR = "any").
// -------------------------------------------------------------------------------------
extern "C" void *XPhysicalAlloc(unsigned long dwSize, unsigned long ulPhysicalAddress,
                                unsigned long dwAlignment, unsigned long flProtect);
extern "C" void XPhysicalFree(void *lpAddress);

namespace rw
{
namespace audio
{
namespace core
{

// The audio-core System singleton (off_83271928). Set by CreateInstance (below) and
// cleared by Release (still to be reconstructed); read by
// GetPlugInRegistry. Defined here -- its home -- as zero-initialised storage so the many
// consumers that `extern "C"` it link. C linkage: the name is namespace-independent.
extern "C" System *off_83271928 = 0;

// The Csis integration's mutex primitives, installed into the System lock hooks by
// VectorToCsisMutex. Their bodies live in the Csis glue TU (each is a `b` thunk onto the
// nullary Csis::System::Lock/Unlock @0x82B0F248/@0x82B0F278, which wait on a GLOBAL
// mutex handle and ignore r3); here they are referenced only by address, so a forward
// declaration is sufficient (and faithful -- the asm stores the function addresses).
void CsisMutexLock();
void CsisMutexUnlock();

// The shared 4-word table the constructor points mpObjectTable at (dword_83271930..0x3C).
// rwaudio PDB: this is the static StackAllocator {System* mpSystem, u32 mpUpperLimit,
// u32 mpLowerLimit, u32 mpTop} -- Mixer::Mixer @0x82B6D880 seeds mpSystem/the limits, and
// AiffWriter/Delay read slot [3] (mpTop) as the mixer scratch-stack top. Kept as the
// opaque word table this TU's own bodies treat it as (only the ctor zeroes it here);
// home the real StackAllocator type when a Mixer/StackAllocator slice needs it.
static u32 skSystemObjectTable[4] = { 0, 0, 0, 0 };

// The 8-byte deferred-teardown record ReleaseHandler reads: { handler, System* }.
struct SystemReleaseCommand
{
    int (*mpHandler)(void *); // +0x00 -- &System::ReleaseHandler
    System *mpSystem;          // +0x04 -- the owning System
};

// The file-scope Profiler singleton GetProfiler lazily seeds and publishes at
// System::mpProfiler. On the X360 this is the zero-initialised .data block at
// dword_832718F0 (the whole object is written field-by-field by GetProfiler, never by a
// constructor), so it is modelled the same way here: static zero-initialised storage.
static Profiler skProfiler;

// Round a carved-block base up to 8, exactly as the X360 CreateInstance does with its
// `addi rN, rN, 7 / clrrwi rN, rN, 3` pairs -- but on the host's own pointer width. This
// is arithmetic on the raw allocation the System was placement-constructed into (the
// original allocation-carving algorithm), not offset access into a typed object.
static u8 *AlignUp8(u8 *pAddress)
{
    return reinterpret_cast<u8 *>(
        (reinterpret_cast<std::uintptr_t>(pAddress) + 7) & ~static_cast<std::uintptr_t>(7));
}

// -------------------------------------------------------------------------------------
// System_ctor @0x82B6DD20 -- the placement constructor. Clears the two list heads,
// default-constructs the embedded TimerManager, zeroes + points at the shared object table.
// -------------------------------------------------------------------------------------
System *System::System_ctor(System *self)
{
    self->mpExpelledVoiceList = 0;   // +0x10
    self->mpTimerListHead = 0;       // +0x5C
    TimerManager::TimerManager_ctor(&self->mTimerManager); // +0x60
    skSystemObjectTable[0] = 0;
    skSystemObjectTable[1] = 0;
    skSystemObjectTable[2] = 0;
    skSystemObjectTable[3] = 0;
    self->mpObjectTable = skSystemObjectTable; // +0x00
    return self;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82B6F420 -- allocate the singleton System (16-aligned) through
// `allocator`, placement-construct it, carve the two EA::Thread::Mutex objects and the two
// thread-id slots out of the allocation tail, allocate the first active-voice node and the
// deferred-command ring, and seed every field. Returns null (having freed whatever it did
// allocate) when any allocation fails.
//
// Two faithful oddities, both preserved: off_83271928 is published very early -- before the
// mutexes are even constructed -- and the failure path does NOT clear it again.
// -------------------------------------------------------------------------------------
System *System::CreateInstance(EA::Allocator::ICoreAllocator *allocator, u32 commandBufferSize)
{
    // asm: MutexParameters(&stack, r4 = 1, r5 = 0).
    EA::Thread::MutexParameters lMutexParams(true, 0);

    // X360-LITERAL TRAP: the immediate here is 0x1174 == the X360 sizeof(System) (0x1108)
    // + two 0x30-byte EA::Thread::Mutex objects + the 8-byte lock-thread-id block + the
    // 4-byte rwaudio thread-id word, i.e. a console sizeof sum. Every one of those widens on
    // the host, so the size is COMPUTED from host sizeofs; the trailing +7 covers the align8
    // round-up of the first carved base so the carve below can never overrun.
    const size_t luBlockSize =
        sizeof(System) + 2 * sizeof(EA::Thread::Mutex) + 8 + sizeof(u32) + 7;

    void *lpBlock = allocator->Alloc(luBlockSize, "rw::audio::core::System", 1, 16, 0);
    if (!lpBlock)
        return 0;

    System *lpSelf = System_ctor(static_cast<System *>(lpBlock));

    lpSelf->mfCpuClockRate = 1.0f; // flt_82001C98
    lpSelf->mpDeferredRingBase = 0;
    lpSelf->mppVoiceListNodes = 0;
    lpSelf->muExpelAfterDecayCount = 0;
    off_83271928 = lpSelf; // published here, and deliberately not cleared on failure
    lpSelf->mpAllocator = allocator;
    lpSelf->mpfnPhysicalAlloc = 0;
    lpSelf->mpfnPhysicalFree = 0;

    // ---- allocation-tail carve (asm: (this + 0x110F) & ~7 == align8(this + sizeof(System)),
    // then one 0x30-byte mutex slot at a time). The X360's null test around each construction
    // is the compiler's placement-new guard, which `new (p) T` re-emits.
    u8 *lpTail = AlignUp8(reinterpret_cast<u8 *>(lpSelf) + sizeof(System));
    lpSelf->mpSystemMutex = new (lpTail) EA::Thread::Mutex(0, true); // asm: Mutex(p, 0, 1)
    lpTail = AlignUp8(lpTail + sizeof(EA::Thread::Mutex));
    lpSelf->mpCommandMutex = new (lpTail) EA::Thread::Mutex(0, true);
    lpTail = AlignUp8(lpTail + sizeof(EA::Thread::Mutex));
    lpSelf->mpLockThreadId = lpTail;
    // asm: (lockThreadId + 0xB) & ~7 == align8(lockThreadId + 4), the 4 being the X360
    // sizeof(ThreadId); on the host a ThreadId is pointer-sized, and either way an 8-aligned
    // base lands the word 8 bytes on -- exactly the block reserved above.
    lpSelf->mppThreadId =
        reinterpret_cast<u32 *>(AlignUp8(lpTail + sizeof(EA::Thread::ThreadId)));

    lpSelf->mpSystemMutex->Init(&lMutexParams);
    lpSelf->mpCommandMutex->Init(&lMutexParams);

    std::memset(lpSelf->maucDebugFeatures, 1, 6); // XMemSet(this + 0x10FB, 1, 6)

    lpSelf->miRefCount = 0;
    lpSelf->mpMasteringSubMix = 0;
    lpSelf->mpPlugInRegistry = 0;
    lpSelf->muMixerCpuTicks = 0;
    lpSelf->mfSampleRate = 0.0f;  // flt_82001CC0
    lpSelf->muCommandExecutionCpuTicks = 0;
    lpSelf->muTimerExecutionCpuTicks = 0;
    lpSelf->muBalanceCycles = 0;
    lpSelf->muExpelVoicesCpuTicks = 0;
    lpSelf->mfSystemTime = 0.0;    // dbl_82001CA8
    lpSelf->mfCpuLoadPercent = 90.0f; // flt_82004F64

    // The X360 immediate 8 is its sizeof(VoiceListNode) {Voice*, u32} -- host sizeof here.
    lpSelf->mppVoiceListNodes = static_cast<VoiceActiveNode *>(
        allocator->Alloc(sizeof(VoiceActiveNode), "rw::audio::core::System::mpVoiceListNodes",
                         1, 16, 0));
    if (lpSelf->mppVoiceListNodes)
    {
        lpSelf->muActiveVoiceCount = 0;
        lpSelf->muActiveVoiceCapacity = 0;
        lpSelf->muDeferredRingHighWater = 0;
        lpSelf->muDeferredRingCursor = 0;
        lpSelf->muCommandBufferSize = commandBufferSize;
        lpSelf->muFrameCounter = 1;

        lpSelf->mpDeferredRingBase = static_cast<char *>(
            allocator->Alloc(commandBufferSize, "rw::audio::core::CommandBuffer", 1, 4, 0));
        if (lpSelf->mpDeferredRingBase)
        {
            lpSelf->mpDecoderRegistry = 0;
            lpSelf->mpEncoderRegistry = 0;
            lpSelf->mpProfiler = 0;
            lpSelf->mpJobScheduler = 0;
            lpSelf->mpfnLock = 0;
            lpSelf->mpfnUnlock = 0;
            lpSelf->mpfnIsLocked = 0;
            lpSelf->mucThreadProcessor = 4;
            lpSelf->miThreadPriority = 15;
            lpSelf->muThreadStackSize = 0;
            return lpSelf;
        }
    }

    // Failure: give back whatever was allocated, through the System's OWN allocator slot
    // (the asm reloads it from +0x14 each time) -- then the System block itself.
    if (lpSelf->mpDeferredRingBase)
        lpSelf->mpAllocator->Free(lpSelf->mpDeferredRingBase, 0);
    if (lpSelf->mppVoiceListNodes)
        lpSelf->mpAllocator->Free(lpSelf->mppVoiceListNodes, 0);
    lpSelf->mpAllocator->Free(lpSelf, 0);
    return 0;
}

// -------------------------------------------------------------------------------------
// DefaultPhysicalAlloc @0x82B6BE88 -- XPhysicalAlloc(size, MAXULONG_PTR, align, protect).
// (The fourth parameter is passed by the call site but unused by the default.)
// -------------------------------------------------------------------------------------
void *System::DefaultPhysicalAlloc(u32 size, u32 align, u32 protect, u32 /*unused*/)
{
    return XPhysicalAlloc(size, 0xFFFFFFFFUL, align, protect);
}

// -------------------------------------------------------------------------------------
// DefaultPhysicalFree @0x82B6BE98 -- b XPhysicalFree(block).
// -------------------------------------------------------------------------------------
void System::DefaultPhysicalFree(void *block)
{
    XPhysicalFree(block);
}

// -------------------------------------------------------------------------------------
// PhysicalAlloc @0x82B6DCD8 -- lazily seed the physical hooks, then allocate through them.
// -------------------------------------------------------------------------------------
void *System::PhysicalAlloc(System *self, u32 size, u32 align, u32 protect, u32 unused)
{
    if (!self->mpfnPhysicalAlloc)
    {
        self->mpfnPhysicalAlloc = &DefaultPhysicalAlloc;
        self->mpfnPhysicalFree = &DefaultPhysicalFree;
    }
    return self->mpfnPhysicalAlloc(size, align, protect, unused);
}

// -------------------------------------------------------------------------------------
// PhysicalFree @0x82B6BE70 -- free through the physical hook.
// -------------------------------------------------------------------------------------
void System::PhysicalFree(System *self, void *block)
{
    self->mpfnPhysicalFree(block);
}

// -------------------------------------------------------------------------------------
// Free @0x82B6BE48 -- free `mem` through `allocatorOverride`, or the System's own allocator
// when null.
// -------------------------------------------------------------------------------------
void System::Free(System *self, void *mem, EA::Allocator::ICoreAllocator *allocatorOverride)
{
    EA::Allocator::ICoreAllocator *allocator =
        allocatorOverride ? allocatorOverride : self->mpAllocator;
    allocator->Free(mem, 0);
}

// -------------------------------------------------------------------------------------
// ExecuteCommandsLock @0x82B6BCA0 / ExecuteCommandsUnlock @0x82B6BCB0 -- the +0x48 mutex.
// -------------------------------------------------------------------------------------
int System::ExecuteCommandsLock(System *self)
{
    return self->mpCommandMutex->Lock();
}

int System::ExecuteCommandsUnlock(System *self)
{
    return self->mpCommandMutex->Unlock();
}

// -------------------------------------------------------------------------------------
// Lock @0x82B6BCC8 / Unlock @0x82B6BCF0 -- prefer the swappable hook (@+0x40/+0x44); fall
// back to the +0x4C system mutex. The hooks are NULLARY (see the typedef note in
// PlugIn.h: the PDB prototypes them void()(), and ExecuteCommands' second lock site
// dispatches them with a stale r3).
// -------------------------------------------------------------------------------------
void System::Lock(System *self)
{
    if (self->mpfnLock)
        self->mpfnLock();
    else
        self->mpSystemMutex->Lock();
}

void System::Unlock(System *self)
{
    if (self->mpfnUnlock)
        self->mpfnUnlock();
    else
        self->mpSystemMutex->Unlock();
}

// -------------------------------------------------------------------------------------
// IsLocked @0x82B6BD10 -- query the "is locked" hook (@+0x3C); 0 when unset.
// -------------------------------------------------------------------------------------
int System::IsLocked(System *self)
{
    if (self->mpfnIsLocked)
        return self->mpfnIsLocked();
    return 0;
}

// -------------------------------------------------------------------------------------
// IsCommandComplete @0x82B6BD30 -- under a scratch mutex, test commandId against the
// executed-frame counter. (The local mutex is constructed, locked, unlocked and destroyed
// exactly as the asm does -- it guards the single counter read.)
// -------------------------------------------------------------------------------------
bool System::IsCommandComplete(System *self, u32 commandId)
{
    EA::Thread::Mutex mutex;
    mutex.Lock();
    bool complete = commandId < self->muFrameCounter;
    mutex.Unlock();
    return complete;
}

// -------------------------------------------------------------------------------------
// GetPlugInRegistry @0x82B6DDC0 -- lazily create the registry (@+0x28).
// -------------------------------------------------------------------------------------
PlugInRegistry *System::GetPlugInRegistry(System *self)
{
    if (!self->mpPlugInRegistry)
        self->mpPlugInRegistry = PlugInRegistry::CreateInstance(off_83271928);
    return self->mpPlugInRegistry;
}

// -------------------------------------------------------------------------------------
// GetProfiler @0x82B6DE08 -- lazily seed the file-scope Profiler singleton (@dword_832718F0)
// and cache it at mpProfiler (@+0x34). Field order is the asm's store order.
// -------------------------------------------------------------------------------------
Profiler *System::GetProfiler(System *self)
{
    if (!self->mpProfiler)
    {
        skProfiler.mfNextReportTime = 0.0;          // dbl_82001CA8
        skProfiler.mfAverage = 0.0f;                // flt_82001CC0
        skProfiler.mpVTable = KPF_ProfilerVTable;   // off_8214B260
        skProfiler.mpTimeSource = self;             // the profiler's wall clock is mfSystemTime
        skProfiler.muAccumTicks = 0;
        skProfiler.muReportBaseCycle = 0;
        skProfiler.muBeginCycle = 0;
        skProfiler.muLastEndCycle = 0;
        skProfiler.muMaxTicks = 0;
        skProfiler.muHasBaseline = 0;
        skProfiler.muWrapCount = 0;
        self->mpProfiler = &skProfiler;
    }
    return self->mpProfiler;
}

// -------------------------------------------------------------------------------------
// ReleaseHandler @0x82B6EA50 -- deferred teardown: release every active voice, then every
// voice threaded onto the expelled list, returning the record's own size (the X360 `li
// r3,8` IS sizeof(SystemReleaseCommand) on X360; ExecuteCommands advances the ring by
// this return, so on the host it must be the HOST record size -- the record-stride rule).
// -------------------------------------------------------------------------------------
int System::ReleaseHandler(void *cmd)
{
    System *self = static_cast<SystemReleaseCommand *>(cmd)->mpSystem;

    while (self->muActiveVoiceCount)
        Voice::ReleaseImmediate(self->mppVoiceListNodes->mpVoice, 0);

    VoiceListLink *link = self->mpExpelledVoiceList;
    while (link)
    {
        // The link is embedded at Voice::mExpelLink; recover the owning voice (container-of),
        // advance before releasing (the release unlinks it).
        Voice *voice = reinterpret_cast<Voice *>(
            reinterpret_cast<char *>(link) - offsetof(Voice, mExpelLink));
        link = link->mpNext;
        Voice::ReleaseImmediate(voice, 0);
    }
    return static_cast<int>(sizeof(SystemReleaseCommand));
}

// -------------------------------------------------------------------------------------
// Release @0x82B6F998 -- tear the audio sub-system down. Reconstructed store-for-store
// from the ARTIST asm:
//   1. under the system lock, push a SystemReleaseCommand (the deferred ReleaseHandler)
//      into the command ring;
//   2. holding the command mutex, pump ExecuteCommands + sleep 1 ms until the ring drains
//      AND the reference count falls to <= 0;
//   3. under the system lock again, free the XMA hardware contexts, the three registries,
//      the profiler, the TimerManager, the ring + voice-node allocations, and finally the
//      System block itself; clear the singleton and unlock.
// The X360 r3 return is the dead unlock-result passthrough (declared void, as Lock/Unlock).
// -------------------------------------------------------------------------------------
void System::Release(System *self)
{
    // ---- 1. queue the deferred teardown record ---------------------------------------
    Lock(self);
    SystemReleaseCommand *command = reinterpret_cast<SystemReleaseCommand *>(
        self->mpDeferredRingBase + self->muDeferredRingCursor);
    // RECORD STRIDE (X360-literal trap): the asm's `addi r11,r11,8` IS the console
    // sizeof(SystemReleaseCommand) ({int(*)(void*), System*} = 4+4). On the host the same
    // record is 16 bytes, and ExecuteCommands advances the ring by the handler's return --
    // ReleaseHandler above likewise returns sizeof(SystemReleaseCommand) -- so the enqueue
    // stride must be the HOST sizeof, never the literal 8. Cursor is advanced before the
    // fields are written, exactly as the asm orders the stores.
    self->muDeferredRingCursor += static_cast<u32>(sizeof(SystemReleaseCommand));
    command->mpHandler = &ReleaseHandler;
    command->mpSystem = self;
    Unlock(self);

    // ---- 2. pump the ring dry --------------------------------------------------------
    // The command mutex is held across the WHOLE loop, the 1 ms sleeps included (faithful:
    // the asm locks +0x48 once, before the loop, and unlocks once after it).
    self->mpCommandMutex->Lock();
    while (self->muDeferredRingCursor != 0 || self->miRefCount > 0)
    {
        ExecuteCommands(self);
        u32 luSleepMs = 1;
        EA::Thread::ThreadSleep(&luSleepMs);
    }
    self->mpCommandMutex->Unlock();

    // ---- 3. teardown, under the system lock ------------------------------------------
    Lock(self);

    // Nullary: the codec's shared XMA contexts live in file-scope statics (the r3 at this
    // call site is the stale unlock result).
    EaXmaDec::DeallocateResources();

    // Each registry is freed through ITS OWN back-pointer's allocator, exactly as the asm
    // chains registry -> mpSystem -> mpAllocator -> vtable slot 0xC (Free).
    PlugInRegistry *plugInRegistry = self->mpPlugInRegistry;
    if (plugInRegistry)
        plugInRegistry->mpSystem->mpAllocator->Free(plugInRegistry, 0);

    DecoderRegistry *decoderRegistry = self->mpDecoderRegistry;
    if (decoderRegistry)
        decoderRegistry->mpSystem->mpAllocator->Free(decoderRegistry, 0);

    EncoderRegistry *encoderRegistry = self->mpEncoderRegistry;
    if (encoderRegistry)
    {
        // FLAG (semantic parity, not a literal transcription): the asm walks
        // encoderRegistry->mpSystem (@+0x0C, the same shape as DecoderRegistry) to reach
        // the allocator. EncoderRegistry is forward-declared only (PlugIn.h) -- it has no
        // reconstructed layout yet -- so that member is not reachable by name, and a raw
        // +0x0C cast would be an offset hack. Every registry's back-pointer is this very
        // System by construction (CreateInstance is the only producer), so the System's own
        // allocator is the identical object. Re-point this at
        // `encoderRegistry->mpSystem->mpAllocator` when the EncoderRegistry layout is homed.
        self->mpAllocator->Free(encoderRegistry, 0);
    }

    // The profiler is torn down through vtable slot 0 of off_8214B260, whose slot 0 is
    // RECOVERED as Profiler::Release (leaf class -> the static resolution is exact).
    Profiler *profiler = self->mpProfiler;
    if (profiler)
        Profiler::Release(profiler);

    TimerManager::Release(&self->mTimerManager);

    if (self->mpDeferredRingBase)
        self->mpAllocator->Free(self->mpDeferredRingBase, 0);
    if (self->mppVoiceListNodes)
        self->mpAllocator->Free(self->mppVoiceListNodes, 0);
    self->mpAllocator->Free(self, 0);

    off_83271928 = 0;

    // FLAG (original defect, reproduced deliberately): the final unlock re-reads the
    // JUST-FREED System block (the +0x44 hook, else the +0x4C mutex pointer) -- a
    // use-after-free in the shipped binary. The ORDER is faithful to the asm; do not
    // "fix" it by hoisting the unlock above the frees.
    Unlock(self);
}

// -------------------------------------------------------------------------------------
// RemoveTimer @0x82B6EB80 -- forward to the embedded TimerManager (@+0x60).
// -------------------------------------------------------------------------------------
void System::RemoveTimer(System *self, TimerHandle *handle)
{
    TimerManager::RemoveTimer(&self->mTimerManager, handle);
}

// -------------------------------------------------------------------------------------
// RemoveVoiceFromExpulsionCandidateList @0x82B6BDA8 -- find `voice` in the candidate array
// and swap-remove it (move the last entry into its slot).
// -------------------------------------------------------------------------------------
void System::RemoveVoiceFromExpulsionCandidateList(System *self, Voice *voice)
{
    u32 count = self->muExpelAfterDecayCount;
    if (!count)
        return;

    u32 index = 0;
    while (self->mpExpelAfterDecayList[index] != voice)
    {
        if (++index >= self->muExpelAfterDecayCount)
            return;
    }

    if (index < count)
    {
        u32 last = count - 1;
        self->muExpelAfterDecayCount = last;
        if (count != 1 && last != index)
            self->mpExpelAfterDecayList[index] = self->mpExpelAfterDecayList[last];
    }
}

// -------------------------------------------------------------------------------------
// SetRwAudioCoreThreadId @0x82B6BCB8 -- store *pThreadId into the thread-id slot (@+0x54).
// -------------------------------------------------------------------------------------
System *System::SetRwAudioCoreThreadId(System *self, u32 *pThreadId)
{
    *self->mppThreadId = *pThreadId;
    return self;
}

// -------------------------------------------------------------------------------------
// SetThreadProcessor @0x82B6BC98 -- record the audio-update hardware thread (@+0x10FA).
// -------------------------------------------------------------------------------------
System *System::SetThreadProcessor(System *self, u8 processor)
{
    self->mucThreadProcessor = processor;
    return self;
}

// -------------------------------------------------------------------------------------
// UpdateExpellingVoices @0x82B6EAD0 -- walk the expel-after-decay candidates: for a still-
// expelling voice, clamp its fade window and expel it once fully faded; otherwise drop it
// from the list by swap-remove. Loops while entries remain.
// -------------------------------------------------------------------------------------
Voice *System::UpdateExpellingVoices(System *self)
{
    // The asm returns r3, which is the System on entry (used only when the candidate list
    // is empty) and the last-touched voice otherwise. The sole caller discards it, so this
    // register passthrough is not load-bearing.
    Voice *result = reinterpret_cast<Voice *>(self);
    u32 index = 0;
    if (self->muExpelAfterDecayCount)
    {
        do
        {
            Voice *voice = self->mpExpelAfterDecayList[index];
            result = voice;
            if (voice->mucState == 1)
            {
                if (voice->mfFadeEnd < voice->mfFadeStart)
                    voice->mfFadeEnd = voice->mfFadeStart;
                if (voice->mfParam2C >= voice->mfFadeEnd)
                    result = Voice::ExpelImmediate(voice, 3);
                ++index;
            }
            else
            {
                u32 count = self->muExpelAfterDecayCount;
                if (index < count)
                {
                    u32 last = count - 1;
                    self->muExpelAfterDecayCount = last;
                    if (last != 0 && last != index)
                        self->mpExpelAfterDecayList[index] = self->mpExpelAfterDecayList[last];
                }
            }
        } while (index < self->muExpelAfterDecayCount);
    }
    return result;
}

// -------------------------------------------------------------------------------------
// VectorToCsisMutex @0x82B6FC50 -- route the lock/unlock hooks to the Csis primitives.
// -------------------------------------------------------------------------------------
System *System::VectorToCsisMutex(System *self)
{
    if (self->mpfnLock != &CsisMutexLock)
    {
        self->mpfnLock = &CsisMutexLock;
        self->mpfnUnlock = &CsisMutexUnlock;
    }
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
