// ============================================================================
// ParticleRandomSeed.cpp -- cParticleRandomSeed runtime bodies.
//
// Bodied from the X360 asm: Init @0x82911BE0, Offset @0x8290E8B0,
// Update @0x8290EAB0, BuildLerp @0x8290A7A8. The first three share one inlined
// refill loop (RefillCache).
//
// The generator is a 64-bit LCG using the MMIX / Knuth multiplier
// 6364136223846793005 (0x5851F42D4C957F2D, built on X360 from the two halves
// 0x4C957F2D and 0x5851F42D via insrdi) with increment 1. Each cache entry is a
// canonical float in [1.0, 2.0): the top 23 bits of the state's high dword are
// stuffed into the mantissa of an IEEE-754 float whose sign/exponent bits are
// 0x3F800000 (the asm does this with `inslwi rD,rHigh,23,9` over a preloaded
// 0x3F800000, i.e. bits = 0x3F800000 | ((state>>32) >> 9)).
//
// DECLARATION SHAPE comes from the DecFIGS DWARF for this very header (see
// ParticleRandomSeed.h's banner): mBaseSeed is the class's static member
// (X360 dword_82F350FC), mSeed the 32-bit reseed value, and the ring/state/index
// spine is the embedded CgsNumeric::Random (modelled flat here for now).
//
// 2026-09-03: the two remaining Build overloads are BODIED HERE now -- FP32
// Build(FP32,FP32) @0x8290A360 and Vector3Plus Build(Vector4,Vector4) @0x8290A648
// (unnamed sub_8290A648 in the X360 idb, named by the DWARF at :117). Both were
// ledger-filed under other TUs; this class is their real home, and
// cParticleEmitter::InitialiseParticle @0x829116A8 calls them fourteen times
// between them, so nothing downstream could link until they landed.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRandomSeed.h"

#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu
#include "GameShared/GameClasses/Core/CgsAssert.h"   // the two CgsRandom.h range asserts Build(s32,s32) fires

#include "rw/math/vpu/vector4_operation.h"  // Vector4 operator+/operator*, Splat

#include <cstring>   // memcpy

// DWARF ParticleRandomSeed.h:168 (X360 dword_82F350FC): the shared reseed
// counter, bumped by 42 on every Init and OR'd with 1 to keep the seed odd.
// Initialised elsewhere at boot; its start value is not encoded in this TU's
// functions, so it is modelled zero-initialised.
u32 cParticleRandomSeed::mBaseSeed = 0u;

namespace
{
    // MMIX / Knuth 64-bit LCG multiplier (0x5851F42D4C957F2D). On X360 it is
    // assembled from lis/ori 0x4C957F2D (low) + lis/ori 0x5851F42D (high) folded
    // in with insrdi r11,r9,32,0 -> the 0x5851F42D half lands in the HIGH dword.
    const u64 KU64_LCG_MULTIPLIER = 0x5851F42D4C957F2DULL;

    // CPU perf-monitor handle for Update (X360 dword_82FAB690); -1 == unregistered.
    s32 guUpdateMonitor = -1;

    // CPU perf-monitor handle shared by the Build family -- BuildLerp @0x8290A7A8,
    // FP32 Build @0x8290A360 and the unnamed Vector3Plus Build @0x8290A648 all
    // load the SAME global (X360 dword_82FAB68C); -1 == unregistered.
    s32 guBuildMonitor = -1;

    // Build a canonical float in [1.0, 2.0) from the current LCG state: take the
    // high 32 bits, keep the top 23 (>> 9), and OR them into 0x3F800000.
    inline f32 MakeCanonicalFloat(u64 au64State)
    {
        const u32 luBits = 0x3F800000u | (static_cast<u32>(au64State >> 32) >> 9);
        f32 lfValue;
        std::memcpy(&lfValue, &luBits, sizeof(lfValue));
        return lfValue;
    }
}

// Seed the 64-bit LCG with mSeed in the high dword and fill all 8 cache slots.
// The X360 emits this as a fully unrolled 8x sequence inside each caller; it is
// re-rolled here into the loop it started as. Each iteration writes the current
// slot from the live state, advances the LCG one step, and bumps the wrapping
// index -- leaving muIndex back at 0 after eight entries.
void cParticleRandomSeed::RefillCache()
{
    muIndex   = 0;
    mu64State = static_cast<u64>(mSeed) << 32;

    for (u32 luEntry = 0; luEntry < 8; ++luEntry)
    {
        mafRandom[muIndex] = MakeCanonicalFloat(mu64State);
        mu64State = mu64State * KU64_LCG_MULTIPLIER + 1;
        muIndex   = (muIndex + 1) & 7;
    }
}

