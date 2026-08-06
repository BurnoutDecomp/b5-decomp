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
// Two more cParticleRandomSeed methods exist in the X360 binary but belong to
// OTHER ledger TUs and are deliberately NOT bodied here yet (see the note at the
// foot of this file): FP32 Build(FP32,FP32) @0x8290A360 and
// Vector3Plus Build(Vector4,Vector4) @0x8290A648 (unnamed in the X360 idb).
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRandomSeed.h"

#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu

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

// ----------------------------------------------------------------------------
// NOT BODIED HERE (other ledger TUs -- see the wave spec for the full decoded
// recipes before implementing; landing them ANYWHERE else is an ODR trap):
//
//   FP32 cParticleRandomSeed::Build(FP32, FP32) @ 0x8290A360 -- ledger-filed
//   under TU 'GameShared/.../PerfMon/Cpu/PS3/CgsPerfMonCpuPS3.h' (a DecFIGS
//   attribution quirk; its real home is THIS file). Scalar draw: consumes
//   mafRandom[muIndex], refills that slot, muIndex=(muIndex+1)&7, then returns
//   fmadds((a+b)-a, t, a) -- source-shaped Lerp(a, a+b, t).
//
//   Vector3Plus cParticleRandomSeed::Build(Vector4, Vector4) @ 0x8290A648 --
//   unnamed sub_8290A648 in the X360 idb, DWARF-attested as this class's
//   Build(Vector4,Vector4) (ParticleRandomSeed.h:117). Per-lane draw: consumes
//   the whole 16-byte half at slot=(muIndex+3)&4, result = A + B.*t per lane
//   (vmaddfp128 @0x8290A6F4, addend = A), refills all four slots from THREE LCG
//   steps with cross-word mantissa stitching, muIndex ^= 4.
// ----------------------------------------------------------------------------
