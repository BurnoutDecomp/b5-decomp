#pragma once

// =====================================================================================
// Canonical RenderWare audio-core home for the plug-in family:
//   rw::audio::core::PlugIn          -- abstract processing-graph node base
//   rw::audio::core::PlugInDescRunTime      -- per-plug-in registry record (run-time type)
//   rw::audio::core::PlugInRegistry  -- intrusive singly-linked list of PlugInDescRunTime
//   rw::audio::core::Iir2            -- a 2nd-order (biquad) IIR filter kernel
//   rw::audio::core::System          -- owning sub-system (only the surface this
//                                       family touches is reconstructed here)
//
// EARenderWare "rwaudio" middleware. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC) -- the X360 asm is authoritative for member layout. There is NO
// matching translation unit in the Feb-2007 PS3 leak and no DecFIGS DWARF for
// these types, so every offset below is grounded directly in the disassembly of
// the bodied member functions (see the .cpp siblings of this header).
//
// Layout is byte-for-byte from the asm; member NAMES are reconstructed to match
// the observed semantics and the public rwaudio API shape. Lowercase rw::audio::
// namespaces match the third-party middleware API (per CXX_NAMING_CONVENTIONS:
// lowercase namespaces are acceptable for matching a third-party/legacy API).
// =====================================================================================

#include "types.hpp" // f32, u32 (project primitives)
#include "coreallocator/icoreallocator_interface.h" // EA::Allocator::ICoreAllocator
#include "rw/audio/core/TimerManager.h" // rw::audio::core::TimerManager -- embedded by value in System (+0x60)
#include "rw/audio/core/ITask.h" // rw::audio::core::ITask / ListDNode -- the System timer list (+0x5C)

#include <new> // placement new (System::New2 constructs in the allocation)

// EA::Thread::Mutex is used only through pointer members of System here; the full
// definition (eathread_mutex.h) is pulled in by the System .cpp, not by this widely
// included header.
namespace EA { namespace Thread { class Mutex; } }
// EA::Jobs::JobScheduler is a pointer-only System member (@+0x38); the ARTIST build only
// ever zeroes the slot (CreateInstance), so a forward declaration is the whole surface.
namespace EA { namespace Jobs { class JobScheduler; } }

namespace rw
{
namespace audio
{
namespace core
{

class PlugIn;
class PlugInDescRunTime;  // rwaudio PDB name (the header's former "PlugInInfo") -- registry RTTI record
class PlugInRegistry;

// off_820AA810 -- the SHARED base-PlugIn-vtable host sentinel (defined once in
// PlugIn.cpp; see its note). Store-only.
extern void* const gpBasePlugInVTableSentinel;
class System;
class Voice;              // rwaudio PDB: PlugIn::mpVoice (+0x08)
struct VoiceListLink;     // intrusive expelled-voice list node (defined in Voice.h)
struct VoiceActiveNode;   // one entry of System::mppVoiceListNodes (defined in Voice.h)
class DecoderRegistry;    // System::mpDecoderRegistry (+0x2C); defined in DecoderRegistry.h
class EncoderRegistry;    // System::mpEncoderRegistry (+0x30); ProStreet PDB [sizeof=16]:
                          //   ListQueue mEncoderDescList {phead,ptail,entries} + System* mpSystem @+0x0C.
                          //   No ARTIST body touches its interior beyond the teardown in
                          //   System::Release (frees it via its +0x0C back-pointer), so it is
                          //   forward-declared only until an encoder TU needs the layout.
class Profiler;           // System::mpProfiler (+0x34); defined in Profiler.h

// The System's swappable lock/unlock/is-locked hooks (function slots @+0x3C/+0x40/+0x44).
// NULLARY -- rwaudio PDB: bool (*mMutexIsLockedFn)() / void (*mMutexLockFn)() /
// void (*mMutexUnlockFn)(); confirmed by the ARTIST asm: the second lock site inside
// ExecuteCommands @0x82B6F7D4 dispatches the hook while r3 still holds a GetCpuCycle
// result, so the hook cannot take the System. (An earlier recon pass modelled these as
// taking System* because the FIRST call sites happen to leave `this` in r3.) When null
// the System falls back to its own EA::Thread::Mutex; when set (VectorToCsisMutex) they
// route to the Csis integration's global-mutex primitives, which are likewise nullary
// (Csis::System::Lock @0x82B0F248 ignores r3, waits on a global handle).
typedef bool (*SystemMutexIsLockedHook)();
typedef void (*SystemMutexHook)();
// Physical (non-cached) memory allocator hooks (@+0x18/+0x1C). PhysicalAlloc lazily seeds
// them to the Xbox defaults. The alloc hook is called with four arguments (only the first
// three are consumed by the default); the free hook with the block pointer. The fourth
// argument was a 32-bit word on the console, but the call sites ride a debug-name string
// through it (EaXmaDec::AllocateResources @0x82B8F580 passes "XMA input buffers" /
// "XMA output buffers"), so on the x64 host it must stay pointer-wide.
typedef void *(*PhysicalAllocHook)(u32 size, u32 align, u32 protect, const char *pName);
typedef void (*PhysicalFreeHook)(void *block);

// -------------------------------------------------------------------------------------
// PlugIn -- base class for a node in the audio processing graph.
//
// Layout grounded in PlugIn::CreateInstance @0x82B6A818 (the placement-construct path).
// (The old pre-reconcile per-slot table that stood here -- naming +0x08 "mpInput
// (upstream PlugIn / input handle)" etc. -- is RETIRED 2026-08-25 wave 4: it
// contradicted the PDB reconcile below in the same header. The a2 argument at +0x08
// is the OWNING VOICE: CreateInstance's only caller is Voice::CreateInstance
// @0x82B6EC50 passing the fresh Voice in r4. See the reconciled member list below.)
// -------------------------------------------------------------------------------------
class PlugIn
{
public:
    // The attribute slot pair the GetAttribute/SetAttribute helpers index into.
    // SetAttribute defers the actual write to SetAttributeHandler via the System's
    // deferred-command ring (see SetAttribute body), so this is the resolved table.
    // rwaudio PDB: rw::audio::core::PlugIn::Attribute_t (an 8-byte attribute slot).
    struct Attribute_t
    {
        f32 mfValue; // +0x00 -- the attribute value (8-byte stride: value + pad/id)
        u32 muPad;   // +0x04
    };

