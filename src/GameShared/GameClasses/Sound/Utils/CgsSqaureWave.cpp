// ============================================================================
// CgsSqaureWave.cpp -- CgsSound::Utils::SqaureWave home.
//
// Bodied store-for-store from BURNOUT_X360_ARTIST.XEX:
//   CgsSound::Utils::SqaureWave::Update  @ 0x82691598
//
// (The class name carries the engine's original spelling "SqaureWave"; kept as-is
// because it is the mangled symbol name in the X360 build.)
//
// SqaureWave is a randomised square-wave LFO: it toggles between a "low" and a
// "high" output level, and each time it flips it schedules the next flip a random
// interval into the future. The random interval is drawn from a per-instance noise
// pool of 8 f32 samples that is refilled in place by a 64-bit linear-congruential
// generator (LCG) -- the classic Knuth/PCG multiplier 0x5851F42D4C957F2D.
//
// Update(result, deltaTime):
//   * counts the active phase's timer down by deltaTime;
//   * while the timer is still >= 0, emits the current level (held);
//   * when the timer goes negative, flips the level, draws a fresh random
//     normalised fraction r in [0,1) from the noise pool, and reschedules the
//     timer to lerp(phaseMin, phaseMax, r) for the new phase.
//
// `result` is a 2-byte output: result[0] = the new "primary" level bit,
// result[1] = the complementary bit (the X360 writes both bytes).
//
// X360 state layout (r4 == this; byte offsets proven by the asm loads/stores):
//   +0x00 f32 mfLowMin        \  low-phase interval range [min,max]
//   +0x04 f32 mfLowMax        /
//   +0x08 f32 mfHighMin       \  high-phase interval range [min,max]
//   +0x0C f32 mfHighMax       /
//   +0x10 f32 mfTimer          countdown to the next flip
//   +0x14 u8  mbHigh           current phase flag (0 = low, 1 = high)
//   +0x20 f32 maNoisePool[8]   the 8-sample noise pool (base r11 = this+0x20)
//   +0x40 u64 mu64RngState     LCG state            (ld/std 0x20(r11))
//   +0x48 u32 muNoiseIndex     pool write/read index (0..7), &7-wrapped
//
// Constants from rodata (confirmed by the pseudocode, no source available):
//   flt_82001CC0 == 0.0f  (the `timer >= 0` compare)
//   flt_82001C98 == 1.0f  (the `sample - 1.0` that normalises [1,2) -> [0,1))
//
// FLAG (faithful, not byte-identical): the X360 builds the random sample by ORing
// the LCG's high-23 mantissa bits onto the bit pattern 0x3F800000, yielding a float
// in [1.0, 2.0); it then subtracts 1.0 to get a fraction in [0.0, 1.0). That exact
// bit-twiddle is reproduced below via a u32 bit assembly so the produced fraction
// is identical store-for-store (not an approximation). The noise pool is refilled
// in place: each call reads the OLD sample at the current index, then overwrites
// that slot with the freshly-built sample, then advances the index -- matching the
// lfsx-before-stwx ordering in the asm.
// ============================================================================

#include "types.hpp"

#include <cstring> // std::memcpy (bit-exact reinterpret of the LCG mantissa bits)

namespace CgsSound
{
namespace Utils
{

// Reconstructed SqaureWave state. Member access is BY NAME; absolute offsets are
// not static_asserted. The class has no DWARF/leak home; the layout above is
// recovered solely from the Update asm at 0x82691598.
struct SqaureWave
{
    f32 mfLowMin;        // +0x00
    f32 mfLowMax;        // +0x04
    f32 mfHighMin;       // +0x08
    f32 mfHighMax;       // +0x0C
    f32 mfTimer;         // +0x10
    u8  mbHigh;          // +0x14
    // padding to +0x20 in the X360 layout (between the flag and the pool); not
    // touched by Update, modelled implicitly so the pool/RNG members keep their
    // recovered relative order.
    u8  maPad[11];       // +0x15 .. +0x1F (opaque; untouched by recovered code)
    f32 maNoisePool[8];  // +0x20 .. +0x3F
    u64 mu64RngState;    // +0x40
    u32 muNoiseIndex;    // +0x48

    // Output struct the X360 writes through r3 (result): two level bytes.
    struct Output
    {
        u8 mu8Primary;     // result[0]
        u8 mu8Complement;  // result[1]
    };

