#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRandomSeed.h
//
// cParticleRandomSeed -- the Lion (eauk_lion) particle runtime's tiny per-effect
// random number generator. It keeps a 64-bit LCG state (the classic MMIX / Knuth
// multiplier 6364136223846793005) and precomputes a small ring of 8 canonical
// floats in the range [1.0, 2.0) that the particle simulation samples cheaply.
// Init / Offset / Update each reseed the generator (from a global counter, an
// explicit offset, or a Park-Miller step of the current seed respectively) and
// then refill the whole 8-entry cache.
//
// LAYOUT AUTHORITY: the member offsets are pinned by the X360 asm for the three
// bodied methods (Init @0x82911BE0, Offset @0x8290E8B0, Update @0x8290EAB0):
//
//   mafRandom[i]  @ 0x00 + 4*i   (stwx r,index*4,this ; stw r,0(this))
//   mu64State     @ 0x20         (ld / std -- 64-bit LCG state)
//   muIndex       @ 0x28         (lwz / stw ; kept in 0..7 by clrlwi ..,29)
//   muSeed        @ 0x30         (lwz / stw ; the 32-bit reseed value)
//
// The struct's total size (0x34 == 52 bytes) is corroborated independently by
// cParticleBucket, whose DWARF places mRandomSeed at bucket+0x1C and the next
// member (mnNextParticlePositionToFill) at bucket+0x50 -- a gap of exactly 0x34.
// (On the 64-bit host the u64 forces 8-byte struct alignment, so sizeof rounds up
// to 56; the load-bearing facts are the RELATIVE member offsets above, pinned by
// the _AssertLayout static_asserts, not the padded tail.)
// ============================================================================

#include "types.hpp"

#include <cstddef>   // offsetof

struct cParticleRandomSeed
{
    // ParticleRandomSeed.h -- reseed from the shared global counter (+= 42, then
    // force odd) and refill the cache. X360 Init @ 0x82911BE0.
    void Init();

    // Reseed by adding an explicit offset to the current seed, then refill.
    // X360 Offset @ 0x8290E8B0.
    void Offset(u32 auOffset);

    // Advance the seed one Park-Miller step (seed *= 69621), then refill. Bracketed
    // by the "Lion particle random update" CPU perf monitor. X360 Update @ 0x8290EAB0.
    void Update();

private:
    // Rebuild the 8-entry [1,2) float cache from the current muSeed: seed the 64-bit
    // LCG with muSeed in the high dword, then step it once per entry. Shared body the
    // X360 compiler inlined into Init / Offset / Update.
    void RefillCache();

    // ----- members (offsets pinned against the X360 asm) --------------------
    f32 mafRandom[8];   // 0x00  cache of 8 canonical floats in [1.0, 2.0)
    u64 mu64State;      // 0x20  64-bit LCG state
    u32 muIndex;        // 0x28  next cache slot to fill (wraps 0..7)
    u32 muReserved2C;   // 0x2C  not touched by the RNG methods (padding to 0x30)
    u32 muSeed;         // 0x30  32-bit reseed value

    // Byte-layout pin: the relative member offsets are ground truth from the asm.
    static void _AssertLayout()
    {
        static_assert(offsetof(cParticleRandomSeed, mafRandom) == 0x00, "mafRandom@0");
        static_assert(offsetof(cParticleRandomSeed, mu64State) == 0x20, "mu64State@0x20");
        static_assert(offsetof(cParticleRandomSeed, muIndex)   == 0x28, "muIndex@0x28");
        static_assert(offsetof(cParticleRandomSeed, muSeed)    == 0x30, "muSeed@0x30");
    }
};
