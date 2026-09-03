#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRandomSeed.h
//
// cParticleRandomSeed -- the Lion (eauk_lion) particle runtime's tiny per-effect
// random number generator. It keeps a 64-bit LCG state (the classic MMIX / Knuth
// multiplier 6364136223846793005) and precomputes a small ring of 8 canonical
// floats in the range [1.0, 2.0) that the particle simulation samples cheaply.
// Init / Offset / Update each reseed the generator (from the shared static
// mBaseSeed counter, an explicit offset, or a Park-Miller step of the current
// seed respectively) and then refill the whole 8-entry cache. BuildLerp draws
// ONE cached random t in [0,1) and returns avBase + avRange*t across all four
// vector lanes (a lerp along the segment [base, base+range]).
//
// DECLARATION SHAPE (DecFIGS DWARF, found 2026-08-04 at
// references/DecFIGS/dwarfdump/SDKs/Packages/Lion/Final/eauk_lion/Dev/
// LionRuntime/include/ParticleRandomSeed.h -- the earlier "DWARF none found"
// dossier line was the known class-keyed-TU false negative):
//   struct cParticleRandomSeed {           // ParticleRandomSeed.h:27
//     protected:
//       Random mCgsRandom;                 // :167  == CgsNumeric::Random
//       static U32 mBaseSeed;              // :168  (X360 dword_82F350FC)
//       U32 mSeed;                         // :169
//     public:
//       void Init();                       // :35
//       void Set(U32);                     // :38   PS3-only on the X360 ledger
//       void Offset(U32);                  // :45
//       U32 Get() const;                   // :52   PS3-only on the X360 ledger
//       void Update();                     // :58
//       FP32 Build(FP32, FP32);            // :71   X360 @0x8290A360 (OTHER TU, see below)
//       S32 Build(S32, S32);               // :87   not in the X360 ledger
//       void Build(Vector3&, Vector3, Vector3);        // :104  not in the X360 ledger
//       Vector3Plus Build(Vector4, Vector4);           // :117  X360 unnamed sub_8290A648
//       Vector3Plus BuildLerp(Vector4, Vector4);       // :130  X360 @0x8290A7A8
//       void Build(cVector&, const cVector&, const cVector&); // :144  not in the X360 ledger
//   };
// Methods absent from the X360 ledger are NOT declared here (attestation rule).
// ⭐ 2026-09-03: the two Build overloads present in the X360 binary ARE now declared and
// bodied in this file -- FP32 Build @0x8290A360 (ledger-filed under CgsPerfMonCpuPS3.h, a
// DecFIGS attribution quirk) and Vector3Plus Build(Vector4,Vector4) @0x8290A648 (unnamed
// sub_8290A648, named by the DWARF at :117). cParticleEmitter::InitialiseParticle calls
// BOTH fourteen times between them, so this is where they had to land.
//
// LAYOUT AUTHORITY: the member offsets are pinned by the X360 asm for the four
// bodied methods (Init @0x82911BE0, Offset @0x8290E8B0, Update @0x8290EAB0,
// BuildLerp @0x8290A7A8):
//
//   mafRandom[i]  @ 0x00 + 4*i   (stwx r,index*4,this ; stw r,0(this))
//   mu64State     @ 0x20         (ld / std -- 64-bit LCG state)
//   muIndex       @ 0x28         (lwz / stw)
//   mSeed         @ 0x30         (lwz / stw ; the 32-bit reseed value)
//
// NOTE ON THE FIRST THREE MEMBERS: per the DWARF they are not loose members of
// this class -- they are the embedded `Random mCgsRandom` sub-object, and
// `Random` is CgsNumeric::Random (GameShared/GameClasses/Numeric/CgsRandom.h),
// whose DWARF spine matches slot for slot:
//   mafRandom  == mCgsRandom's union { mafFloatBuffer[8] / mauIntegerBuffer[8] }
//   mu64State  == mCgsRandom.muSeed
//   muIndex    == mCgsRandom.muOldestBufferIndex
//   muReserved2C == mCgsRandom's tail padding (0x2C..0x2F, sizeof(Random)==0x30)
// The flat modelling is kept FOR NOW because the draw/refill sequences the X360
// inlines into this TU touch Random's PRIVATE spine directly (exactly like the
// already-befriended BrnEffects::Utils randomisers); embedding the real
// CgsNumeric::Random needs a `friend struct cParticleRandomSeed;` grant in
// CgsRandom.h (a shared header this TU does not own -- requested, see the wave
// spec). Behaviour is identical either way; this note is the guard against the
// two spines drifting apart.
//
// The struct's console size (0x34 == 52 bytes) is corroborated independently by
// cParticleBucket, whose DWARF places mRandomSeed at bucket+0x1C and the next
// member (mnNextParticlePositionToFill) at bucket+0x50 -- a gap of exactly 0x34.
// (On the 64-bit host the u64 forces 8-byte struct alignment, so sizeof rounds up
// to 56; the load-bearing facts are the RELATIVE member offsets above, pinned by
// the _AssertLayout static_asserts, not the padded tail.)
// ============================================================================

#include "types.hpp"

#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector4 / Vector3Plus (BuildLerp)

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

    // ParticleRandomSeed.h:130 -- draw ONE cached random t in [0,1) (consumed
    // from the "Vector slot" (muIndex+3)&4, which is then refilled and the index
    // parked just past it) and return avBase + avRange*t on all four lanes: a
    // lerp along [base, base+range] with a single shared t. The DWARF spells the
    // parameter/return types bare Vector4 / Vector3Plus (the rw::math::vpu types).
    // X360 BuildLerp @ 0x8290A7A8; bracketed by the same perf monitor as the
    // Build overloads (dword_82FAB68C).
    rw::math::vpu::Vector3Plus BuildLerp(rw::math::vpu::Vector4 avBase,
                                         rw::math::vpu::Vector4 avRange);

private:
    // Rebuild the 8-entry [1,2) float cache from the current mSeed: seed the 64-bit
    // LCG with mSeed in the high dword, then step it once per entry. Shared body the
    // X360 compiler inlined into Init / Offset / Update.
    void RefillCache();

    // ----- members (offsets pinned against the X360 asm; the first three are
    // ----- the embedded CgsNumeric::Random spine -- see the banner) ----------
    f32 mafRandom[8];   // 0x00  cache of 8 canonical floats in [1.0, 2.0)
    u64 mu64State;      // 0x20  64-bit LCG state          (Random::muSeed)
    u32 muIndex;        // 0x28  next cache slot to fill    (Random::muOldestBufferIndex)
    u32 muReserved2C;   // 0x2C  tail padding of the embedded Random (never touched)
    u32 mSeed;          // 0x30  32-bit reseed value (DWARF ParticleRandomSeed.h:169)

    // DWARF ParticleRandomSeed.h:168 -- the shared reseed counter every Init
    // bumps by 42 (X360 dword_82F350FC). A static MEMBER per the DWARF, not a
    // file-local global. Its boot-time start value is not encoded in this TU's
    // functions; modelled zero-initialised.
    static u32 mBaseSeed;

    // Byte-layout pin: the relative member offsets are ground truth from the asm.
    static void _AssertLayout()
    {
        static_assert(offsetof(cParticleRandomSeed, mafRandom) == 0x00, "mafRandom@0");
        static_assert(offsetof(cParticleRandomSeed, mu64State) == 0x20, "mu64State@0x20");
        static_assert(offsetof(cParticleRandomSeed, muIndex)   == 0x28, "muIndex@0x28");
        static_assert(offsetof(cParticleRandomSeed, mSeed)     == 0x30, "mSeed@0x30");
    }
};
