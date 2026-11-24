#pragma once

// Lion seed record (DecFIGS ParticleRandomSeed.h:27, :167-169). ARTIST's
// integer draw reads the embedded Random state at +0x20, and Init writes the
// Lion seed at +0x30. SubEmitterInit at 0x82911308 copies the full 0x40-byte
// record from bucket+0x10. The embedded Random supplies the 16-byte alignment.

#include "types.hpp"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"

#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector4 / Vector3Plus (BuildLerp)
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h"  // cVector -- Build(cVector&,...)

#include <cstddef>   // offsetof

// DWARF ParticleRandomSeed.h:23 (global-namespace const in the original header,
// name verbatim -- [sic], the original author's spelling of "Multiplication").
// The Park-Miller-style step Update applies to the 32-bit seed
// (X360 @0x8290EAD4..EAE8: lis 1; ori 0xFF5 -> 0x10FF5 == 69621; mullw).
const u32 kuSeedMultication = 69621;

struct cParticleRandomSeed
{
    // ParticleRandomSeed.h:35 -- reseed from the shared static counter (mBaseSeed
    // += 42, then force odd) and refill the cache. X360 Init @ 0x82911BE0.
    void Init();

    // ParticleRandomSeed.h:45 -- reseed by adding an explicit offset to the
    // current seed, then refill. X360 Offset @ 0x8290E8B0.
    void Offset(u32 auOffset);

    // ParticleRandomSeed.h:58 -- advance the seed one Park-Miller step
    // (mSeed *= kuSeedMultication), then refill. Bracketed by the Lion particle
    // update CPU perf monitor. X360 Update @ 0x8290EAB0.
    void Update();

    // ParticleRandomSeed.h:71 -- draw ONE cached random t in [0,1) from the slot at
    // muIndex, refill that slot, advance the index (wrapping mod 8), and return
    // afBase + ((afBase + afVariance) - afBase) * t. The source shape is Lerp(base,
    // base+variance, t) and the console keeps the redundant subtraction (fadds / fsubs /
    // fmadds at 0x8290A3FC..0x8290A410), so it is kept here rather than folded to
    // base + variance*t -- float arithmetic is not associative and this is the console's.
    // X360 @0x8290A360 (ledger-filed under CgsPerfMonCpuPS3.h -- a DecFIGS attribution
    // quirk; its real home is this class, per the DWARF).
    f32 Build(f32 afBase, f32 afVariance);

    // ParticleRandomSeed.h:87 -- the INTEGER draw: a uniform value in
    // [aiBase, aiBase + aiVariance] inclusive. X360 sub_8290A438 (unnamed in the idb; named
    // by the DWARF, and reached from cParticleEmitter::Generate @0x82915158 twice to decide
    // how many particles an emission burst produces).
    //
    // ⚠ IT DOES NOT TOUCH THE FLOAT CACHE. Unlike every other draw on this class it reads the
    // raw 64-bit LCG state's high word directly (`srdi r11, r11, 32` @0x8290A4E4) and never
    // reads, refills or advances mafRandom / muIndex -- so an integer draw and a float draw
    // interleaved do NOT consume the same stream position.
    s32 Build(s32 aiBase, s32 aiVariance);

    // ParticleRandomSeed.h:117 -- the PER-LANE sibling of BuildLerp: consume the whole
    // 16-byte cache half at slot (muIndex+3)&4 as FOUR independent randoms, refill all
    // four slots from three LCG steps, and return avBase + avVariance * t per lane.
    // X360 sub_8290A648 (unnamed in the idb; named by the DWARF).
    rw::math::vpu::Vector3Plus Build(rw::math::vpu::Vector4 avBase,
                                     rw::math::vpu::Vector4 avVariance);

    // ParticleRandomSeed.h:144 -- the OUT-PARAMETER sibling of Build(Vector4, Vector4): it
    // consumes the same 16-byte cache half at slot (muIndex+3)&4 as four per-lane randoms and
    // writes arBase + arVariance*t through arOut instead of returning it. X360 sub_8290A510
    // (unnamed in the idb; the DWARF names it, and cParticleEmitter::ParticleBuild @0x82910118
    // is its ONLY caller -- the DO_RADIAL arm asks it for a random direction in [-1,1) with
    // base splat4(-1) and variance splat4(2)).
    //
    // ⚠ IT IS NOT THE SAME FUNCTION AS Build(Vector4, Vector4) @0x8290A648, and the difference
    // is not the out parameter. That one takes THREE LCG steps and refills all FOUR cache
    // slots from a 96-bit stream; this one takes TWO and refills only THREE, advancing muIndex
    // by 3 rather than toggling it by 4. So the two leave the generator in different states
    // and are not interchangeable.
    void Build(cVector& arOut, const cVector& arBase, const cVector& arVariance);

    // ParticleRandomSeed.h:130 -- draw ONE cached random t in [0,1) (consumed
    // from the "Vector slot" (muIndex+3)&4, which is then refilled and the index
    // parked just past it) and return avBase + avRange*t on all four lanes: a
    // lerp along [base, base+range] with a single shared t. The DWARF spells the
    // parameter/return types bare Vector4 / Vector3Plus (the rw::math::vpu types).
    // X360 BuildLerp @ 0x8290A7A8; bracketed by the same perf monitor as the
    // Build overloads (dword_82FAB68C).
    rw::math::vpu::Vector3Plus BuildLerp(rw::math::vpu::Vector4 avBase,
                                         rw::math::vpu::Vector4 avRange);

protected:
    CgsNumeric::Random mCgsRandom; // +0x00
    u32 mSeed;                    // +0x30

    // DWARF ParticleRandomSeed.h:168 -- the shared reseed counter every Init
    // bumps by 42 (X360 dword_82F350FC). A static MEMBER per the DWARF, not a
    // file-local global. Its boot-time start value is not encoded in this TU's
    // functions; modelled zero-initialised.
    static u32 mBaseSeed;

private:
    static void _AssertLayout()
    {
        CGS_STATIC_ASSERT((offsetof(cParticleRandomSeed, mCgsRandom) == 0x00), "Lion random at +0x00");
        CGS_STATIC_ASSERT((offsetof(cParticleRandomSeed, mSeed) == 0x30), "Lion seed at +0x30");
        CGS_STATIC_ASSERT((sizeof(cParticleRandomSeed) == 0x40), "Lion ARTIST seed record size");
        CGS_STATIC_ASSERT((__alignof(cParticleRandomSeed) == 16), "Lion ARTIST seed alignment");
    }
};
