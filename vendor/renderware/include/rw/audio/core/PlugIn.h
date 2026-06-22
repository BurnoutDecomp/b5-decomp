#pragma once

// =====================================================================================
// Canonical RenderWare audio-core home for the plug-in family:
//   rw::audio::core::PlugIn          -- abstract processing-graph node base
//   rw::audio::core::PlugInInfo      -- per-plug-in registry record (run-time type)
//   rw::audio::core::PlugInRegistry  -- intrusive singly-linked list of PlugInInfo
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
class PlugInInfo;
class PlugInRegistry;
class System;

// -------------------------------------------------------------------------------------
// PlugIn -- base class for a node in the audio processing graph.
//
// Layout grounded in PlugIn::CreateInstance @0x82B6A818 (the placement-construct path):
//   +0x00  vtable pointer            (virtual dispatch; vt[0]=dtor-ish, vt[3]=Destroy,
//                                      vt[1]=Event handler reached via PlugIn::Event)
//   +0x04  mpInfoVTable              (off_83271928: the PlugInInfo's secondary v-table /
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
    struct Attribute
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
    static PlugIn *CreateInstance(PlugIn *self, PlugIn *input, System *factory,
                                  void *typeRecord, char flag);
    static int Event(PlugIn *self);
    static PlugIn *GetAttribute(PlugIn *self, int index, f32 *outValue);
    static PlugIn *SetAttribute(PlugIn *self, int index, f32 value);
    static int SetAttributeHandler(void *cmd);

    void *mpVTable;        // +0x00
    System *mpSystem;      // +0x04 (off_83271928: the shared System singleton that
                           //         owns the deferred command ring SetAttribute uses)
    PlugIn *mpInput;       // +0x08
    Attribute *mpAttributes; // +0x0C
    System *mpFactory;     // +0x10
    f32 mfAttrib0;         // +0x14
    f32 mfAttrib1;         // +0x18
    int mState;            // +0x1C
    char mbFlag20;         // +0x20
    char mbFlag21;         // +0x21
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
// PlugInInfo -- the run-time-type record for one registered plug-in. Forms an
// intrusive singly-linked list inside the registry. Layout grounded in
// PlugInRegistry::GetPlugInHandle @0x82B6A908 and RegisterPlugInRunTime @0x82B6A938:
//   ...                                  (object header / v-tables / construct hooks)
//   +0x24  mpNext   (the link field; the registry stores &mpNext as its node handle,
//                    and the public "handle" is node = &mpNext - 0x24)
//   +0x28  muId     (the registration id, compared in GetPlugInHandle)
//   +0x32  mbSeq    (byte; the registry's running id counter snapshotted here)
//
// The registry threads nodes by their +0x24 link; GetPlugInHandle walks links and
// returns the owning PlugInInfo (link - 0x24).
// -------------------------------------------------------------------------------------
class PlugInInfo
{
public:
    // Only the fields the bodied registry walks touch are modelled by name; the
    // gap before +0x24 is the (un-homed) object header / per-type hooks and is
    // preserved as opaque storage so offsets stay exact.
    char mHeader[0x24]; // +0x00 .. +0x23 -- opaque object header (un-homed here)
    void *mpNext;       // +0x24 -- intrusive next link
    u32 muId;           // +0x28 -- registration id
    char mPad2C[0x32 - 0x2C]; // +0x2C .. +0x31 -- opaque
    char mbSeq;         // +0x32 -- registry sequence snapshot
};

// -------------------------------------------------------------------------------------
// PlugInRegistry -- owns the linked list of PlugInInfo records. Layout grounded in
// CreateInstance @0x82B6DBC0, GetPlugInHandle @0x82B6A908, RegisterPlugInRunTime
// @0x82B6A938:
//   +0x00  mpHead    (pointer to the head node's +0x24 link, i.e. node->mpNext slot)
//   +0x04  mppTail   (pointer to the tail link slot; set on first registration)
//   +0x08  muCount   (number of registered plug-ins)
//   +0x0C  mField0C  (int, init 0)
//   +0x10  mpSystem  (back-pointer to the owning System, set in CreateInstance)
//   +0x14  mbNextSeq (byte; running sequence counter, post-incremented per register)
// Allocated 24 bytes (0x18), 16-aligned, via System::New2<PlugInRegistry>.
// -------------------------------------------------------------------------------------
class PlugInRegistry
{
public:
    static PlugInRegistry *CreateInstance(System *system);
    static void *GetPlugInHandle(PlugInRegistry *self, int id);
    static void *RegisterPlugInRunTime(PlugInRegistry *self, PlugInInfo *info);

    void *mpHead;     // +0x00
    void **mppTail;   // +0x04
    u32 muCount;      // +0x08
    int mField0C;     // +0x0C
    System *mpSystem; // +0x10
    char mbNextSeq;   // +0x14
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

    // +0x00 .. +0x1F -- opaque System header.
    char mHeader20[0x20];
    // +0x20 -- base address of the per-System deferred-command ring buffer.
    char *mpDeferredRingBase;
    // +0x24 .. +0x10B7 -- opaque System body.
    char mBody[0x10B8 - 0x24];
    // +0x10B8 -- byte cursor into the command ring (advanced by 16 per queued command).
    u32 muDeferredRingCursor;
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
