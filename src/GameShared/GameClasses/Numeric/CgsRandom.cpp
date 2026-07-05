// ===========================================================================
// CgsNumeric::Random -- the out-of-line draw/seed methods.
//   class:CgsNumeric::Random
//
// The X360 build INLINES this LCG everywhere (no out-of-line bodies exist in
// the ledger); the semantics are attested at the inline sites:
//   * the LCG step: muSeed = muSeed * 0x5851F42D4C957F2D + 1, draw = hi 32 bits
//     (Vehicle::SetFlashingHeadlights @0x827537D0..E4: "return muSeed >> 32,
//     then muSeed = muSeed * 0x5851F42D4C957F2D + 1"; the same constant pair
//     1284865837 / 0x5851F42D in every inlined site).
//   * SetSeed (SelectionHistory<512,u16,u16,65536>::Randomize @0x826C5900 head):
//     the 32-bit seed word is OR'd under the multiplier's HIGH half
//     (seed | 0x5851F42D00000000), one LCG prime step runs, and the float-ring
//     oldest index resets to 0 (`*(a1+48) = (seed|K_hi)*K + 1; *(a1+56) = 0`).
//
// Homed 2026-07-05 because wave49's SelectionHistory::Randomize (CgsSoundUtils)
// calls SetSeed/RandomUInt(min,max) out-of-line, which broke the exe link (the
// methods were declared-only). This TU is their canonical home when the full
// CgsRandom ledger work lands; the bodies below are the attested LCG.
//
// FLAG (bounded draw): RandomUInt(min,max) has no exported out-of-line X360
// body (always inlined + strength-reduced, e.g. the %3 mulhwu idiom in
// SetFlashingHeadlights). The canonical modulo reduction over the raw draw is
// reconstructed by that idiom's intent: min + draw % (max - min). Its one
// caller today (Randomize's Fisher-Yates, itself CONFIDENCE-low) passes
// (0, 512).
// ===========================================================================

#include "GameShared/GameClasses/Numeric/CgsRandom.h"

namespace CgsNumeric
{
    // The shared LCG multiplier (every inlined site: hi 0x5851F42D, lo 0x4C957F2D).
    static const u64 KU_RANDOM_LCG_MULTIPLIER = 0x5851F42D4C957F2Dull;

    // Inline-site semantics (@0x826C5900 head): fold the seed under the multiplier's
    // high half, run one prime step, reset the float-ring cursor.
    void Random::SetSeed(u64 lu64Seed)
    {
        muSeed = (lu64Seed | 0x5851F42D00000000ull);
        muSeed = muSeed * KU_RANDOM_LCG_MULTIPLIER + 1u;
        muOldestBufferIndex = 0;
    }

    // Inline-site semantics (@0x827537D0..E4): draw the high word, then step.
    u32 Random::RandomUInt()
    {
        const u32 luDraw = static_cast<u32>(muSeed >> 32);
        muSeed = muSeed * KU_RANDOM_LCG_MULTIPLIER + 1u;
        return luDraw;
    }

    // FLAG: reconstructed by the inline sites' reduction intent (see the header
    // note) -- min + draw % span; a zero span returns min.
    u32 Random::RandomUInt(u32 luMin, u32 luMax)
    {
        const u32 luSpan = luMax - luMin;
        if (luSpan == 0u)
            return luMin;
        return luMin + (RandomUInt() % luSpan);
    }
}
