// =====================================================================================
// rw::audio::core::PlugIn -- member-function bodies.
//
// EARenderWare "rwaudio" plug-in base. Reconstructed from BURNOUT_X360_ARTIST.XEX;
// the PowerPC asm is authoritative for every store. No Feb-2007 leak source and no
// DecFIGS DWARF exist for this type. Each body is a store-for-store translation of
// the disassembly cited in rw/audio/core/PlugIn.h.
// =====================================================================================

#include "rw/audio/core/PlugIn.h"

namespace rw
{
namespace audio
{
namespace core
{

// The factory/feature passed to CreateInstance exposes a virtual-like slot at +8
// that is a "begin/validate" hook returning a bool (asm: lwz r11,8(a3); bctrl, with
// r3=self and r4=*(a4)). The companion descriptor `a4` carries the initial-state
// payload: a "context" pointer at +0 and an init flag byte at +8. Both are modelled
// as reconstructed views so the call/stores stay member-by-name.
struct PlugInFactory
{
    void *mpSlot0;                         // +0x00
    void *mpSlot4;                         // +0x04
    int (*mpBegin)(PlugIn *self, void *context); // +0x08 -- the validate/begin hook
};

struct PlugInCreateDesc
{
    void *mpContext; // +0x00 -- passed as the argument to PlugInFactory::mpBegin
    int mField4;     // +0x04
    char mbInitFlag; // +0x08 -- copied into PlugIn::mbFlag21
};

// off_83271928 -- the shared System singleton installed into PlugIn::mpSystem at
// construction time (the owner of the deferred SetAttribute command ring). It lives
// in the System TU; declared here so CreateInstance can take its address. FLAGGED:
// the actual object is defined/initialised elsewhere (System sub-system TU).
extern System g_PlugInSystem;

// -------------------------------------------------------------------------------------
// PlugIn::CreateInstance @0x82B6A818
// Placement-constructs a PlugIn over `self`, wires its links, and runs the factory's
// begin hook; on failure it virtually destroys/cleans up and returns null.
// -------------------------------------------------------------------------------------
PlugIn *PlugIn::CreateInstance(PlugIn *self, PlugIn *input, System *factory,
                               void *typeRecord, char flag)
{
    self->mfAttrib0 = 0.0f;                  // stfs flt(0) -> 0x14
    self->mpInput = input;                    // stw a2 -> 0x08
    self->mfAttrib1 = 0.0f;                   // stfs flt(0) -> 0x18
    self->mpFactory = factory;                // stw a3 -> 0x10
    self->mState = 0;                         // stw 0 -> 0x1C
    self->mpSystem = &g_PlugInSystem;         // stw off_83271928 -> 0x04
    self->mbFlag20 = flag;                    // stb a5 -> 0x20

    PlugInCreateDesc *desc = static_cast<PlugInCreateDesc *>(typeRecord);
    self->mbFlag21 = desc->mbInitFlag;        // lbz 8(a4) -> stb 0x21

    PlugInFactory *fac = reinterpret_cast<PlugInFactory *>(factory);
    if (!fac->mpBegin(self, desc->mpContext)) // (*(a3+8))(self, *(a4))
    {
        // Begin failed: virtually destruct (vt[0]) then Destroy(0) (vt[3]) and bail.
        self->~PlugIn();                      // (**self)(self)
        self->Destroy(0);                     // (*(*self+12))(self, 0)
        return 0;
    }
    return self;
}

// -------------------------------------------------------------------------------------
// PlugIn::Event @0x82B6A8F8 -- tail-call into the per-instance Event virtual (vt[1]).
// -------------------------------------------------------------------------------------
int PlugIn::Event(PlugIn *self)
{
    return self->Event(); // (*(*self+4))(self)
}

// -------------------------------------------------------------------------------------
// PlugIn::GetAttribute @0x82B6A8C8
// Reads attribute `index` (8-byte stride) from the resolved attribute table.
// -------------------------------------------------------------------------------------
PlugIn *PlugIn::GetAttribute(PlugIn *self, int index, f32 *outValue)
{
    *outValue = self->mpAttributes[index].mfValue; // *(8*a2 + *(self+0xC))
    return self;
}

// -------------------------------------------------------------------------------------
// PlugIn::SetAttribute @0x82B6E848
// Queues a deferred attribute write into the System's command ring (16-byte slot),
// rather than writing immediately. Replayed later by SetAttributeHandler.
// -------------------------------------------------------------------------------------
PlugIn *PlugIn::SetAttribute(PlugIn *self, int index, f32 value)
{
    System *system = self->mpSystem;                            // lwz 4(self)
    u32 cursor = system->muDeferredRingCursor;                  // lwz 0x10B8
    PlugInSetAttributeCommand *cmd =                            // *(self+0x20)+cursor
        reinterpret_cast<PlugInSetAttributeCommand *>(system->mpDeferredRingBase + cursor);
    system->muDeferredRingCursor = cursor + 16;                 // cursor += 0x10

    cmd->mfValue = value;                                       // stfs f1 -> +0xC
    cmd->mpHandler = &PlugIn::SetAttributeHandler;              // stw &handler -> +0x00
    cmd->mpTarget = self;                                       // stw self -> +0x04
    cmd->miIndex = index;                                       // stw a2 -> +0x08
    return self;
}

// -------------------------------------------------------------------------------------
// PlugIn::SetAttributeHandler @0x82B6DBA0
// The deferred-command callback: performs the actual attribute store, returns 16
// (the consumed command size, so the ring consumer can advance its cursor).
// -------------------------------------------------------------------------------------
int PlugIn::SetAttributeHandler(void *cmd)
{
    PlugInSetAttributeCommand *c = static_cast<PlugInSetAttributeCommand *>(cmd);
    c->mpTarget->mpAttributes[c->miIndex].mfValue = c->mfValue; // *(8*idx + *(target+0xC)) = value
    return 16;
}

} // namespace core
} // namespace audio
} // namespace rw