// cParticleRandomSeed::Init @ 0x82911BE0
void cParticleRandomSeed::Init()
{
    mBaseSeed += 42;              // dword_82F350FC += 42 (stored WITHOUT the |1)
    mSeed = mBaseSeed | 1u;       // force odd
    RefillCache();
}

// cParticleRandomSeed::Offset @ 0x8290E8B0
void cParticleRandomSeed::Offset(u32 auOffset)
{
    mSeed = auOffset + mSeed;
    RefillCache();
}

// cParticleRandomSeed::Update @ 0x8290EAB0
void cParticleRandomSeed::Update()
{
    const s32 liMonitor = guUpdateMonitor;         // dword_82FAB690, loaded once
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    mSeed = mSeed * kuSeedMultication;             // seed *= 69621 (mullw, 32-bit wrap)
    RefillCache();

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}

// cParticleRandomSeed::BuildLerp @ 0x8290A7A8
//
// Draw ONE cached random t in [0,1) and lerp: every lane of the result is
// base + range*t with the SAME t (the sibling Build @0x8290A648 instead uses
// four independent per-lane randoms -- that is the whole difference).
//
// MEASURED (headless IDA, instruction word @0x8290A860 = 0x17FE04FC): the
// VMX128 FMA `vmaddfp128 v127,v126,v0,v127` decodes VD=v127 VA=v126 VB=v0
// (xop 208's mask-0x3D0 bits check out; the sibling @0x8290A6F4 = 0x17E0F0DF
// genuinely swaps VA/VB, proving the fields are real). The VX128 form has only
// THREE register fields -- the addend is architecturally the destination:
// VD <- VA*VB + VD. So the addend is v127 = v1 = avBase, and the random splat
// v0 is a MULTIPLIER on v126 = v2 = avRange: result = avBase + avRange*t.
// (The old parked reading "t as addend" is refuted by the encoding -- there is
// nowhere to encode a separate addend.)
//
// Draw mechanics, store for store from the asm @0x8290A7E8..0x8290A85C:
//   slot = (muIndex + 3) & 4              (rlwinm ..,0,29,29 -> 0 or 4)
//   t    = mafRandom[slot] - 1.0f         (lvx128 of the 16B-aligned half at
//                                          slot*4 & ~15, vperm-splat of its
//                                          FIRST word == buf[slot], read BEFORE
//                                          the refill store; vsubfp 1.0)
//   mafRandom[slot] = canonical(state)    (inslwi over 0x3F800000, PRE-step hi)
//   state = state * KU64_LCG_MULTIPLIER + 1
//   muIndex = slot + 1                    (addi 1 -- NO &7 wrap here: slot is 0
//                                          or 4, so the index parks at 1 or 5;
//                                          this differs from both RefillCache's
//                                          &7 wrap and Build @0x8290A648's ^= 4)
rw::math::vpu::Vector3Plus cParticleRandomSeed::BuildLerp(rw::math::vpu::Vector4 avBase,
                                                          rw::math::vpu::Vector4 avRange)
{
    using rw::math::vpu::Splat;
    using rw::math::vpu::Vector3Plus;
    using rw::math::vpu::Vector4;

    const s32 liMonitor = guBuildMonitor;          // dword_82FAB68C, loaded once
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    // Consume the Vector-slot random (the value CURRENTLY buffered there) ...
    const u32 luSlot   = (muIndex + 3) & 4;
    const f32 lfRandom = mafRandom[luSlot] - 1.0f; // [1,2) -> [0,1)

    // ... then refill that one slot from the live state and park the index.
    mafRandom[luSlot] = MakeCanonicalFloat(mu64State);
    mu64State = mu64State * KU64_LCG_MULTIPLIER + 1;
    muIndex   = luSlot + 1;

    // vmaddfp128: result = avBase + avRange * splat(t), all four lanes (the
    // Vector3Plus "plus" lane gets avBase.w + avRange.w*t like the others).
    const Vector4 lvResult = avBase + avRange * Splat(lfRandom);

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);

    return Vector3Plus{ lvResult.x, lvResult.y, lvResult.z, lvResult.w };
}

