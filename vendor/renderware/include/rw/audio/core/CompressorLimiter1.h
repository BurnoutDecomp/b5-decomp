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
// =====================================================================================

#include "types.hpp" // f32, s32, u32, u8

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// CompressorLimiter1
//
// The leading +0x00..+0x2F span is the per-channel envelope/gain scratch the Process
// kernel reads and writes (ClearBuffer zeroes exactly 0x30 bytes of it). The config
// block starts at +0x30:
//   +0x00..+0x2F  maEnvelope  (48 bytes; per-channel envelope/gain working state,
//                              zeroed by ClearBuffer)
//   +0x30  mfThreshold   (f32, Configure a2 -> threshold)
//   +0x34  mfRatioParam  (f32, Configure a3 -> the ratio/knee parameter)
//   +0x38  mfMakeupGain  (f32, Configure a4 -> the numerator of both time constants)
//   +0x3C  miAttack      (s32, Configure a5 -> attack length in samples)
//   +0x40  miRelease     (s32, Configure a6 -> release length in samples)
//   +0x44  mfAttackCoeff (f32 = mfMakeupGain / miAttack)
//   +0x48  mfReleaseCoeff(f32 = mfMakeupGain / miRelease)
//   +0x4C  mbStereoLink  (u8  = (Configure a7 != 0); the "linked channels" flag the
//                              Process kernel tests at +76)
// (sizeof rounded up by the embedding plug-in; the type itself spans +0x00..+0x4C.)
// -------------------------------------------------------------------------------------
class CompressorLimiter1
{
public:
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
    static int Process(CompressorLimiter1 *self);

    f32 maEnvelope[12];  // +0x00..+0x2F (48 bytes)
    f32 mfThreshold;     // +0x30
    f32 mfRatioParam;    // +0x34
    f32 mfMakeupGain;    // +0x38
    s32 miAttack;        // +0x3C
    s32 miRelease;       // +0x40
    f32 mfAttackCoeff;   // +0x44
    f32 mfReleaseCoeff;  // +0x48
    u8  mbStereoLink;    // +0x4C
};

} // namespace core
} // namespace audio
} // namespace rw
