#pragma once

// =====================================================================================
// rw::audio::core::CompressorLimiter1 -- the shared envelope/gain engine behind the
// Compressor1 and Limiter1 audio plug-ins (both call Configure/ClearBuffer/Process).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every member offset. No Feb-2007 leak source and no DecFIGS DWARF
// exist, so each offset below is grounded directly in the disassembly of Configure
// @0x82B67188 and ClearBuffer @0x82B671F0.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API.
//
// FLAG (rwaudio PDB reconcile 2026-06-27): names/types reconciled against the NFS
// ProStreet 08 Milestone X360 PDB (class rw::audio::core::CompressorLimiter1,
// sizeof=80 incl. 3 tail-pad bytes). Field order and all +0xNN offsets MATCH the
// X360 ARTIST layout, so this is match-reconciled. Notable corrections the asm guess
// got semantically wrong: +0x34 was guessed "ratio" but is mThresholdOff; +0x38 was
// "makeup gain" but is mCompExponent; the two coeffs at +0x44/+0x48 are the
// compressor-exponent step values; the +0x4C flag is mGroupChannels (channel link).
// The +0x00 scratch is a History[6] array (per-channel {lpfDelay1, compExponentCurrent}),
// not a flat f32[12] -- same 48 bytes, retyped to the PDB nested struct.
// =====================================================================================

#include "types.hpp" // f32, s32, u32, u8

namespace rw
{
namespace audio
{
namespace core
{

// The per-block audio process context passed to Process (defined in Iir2Filters.h). Only a
// pointer is needed here, so forward-declare it to avoid the include cascade.
class Mixer;                             // the stage process context (unified; Mixer.h)
typedef Mixer AudioProcessContext;

// -------------------------------------------------------------------------------------
// CompressorLimiter1
//
// The leading +0x00..+0x2F span is the per-channel envelope/gain scratch the Process
// kernel reads and writes (ClearBuffer zeroes exactly 0x30 bytes of it). The config
// block starts at +0x30:
//   +0x00..+0x2F  mChannelHistory[6] (48 bytes; per-channel History working state,
//                                     zeroed by ClearBuffer)
//   +0x30  mThresholdOn        (f32, Configure a2 -> on threshold)
//   +0x34  mThresholdOff       (f32, Configure a3 -> off threshold)
//   +0x38  mCompExponent       (f32, Configure a4 -> the compressor exponent / numerator
//                                    of both step constants)
//   +0x3C  mAttackSamples      (s32, Configure a5 -> attack length in samples)
//   +0x40  mReleaseSamples     (s32, Configure a6 -> release length in samples)
//   +0x44  mCompExponentStepOn (f32 = mCompExponent / mAttackSamples)
//   +0x48  mCompExponentStepOff(f32 = mCompExponent / mReleaseSamples)
//   +0x4C  mGroupChannels      (u8  = (Configure a7 != 0); the "linked channels" flag the
//                                    Process kernel tests at +76)
// (PDB sizeof=80 incl. 3 tail-pad bytes; the named fields span +0x00..+0x4C.)
// -------------------------------------------------------------------------------------
class CompressorLimiter1
{
public:
    // PDB-confirmed: per-channel envelope/gain history (mChannelHistory[6], 8 bytes each).
    struct History
    {
        f32 lpfDelay1;            // +0x00
        f32 compExponentCurrent;  // +0x04
    };

    // @0x82B671F0 -- zero the 48-byte per-channel envelope/gain scratch.
    static void *ClearBuffer(CompressorLimiter1 *self);

    // @0x82B67188 -- store the threshold/ratio/makeup, the attack/release lengths and
    // their derived per-sample coefficients, plus the stereo-link flag.
    static CompressorLimiter1 *Configure(CompressorLimiter1 *self, f32 threshold,
                                         f32 ratioParam, f32 makeupGain, s32 attack,
                                         s32 release, s32 stereoLink, u32 a8, u32 a9,
                                         u8 a10);

    // @0x82B64DB0 -- the per-block envelope-follower + gain-curve kernel. KEYSTONE:
    // this body is hand-written X360 VMX (Altivec/VMX128) over lvx128/stvx128 vectors
    // with vlogefp/vexptefp/vctsxs/vrfiz/vsel lane ops; its DSP semantics are NOT
    // recoverable store-for-store as portable C++ from the Hex-Rays transliteration.
    // Declared here for the layout/ABI; intentionally NOT bodied (see CompressorLimiter1.cpp).
    //
    // ABI corrected 3-arg (was Process(self) placeholder) from the asm-attested call sites in
    // Compressor1::Process @0x82B9DA8C and Limiter1::Process: r3=self, r4=the audio process
    // context, r5=the base channel count (lbz 0x21). The keystone body still ignores them.
    static int Process(CompressorLimiter1 *self, AudioProcessContext *ctx, u8 channelCount);

    History mChannelHistory[6];  // +0x00..+0x2F (48 bytes)
    f32 mThresholdOn;            // +0x30
    f32 mThresholdOff;           // +0x34
    f32 mCompExponent;           // +0x38
    s32 mAttackSamples;          // +0x3C
    s32 mReleaseSamples;         // +0x40
    f32 mCompExponentStepOn;     // +0x44
    f32 mCompExponentStepOff;    // +0x48
    u8  mGroupChannels;          // +0x4C
};

} // namespace core
} // namespace audio
} // namespace rw