    // vtable[0] / vtable[1] / vtable[3] are virtual; declared so dispatch compiles.
    virtual ~PlugIn() {}
    virtual int Event() { return 0; }     // vt[1] -- reached through PlugIn::Event
    virtual int VFunc2() { return 0; }     // vt[2] -- unmodelled slot
    virtual void Destroy(int /*a*/) {}     // vt[3] -- reached through CreateInstance fail path

    // ---- bodied in PlugIn.cpp (offsets above are authoritative) ----
    static PlugIn *CreateInstance(PlugIn *self, Voice *voice, PlugInDescRunTime *pDesc,
                                  void *typeRecord, char flag);
    static int Event(PlugIn *self);
    static PlugIn *GetAttribute(PlugIn *self, int index, f32 *outValue);
    static PlugIn *SetAttribute(PlugIn *self, int index, f32 value);
    static int SetAttributeHandler(void *cmd);

    // FLAG (rwaudio PDB reconcile -- IDA Files/ProStreet08Milestone.pdb,
    // rw::audio::core::PlugIn [sizeof=36]): the ARTIST-asm-derived offsets below MATCH
    // the PDB exactly (every field aligns), so the PDB member NAMES/TYPES are
    // authoritative and replace the earlier semantic guesses. x64 widths (real pointers);
    // the +0xNN are the X360 (32-bit) offsets, now PDB-verified.
    void *mpVTable;                       // +0x00  vfptr
    System *mpSystemUseGetSystemAccessor; // +0x04  (PDB name; access via GetSystem())
    Voice *mpVoice;                       // +0x08  (was guessed mpInput/PlugIn*)
    Attribute_t *mpAttribute;             // +0x0C  (was mpAttributes)
    PlugInDescRunTime *mpPlugInDescRunTime; // +0x10 (was guessed mpFactory/System*)
    f32 mLatencyInSamples;                // +0x14  (was mfAttrib0)
    f32 mDecaySamples;                    // +0x18  (was mfAttrib1)
    u32 mCpuTicks;                        // +0x1C  (was mState)
    u8 mInputChannels;                    // +0x20  (was mbFlag20)
    u8 mOutputChannels;                   // +0x21  (was mbFlag21)
};

// A queued SetAttribute command, pushed into the System command ring by
// SetAttribute and replayed by SetAttributeHandler. Grounded in SetAttribute
// @0x82B6E848 (handler / target / index / value; 16 bytes on the X360).
// RECORD-STRIDE RULE (X360-literal trap): the producer's cursor advance and the
// handler's return are BOTH this record's size, and on the host that is
// sizeof(PlugInSetAttributeCommand) (24 on x64 -- the pointers widened), never the
// console literal 16. Producer and handler must always agree, or each enqueue
// overruns its reserved stride and the ring replay eats the record's tail.
struct PlugInSetAttributeCommand
{
    int (*mpHandler)(void *); // +0x00 -- always &PlugIn::SetAttributeHandler
    PlugIn *mpTarget;          // +0x04 -- the PlugIn whose attribute is written
    int miIndex;               // +0x08 -- attribute index
    f32 mfValue;               // +0x0C -- the value to store
};

// -------------------------------------------------------------------------------------
// PlugInDescRunTime -- the run-time-type record for one registered plug-in. Forms an
// intrusive singly-linked list inside the registry. Layout grounded in
// PlugInRegistry::GetPlugInHandle @0x82B6A908 and RegisterPlugInRunTime @0x82B6A938:
//   ...                                  (object header / v-tables / construct hooks)
//   +0x24  mpNext   (the link field; the registry stores &mpNext as its node handle,
//                    and the public "handle" is node = &mpNext - 0x24)
//   +0x28  muId     (the registration id, compared in GetPlugInHandle)
//   +0x32  mbSeq    (byte; the registry's running id counter snapshotted here)
//
// The registry threads nodes by their +0x24 link; GetPlugInHandle walks links and
// returns the owning PlugInDescRunTime (link - 0x24).
// -------------------------------------------------------------------------------------
class PlugInDescRunTime
{
public:
    // Only the fields the bodied registry walks touch are modelled by name; the
    // gap before +0x24 is the (un-homed) object header / per-type hooks and is
    // preserved as opaque storage so offsets stay exact.
    char mHeader[0x24]; // +0x00 .. +0x23 -- opaque object header (un-homed here)
    // FLAG (rwaudio PDB reconcile -- ProStreet08Milestone.pdb): PDB struct
    // rw::audio::core::PlugInDescRunTime [sizeof=52] names the +0x00..+0x23 prefix this
    // recon left opaque: +0x00 char* name; +0x04 GetSize(PlugInConfig*); +0x08
    // CreateInstance(PlugIn*,void*); +0x0C pPreProcess; +0x10 pProcess; +0x14 pChannelMaps;
    // +0x18 pParameterDescRunTime; +0x1C pEventDescRunTime; +0x20 pPlugInDescToolSide; then
    // +0x24 listNode (== mpNext), +0x28 guid (== muId), +0x2C plugInType, +0x2D
    // numConstructorParameters, +0x2E numAttributes... Kept opaque here (the registry walk
    // only touches +0x24/+0x28); expand when a TU needs the descriptor body.
    void *mpNext;       // +0x24 -- intrusive next link (PDB listNode)
    u32 muId;           // +0x28 -- registration id (PDB guid)
    char mPad2C[0x32 - 0x2C]; // +0x2C .. +0x31 -- opaque (PDB plugInType/numCtorParams/numAttributes..)
    char mbSeq;         // +0x32 -- registry sequence snapshot
};

// -------------------------------------------------------------------------------------
// PlugInRegistry -- owns the linked list of PlugInDescRunTime records. Layout grounded in
// CreateInstance @0x82B6DBC0, GetPlugInHandle @0x82B6A908, RegisterPlugInRunTime
// @0x82B6A938:
// FLAG (rwaudio PDB reconcile -- rw::audio::core::PlugInRegistry [sizeof=24], offsets
// MATCH): PDB models +0x00..+0x08 as a ListQueue mPlugInDescRunTimeList{phead,ptail,
// entries}; +0x0C mpEnumerator; +0x10 mpSystem; +0x14 mCurrentRegistryIndex. Kept the flat
// head/tail/count shape (ARTIST asm models it flat) and applied PDB names to the two
// previously-guessed members.
//   +0x00  mpHead    (ListQueue.phead -- head node's +0x24 link, i.e. node->mpNext slot)
//   +0x04  mppTail   (ListQueue.ptail -- tail link slot; set on first registration)
//   +0x08  muCount   (ListQueue.entries -- number of registered plug-ins)
//   +0x0C  mpEnumerator (PDB; was mField0C; int/ListNode*, init 0)
//   +0x10  mpSystem  (back-pointer to the owning System, set in CreateInstance)
//   +0x14  mCurrentRegistryIndex (PDB; was mbNextSeq; running index, post-inc per register)
// Allocated 24 bytes (0x18), 16-aligned, via System::New2<PlugInRegistry>.
// -------------------------------------------------------------------------------------
class PlugInRegistry
{
public:
    // Default ctor = zero the ListQueue state. Grounded in System::New2<PlugInRegistry>
    // @0x82B6C350, whose inlined construction zeroes exactly these three words (the
    // remaining members are written by CreateInstance afterwards).
    PlugInRegistry() : mpHead(0), mppTail(0), muCount(0) {}