// ================================================================================================
// cParticleRandomSeed::Build(FP32, FP32)  @ 0x8290A360      (DWARF ParticleRandomSeed.h:71)
//
// The SCALAR draw. Store for store from the asm:
//
//   0x8290A3A8  state   = mu64State                       ld   r11, 0x20(r31)
//   0x8290A3B0  index   = muIndex                         lwz  r10, 0x28(r31)
//   0x8290A3B8  hi      = state >> 32                     srdi r7, r11, 32
//   0x8290A3C4  state'  = state * MULT + 1                mulld / addi 1
//   0x8290A3DC  t_raw   = mafRandom[index]                lfsx f29, r10*4, r31  (read BEFORE refill)
//   0x8290A3E0  mu64State = state'                        std
//   0x8290A3E4  mafRandom[index] = 0x3F800000|(hi>>9)     stwx  (inslwi r8,r11,23,9)
//   0x8290A3F0  muIndex = (index + 1) & 7                 clrlwi r11, r11, 29
//   0x8290A3FC  return  fmadds((base+variance) - base, t_raw - 1.0, base)
//
// The redundant subtraction is the console's and it is kept. `(base + variance) - base` is
// not `variance` in float arithmetic, and the X360 emits all three ops (fadds 0x8290A3FC,
// fsubs 0x8290A40C, fmadds 0x8290A410). Folding it would be a tuning change dressed as a
// simplification. What the shape says is that the ORIGINAL source read Lerp(base,
// base + variance, t) -- i.e. "variance" is an extent, not a +/- radius.
//
// The perf monitor STOPS BEFORE the arithmetic (bl StopMonitor at 0x8290A3F8, the fadds at
// 0x8290A3FC). Reproduced in that order -- the monitor brackets the DRAW, not the lerp.
// ================================================================================================
f32 cParticleRandomSeed::Build(f32 afBase, f32 afVariance)
{
    const s32 liMonitor = guBuildMonitor;          // dword_82FAB68C, loaded once
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    // Consume the value CURRENTLY buffered at muIndex ...
    const u32 luIndex  = muIndex;
    const f32 lfRandom = mafRandom[luIndex];       // still in [1,2) here

    // ... then refill that one slot from the live state and advance the ring.
    mafRandom[luIndex] = MakeCanonicalFloat(mu64State);
    mu64State = mu64State * KU64_LCG_MULTIPLIER + 1;
    muIndex   = (luIndex + 1) & 7;

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);

    const f32 lfT = lfRandom - 1.0f;               // [1,2) -> [0,1)   (flt_82001C98 == 1.0)
    return ((afBase + afVariance) - afBase) * lfT + afBase;
}

