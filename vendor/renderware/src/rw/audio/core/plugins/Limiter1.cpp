// =====================================================================================
// rw::audio::core::Limiter1 bodies -- the "Limiter1" dynamics audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 rwaudio PDB entry exist for this type. See plugins/Limiter1.h for the
// byte-exact layout and the per-function X360 addresses.
//   GetPlugInDescRunTime         @0x82B97AA0 -- returns the registered "Limiter1" descriptor
//   GetSize                      @0x82B97DA8 -- returns 168 (0xA8)
//   `scalar deleting destructor' @0x82BA18C0 -- reinstalls base vtable, conditional free
//   CreateInstance               @0x82BA2F28 -- store-for-store
//   Configure                    @0x82B97AB0 -- BLOCKED (see below), declared-only
//
// Configure @0x82B97AB0 is DELIBERATELY NOT BODIED. Its X360 body (a) forwards its params
// into sub_82C09970 -- the anonymous, fully un-typed helper whose register-level argument
// shape IDA cannot recover, already flagged un-decodable where Butterworth.cpp uses it --
// and (b) calls CompressorLimiter1::Configure with two un-recovered rodata DSP constants
// (flt_82004FDC feeding the ratio arg, flt_8200D5A4 the makeup arg). The Hex-Rays render even
// shows uninitialised stack slots (v11/v12) passed to the helper, confirming its ABI is not
// recovered. Both the helper's calling convention and those coefficients are load-bearing, so
// per the no-fabrication rule the body is BLOCKED rather than guessed. It did not execute in
// the boot-trace milestone.
// =====================================================================================

#include "rw/audio/core/plugins/Limiter1.h"

namespace rw
{
namespace audio
{
namespace core
{

// off_82F8D150 -- the registered run-time descriptor record (its +0x00 label is the string
// "Limiter1"). off_820AA810 -- the base PlugIn v-table the deleting destructor reinstalls
// before any free (shared with Gain / ReverbModel1). The concrete Limiter1 v-table symbol
// installed at construction is NOT exposed in CreateInstance's asm -- PlugIn::Initialize<T>
// installs it inside its own (separate-TU) body -- so it is modelled as honest placeholder
// storage here rather than fabricating an address. These are opaque data symbols in the XEX
// (no exported contents); modelled as placeholders so the bodies below link without
// fabricating their contents.
static char       *g_Limiter1Desc = nullptr;         // off_82F8D150 (the "Limiter1" record)
static void *const KLM_BasePlugInVTable = nullptr;   // off_820AA810
static void *const KLM_Limiter1VTable = nullptr;     // the Limiter1 v-table (installed by Initialize)

// CreateInstance immediates (single numeric rodata constants shared across the rwaudio
// plug-in family; values grounded by their reuse in the sibling CreateInstance paths).
static const f32 KF_ATTR0_DEFAULT = 120.0f; // flt_82004A28
static const f32 KF_ATTR1_DEFAULT = 0.1f;   // flt_8215BF00
static const f32 KF_ONE = 1.0f;             // flt_82001C98
static const f32 KF_ZERO = 0.0f;            // flt_82001CC0

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B97AA0 -- return the address of the registered descriptor record
// (its label is the string "Limiter1").
//   return &off_82F8D150;
// -------------------------------------------------------------------------------------
char **Limiter1::GetPlugInDescRunTime()
{
    return &g_Limiter1Desc; // &off_82F8D150
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B97DA8 -- the plug-in instance footprint.
//   li r3, 0xA8
// -------------------------------------------------------------------------------------
int Limiter1::GetSize()
{
    // X360-LITERAL TRAP (the stage-carve audit, phase-D follow-up): the console
    // immediate under-allocates the widened host object -- GetSize is the stage
    // factory's allocation stride, so return host sizeof (the RawPuller2/Send/
    // SinePlayer precedent).
    return static_cast<int>(sizeof(Limiter1));   // X360: li r3, 0xA8 (168)
}

// -------------------------------------------------------------------------------------
// `scalar deleting destructor' @0x82BA18C0 -- reinstall the base PlugIn v-table, then
// conditionally free. (~Limiter1 is trivial and folded away -- the asm performs no member
// teardown, only the vtable reinstall.)
//   self->mBase.mpVTable = off_820AA810;
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Limiter1::ScalarDeletingDestructor(Limiter1 *self, char flags)
{
    self->mBase.mpVTable = KLM_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA2F28 -- placement-init a Limiter1 over `self`.
//
// PlugIn::Initialize<Limiter1>(self, 0x28) constructs the plug-in base and bases its
// 8-byte-stride attribute table at self+0x28 (mfAttribute0). The templated PlugIn::Initialize
// helper lives in another (separate) TU; its locally-observable effect (v-table install +
// attribute-base wiring) is reproduced inline here, exactly as the Gain / ReverbModel1 /
// Iir2* shapes do. FLAGGED: the real Initialize<T> also performs the base mpSystem/mpVoice
// wiring from the owning voice/factory.
//
// Then the limiter latches its three graph-attribute defaults (120.0 / 0.1 / 1.0), mirrors
// them into the default snapshot block at +0x90, and clears the +0x9C/+0xA0 words. The store
// order below matches the asm. Returns 1.
// -------------------------------------------------------------------------------------
int Limiter1::CreateInstance(Limiter1 *self)
{
    // PlugIn::Initialize<Limiter1>(self, 0x28) -- observable effect reproduced inline.
    self->mBase.mpVTable = KLM_Limiter1VTable;      // installed by Initialize (symbol hidden)
    self->mBase.mpAttributes = &self->mfAttribute0; // attribute table base @ self+0x28

    self->mfAttribute0        = KF_ATTR0_DEFAULT; // stfs @ +0x28 (120.0)
    self->mfDefaultAttribute0 = KF_ATTR0_DEFAULT; // stfs @ +0x90 (120.0)
    self->muFieldA0           = 0;                // stw  @ +0xA0
    self->mfAttribute2        = KF_ONE;           // stfs @ +0x38 (1.0)
    self->mfAttribute1        = KF_ATTR1_DEFAULT; // stfs @ +0x30 (0.1)
    self->mfDefaultAttribute1 = KF_ATTR1_DEFAULT; // stfs @ +0x94 (0.1)
    self->mfDefaultAttribute2 = KF_ONE;           // stfs @ +0x98 (1.0)
    self->mfField9C           = KF_ZERO;          // stfs @ +0x9C (0.0)
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
