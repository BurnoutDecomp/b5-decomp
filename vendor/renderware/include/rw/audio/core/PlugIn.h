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

// EA::Thread::Mutex is used only through pointer members of System here; the full
// definition (eathread_mutex.h) is pulled in by the System .cpp, not by this widely
// included header.
namespace EA { namespace Thread { class Mutex; } }

namespace rw
{
namespace audio
{
namespace core
{

class PlugIn;
class PlugInDescRunTime;  // rwaudio PDB name (the header's former "PlugInInfo") -- registry RTTI record
class PlugInRegistry;
class System;
class Voice;              // rwaudio PDB: PlugIn::mpVoice (+0x08)
struct VoiceListLink;     // intrusive expelled-voice list node (defined in Voice.h)
struct VoiceActiveNode;   // one entry of System::mppVoiceListNodes (defined in Voice.h)

// The System's swappable lock/unlock/is-locked hooks (function slots @+0x3C/+0x40/+0x44).
// Each is tail-called with the System in r3 (the pseudocode renders it as a nullary call
// because r3 is preserved), so the real prototype takes the System. When null the System
// falls back to its own EA::Thread::Mutex; when set (VectorToCsisMutex) they route to the
// Csis integration's mutex primitives.
typedef int (*SystemLockHook)(System *self);
// Physical (non-cached) memory allocator hooks (@+0x18/+0x1C). PhysicalAlloc lazily seeds
// them to the Xbox defaults. The alloc hook is called with four words (only the first three
// are consumed by the default); the free hook with the block pointer.
typedef void *(*PhysicalAllocHook)(u32 size, u32 align, u32 protect, u32 unused);
typedef void (*PhysicalFreeHook)(void *block);

// -------------------------------------------------------------------------------------
// PlugIn -- base class for a node in the audio processing graph.
//
// Layout grounded in PlugIn::CreateInstance @0x82B6A818 (the placement-construct path):
//   +0x00  vtable pointer            (virtual dispatch; vt[0]=dtor-ish, vt[3]=Destroy,
//                                      vt[1]=Event handler reached via PlugIn::Event)
//   +0x04  mpInfoVTable              (off_83271928: the PlugInDescRunTime's secondary v-table /
//                                      run-time-type record installed at construct time)
//   +0x08  mpInput                   (a2: upstream PlugIn / input handle)
//   +0x0C  mpAttributes              (float[2]-stride attribute table; set up elsewhere,
//                                      read by GetAttribute/SetAttribute as 8-byte stride)
//   +0x10  mpFactory                 (a3: the owning factory/feature whose +8 is a
//                                      "begin/validate" virtual returning a bool)
//   +0x14  mfAttrib0                 (float, init 0.0)
//   +0x18  mfAttrib1                 (float, init 0.0)
//   +0x1C  mState                    (int, init 0)
//   +0x20  mbFlag20                  (char a5, init flag)
//   +0x21  mbFlag21                  (char, copied from a4+8)
// (the IDA "char a5" arg and the "*(a4+8)" byte are the only sub-byte members)
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
// @0x82B6E848 (16-byte stride: handler / target / index / value).
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
// System -- the owning rwaudio sub-system. Only the surface the plug-in family
// touches is reconstructed:
//   +0x20    mDeferredRingBase  (base offset added to the ring cursor; the per-System
//                                command ring used by PlugIn::SetAttribute)
//   +0x10B8  muDeferredRingCursor (4280: byte cursor into the command ring; advanced
//                                  by 16 per queued PlugInSetAttributeCommand)
// (grounded in SetAttribute @0x82B6E848: v3=this->mpFactory; ring = *(this+0x20) +
//  *(this+0x10B8); cursor += 16.)
//
// New2<T> is the placement allocator helper (mangled
//   ??$New2@VPlugInRegistry@...@System@... ) defined in another TU; reconstructed
// here as an inline template so the registry's CreateInstance compiles. FLAGGED:
// the real definition lives in the System allocator TU.
// -------------------------------------------------------------------------------------
class System
{
public:
    template <class T>
    static void New2(System *self, T **outResult, const char *name, unsigned size,
                     unsigned align, EA::Allocator::ICoreAllocator *allocator);

