#pragma once

// =====================================================================================
// rw::audio::core::EaLayer3DecBase / EaLayer31Dec / EaLayer32PcmDec
//
// The EARenderWare "rwaudio" EALayer3 (MP3-derived) stream decoder plug-in family.
// EaLayer3DecBase is the shared decoder; EaLayer31Dec ("EALayer3 v1") and
// EaLayer32PcmDec ("EALayer3 v2 / PCM") are the two concrete variants. Each variant's
// "create instance" plug-in event simply forwards to the base allocator with a variant
// flag (0 = v1, 1 = v2/PCM).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm of the two thunks
//   EaLayer31Dec::CreateInstanceEvent  @0x82B95EA0
//   EaLayer32PcmDec::CreateInstanceEvent @0x82B95EA8
// is authoritative. No prior source and no DecFIGS DWARF exist for this TU.
// Sibling to the committed rw::audio::core homes (Unpack0, PlugIn, BitGetter, ...).
//
// FLAG (rwaudio PDB reconcile): the NFS ProStreet 08 X360 PDB confirms all three class
// names verbatim -- rw::audio::core::EaLayer3DecBase [sizeof=88] : public Decoder, and
// the two concrete variants rw::audio::core::EaLayer31Dec [sizeof=88] and
// rw::audio::core::EaLayer32PcmDec [sizeof=88], each : public EaLayer3DecBase. This
// header models only the static thunk-forwarding entry points (no data members are
// declared here), so there is nothing to rename/retype; names already match the PDB.
// The full EaLayer3DecBase data layout (mpLoadedEALayer3Core @+0x34, mpEncodedSample
// @+0x38, mppEaLayer3Core @+0x3c, mRemainingSamples @+0x40, mTotalChannels @+0x44,
// mNumEaLayer3CoreInstances @+0x48, mLatency @+0x4c, mSkipSamples @+0x50, mVersion
// @+0x54, mNewFeed @+0x55) remains intentionally un-homed in its own TU as noted below.
//
// EaLayer3DecBase::CreateInstance @0x82B953A0 is NOT homed here: it is a large
// hand-written initializer that writes through ~a dozen still-un-homed member offsets
// (a1+46, a1+56..85, the a1+88 EALayer3Core* array) and allocates an array of
// 732-byte rw::audio::core::EALayer3Core instances via System::Alloc. Homing it
// would require modelling the full EaLayer3DecBase layout + EALayer3Core, neither of
// which is committed. It is FLAGGED and left to its own TU; only its declaration is
// provided here so the two forwarding thunks compile and link.
// =====================================================================================

#include "types.hpp" // s32, u8

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// EaLayer3DecBase -- shared EALayer3 decoder plug-in. Layout NOT modelled here (see the
// file header): CreateInstance is declared only, defined in the EaLayer3DecBase TU.
// `self` is the plug-in instance being constructed; `variant` selects v1 (0) vs
// v2/PCM (1). Returns 1 on success (the asm always returns 1).
// -------------------------------------------------------------------------------------
class EaLayer3DecBase
{
public:
    // @ 0x82B953A0 -- FLAGGED non-leaf keystone, defined in its own (un-homed) TU.
    static s32 CreateInstance(s32 self, u8 variant);
};

// -------------------------------------------------------------------------------------
// EaLayer31Dec -- EALayer3 v1 variant. Its CreateInstance plug-in event forwards to the
// base with variant 0.
// -------------------------------------------------------------------------------------
class EaLayer31Dec
{
public:
    // @ 0x82B95EA0
    static s32 CreateInstanceEvent(s32 self);
};

// -------------------------------------------------------------------------------------
// EaLayer32PcmDec -- EALayer3 v2 / PCM variant. Its CreateInstance plug-in event
// forwards to the base with variant 1.
// -------------------------------------------------------------------------------------
class EaLayer32PcmDec
{
public:
    // @ 0x82B95EA8
    static s32 CreateInstanceEvent(s32 self);
};

} // namespace core
} // namespace audio
} // namespace rw