    static PlugInRegistry *CreateInstance(System *system);
    static void *GetPlugInHandle(PlugInRegistry *self, int id);
    static void *RegisterPlugInRunTime(PlugInRegistry *self, PlugInDescRunTime *info);

    void *mpHead;                 // +0x00  ListQueue.phead
    void **mppTail;               // +0x04  ListQueue.ptail
    u32 muCount;                  // +0x08  ListQueue.entries
    int mpEnumerator;             // +0x0C  (was mField0C)
    System *mpSystem;             // +0x10
    char mCurrentRegistryIndex;   // +0x14  (was mbNextSeq)
};

// -------------------------------------------------------------------------------------
// System -- the owning rwaudio sub-system: the hub every audio-core family hangs off
// (the singleton lives at off_83271928, defined in System.cpp). The layout below is the
// FULL System (rwaudio PDB reconcile, wave E -- see the FLAG note over the member list);
// the bodies live in System.cpp. The deferred-command ring (mpDeferredRingBase +
// muDeferredRingCursor) is the queue PlugIn::SetAttribute / Voice::Release / etc. push
// {handler, args...} records into (8-24 bytes each on the X360); ExecuteCommands replays
// it once per audio frame: it calls each record's leading handler with the record's own
// address and advances the byte cursor by the handler's return. THE RING CONTRACT: a
// record's producer advance and its handler return are the SAME value, and on the host
// that value is the HOST sizeof of the record struct (the X360 immediates are console
// record sizes that do not survive pointer widening). The full producer/handler table
// with the asm evidence lives in scratchpad/waveF/audio_ring.spec.md.
//
// New2<T> is the templated allocate-and-default-construct helper (mangled
//   ??$New2@VPlugInRegistry@...@System@... ); its three X360 instantiations belong to
// this TU and are reconstructed as the single uniform inline template below.
// -------------------------------------------------------------------------------------
class System
{
public:
    template <class T>
    static void New2(System *self, T **outResult, const char *name, unsigned size,
                     unsigned align, EA::Allocator::ICoreAllocator *allocator);