    // Placement-construct the System's leaf fields: clear the expelled-voice list head and
    // the per-frame processor list head, default-construct the embedded TimerManager, and
    // wire the object table pointer. (The full CreateInstance allocate-and-init path is a
    // separate ledger function.) X360 @0x82B6DD20.
    static System *System_ctor(System *self);

    // Free `mem` back through `allocatorOverride` (or, when null, the System's own
    // ICoreAllocator @+0x14). Grounded in Free @0x82B6BE48: v3 = a3 ? a3 : this->mpAllocator;
    // v3->vtable[Free](v3, mem, 0). The third argument is an OPTIONAL ALLOCATOR OVERRIDE
    // (the asm dispatches through it directly), not a flag word -- every call site passes 0.
    static void Free(System *self, void *mem, EA::Allocator::ICoreAllocator *allocatorOverride);

    // Allocate `size` bytes through the System's ICoreAllocator (slot +0x14), tagged with
    // the `name` debug string and aligned to `align`. `flags` is the allocator flag word.
    // Additive counterpart to Free; the body lives in the System allocator TU (mangled
    // ?Alloc@System@core@audio@rw@@). Grounded in CMpegBase::AllocateSynth @0x82B8BFF0,
    // which calls rw::audio::core::System::Alloc(off_83271928, size, "PolySynthHistoryF",
    // 16, 0) with the argument order (self=r3, size=r4, name=r5, align=r6, flags=r7).
    static void *Alloc(System *self, u32 size, const char *name, u32 align, s32 flags);

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
    static void *DefaultPhysicalAlloc(u32 size, u32 align, u32 protect, u32 unused);
    static void DefaultPhysicalFree(void *block);

