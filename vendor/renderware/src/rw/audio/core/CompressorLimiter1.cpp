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
//   self->mThresholdOn        = thresholdOn;                 // stfs f1 @ +0x30
//   self->mThresholdOff       = thresholdOff;                // stfs f2 @ +0x34
//   self->mCompExponent       = compExponent;                // stfs f3 @ +0x38
//   self->mAttackSamples      = attackSamples;               // stw  r7 @ +0x3C
//   self->mReleaseSamples     = releaseSamples;              // stw  r8 @ +0x40
//   self->mGroupChannels      = (groupChannels != 0);        // cntlzw/extrwi/xori @ +0x4C
//   self->mCompExponentStepOn = compExponent / (f32)attack;  // fcfid/frsp/fdivs @ +0x44
//   self->mCompExponentStepOff= compExponent / (f32)release; // fcfid/frsp/fdivs @ +0x48
// (store ORDER per the asm: +0x30, +0x34, +0x38, +0x3C, +0x40, +0x4C, then +0x44, +0x48 --
// the two derived steps come last, not in ascending offset order.)
//
// The +0x4C byte is the asm's `cntlzw(groupChannels&0xFF); extrwi bit26; xori 1`, i.e. it
// is set exactly when the low byte is nonzero. The attack/release divisors are
// sign-extended-to-i64 then converted to f32 (fcfid+frsp) before the divide, matching the
// asm's stack round-trip. r3 is never written, so the machine return is the incoming self.
// -------------------------------------------------------------------------------------
CompressorLimiter1 *CompressorLimiter1::Configure(CompressorLimiter1 *self, f32 thresholdOn,
                                                  f32 thresholdOff, f32 compExponent,
                                                  s32 attackSamples, s32 releaseSamples,
                                                  s32 groupChannels)
{
    self->mThresholdOn = thresholdOn;
    self->mThresholdOff = thresholdOff;
    self->mCompExponent = compExponent;
    self->mAttackSamples = attackSamples;
    self->mReleaseSamples = releaseSamples;
    self->mGroupChannels = static_cast<u8>((groupChannels & 0xFF) != 0 ? 1 : 0);
    self->mCompExponentStepOn = compExponent / static_cast<f32>(static_cast<s64>(attackSamples));
    self->mCompExponentStepOff = compExponent / static_cast<f32>(static_cast<s64>(releaseSamples));
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
//
// AUDIBLE CONSEQUENCE, stated plainly (phase E 2026-08-28, when Limiter1 went LIVE in the
// RWAC registration pass): both callers -- Limiter1::Process @0x82B9E3A0 and
// Compressor1::Process @0x82B9D988 -- run this kernel for its IN-PLACE effect on the
// context's source buffer and then return BUFFERSTATUS_AVAILABLE WITHOUT swapping the
// src/dst slots. With the kernel inert the audio therefore passes through the stage
// COMPLETELY UNMODIFIED (a transparent limiter), which is a safe, honest degradation
// rather than silence or a scribble: no buffer is written, no count is republished, and
// the surrounding Configure/state machine is fully faithful. What is lost is only the
// dynamics processing itself -- peaks that the console would have limited are passed at
// full level. Decoding the VMX128 lane math is the standing follow-up.
// -------------------------------------------------------------------------------------
int CompressorLimiter1::Process(CompressorLimiter1 * /*self*/,
                                AudioProcessContext * /*ctx*/, u8 /*channelCount*/)
{
    // Unrecoverable VMX128 hand-asm: not reconstructed (keystone). No fabricated math.
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