    // Placement-construct the System's leaf fields: clear the expelled-voice list head and
    // the timer-task list head, default-construct the embedded TimerManager, and wire the
    // object table pointer. (The full CreateInstance allocate-and-init path is a separate
    // ledger function.) X360 @0x82B6DD20.
    static System *System_ctor(System *self);

    // Allocate the singleton System (align 16) through `allocator`, placement-construct it
    // (System_ctor), carve the two mutexes + the two thread-id slots from the allocation
    // tail, allocate the first VoiceListNode entry and the deferred-command ring
    // (`commandBufferSize` bytes), and seed every field. Publishes off_83271928 (BEFORE
    // the mutexes are initialised, and it is NOT cleared on the failure path -- both
    // faithful to the asm). Returns the System, or null when any allocation fails (the
    // partial allocations are freed). X360 @0x82B6F420. Full per-store map:
    // scratchpad/waveE/AudioSystem.spec.md.
    static System *CreateInstance(EA::Allocator::ICoreAllocator *allocator,
                                  u32 commandBufferSize);

    // Tear the System down: queue a deferred ReleaseHandler record into the ring, pump
    // ExecuteCommands (sleeping 1ms per lap, the command mutex held) until the ring is
    // empty and miRefCount drains to 0, then release the XMA contexts, the three
    // registries, the profiler, the TimerManager, the ring + voice-node allocations and
    // finally the System block itself; off_83271928 is cleared. (The X360 r3 return is a
    // dead unlock-result passthrough -- modelled void, same as Lock/Unlock.)
    // X360 @0x82B6F998.
    static void Release(System *self);

