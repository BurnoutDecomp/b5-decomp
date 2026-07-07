// ============================================================================
// ParticleRandomSeed.cpp -- cParticleRandomSeed runtime bodies.
//
// Bodied from the X360 asm: Init @0x82911BE0, Offset @0x8290E8B0,
// Update @0x8290EAB0. All three share one inlined refill loop (RefillCache).
//
// The generator is a 64-bit LCG using the MMIX / Knuth multiplier
// 6364136223846793005 (0x5851F42D4C957F2D, built on X360 from the two halves
// 0x4C957F2D and 0x5851F42D via insrdi) with increment 1. Each cache entry is a
// canonical float in [1.0, 2.0): the top 23 bits of the state's high dword are
// stuffed into the mantissa of an IEEE-754 float whose sign/exponent bits are
// 0x3F800000 (the asm does this with `inslwi rD,rHigh,23,9` over a preloaded
// 0x3F800000, i.e. bits = 0x3F800000 | ((state>>32) >> 9)).
//
// cParticleRandomSeed::BuildLerp @0x8290A7A8 is NOT reconstructed here -- see the
// note at the foot of this file for the precise blocking reason.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRandomSeed.h"

#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu

#include <cstring>   // memcpy

namespace
{
    // MMIX / Knuth 64-bit LCG multiplier (0x5851F42D4C957F2D). On X360 it is
    // assembled from lis/ori 0x4C957F2D (low) + lis/ori 0x5851F42D (high) folded
    // in with insrdi r11,r9,32,0 -> the 0x5851F42D half lands in the HIGH dword.
    const u64 KU64_LCG_MULTIPLIER = 0x5851F42D4C957F2DULL;

    // Park-Miller-style step applied to the 32-bit seed in Update (ori of lis 1
    // with 0xFF5 -> 0x10FF5 == 69621; mullw seed,seed,69621).
    const u32 KU_SEED_STEP = 69621u;

    // Init's shared reseed counter (X360 dword_82F350FC): bumped by 42 on every
    // Init and OR'd with 1 to keep the seed odd. Initialised elsewhere at boot;
    // modelled as a zero-initialised global here (its start value is not encoded
    // in these functions' asm).
    u32 guInitSeedCounter = 0u;

    // CPU perf-monitor handle for Update (X360 dword_82FAB690); -1 == unregistered.
    s32 guUpdateMonitor = -1;

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

// Seed the 64-bit LCG with muSeed in the high dword and fill all 8 cache slots.
// The X360 emits this as a fully unrolled 8x sequence inside each caller; it is
// re-rolled here into the loop it started as. Each iteration writes the current
// slot from the live state, advances the LCG one step, and bumps the wrapping
// index -- leaving muIndex back at 0 after eight entries.
void cParticleRandomSeed::RefillCache()
{
    muIndex   = 0;
    mu64State = static_cast<u64>(muSeed) << 32;

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
    guInitSeedCounter += 42;              // dword_82F350FC += 42
    muSeed = guInitSeedCounter | 1u;      // force odd
    RefillCache();
}

// cParticleRandomSeed::Offset @ 0x8290E8B0
void cParticleRandomSeed::Offset(u32 auOffset)
{
    muSeed = auOffset + muSeed;
    RefillCache();
}

// cParticleRandomSeed::Update @ 0x8290EAB0
void cParticleRandomSeed::Update()
{
    const s32 liMonitor = guUpdateMonitor;         // dword_82FAB690, loaded once
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    muSeed = muSeed * KU_SEED_STEP;                // seed *= 69621
    RefillCache();

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}

// ----------------------------------------------------------------------------
// BLOCKED: cParticleRandomSeed::BuildLerp @ 0x8290A7A8
//
// Not reconstructed. It is a VMX128 float-vector routine (vmaddfp128 / lvsl /
// vperm / vsubfp) that draws one cached random, refills that lane, and blends two
// vector arguments. Three independent pieces cannot be grounded without guessing,
// and a wrong blend is worse than an honest gap:
//   1. The vmaddfp128 multiplier/addend assignment: the disassembly prints
//      `vmaddfp128 v127,v126,v0,v127`, which under the project's "multiplier is the
//      last operand" rule yields v127 = v126*v127 + v0 -- i.e. the random t (v0) as
//      an ADDEND, not the interpolation weight -- contradicting the "Lerp" name
//      (which wants t as the multiplier). The operand direction is unresolved.
//   2. The lane selected by `lvsl(0)` + `vspltw v7,v0,0` + `vperm v0,v0,v0,v7` off
//      the 16-byte-aligned half of mafRandom is not determinable with confidence.
//   3. The calling shape: `this` (r3) is the output vector and the seed object is
//      passed in r4 (a2) -- so the two vector arguments' types (cVector / rw VMX
//      value type) are un-homed and the member-vs-static shape is ambiguous.
// ----------------------------------------------------------------------------
