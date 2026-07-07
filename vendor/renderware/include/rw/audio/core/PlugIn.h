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

    // Free `mem` back through the System's ICoreAllocator (slot +0x14). `flags` is the
    // allocator flag word (always 0 at the call sites seen so far). Declared here as the
    // additive counterpart to New2; the body lives in the System allocator TU (mangled
    // ?Free@System@core@audio@rw@@). Grounded in Decoder::Release @0x82691528, which calls
    // rw::audio::core::System::Free(off_83271928, ptr, 0).
    static void Free(System *self, void *mem, s32 flags);

    // Unlink `voice` from the System's expulsion-candidate bookkeeping. The body lives in
    // the System TU (mangled ?RemoveVoiceFromExpulsionCandidateList@System@core@audio@rw@@);
    // declared here because Voice::RemoveActiveVoice @0x82B6C1A8 calls it (r3=System, r4=voice).
    static void RemoveVoiceFromExpulsionCandidateList(System *self, Voice *voice);

    // ----------------------------------------------------------------------------------
    // Layout. The +0xNN annotations are the X360 (32-bit-pointer) offsets from the asm and
    // are documentary only; members are declared with x64 widths so only the ORDER is
    // load-bearing, and every access is by name. The additional members below (over the
    // original PlugIn-family surface) are the fields Voice::* touch, grounded in the
    // Voice.cpp disassembly (mpDeferredRingBase/muDeferredRingCursor keep their names/roles
    // so PlugIn/Route/RawPuller2/Decoder are unaffected).
    // ----------------------------------------------------------------------------------
    u8 mHeader00[0x10];                          // +0x00  opaque header
    VoiceListLink *mpExpelledVoiceList;          // +0x10  head of the expelled/pending Voice list
    EA::Allocator::ICoreAllocator *mpAllocator;  // +0x14  the sub-system allocator
    u8 mPad18[0x20 - 0x18];                       // +0x18..0x1F
    char *mpDeferredRingBase;                     // +0x20  deferred-command ring base
    u8 mPad24[0x58 - 0x24];                        // +0x24..0x57
    VoiceActiveNode *mppVoiceListNodes;          // +0x58  sorted active-voice array
    u8 mPad5C[0xA8 - 0x5C];                        // +0x5C..0xA7
    // +0xA8  inline "expel after decay" candidate list (Voice::ExpelAfterDecay stores into
    // it at index muExpelAfterDecayCount). Capacity inferred from the 0xA8..0x10A8 gap
    // (1024 slots on the X360 image); indexed by name so the exact span is not load-bearing.
    Voice *mpExpelAfterDecayList[(0x10A8 - 0xA8) / 4];
    u32 muExpelAfterDecayCount;                   // +0x10A8  live entries in mpExpelAfterDecayList
    u8 mPad10AC[0x10B8 - 0x10AC];                  // +0x10AC..0x10B7
    u32 muDeferredRingCursor;                     // +0x10B8  byte cursor into the command ring
    u8 mPad10BC[0x10E4 - 0x10BC];                  // +0x10BC..0x10E3
    u32 muFrameCounter;                           // +0x10E4  free-running counter snapshotted per Voice
    u8 mPad10E8[0x10F4 - 0x10E8];                  // +0x10E8..0x10F3
    u16 muActiveVoiceCount;                       // +0x10F4  live entries in mppVoiceListNodes
    u16 muActiveVoiceCapacity;                    // +0x10F6  capacity of mppVoiceListNodes
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
