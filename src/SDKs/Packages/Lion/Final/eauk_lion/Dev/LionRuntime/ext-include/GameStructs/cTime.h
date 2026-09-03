#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/ext-include/GameStructs/cTime.h
//
// cTime -- the game time stamp every Lion (eauk_lion) entry point takes by reference:
// cLionFX::Update / Render / Dispatch, cParticleEmitter::Update / Generate / Emit /
// InitialiseParticle, cParticleBucketManager's birth-time eviction, cParticleTrigger.
//
// ⛔⛔ IT WAS TWO DIFFERENT TYPES, OF TWO DIFFERENT WIDTHS, under one name:
//   ParticleBucket.h:78    struct cTime { u64 muTicks;   }   -- 8 bytes
//   ParticleTrigger.h:44   struct cTime { s32 mi32Ticks; }   -- 4 bytes
// Both were labelled HONEST PLACEHOLDER, and the second's own banner had already reasoned
// its way to the right answer from cParticleTrigger::Update @0x82908808 ("each stamp is
// loaded/stored as one 32-bit word ... which only fits if each stamp is 4 bytes"), while
// the first stayed 8 and nobody reconciled them. That is an ODR fork that links silently,
// and it carried a live width bug with it: ParticleBucketManager.cpp read
// `(s32)(u32)arTime.muTicks`, i.e. the LOW half of a 64-bit word -- which on the
// big-endian console would have been the WRONG half of a value that is not 64 bits at all.
//
// ⭐ THE DWARF SETTLES IT, AND SO DOES THE ASM, INDEPENDENTLY:
//   * DecFIGS DWARF (cTime.h:217, this very path): `private: S32 mTicks;` -- one signed
//     32-bit member, plus the accessor set below.
//   * cParticleEmitter's own record puts three cTime members at console +0x198, +0x19C and
//     +0x1A0 (DWARF ParticleEmitter.h:341-343) -- four bytes apart.
//   * cParticleEmitter::InitialiseParticle @0x829116A8 reads one with `lwz r11, 0(r10)`
//     followed by `extsw r11, r11`: a load of ONE word, sign-extended. A u64 would have
//     been `ld`.
//   * cParticleTrigger::Update @0x82908808 lands mbEnabled at +0x0C behind three stamps.
//
// ⭐⭐ THE TICK RATE IS 3000 Hz, AND THAT IS CORROBORATED TWICE OVER. The DWARF declares
// `const S32 msuTicksPerMilliSecond = 3` (cTime.h:18) -- 3 ticks per millisecond, so 3000
// per second -- and `const FP32 msfOneOverTicksPerSecond` (cTime.h:17). Independently,
// InitialiseParticle's birth-time line multiplies the tick count by flt_82F369A8, which
// tools/re/x360rd.py reads out of the image as 0x39AEC33E == 0.00033333332976326346 ==
// 1/3000. A DWARF constant and an image float agreeing to the bit is not a guess.
//
// ⚠ msfTicksPerSecond / msfOneOverTicksPerSecond are DWARF-declared as `const FP32` in the
// global namespace (cTime.h:16-17) and their stored values are NOT in this TU's functions
// -- the X360 folds the reciprocal into an rodata literal at each call site. They are
// defined here from msuTicksPerMilliSecond, which the DWARF DOES carry a value for, so the
// three constants cannot drift apart.
//
// ⚠ ATTESTATION. The DWARF declares ~30 methods on cTime (the full arithmetic/comparison
// operator set, Lerp, GetWeight, the millisecond accessors). None of them has an X360
// out-of-line body -- every one is inlined at its call sites -- so only the handful the
// reconstructed Lion bodies actually reach are written here, each with the call site that
// attests it. Grow this additively as bodies land; do NOT paste the whole DWARF list in.
// ============================================================================

#include "types.hpp"

// DWARF cTime.h:18 -- 3 ticks per millisecond.
const s32 msuTicksPerMilliSecond = 3;

// DWARF cTime.h:16-17. Derived from msuTicksPerMilliSecond rather than restated, so the
// three cannot drift. msfOneOverTicksPerSecond is the flt_82F369A8 that
// cParticleEmitter::InitialiseParticle @0x82911714 multiplies a tick count by.
const f32 msfTicksPerSecond        = 3000.0f;
const f32 msfOneOverTicksPerSecond = 1.0f / 3000.0f;

// DWARF cTime.h (struct cTime).
struct cTime
{
    // cTime.h:31. The raw signed tick count. Attested by the `lwz` + `extsw` pair at
    // cParticleEmitter::InitialiseParticle @0x82911700.
    s32 GetTicks() const { return mTicks; }

    // cTime.h:64. Ticks -> seconds. Attested by InitialiseParticle @0x82911700..0x82911720:
    // `lwz` / `extsw` / `std`+`lfd`+`fcfid` (the s32 -> double convert) / `frsp` /
    // `fmuls f0, f13, flt_82F369A8`. The console emits that sequence INLINE at the call
    // site; it is de-inlined back to this owning method (semantic parity, one owning body).
    f32 GetTimeSeconds() const
    {
        return static_cast<f32>(mTicks) * msfOneOverTicksPerSecond;
    }

    // cTime.h:79. Elapsed ticks between two stamps. Attested by
    // cParticleBucketManager::AllocateBucket @0x829145A0, whose oldest-bucket eviction
    // signed-compares `now - bucket->mLatestBirthTime`.
    s32 GetTimeDiff(const cTime& arEarlier) const { return mTicks - arEarlier.mTicks; }

private:
    s32 mTicks;   // DWARF cTime.h:217
};

// One signed word. cParticleEmitter's DWARF spaces three cTime members four bytes apart
// (+0x198 / +0x19C / +0x1A0), and InitialiseParticle @0x82911700 reads one with `lwz`.
static_assert(sizeof(cTime) == 4,
              "cTime is ONE S32 tick (DWARF cTime.h:217; cParticleEmitter's mParentTime / "
              "mLastTime / mUpdateLastTime sit at +0x198 / +0x19C / +0x1A0)");