// ================================================================================================
// cParticleRandomSeed::Build(Vector4, Vector4)  @ 0x8290A648   (DWARF ParticleRandomSeed.h:117)
//
// The PER-LANE draw -- the one cParticleEmitter::InitialiseParticle calls twelve times to fill a
// particle's twelve vector channels. Unnamed in the X360 idb (sub_8290A648); the DWARF names it,
// and cParticleEmitter::InitialiseParticle's call sites confirm the class.
//
// WHAT MAKES IT DIFFERENT FROM BuildLerp @0x8290A7A8, which has the same shape: BuildLerp
// SPLATS one random across the four lanes (`lvsl` + `vspltw` + `vperm` at 0x8290A7DC..0x8290A804);
// this one loads the 16-byte cache half straight (`lvx128 v0, r8, r31` @0x8290A6E4, no splat), so
// each lane gets its OWN random. That is the whole difference, and it is why a Lion particle's
// position / velocity / size vary per axis rather than along a single diagonal.
//
// The draw, store for store (0x8290A688..0x8290A780):
//   slot    = (muIndex + 3) & 4                   rlwinm r11, r9, 0,29,29   -> 0 or 4
//   muIndex = slot                                stw @0x8290A6CC (the refill stores re-read it)
//   v_t     = mafRandom[slot..slot+3] - 1.0       lvx128 at this + slot*4 (16-aligned) ; vsubfp
//   result  = avBase + avVariance * v_t           vmaddfp128 v127, v0, v126  (VD is the addend)
//   S1 = S0*MULT+1 ; S2 = S1*MULT+1 ; S3 = S2*MULT+1 ; mu64State = S3      (THREE steps)
//   the four refilled slots (see the bit-stream note below)
//   muIndex ^= 4                                  xori @0x8290A77C
//
// THE FOUR REFILLS ARE ONE 96-BIT STREAM CUT INTO FOUR 23-BIT MANTISSAS, not four independent
// draws -- which is why the asm looks like bit soup. Concatenate the high dwords of the three
// LCG states, hi(S0)||hi(S1)||hi(S2), and take 23 bits at a time:
//   slot+0 : hi0[0..22]                          inslwi r5,hi0,23,9        -> hi0 >> 9
//   slot+1 : hi0[23..31] ++ hi1[0..13]           insrwi r4,hi0,9,9 | (hi1 >> 18)
//   slot+2 : hi1[14..31] ++ hi2[0..4]            insrwi r28,hi1,18,9 | (hi2 >> 27)
//   slot+3 : hi2[5..27]                          rlwimi r27,hi2,28,9,31    -> (hi2 >> 4) & mask
// 9+14 == 23 and 18+5 == 23 -- the two stitched entries close exactly, and 4*23 == 92 of the 96
// bits are consumed (hi2's last four are dropped). Each result is OR'd into a preloaded
// 0x3F800000, i.e. the same "canonical float in [1,2)" the rest of this class uses.
// Only the FIRST of the four is MakeCanonicalFloat(state); the other three are NOT, and calling
// the shared helper for them would silently substitute three different numbers.
//
// The result is stored to the sret pointer mid-function (stvx128 v127, r0, r30 @0x8290A708,
// before the refill stores). Immaterial here -- it is one function's local -- and reproduced as
// a plain return, which is what the source wrote.
// ================================================================================================
rw::math::vpu::Vector3Plus cParticleRandomSeed::Build(rw::math::vpu::Vector4 avBase,
                                                      rw::math::vpu::Vector4 avVariance)
{
    using rw::math::vpu::Vector3Plus;

    const s32 liMonitor = guBuildMonitor;          // dword_82FAB68C, loaded once
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    // Consume the whole 16-byte cache half -- four independent randoms, one per lane.
    const u32 luSlot = (muIndex + 3) & 4;
    muIndex = luSlot;

    const f32 lfT0 = mafRandom[luSlot + 0] - 1.0f;   // [1,2) -> [0,1), per lane
    const f32 lfT1 = mafRandom[luSlot + 1] - 1.0f;
    const f32 lfT2 = mafRandom[luSlot + 2] - 1.0f;
    const f32 lfT3 = mafRandom[luSlot + 3] - 1.0f;

    const Vector3Plus lvResult = {
        avBase.x + avVariance.x * lfT0,
        avBase.y + avVariance.y * lfT1,
        avBase.z + avVariance.z * lfT2,
        avBase.w + avVariance.w * lfT3,
    };

    // Three LCG steps; their three high dwords are the 96-bit stream cut into four mantissas.
    const u64 lu64S0 = mu64State;
    const u64 lu64S1 = lu64S0 * KU64_LCG_MULTIPLIER + 1;
    const u64 lu64S2 = lu64S1 * KU64_LCG_MULTIPLIER + 1;
    const u64 lu64S3 = lu64S2 * KU64_LCG_MULTIPLIER + 1;
    mu64State = lu64S3;

    const u32 luHi0 = static_cast<u32>(lu64S0 >> 32);
    const u32 luHi1 = static_cast<u32>(lu64S1 >> 32);
    const u32 luHi2 = static_cast<u32>(lu64S2 >> 32);

    const u32 KU_ONE_BITS = 0x3F800000u;             // the preloaded `lis rN, 0x3F80`
    const u32 lauBits[4] = {
        KU_ONE_BITS |  (luHi0 >> 9),                                    // hi0[0..22]
        KU_ONE_BITS | ((luHi0 & 0x1FFu) << 14) | (luHi1 >> 18),         // hi0[23..31]++hi1[0..13]
        KU_ONE_BITS | ((luHi1 & 0x3FFFFu) << 5) | (luHi2 >> 27),        // hi1[14..31]++hi2[0..4]
        KU_ONE_BITS | ((luHi2 >> 4) & 0x7FFFFFu),                       // hi2[5..27]
    };
    for (u32 luLane = 0; luLane < 4; ++luLane)
    {
        std::memcpy(&mafRandom[luSlot + luLane], &lauBits[luLane], sizeof(f32));
    }

    muIndex ^= 4u;

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);

    return lvResult;
}

// ----------------------------------------------------------------------------
// EVERY cParticleRandomSeed METHOD THE X360 LEDGER ATTESTS NOW HAS A BODY IN THIS FILE:
// Init @0x82911BE0, Offset @0x8290E8B0, Update @0x8290EAB0, BuildLerp @0x8290A7A8,
// Build(FP32,FP32) @0x8290A360 and Build(Vector4,Vector4) @0x8290A648. The DWARF also
// declares Set(U32) / Get() const / Build(S32,S32) / Build(Vector3&,Vector3,Vector3) /
// Build(cVector&,const cVector&,const cVector&); none of those has an X360 body, so per
// the attestation rule they are neither declared nor written.
// ----------------------------------------------------------------------------