    // The audio-frame pump: under the system lock, tick every ITask on the timer list and
    // the TimerManager's due timers; then (unlocked) expel fully-faded voices; then (locked
    // again) replay the deferred-command ring, latch the ring high-water mark, advance
    // muFrameCounter; finally run the deferred-removal timer phase + defragment. The three
    // phases' CPU cycle costs land in muTimerExecutionCpuTicks / muExpelVoicesCpuTicks /
    // muCommandExecutionCpuTicks. (X360 r3 return = dead unlock-result passthrough --
    // modelled void.) X360 @0x82B6F6D0.
    static void ExecuteCommands(System *self);

    // Lazily construct the file-scope Profiler singleton (vtable install + field seeding,
    // time source = this System) and cache it at mpProfiler (@+0x34). X360 @0x82B6DE08.
    static Profiler *GetProfiler(System *self);

    // Free `mem` back through `allocatorOverride` (or, when null, the System's own
    // ICoreAllocator @+0x14). Grounded in Free @0x82B6BE48: v3 = a3 ? a3 : this->mpAllocator;
    // v3->vtable[Free](v3, mem, 0). The third argument is an OPTIONAL ALLOCATOR OVERRIDE
    // (the asm dispatches through it directly), not a flag word -- every call site passes 0.
    static void Free(System *self, void *mem, EA::Allocator::ICoreAllocator *allocatorOverride);

    // Allocate `size` bytes through `allocatorOverride`, or the System's ICoreAllocator
    // (slot +0x14) when null -- the exact allocate counterpart of Free. Bodied in
    // System.cpp from the XEX-decoded @0x82B6BE18 (2026-08-25; the r7 argument the old
    // declaration guessed to be "flags" is the allocator override, tested for null just
    // like Free's -- the real flag word is the constant 1, alignment offset 0). Callers:
    // CMpegBase::AllocateSynth @0x82B8BFF0, EaXmaDec::AllocateResources @0x82B8F580
    // (both pass 0 -> the System's own allocator).
    static void *Alloc(System *self, u32 size, const char *name, u32 align,
                       EA::Allocator::ICoreAllocator *allocatorOverride);

    // Unlink `voice` from the System's expulsion-candidate bookkeeping. The body lives in
    // the System TU (mangled ?RemoveVoiceFromExpulsionCandidateList@System@core@audio@rw@@);
    // declared here because Voice::RemoveActiveVoice @0x82B6C1A8 calls it (r3=System, r4=voice).
    static void RemoveVoiceFromExpulsionCandidateList(System *self, Voice *voice);

    // Take / release the audio-core System mutex that guards the shared voice/module-bank
    // registries and the deferred-command ring. The bodies live in the System TU (mangled
    // ?Lock@System@core@audio@rw@@ / ?Unlock@System@core@audio@rw@@); declared here (r3=System)
    // because the game AEMS code (Snd9::Aems::BeginRemoveModuleBank / IsModuleBankRemoved)
    // and the RWAC scoped-lock guard bracket their registry walks with them.
    static void Lock(System *self);
    static void Unlock(System *self);

    // ---- additional System-TU bodies (grounded in the System.cpp disassembly) ----

    // Xbox physical-memory default hooks (installed lazily by PhysicalAlloc). @0x82B6BE88 /
    // @0x82B6BE98. DefaultPhysicalAlloc forwards to XPhysicalAlloc(size, MAXULONG_PTR, align,
    // protect); DefaultPhysicalFree tail-calls XPhysicalFree(block).
    static void *DefaultPhysicalAlloc(u32 size, u32 align, u32 protect, const char *pName);
    static void DefaultPhysicalFree(void *block);

    // Allocate/free through the physical hooks (@+0x18/+0x1C), seeding them to the defaults
    // on first use. @0x82B6DCD8 / @0x82B6BE70.
    static void *PhysicalAlloc(System *self, u32 size, u32 align, u32 protect,
                               const char *pName);
    static void PhysicalFree(System *self, void *block);

    // Take / release the command-execution mutex (@+0x48; distinct from the +0x4C system
    // mutex used by Lock/Unlock). @0x82B6BCA0 / @0x82B6BCB0.
    static int ExecuteCommandsLock(System *self);
    static int ExecuteCommandsUnlock(System *self);

    // Query the current-lock hook (@+0x3C); 0 when unset. @0x82B6BD10.
    static int IsLocked(System *self);

    // Under a scratch mutex, test whether command `commandId` has been executed
    // (commandId < the executed-frame counter @+0x10E4). @0x82B6BD30.
    static bool IsCommandComplete(System *self, u32 commandId);

