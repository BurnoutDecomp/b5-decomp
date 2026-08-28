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

    // @0x82B67188 -- store the two thresholds and the compressor exponent, the attack/release
    // lengths and their derived per-sample step coefficients, plus the channel-link flag.
    //
    // ABI CORRECTED (phase E 2026-08-28, decode report limiter1_configure_decode_codex.md):
    // the former `u32 a8, u32 a9, u8 a10` tail was a Hex-Rays artifact. The callee reads
    // exactly r3/f1/f2/f3/r7/r8/r9 -- the FP arguments occupy ABI positions 2..4, which is
    // why the three integers land in r7..r9 and IDA invented phantom slots for r4..r6. No
    // caller (Limiter1::Configure @0x82B97AB0, Compressor1::Configure @0x82B96B28) prepares
    // an r10 or a stack tail argument. Parameter names are the members each one is stored to.
    static CompressorLimiter1 *Configure(CompressorLimiter1 *self, f32 thresholdOn,
                                         f32 thresholdOff, f32 compExponent,
                                         s32 attackSamples, s32 releaseSamples,
                                         s32 groupChannels);

    // @0x82B64DB0 -- the per-block envelope-follower + gain-curve kernel. Hand-written
    // X360 VMX128 (2,294 instructions); DECODED AND BODIED 2026-08-28 (phase E; report
    // progress/scratch_dossiers/compressorlimiter1_process_vmx_decode_codex.md). Every
    // algorithmic element is recovered -- the one-pole envelope and its three rodata
    // coefficients, the on/off hysteresis, the exponent attack/release stepping, the
    // level^exponent gain curve, both channel topologies, the fixed 256-sample frame, the
    // destination-as-gain-scratch protocol and the buffer-slot swap. See the .cpp for the
    // single marked deviation (the vlogefp/vexptefp ESTIMATE pair has no bit-identical
    // portable form) and for the exact state/rounding order.
    //
    // ABI corrected 3-arg (was Process(self) placeholder) from the asm-attested call sites in
    // Compressor1::Process @0x82B9DA8C and Limiter1::Process: r3=self, r4=the audio process
    // context, r5=the base channel count (lbz 0x21). The console forms NO return value (the
    // independent path even reuses r3 as a pointer delta) and both callers discard it; the
    // `int` return is kept only as the committed declaration's shape and always returns 0.
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
