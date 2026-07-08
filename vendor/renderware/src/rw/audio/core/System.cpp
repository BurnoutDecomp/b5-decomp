// =====================================================================================
// rw::audio::core::System -- the owning EARenderWare "rwaudio" sub-system: the base/hub
// of the audio-core family (DelayLine / DelayFilter / Decoder / Gain / Send / Voice /
// TimerManager all hang off it). Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC);
// the asm is authoritative for every store. The reconstructed layout lives in PlugIn.h
// (the canonical home of the plug-in/System family). No Feb-2007 source, no DecFIGS DWARF,
// and no ProStreet08 PDB entry exist for these bodies.
//
// FOUR ledger functions are intentionally NOT reconstructed here (BLOCKED -- honest gaps,
// each noted at the bottom of this file):
//   CreateInstance   -- carves its mutex sub-objects out of the object's own tail at a
//                       hard-coded X360 offset ((this+0x110F)&~7, 0x30 strides) that cannot
//                       be reproduced with x64-width members, and its EA::Thread::Mutex
//                       constructor callee is unresolved in the dossier.
//   Release          -- tears down un-homed sub-objects (@+0x2C/+0x30) through multi-level
//                       vtable dispatch whose element types are not homed in this TU.
//   ExecuteCommands  -- walks an un-homed intrusive "processor" list (@+0x5C) via a
//                       container-of + virtual dispatch on a type not homed here.
//   GetProfiler      -- initialises a file-scope profiler singleton whose v-table
//                       (off_8214B260) is an un-homed foreign static and whose +0x08 seed
//                       is an unrecovered rodata double (dbl_82001CA8).
// =====================================================================================

#include "rw/audio/core/PlugIn.h"       // rw::audio::core::System (+ PlugInRegistry, hooks)
#include "rw/audio/core/Voice.h"        // Voice / VoiceListLink / VoiceActiveNode
#include "rw/audio/core/TimerManager.h" // rw::audio::core::TimerManager
#include "SDKs/EATech/eathread/eathread_mutex.h" // EA::Thread::Mutex

#include <cstddef> // offsetof

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

// The audio-core System singleton (off_83271928). Set by CreateInstance and cleared by
// Release (both live elsewhere in this TU family / are BLOCKED here); read by
// GetPlugInRegistry. Defined here -- its home -- as zero-initialised storage so the many
// consumers that `extern "C"` it link. C linkage: the name is namespace-independent.
extern "C" System *off_83271928 = 0;

// The Csis integration's mutex primitives, installed into the System lock hooks by
// VectorToCsisMutex. Their bodies live in the Csis glue TU; here they are referenced only
// by address, so a forward declaration is sufficient (and faithful -- the asm stores the
// function addresses).
int CsisMutexLock(System *self);
int CsisMutexUnlock(System *self);

// The shared 4-word table the constructor points mpObjectTable at (dword_83271930..0x3C).
// Opaque here (zeroed by the ctor, never otherwise touched by this TU).
static u32 skSystemObjectTable[4] = { 0, 0, 0, 0 };

// The 8-byte deferred-teardown record ReleaseHandler reads: { handler, System* }.
struct SystemReleaseCommand
{
    int (*mpHandler)(void *); // +0x00 -- &System::ReleaseHandler
    System *mpSystem;          // +0x04 -- the owning System
};

// -------------------------------------------------------------------------------------
// System_ctor @0x82B6DD20 -- the placement constructor. Clears the two list heads,
// default-constructs the embedded TimerManager, zeroes + points at the shared object table.
// -------------------------------------------------------------------------------------
System *System::System_ctor(System *self)
{
    self->mpExpelledVoiceList = 0;   // +0x10
    self->mpProcessorListHead = 0;   // +0x5C
    TimerManager::TimerManager_ctor(&self->mTimerManager); // +0x60
    skSystemObjectTable[0] = 0;
    skSystemObjectTable[1] = 0;
    skSystemObjectTable[2] = 0;
    skSystemObjectTable[3] = 0;
    self->mpObjectTable = skSystemObjectTable; // +0x00
    return self;
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
// back to the +0x4C system mutex.
// -------------------------------------------------------------------------------------
void System::Lock(System *self)
{
    if (self->mpfnLock)
        self->mpfnLock(self);
    else
        self->mpSystemMutex->Lock();
}

void System::Unlock(System *self)
{
    if (self->mpfnUnlock)
        self->mpfnUnlock(self);
    else
        self->mpSystemMutex->Unlock();
}

// -------------------------------------------------------------------------------------
// IsLocked @0x82B6BD10 -- query the "is locked" hook (@+0x3C); 0 when unset.
// -------------------------------------------------------------------------------------
int System::IsLocked(System *self)
{
    if (self->mpfnIsLocked)
        return self->mpfnIsLocked(self);
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
// ReleaseHandler @0x82B6EA50 -- deferred teardown: release every active voice, then every
// voice threaded onto the expelled list, returning the record's own 8-byte size.
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
    return 8;
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