// ================================================================================================
// cParticleRandomSeed::Build(S32, S32)  @0x8290A438      (DWARF ParticleRandomSeed.h:87)
//
// Unnamed in the idb (sub_8290A438); the DWARF names it and cParticleEmitter::Generate
// @0x82915158 is the caller that matters -- it asks this for the particle COUNT of an emission
// burst, twice, from the behaviour's base/variance pair.
//
// Store for store from the asm:
//
//   0x8290A464  liMax = aiBase + aiVariance
//   0x8290A474  assert(liMax >= liMin)                 CgsRandom.h:320
//   0x8290A49C  luMod = liMax - aiBase + 1
//   0x8290A4A4  assert(luMod > 0)                      CgsRandom.h:323
//   0x8290A4C4  state = mu64State
//   0x8290A4E4  hi    = (u32)(state >> 32)             -- the PRE-step high word
//   0x8290A4DC  mu64State = state * 0x5851F42D4C957F2D + 1
//   0x8290A4F0  StopMonitor                            -- BEFORE the modulo, as with Build(f32,f32)
//   0x8290A4F4  return hi % luMod + aiBase             divwu / mullw / subf / add
//
// ⭐ THE MULTIPLIER IS BUILT IN TWO HALVES AND IS THE SAME MMIX CONSTANT the rest of this class
// uses: `lis r9, 0x5851 ; ori r9, r9, 0xF42D` gives the high word and `ori r10, r10, 0x7F2D`
// the low, spliced by `insrdi r10, r9, 32, 0` into 0x5851F42D4C957F2D. It is written as
// KU64_LCG_MULTIPLIER here rather than re-derived.
//
// ⭐ THE HIGH WORD IS TAKEN BEFORE THE STEP, not after (the `srdi` at 0x8290A4E4 reads r11,
// which was loaded at 0x8290A4C4 and is never overwritten -- the stepped value goes to r10).
// The same pre-step convention as Build(f32,f32)'s cache refill.
//
// ⚠ THE PERF MONITOR STOPS BEFORE THE ARITHMETIC, exactly as in the f32 overload: the monitor
// brackets the DRAW, not the range fold. Reproduced in that order.
//
// ⚠ THE MODULO IS UNSIGNED (`divwu`), on a value whose top bit is uniformly random, so the
// result is very slightly biased toward the low end of the range for a luMod that does not
// divide 2^32. That is the console's generator and it is not "improved" here -- a different
// distribution is a different game.
// ================================================================================================
s32 cParticleRandomSeed::Build(s32 aiBase, s32 aiVariance)
{
    const s32 liMonitor = guBuildMonitor;          // dword_82FAB68C, loaded once
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    const s32 liMax = aiBase + aiVariance;
    CGS_ASSERT(liMax >= aiBase, "liMax >= liMin");

    const u32 luMod = static_cast<u32>(liMax - aiBase) + 1u;
    CGS_ASSERT(luMod > 0, "luMod > 0");

    // The raw LCG high word -- this overload never touches mafRandom / muIndex.
    const u64 lu64State = mu64State;
    const u32 luHigh    = static_cast<u32>(lu64State >> 32);
    mu64State = lu64State * KU64_LCG_MULTIPLIER + 1;

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);

    return static_cast<s32>(luHigh % luMod) + aiBase;
}

