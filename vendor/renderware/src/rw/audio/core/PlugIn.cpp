// =====================================================================================
// rw::audio::core::PlugIn -- member-function bodies.
//
// EARenderWare "rwaudio" plug-in base. Reconstructed from BURNOUT_X360_ARTIST.XEX;
// the PowerPC asm is authoritative for every store. No Feb-2007 leak source and no
// DecFIGS DWARF exist for this type. Each body is a store-for-store translation of
// the disassembly cited in rw/audio/core/PlugIn.h.
// =====================================================================================

#include "rw/audio/core/PlugIn.h"
#include "rw/audio/core/Voice.h" // VoiceStageConfig (the a4 record CreateInstance reads)

namespace rw
{
namespace audio
{
namespace core
{

// (The former PlugInFactory local view over the then-opaque descriptor is RETIRED --
// descriptor-record wave 2026-08-28: the "+8 begin/validate hook" IS the typed
// PlugInDescRunTime::pCreateInstance slot; the dispatch below casts at the call, the
// console's own generic-dispatch site. The companion `a4` record IS the
// VoiceStageConfig -- see Voice.h's phase-D root-cause note.)
typedef int (*PlugInCreateInstanceFn)(PlugIn *self, void *context);

// off_83271928 -- the shared System-singleton POINTER installed into PlugIn::mpSystem
// at construction time (the owner of the deferred SetAttribute command ring). The
// System TU defines it (`System *off_83271928`, published by System::CreateInstance);
// the X360 store here is `lwz off_83271928 ; stw -> +0x04` -- the pointer VALUE, not
// an object address. (2026-08-25, faithful-audio-engine phase A: this TU used to
// model it as an `extern System g_PlugInSystem` object and take its address -- a
// model mismatch with the System TU's pointer definition that broke the mounted
// link; reconciled to the System TU's shape.)
extern "C" System *off_83271928;

// -------------------------------------------------------------------------------------
// PlugIn::CreateInstance @0x82B6A818
// Placement-constructs a PlugIn over `self`, wires its links, and runs the factory's
// begin hook; on failure it virtually destroys/cleans up and returns null.
// -------------------------------------------------------------------------------------
PlugIn *PlugIn::CreateInstance(PlugIn *self, Voice *voice, PlugInDescRunTime *pDesc,
                               VoiceStageConfig *apConfig, char flag)
{
    self->mLatencyInSamples = 0.0f;          // stfs flt(0) -> 0x14
    self->mpVoice = voice;                     // stw a2 -> 0x08
    self->mDecaySamples = 0.0f;               // stfs flt(0) -> 0x18
    self->mpPlugInDescRunTime = pDesc;        // stw a3 -> 0x10
    self->mCpuTicks = 0;                      // stw 0 -> 0x1C
    self->mpSystemUseGetSystemAccessor = off_83271928; // lwz off_83271928 ; stw -> 0x04
    self->mInputChannels = flag;             // stb a5 -> 0x20

    self->mOutputChannels =
        static_cast<u8>(apConfig->mFlagAndField8); // lbz 8(a4) -> stb 0x21

    if (!reinterpret_cast<PlugInCreateInstanceFn>(pDesc->pCreateInstance)(
            self, apConfig->mpContext)) // (*(a3+8))(self, *(a4))
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
// THREE-ARG (phase-D Dac slice): the console entry is `lwz r11,0(r3); lwz r11,4(r11);
// mtctr; bctr` -- r4/r5 pass through untouched into the slot, and the Dac's vt[1]
// (Dac::EventEvent @0x82BA27F0) consumes them as (eventId, paramPtr). Callers:
// Environment::StartDac/StopDac @0x82680F50/@0x82680FE8 (events 3/4, param 0), the
// splice-voice pair frees, the factory CreateInstance paths.
// -------------------------------------------------------------------------------------
int PlugIn::Event(PlugIn *self, int aiEventId, void *apParam)
{
    return self->Event(aiEventId, apParam); // (*(*self+4))(self, r4, r5)
}

// -------------------------------------------------------------------------------------
// PlugIn::GetAttribute @0x82B6A8C8
// Reads attribute `index` (8-byte stride) from the resolved attribute table.
// -------------------------------------------------------------------------------------
PlugIn *PlugIn::GetAttribute(PlugIn *self, int index, f32 *outValue)
{
    *outValue = self->mpAttribute[index].mfValue; // *(8*a2 + *(self+0xC))
    return self;
}

// -------------------------------------------------------------------------------------
// PlugIn::SetAttribute @0x82B6E848
// Queues a deferred attribute write into the System's command ring (one
// PlugInSetAttributeCommand slot -- 16 bytes on the X360), rather than writing
// immediately. Replayed later by SetAttributeHandler.
// -------------------------------------------------------------------------------------
PlugIn *PlugIn::SetAttribute(PlugIn *self, int index, f32 value)
{
    System *system = self->mpSystemUseGetSystemAccessor;        // lwz 4(self)
    u32 cursor = system->muDeferredRingCursor;                  // lwz 0x10B8
    PlugInSetAttributeCommand *cmd =                            // *(self+0x20)+cursor
        reinterpret_cast<PlugInSetAttributeCommand *>(system->mpDeferredRingBase + cursor);
    // RECORD STRIDE (X360-literal trap): the asm's `addi r9,r9,0x10` IS the console
    // sizeof(PlugInSetAttributeCommand) ({int(*)(void*), PlugIn*, int, f32} = 4+4+4+4).
    // On the host the widened pointers make the same record 24 bytes, and ExecuteCommands
    // advances the ring by the handler's return -- SetAttributeHandler below likewise
    // returns sizeof(PlugInSetAttributeCommand) -- so the enqueue stride must be the HOST
    // sizeof, never the literal 16. The cursor is advanced before the fields are written,
    // exactly as the asm orders the stores.
    system->muDeferredRingCursor =
        cursor + static_cast<u32>(sizeof(PlugInSetAttributeCommand)); // X360: cursor += 0x10

    cmd->mfValue = value;                                       // stfs f1 -> +0xC
    cmd->mpHandler = &PlugIn::SetAttributeHandler;              // stw &handler -> +0x00
    cmd->mpTarget = self;                                       // stw self -> +0x04
    cmd->miIndex = index;                                       // stw a2 -> +0x08
    return self;
}

// -------------------------------------------------------------------------------------
// PlugIn::SetAttributeHandler @0x82B6DBA0
// The deferred-command callback: performs the actual attribute store, then returns the
// consumed command size so the ring consumer can advance its cursor.
// -------------------------------------------------------------------------------------
int PlugIn::SetAttributeHandler(void *cmd)
{
    PlugInSetAttributeCommand *c = static_cast<PlugInSetAttributeCommand *>(cmd);
    c->mpTarget->mpAttribute[c->miIndex].mfValue = c->mfValue; // *(8*idx + *(target+0xC)) = value
    // The asm's `li r3,0x10` is the console sizeof(PlugInSetAttributeCommand); it must
    // stay identical to the producer's advance in SetAttribute above, so both sides use
    // the HOST sizeof (24 on x64).
    return static_cast<int>(sizeof(PlugInSetAttributeCommand)); // X360: li r3,0x10
}


// -------------------------------------------------------------------------------------
// off_820AA810 -- the base PlugIn vtable every plug-in's teardown reverts its +0x00
// slot to. ONE shared host sentinel (2026-08-25, audio-faithfulness wave 5): Route.cpp
// and Send.cpp used to each hold their own file-static slot for this SAME console
// object, so the "same vtable" had two distinct host addresses -- any cross-TU vptr
// compare would diverge. The sentinel is store-only today (no committed consumer
// dispatches or compares through it); the null-valued K*_BasePlugInVTable constants in
// the other plug-in TUs are the sibling modelling of the same word.
// -------------------------------------------------------------------------------------
static void* sBasePlugInVTableSlot = 0;
void* const gpBasePlugInVTableSentinel = &sBasePlugInVTableSlot;   // off_820AA810

} // namespace core
} // namespace audio
} // namespace rw
