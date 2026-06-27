// =====================================================================================
// rw::audio::core::CompressorLimiter1 bodies.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store. No Feb-2007 leak source and no DecFIGS DWARF exist.
//   ClearBuffer @0x82B671F0 -- store-for-store
//   Configure   @0x82B67188 -- store-for-store
//   Process     @0x82B64DB0 -- KEYSTONE, not bodied (hand-written X360 VMX128; see below)
// See CompressorLimiter1.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/CompressorLimiter1.h"

#include <cstring> // std::memset (the X360 XMemSet)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// ClearBuffer @0x82B671F0 -- XMemSet(self, 0, 0x30): zero the 48-byte envelope scratch.
// -------------------------------------------------------------------------------------
void *CompressorLimiter1::ClearBuffer(CompressorLimiter1 *self)
{
    return std::memset(self, 0, 0x30);
}

// -------------------------------------------------------------------------------------
// Configure @0x82B67188   (FLAG: rwaudio PDB reconcile 2026-06-27 -- member names)
//   self->mThresholdOn        = threshold;                 // stfs f1 @ +0x30
//   self->mThresholdOff       = ratioParam;                // stfs f2 @ +0x34
//   self->mCompExponent       = makeupGain;                // stfs f3 @ +0x38
//   self->mAttackSamples      = attack;                    // stw  r7 @ +0x3C
//   self->mReleaseSamples     = release;                   // stw  r8 @ +0x40
//   self->mGroupChannels      = (stereoLink != 0);         // cntlzw/extrwi/xori @ +0x4C
//   self->mCompExponentStepOn = makeupGain / (f32)attack;  // fcfid/frsp/fdivs @ +0x44
//   self->mCompExponentStepOff= makeupGain / (f32)release; // fcfid/frsp/fdivs @ +0x48
//
// The +0x4C byte is the asm's `cntlzw(stereoLink&0xFF); extrwi bit26; xori 1`, i.e. it
// is set exactly when the low byte of `stereoLink` is nonzero. The attack/release
// divisors are sign-extended-to-i64 then converted to f32 (fcfid+frsp) before the
// divide, matching the asm's stack round-trip.
// -------------------------------------------------------------------------------------
CompressorLimiter1 *CompressorLimiter1::Configure(CompressorLimiter1 *self, f32 threshold,
                                                  f32 ratioParam, f32 makeupGain,
                                                  s32 attack, s32 release, s32 stereoLink,
                                                  u32 /*a8*/, u32 /*a9*/, u8 /*a10*/)
{
    self->mThresholdOn = threshold;
    self->mThresholdOff = ratioParam;
    self->mCompExponent = makeupGain;
    self->mAttackSamples = attack;
    self->mReleaseSamples = release;
    self->mGroupChannels = static_cast<u8>((stereoLink & 0xFF) != 0 ? 1 : 0);
    self->mCompExponentStepOn = makeupGain / static_cast<f32>(static_cast<s64>(attack));
    self->mCompExponentStepOff = makeupGain / static_cast<f32>(static_cast<s64>(release));
    return self;
}

// -------------------------------------------------------------------------------------
// Process @0x82B64DB0 -- KEYSTONE, intentionally NOT bodied.
//
// The X360 body is a hand-vectorized envelope-follower + gain-curve loop written
// directly in VMX128 (Altivec) intrinsics: lvx128/stvx128 over 4-wide float vectors,
// with vlogefp/vexptefp (log2/exp2 estimates), vctsxs/vrfiz (float<->int round), and a
// long chain of vsel/vand/vandc/vor lane-mask selects implementing the compression
// curve and attack/release smoothing per 4-sample group, unrolled across the block.
// Those VMX128 lane semantics have no portable PC C++ equivalent and cannot be
// reproduced store-for-store from the Hex-Rays transliteration without fabricating the
// per-lane arithmetic the compile gate cannot verify. Per the no-fabrication rule it is
// left unbodied and flagged. It did not execute in the boot-trace milestone.
//
// A scalar reimplementation grounded in the configured coefficients (mThresholdOn,
// mThresholdOff, mCompExponent, mCompExponentStepOn, mCompExponentStepOff,
// mGroupChannels) is the
// follow-up once the VMX128 lane math is decoded; this stub keeps the type's layout/ABI
// linkable in the meantime and never claims a fabricated result.
// -------------------------------------------------------------------------------------
int CompressorLimiter1::Process(CompressorLimiter1 * /*self*/)
{
    // Unrecoverable VMX128 hand-asm: not reconstructed (keystone). No fabricated math.
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