// ================================================================================================
// cParticleRandomSeed::Build(cVector&, const cVector&, const cVector&)  @0x8290A510   (76 instr)
//                                                       (DWARF ParticleRandomSeed.h:144)
//
// Unnamed in the idb (sub_8290A510); the DWARF names it. cParticleEmitter::ParticleBuild
// @0x82910118 is its only caller: the DO_RADIAL arm hands it splat4(-1) and splat4(2) -- both
// dynamically-initialised .bss splats read out of the image (unk_82FAB7B0 <- 0x82C4A150 <-
// flt_820037C8 == -1.0; unk_82FAC140 <- 0x82C4A178 <- flt_82001D9C == 2.0) -- so it is asking
// for a random direction with each lane uniform in [-1, 1).
//
// ⚠ IT IS NOT Build(Vector4, Vector4) @0x8290A648 WITH AN OUT PARAMETER, and treating it as one
// would desynchronise the generator. That overload takes THREE LCG steps and refills all four
// cache slots from a 96-bit stream, then toggles muIndex by 4. This one takes TWO steps
// (0x8290A588 and 0x8290A5B0), refills only THREE slots, and advances muIndex by +1, +2, +3 --
// re-reading muIndex from the record between each store (0x8290A5F4, 0x8290A608, 0x8290A618)
// rather than keeping it in a register, which is exactly what a source-level `mIndex++` per
// store compiles to.
//
// THE THREE REFILLS ARE ONE 64-BIT STREAM CUT INTO THREE 23-BIT MANTISSAS, hi(S0) || hi(S1),
// which is why the asm is bit soup. Each is OR'd into a preloaded 0x3F800000, the same
// "canonical float in [1,2)" the rest of this class uses:
//   slot+0 : insrwi r7, hi1, 21, 9   -> 0x3F800000 | ((hi1 & 0x1FFFFF) << 2)
//   slot+1 : insrwi r6, hi0, 10, 9   -> 0x3F800000 | ((hi0 & 0x3FF) << 13) | (hi1 >> 19)
//   slot+2 : inslwi r5, hi0, 23, 9   -> 0x3F800000 | (hi0 >> 9)
// ⚠ SLOT+0 IS TWO BITS SHORT: `insrwi ..., 21, 9` writes bits 9..29 and leaves bits 30..31 at
// zero, so that mantissa is 21 random bits followed by two zeros, not 23. Nothing rounds it up
// and nothing else in this class does the same -- it is the console's own bit budget for a
// 64-bit stream cut three ways (21 + 23 + 23 == 67 > 64 would not fit, and 21 + 10+13 + 23 == 67
// only closes because the middle entry is stitched), and it is reproduced rather than "fixed".
//
// ⚠ THE RESULT IS FORMED BEFORE THE REFILL, from the OLD cache contents (`lvx128 v0, r4, r31`
// at 0x8290A5B4 precedes every store), and the store through the out pointer is the LAST thing
// before the monitor stops (0x8290A624).
// ================================================================================================
void cParticleRandomSeed::Build(cVector& arOut, const cVector& arBase, const cVector& arVariance)
{
    const s32 liMonitor = guBuildMonitor;          // dword_82FAB68C, loaded once
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    // asm 0x8290A57C..0x8290A5A4 -- the same 16-byte cache half Build(Vector4,Vector4) uses.
    const u64 lu64S0  = mu64State;
    const u32 luSlot  = (muIndex + 3) & 4;
    muIndex = luSlot;

    const f32 lfT0 = mafRandom[luSlot + 0] - 1.0f;   // [1,2) -> [0,1), per lane
    const f32 lfT1 = mafRandom[luSlot + 1] - 1.0f;
    const f32 lfT2 = mafRandom[luSlot + 2] - 1.0f;
    const f32 lfT3 = mafRandom[luSlot + 3] - 1.0f;

    arOut.x = arBase.x + arVariance.x * lfT0;
    arOut.y = arBase.y + arVariance.y * lfT1;
    arOut.z = arBase.z + arVariance.z * lfT2;
    arOut.w = arBase.w + arVariance.w * lfT3;

    // Two LCG steps; their two high dwords are the 64-bit stream cut into three mantissas.
    const u64 lu64S1 = lu64S0 * KU64_LCG_MULTIPLIER + 1;
    const u64 lu64S2 = lu64S1 * KU64_LCG_MULTIPLIER + 1;
    mu64State = lu64S2;

    const u32 luHi0 = static_cast<u32>(lu64S0 >> 32);
    const u32 luHi1 = static_cast<u32>(lu64S1 >> 32);

    const u32 KU_ONE_BITS = 0x3F800000u;             // the three preloaded `lis rN, 0x3F80`
    const u32 lauBits[3] = {
        KU_ONE_BITS | ((luHi1 & 0x1FFFFFu) << 2),                    // hi1[11..31], 2 zeros below
        KU_ONE_BITS | ((luHi0 & 0x3FFu) << 13) | (luHi1 >> 19),      // hi0[22..31] ++ hi1[0..12]
        KU_ONE_BITS |  (luHi0 >> 9),                                 // hi0[0..22]
    };
    for (u32 luLane = 0; luLane < 3; ++luLane)
    {
        std::memcpy(&mafRandom[muIndex + luLane], &lauBits[luLane], sizeof(f32));
    }
    muIndex = muIndex + 3;

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}