    // Allocate/free through the physical hooks (@+0x18/+0x1C), seeding them to the defaults
    // on first use. @0x82B6DCD8 / @0x82B6BE70.
    static void *PhysicalAlloc(System *self, u32 size, u32 align, u32 protect, u32 unused);
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
    // active voice, then every voice on the expelled list. Returns its own 8-byte record
    // size. @0x82B6EA50.
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
    // load-bearing, and every access is by name. The additional members below (over the
    // original PlugIn-family surface) are the fields the System-TU bodies touch, grounded in
    // the System.cpp / Voice.cpp disassembly (mpDeferredRingBase/muDeferredRingCursor keep
    // their names/roles so PlugIn/Route/RawPuller2/Decoder are unaffected). Slots the System
    // TU does not read (the CreateInstance-only init fields and the tail-carved mutex storage)
    // stay as opaque padding.
    // ----------------------------------------------------------------------------------
    void *mpObjectTable;                          // +0x00  points at a shared 4-word table (ctor)
    u8 mHeader08[0x10 - 0x08];                     // +0x08..0x0F  (config double; CreateInstance-only)
    VoiceListLink *mpExpelledVoiceList;          // +0x10  head of the expelled/pending Voice list
    EA::Allocator::ICoreAllocator *mpAllocator;  // +0x14  the sub-system allocator
    PhysicalAllocHook mpfnPhysicalAlloc;         // +0x18  physical alloc hook (lazy-seeded)
    PhysicalFreeHook mpfnPhysicalFree;           // +0x1C  physical free hook (lazy-seeded)
    char *mpDeferredRingBase;                     // +0x20  deferred-command ring base
    u8 mPad24[0x28 - 0x24];                        // +0x24..0x27
    PlugInRegistry *mpPlugInRegistry;            // +0x28  lazily-created plug-in registry
    u8 mPad2C[0x3C - 0x2C];                        // +0x2C..0x3B  (Release-torn sub-objects; opaque)
    SystemLockHook mpfnIsLocked;                 // +0x3C  "is locked" hook (0 = unlocked)
    SystemLockHook mpfnLock;                     // +0x40  lock hook (0 = use mpSystemMutex)
    SystemLockHook mpfnUnlock;                   // +0x44  unlock hook (0 = use mpSystemMutex)
    EA::Thread::Mutex *mpCommandMutex;           // +0x48  guards ExecuteCommands
    EA::Thread::Mutex *mpSystemMutex;            // +0x4C  the general system mutex
    u8 mPad50[0x54 - 0x50];                        // +0x50  (extra tail-mutex slot)
    u32 *mppThreadId;                            // +0x54  points at the tail thread-id storage
    VoiceActiveNode *mppVoiceListNodes;          // +0x58  sorted active-voice array
    void *mpProcessorListHead;                    // +0x5C  per-frame processor list head
    TimerManager mTimerManager;                   // +0x60  embedded profiling-timer manager
    // +0xA8  inline "expel after decay" candidate list (Voice::ExpelAfterDecay stores into
    // it at index muExpelAfterDecayCount). Capacity inferred from the 0xA8..0x10A8 gap
    // (1024 slots on the X360 image); indexed by name so the exact span is not load-bearing.
    Voice *mpExpelAfterDecayList[(0x10A8 - 0xA8) / 4];
    u32 muExpelAfterDecayCount;                   // +0x10A8  live entries in mpExpelAfterDecayList
    u8 mPad10AC[0x10B8 - 0x10AC];                  // +0x10AC..0x10B7
    u32 muDeferredRingCursor;                     // +0x10B8  byte cursor into the command ring
    u8 mPad10BC[0x10C8 - 0x10BC];                  // +0x10BC..0x10C7
    f32 mfCpuClockRate;                           // +0x10C8  CPU clock rate in Hz (CpuLoadBalancer::Init seeds 3.2e9)
    f32 mfCpuLoadPercent;                         // +0x10CC  CPU-load headroom % read by CpuLoadBalancer::Balance's cull gate
    u8 mPad10D0[0x10DC - 0x10D0];                  // +0x10D0..0x10DB
    u32 muBalanceCycles;                          // +0x10DC  CPU cycles spent in the last CpuLoadBalancer::Balance
    u8 mPad10E0[0x10E4 - 0x10E0];                  // +0x10E0..0x10E3  (ring stats + timing; System-TU-only paths blocked)
    u32 muFrameCounter;                           // +0x10E4  executed-frame counter (IsCommandComplete)
    u8 mPad10E8[0x10F4 - 0x10E8];                  // +0x10E8..0x10F3
    u16 muActiveVoiceCount;                       // +0x10F4  live entries in mppVoiceListNodes
    u16 muActiveVoiceCapacity;                    // +0x10F6  capacity of mppVoiceListNodes
    u8 mPad10F8[0x10FA - 0x10F8];                  // +0x10F8..0x10F9
    u8 mucThreadProcessor;                        // +0x10FA  hardware thread for the audio update
};

// New2<T> reconstruction (FLAGGED): the real templated allocator helper is defined in
// the System allocator TU (mangled ??$New2@...@System@core@audio@rw@@). Modelled here
// as the minimal allocate-aligned-then-store behaviour its call site relies on so
// PlugInRegistry::CreateInstance compiles. It allocates `size` bytes at `align` from
// `allocator` (falling back to the System's own allocator when null) and writes the
// result through `outResult`.
template <class T>
inline void System::New2(System * /*self*/, T **outResult, const char * /*name*/,
                         unsigned size, unsigned align,
                         EA::Allocator::ICoreAllocator *allocator)
{
    void *mem = allocator ? allocator->Alloc(size, 0, 0, align) : 0;
    *outResult = static_cast<T *>(mem);
}

} // namespace core
} // namespace audio
} // namespace rw