    // Lazily create + return the plug-in registry (@+0x28). @0x82B6DDC0.
    static PlugInRegistry *GetPlugInRegistry(System *self);

    // Deferred-teardown handler queued into the command ring by Release: release every
    // active voice, then every voice on the expelled list. Returns its own record size --
    // host sizeof(SystemReleaseCommand) (X360: 8) per the ring contract. @0x82B6EA50.
    static int ReleaseHandler(void *cmd);

    // Remove the manager's timer (@+0x60). @0x82B6EB80.
    static void RemoveTimer(System *self, TimerHandle *handle);

    // Publish the calling thread's id into the thread-id slot (@+0x54). @0x82B6BCB8.
    static System *SetRwAudioCoreThreadId(System *self, u32 *pThreadId);

    // Record which hardware thread the audio update runs on (@+0x10FA). @0x82B6BC98.
    static System *SetThreadProcessor(System *self, u8 processor);

    // Walk the expel-after-decay candidate list; expel each fully-faded voice and compact
    // the list. @0x82B6EAD0.
    static Voice *UpdateExpellingVoices(System *self);

    // Swap the lock/unlock hooks (@+0x40/+0x44) over to the Csis mutex primitives. @0x82B6FC50.
    static System *VectorToCsisMutex(System *self);

    // ----------------------------------------------------------------------------------
    // Layout. The +0xNN annotations are the X360 (32-bit-pointer) offsets from the asm and
    // are documentary only; members are declared with x64 widths so only the ORDER is
    // load-bearing, and every access is by name.
    //
    // FLAG (rwaudio PDB reconcile, wave E -- IDA Files/ProStreet08Milestone.pdb,
    // rw::audio::core::System [sizeof=256]): the PDB names the ENTIRE layout. The Burnout
    // System == the ProStreet System with a 0x1000-byte insert at +0xA8 (the inline
    // expel-after-decay Voice* array + its count), i.e. every tail field maps at
    // Burnout-offset == PDB-offset + 0x1004; verified store-for-store against
    // CreateInstance @0x82B6F420 / ExecuteCommands @0x82B6F6D0 / Release @0x82B6F998
    // (mCommandBufferSize/mCommandIndex/mCommandBufferHighWater/mSystemTimerPeriod/
    // mSampleRate/mCpuFrequency/mCpuLoadLimit/the five CpuTicks/mCommandTimeStamp/
    // mRefCount/mThreadPriority/mThreadStackSize/mNumActiveVoices/mMaxActiveVoices/
    // mProcessor/mDebugFeatures all line up exactly). Members ALREADY NAMED by earlier
    // waves keep their committed names (the PDB name rides in the comment) because the
    // in-flight plug-in WIP elsewhere in this directory also uses them; NEW members adopt
    // convention-prefixed PDB names. Two hook slots (+0x10AC/+0x10B0) are PDB-mapped but
    // untouched by any ARTIST body we have -- they are typed void* and flagged as such.
    // ----------------------------------------------------------------------------------
    void *mpObjectTable;                          // +0x00  (PDB: StackAllocator *mpStackAllocator -- the ctor
                                                  //   points it at the static 4-word StackAllocator
                                                  //   {mpSystem,mpUpperLimit,mpLowerLimit,mpTop} @dword_83271930;
                                                  //   Mixer::Mixer @0x82B6D880 seeds it; AiffWriter/Delay read
                                                  //   slot [3] (mpTop) as the scratch-stack top)
    f64 mfSystemTime;                             // +0x08  (PDB: mSystemTime; CreateInstance seeds 0.0
                                                  //   (dbl_82001CA8); the Profiler samples it -- via its
                                                  //   mpTimeSource=System -- as its wall clock)
    VoiceListLink *mpExpelledVoiceList;          // +0x10  (PDB: ListDStack mExpelledVoiceList) expelled Voice list
    EA::Allocator::ICoreAllocator *mpAllocator;  // +0x14  (PDB: mpAllocator) the sub-system allocator
    PhysicalAllocHook mpfnPhysicalAlloc;         // +0x18  (PDB: mpPhysicalAlloc) lazy-seeded physical alloc hook
    PhysicalFreeHook mpfnPhysicalFree;           // +0x1C  (PDB: mpPhysicalFree) lazy-seeded physical free hook
    char *mpDeferredRingBase;                     // +0x20  (PDB: mpCommandBuffer) deferred-command ring base
    PlugIn *mpMasteringSubMix;                    // +0x24  (PDB: mpMasteringSubMix; CreateInstance zeroes it --
                                                  //   no other System-TU access)
    PlugInRegistry *mpPlugInRegistry;            // +0x28  (PDB: mpPlugInRegistry) lazily-created plug-in registry
    DecoderRegistry *mpDecoderRegistry;          // +0x2C  (PDB: mpDecoderRegistry; Release frees it through its
                                                  //   own +0x0C mpSystem back-pointer's allocator)
    EncoderRegistry *mpEncoderRegistry;          // +0x30  (PDB: mpEncoderRegistry; same teardown shape)
    Profiler *mpProfiler;                        // +0x34  (PDB: mpProfiler; GetProfiler lazily points it at the
                                                  //   file-scope singleton; Release invokes its vt[0])
    EA::Jobs::JobScheduler *mpJobScheduler;      // +0x38  (PDB: mpJobScheduler; only zeroed in this build)
    SystemMutexIsLockedHook mpfnIsLocked;        // +0x3C  (PDB: mMutexIsLockedFn) "is locked" hook (0 = unset)
    SystemMutexHook mpfnLock;                    // +0x40  (PDB: mMutexLockFn) lock hook (0 = use mpSystemMutex)
    SystemMutexHook mpfnUnlock;                  // +0x44  (PDB: mMutexUnlockFn) unlock hook (0 = use mpSystemMutex)
    EA::Thread::Mutex *mpCommandMutex;           // +0x48  (PDB: mpExecuteCommandsMutex) guards ExecuteCommands;
                                                  //   points into the tail carve (see CreateInstance)
    EA::Thread::Mutex *mpSystemMutex;            // +0x4C  (PDB: mpMutex) the general system mutex; tail-carved
    void *mpLockThreadId;                        // +0x50  (PDB: mpLockThreadId; points at the 8-byte tail block
                                                  //   after the two carved mutexes)
    u32 *mppThreadId;                            // +0x54  (PDB: mpRwAudioCoreThreadId; points at the 4-byte tail
                                                  //   word -- SetRwAudioCoreThreadId writes through it)
    VoiceActiveNode *mppVoiceListNodes;          // +0x58  (PDB: VoiceListNode *mpVoiceListNodes) active-voice array
    ListDNode *mpTimerListHead;                  // +0x5C  (PDB: ListDStack mTimerList.phead -- the ITask list
                                                  //   ExecuteCommands ticks; each node lives at ITask +0x04 (X360))
    TimerManager mTimerManager;                   // +0x60  embedded profiling-timer manager
    // +0xA8  inline "expel after decay" candidate list (Voice::ExpelAfterDecay stores into
    // it at index muExpelAfterDecayCount). Capacity inferred from the 0xA8..0x10A8 gap
    // (1024 slots on the X360 image); indexed by name so the exact span is not load-bearing.
    // This array + its count are the Burnout-only insert relative to the ProStreet layout.
    Voice *mpExpelAfterDecayList[(0x10A8 - 0xA8) / 4];
    u32 muExpelAfterDecayCount;                   // +0x10A8  live entries in mpExpelAfterDecayList
    void *mpAssertHandler;                        // +0x10AC (PDB: bool (*mpAssertHandler)(const char*).
                                                  //   PDB-mapped only -- no ARTIST body we have touches it, so it
                                                  //   is typed opaque; give it the real signature when a body does)
    void *mpfnGetCsisLibraryType;                 // +0x10B0 (PDB: unsigned char (*mGetCsisLibraryType)().
                                                  //   PDB-mapped only, opaque -- as above)
    u32 muCommandBufferSize;                      // +0x10B4 (PDB: mCommandBufferSize; CreateInstance stores its
                                                  //   commandBufferSize argument here)
    u32 muDeferredRingCursor;                     // +0x10B8 (PDB: mCommandIndex) byte cursor into the command ring
    u32 muDeferredRingHighWater;                  // +0x10BC (PDB: mCommandBufferHighWater; ExecuteCommands latches
                                                  //   the max cursor seen before each ring reset)
    f32 mfSystemTimerPeriod;                      // +0x10C0 (PDB: mSystemTimerPeriod; passed in f1 to every
                                                  //   ITask::Execute by ExecuteCommands)
    f32 mfSampleRate;                             // +0x10C4 (PDB: mSampleRate; CreateInstance seeds 0.0f)
    f32 mfCpuClockRate;                           // +0x10C8 (PDB: mCpuFrequency; CreateInstance seeds 1.0f,
                                                  //   CpuLoadBalancer::Init later seeds 3.2e9)
    f32 mfCpuLoadPercent;                         // +0x10CC (PDB: mCpuLoadLimit; CreateInstance seeds 90.0f; read
                                                  //   by CpuLoadBalancer::Balance's cull gate)
    u32 muMixerCpuTicks;                          // +0x10D0 (PDB: mMixerCpuTicks; CreateInstance zeroes)
    u32 muCommandExecutionCpuTicks;               // +0x10D4 (PDB: mCommandExecutionCpuTicks; ExecuteCommands'
                                                  //   ring-replay phase cycle cost)
    u32 muTimerExecutionCpuTicks;                 // +0x10D8 (PDB: mTimerExecutionCpuTicks; ExecuteCommands' two
                                                  //   timer phases' summed cycle cost)
    u32 muBalanceCycles;                          // +0x10DC (PDB: mLoadBalancerCpuTicks) last CpuLoadBalancer cost
    u32 muExpelVoicesCpuTicks;                    // +0x10E0 (PDB: mExpelVoicesCpuTicks; UpdateExpellingVoices cost)
    u32 muFrameCounter;                           // +0x10E4 (PDB: mCommandTimeStamp; CreateInstance seeds 1;
                                                  //   ExecuteCommands increments; IsCommandComplete compares)
    volatile s32 miRefCount;                      // +0x10E8 (PDB: volatile int mRefCount; Release pumps the ring
                                                  //   until it drains to <= 0)
    s32 miThreadPriority;                         // +0x10EC (PDB: mThreadPriority; CreateInstance seeds 15)
    u32 muThreadStackSize;                        // +0x10F0 (PDB: mThreadStackSize; CreateInstance zeroes)
    u16 muActiveVoiceCount;                       // +0x10F4 (PDB: mNumActiveVoices) live mppVoiceListNodes entries
    u16 muActiveVoiceCapacity;                    // +0x10F6 (PDB: mMaxActiveVoices) mppVoiceListNodes capacity
    u8 mucCommandsExecuting;                      // +0x10F8 (PDB: mCommandsExecuting; PDB-mapped -- untouched by
                                                  //   the bodies reconstructed so far)
    u8 mucMutexLockAttempted;                     // +0x10F9 (PDB: mMutexLockAttempted; PDB-mapped -- as above)
    u8 mucThreadProcessor;                        // +0x10FA (PDB: mProcessor; CreateInstance seeds 4)
    u8 maucDebugFeatures[6];                      // +0x10FB..0x1100 (PDB: mDebugFeatures[6]; CreateInstance
                                                  //   memsets all six to 1)
};

// New2<T> -- the templated allocate-and-default-construct helper. Reconstructed as ONE
// uniform template body from its three exported X360 instantiations, which differ ONLY
// by the inlined default constructor and the default size:
//   New2<Decoder>         @0x82B6C2D0: default size 52 (X360 sizeof), ctor = vtable install
//   New2<PlugInRegistry>  @0x82B6C350: default size 24 (X360 sizeof), ctor = zero the
//                                       ListQueue words (see PlugInRegistry())
//   New2<Voice>           @0x82B6E140: default size 80 (X360 sizeof), ctor = zero mUnk0C
// Uniform behaviour (identical in all three): size 0 -> sizeof(T); allocator null -> the
// System's own mpAllocator (@+0x14); allocate via ICoreAllocator::Alloc(size, name,
// /*flags*/1, align, /*alignOffset*/0) (vtable slot +4); store the result through
// `outResult` both before and after construction, constructing only when non-null.
// (An earlier recon pass stubbed this as `allocator ? Alloc(...) : 0` with no name/flags,
// no default size and no construction -- every call site passes allocator=0, so that stub
// made ALL the CreateInstance paths return null. Fixed wave E.)
template <class T>
inline void System::New2(System *self, T **outResult, const char *name,
                         unsigned size, unsigned align,
                         EA::Allocator::ICoreAllocator *allocator)
{
    if (!size)
        size = sizeof(T);
    if (!allocator)
        allocator = self->mpAllocator;
    T *result = static_cast<T *>(allocator->Alloc(size, name, 1, align, 0));
    *outResult = result;
    if (result)
    {
        new (result) T;
        *outResult = result;
    }
}

} // namespace core
} // namespace audio
} // namespace rw