    Output* Update(Output* apResult, f64 afDeltaTime);
};

// ---------------------------------------------------------------------------
// SqaureWave::Update  @ 0x82691598
// ---------------------------------------------------------------------------
SqaureWave::Output* SqaureWave::Update(Output* apResult, f64 afDeltaTime)
{
    // v3 = mfTimer - deltaTime ; mfTimer = v3  (lfs/fsubs/stfs 0x10)
    const f64 lfNewTimer = static_cast<f64>(mfTimer) - afDeltaTime;
    mfTimer = static_cast<f32>(lfNewTimer);

    if (lfNewTimer >= 0.0) // fcmpu vs flt_82001CC0(0.0) ; bge -> loc_826916C8
    {
        // Timer not yet elapsed: hold the current level on both output bytes.
        const u8 lu8Level = mbHigh;       // lbz 0x14
        apResult->mu8Primary    = lu8Level; // stb 0(r3)
        apResult->mu8Complement = lu8Level; // stb 1(r3)
        return apResult;
    }

    // Timer elapsed: flip the phase and pick the new interval range.
    // The X360 `beq` at 0x826915DC branches on (mbHigh == 0):
    //   * fall-through (mbHigh != 0) @ 0x826915E0: reads mfHighMin/mfHighMax (8/0xC),
    //     sets mbHigh=0, primary=0, complement=1.
    //   * beq-taken (mbHigh == 0)   @ 0x82691654: reads mfLowMin/mfLowMax (0/4),
    //     sets mbHigh=1, primary=1, complement=0.
    // Reproduced store-for-store below.
    f64 lfPhaseMin;
    f64 lfPhaseSpan;
    if (mbHigh != 0)
    {
        lfPhaseMin  = static_cast<f64>(mfHighMin);                       // lfs 8(r4)
        lfPhaseSpan = static_cast<f64>(mfHighMax) - static_cast<f64>(mfHighMin); // 0xC - 8
        mbHigh = 0;                       // stb 0, 0x14
        apResult->mu8Primary    = 0;      // stb 0, 0(r3)
        apResult->mu8Complement = 1;      // stb 1, 1(r3)
    }
    else
    {
        lfPhaseMin  = static_cast<f64>(mfLowMin);                        // lfs 0(r4)
        lfPhaseSpan = static_cast<f64>(mfLowMax) - static_cast<f64>(mfLowMin); // 4 - 0
        mbHigh = 1;                       // stb 1, 0x14
        apResult->mu8Primary    = 1;      // stb 1, 0(r3)
        apResult->mu8Complement = 0;      // stb 0, 1(r3)
    }

    // --- draw a fresh random fraction from the in-place noise pool ---------
    // LCG: state' = state * 0x5851F42D4C957F2D + 1   (mulld/addi; the multiplier
    // is assembled in the asm as high 0x4C957F2D, low 0x5851F42D via insrdi).
    const u64 lu64Old = mu64RngState;                 // ld 0x20(r11)
    const u32 luIndex = muNoiseIndex;                 // lwz 0x28(r11)

    const u64 KU_LCG_MULT = 0x5851F42D4C957F2DULL;
    mu64RngState = lu64Old * KU_LCG_MULT + 1ULL;      // mulld + addi ; std 0x20(r11)

    // Read the OLD pool sample at the current index (lfsx before the overwrite).
    const f32 lfOldSample = maNoisePool[luIndex];

    // Build the new sample: 0x3F800000 with the high-23 bits of the OLD state's
    // high32 inserted at bit 9 (inslwi r7, r10, 23, 9 ; r10 = lu64Old >> 32).
    const u32 luHigh32   = static_cast<u32>(lu64Old >> 32);
    const u32 luMantissa = luHigh32 >> 9;             // top 23 bits -> mantissa
    const u32 luBits     = 0x3F800000u | luMantissa;  // float in [1.0, 2.0)
    f32 lfNewSample;
    std::memcpy(&lfNewSample, &luBits, sizeof(lfNewSample));
    maNoisePool[luIndex] = lfNewSample;               // stwx -> store the new sample

    // Advance the index, wrapping at 8 (clrlwi ...,29 == &7).
    muNoiseIndex = (luIndex + 1) & 7;                 // addi + clrlwi ; stw 0x28(r11)

    // Reschedule: timer = phaseSpan * (oldSample - 1.0) + phaseMin
    // (fsubs sample-1.0 ; fmadds span*frac + min ; stfs 0x10).
    const f64 lfFraction = static_cast<f64>(lfOldSample) - 1.0; // flt_82001C98 == 1.0
    mfTimer = static_cast<f32>(lfPhaseSpan * lfFraction + lfPhaseMin);

    return apResult;
}

} // namespace Utils
} // namespace CgsSound
