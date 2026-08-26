#include "types.hpp"

#include <cmath>

// The declarations this TU defines (RoundFloatToAccuracy / RoundDistanceToNetworkAccuracy /
// RoundTimeToNetworkAccuracy) and CgsSystem::Time, whose fraction the time rounder rewrites
// through GetFraction/SetFraction.
#include "GameSource/Network/Utilities/BrnNetworkRounder.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8258B090
//   (BrnNetwork::NetworkRounder::RoundFloatToAccuracy)
//
// Path note: the X360 line-info attributed this function to a PPC toolchain
// header (.../ppu/include/ppu_intrinsics_gcc.h) because its body is the inlined
// fsel float-rounding idiom. DecFIGS func attribution places it in its real
// home, GameSource/Network/Utilities/BrnNetworkRounder.cpp — used here.
//
// Behaviour-faithful to the X360 pseudocode/asm. The original is the canonical
// PowerPC round-to-nearest expansion:
//   fmuls/fdivs (single)  -> step = a2*2, scaled = *result/step
//   fadds       (single)  -> biased = scaled + 0.5
//   two fsel + ±2^52      -> round-to-nearest-even then correct == floor(biased)
//   frsp/fmuls  (single)  -> *result = floor(biased) * step
// i.e. it snaps *result to the nearest multiple of (a2*2), rounding halves up.

namespace BrnNetwork
{
    namespace NetworkRounder
    {
        float* RoundFloatToAccuracy(float* result, double a2)
        {
            const float step   = static_cast<float>(a2 * 2.0);   // fmuls f13
            const float scaled = *result / step;                 // fdivs f12
            *result = scaled;                                    // stfs (intermediate, mirrors pseudocode)
            const float biased = scaled + 0.5f;                  // fadds f0

            // PPC fsel magic-constant rounding (±2^52) followed by the 0/1
            // floor-correction is exactly floor() of the biased value.
            const double rounded = std::floor(static_cast<double>(biased));

            *result = static_cast<float>(rounded) * step;        // frsp + fmuls
            return result;
        }

        // ====================================================================
        // ACCESSOR CLOSURE (2026-08-26). The two siblings BrnNetworkRounder.h has declared
        // since the ScoringSystem timer wave but never had bodies for -- both are real link
        // residue of the scoring mount (scratch/stuntrace_scout/datafeed/objs/
        // undef_demangled.txt names both). Both are full out-of-line X360 functions, so these
        // are straight transcriptions, not derivations.
        //
        // Both share RoundFloatToAccuracy's PowerPC floor idiom, which is worth stating once:
        //     fsel f13, f0, -2^52, +2^52 ; fsub f12, f0, f13 ; fadd f13, f12, f13
        //         -- the classic add-and-subtract-2^52 round-to-nearest-EVEN of f0
        //     fsub f11, f0, f13 ; fsel f0, f11, 0.0, 1.0 ; fsub f0, f13, f0
        //         -- subtract 1 when the round went UP, turning round-to-nearest into FLOOR
        // (VERIFIED, not assumed: the four dbl_ operands read out of the decrypted ARTIST
        // basefile as dbl_82001CB8 = -4503599627370496.0, dbl_82001CB0 = +4503599627370496.0
        // == +/-2^52, dbl_82001CA8 = 0.0, dbl_82001CA0 = 1.0.) The whole run happens in DOUBLE
        // precision and is narrowed by a trailing frsp, which is what std::floor(double) plus
        // the f32 cast below reproduces. Combined with the `+ 0.5` bias, the pair is
        // "round half up".
        // ====================================================================

        // --------------------------------------------------------------------
        // RoundDistanceToNetworkAccuracy  (X360 0x82591EB0)
        // Snap a distance in metres to the network's 1 cm quantum, half-up. The X360:
        //     lfs   f13, 0(r3) ; fmuls f13, f13, f0(flt_820049E0 == 100.0)
        //     stfs  f13, 0(r3)                       ; <-- intermediate store, see below
        //     fadds f0,  f13, f0(flt_82001DA0 == 0.5)
        //     <the floor idiom>
        //     frsp  f13, f0 ; fmuls f0, f13, f0(flt_82002138 == 0.0099999998) ; stfs f0, 0(r3)
        // Returns the same pointer in r3.
        //
        // The intermediate `*lpfDistance = scaled` store is REAL (0x82591EC4) and is mirrored
        // here, exactly as the committed RoundFloatToAccuracy above mirrors its own: the final
        // store overwrites it, so it is observable only to another reader of the same address,
        // but dropping it would be an unrecorded divergence from the console.
        //
        // Constants are direct big-endian f32 reads of the decrypted ARTIST basefile
        // (image VA - 0x82000000 == file offset): 100.0 / 0.5 / 0.0099999998. Note the scale
        // and the un-scale are NOT exact reciprocals in f32 (100.0 vs 0x3C23D70A), so the
        // multiply-back is written with the console's own literal rather than a division.
        // --------------------------------------------------------------------
        f32* RoundDistanceToNetworkAccuracy(f32* lpfDistance)
        {
            const f32 KF_NETWORK_DISTANCE_SCALE    = 100.0f;      // flt_820049E0
            const f32 KF_ROUND_HALF_UP_BIAS        = 0.5f;        // flt_82001DA0
            const f32 KF_NETWORK_DISTANCE_ACCURACY = 0.0099999998f; // flt_82002138 (1 cm)

            const f32 lfScaled = *lpfDistance * KF_NETWORK_DISTANCE_SCALE;   // fmuls
            *lpfDistance = lfScaled;                                         // stfs (intermediate)

            const f32 lfBiased = lfScaled + KF_ROUND_HALF_UP_BIAS;           // fadds
            const f64 lfFloor  = std::floor(static_cast<f64>(lfBiased));     // the fsel floor idiom

            *lpfDistance = static_cast<f32>(lfFloor) * KF_NETWORK_DISTANCE_ACCURACY;  // frsp + fmuls
            return lpfDistance;
        }

        // --------------------------------------------------------------------
        // RoundTimeToNetworkAccuracy  (X360 0x82591F20)
        // Snap a CgsSystem::Time's sub-second FRACTION to the network's 1/300 s quantum,
        // half-up, and write it back. The seconds word is untouched. The X360:
        //     lfs    f12, 4(r3)                      ; Time::mfFraction @ +4
        //     fmadds f0, f12, f0(flt_8201A258 == 300.0), f13(flt_82001DA0 == 0.5)
        //     <the floor idiom>
        //     frsp   f13, f0 ; fmuls f1, f13, f0(flt_8208B4C4 == 0.0033333334)
        //     fcmpu  cr6, f1, f0(flt_82001C98 == 1.0) ; blt -> skip
        //     fsubs  f1, f1, f0(flt_82087038 == 0.0016666667)
        //     b      CgsSystem::Time::SetFraction    ; tail call
        //
        // THE `>= 1.0` ARM IS NOT A CLAMP, it is a legality fix. Rounding a fraction of e.g.
        // 0.999 half-up to the 1/300 grid produces exactly 1.0, which is OUT OF RANGE for a
        // Time fraction -- CgsSystem::Time::SetFraction asserts `(f >= 0) && (f < 1)`. The
        // console backs off by HALF a quantum (1/600 == 0.0016666667) to land at 0.998333,
        // the largest representable in-range value on that grid. Written as the console's own
        // subtract, NOT as a clamp to nextafter(1.0), so the value matches bit for bit.
        //
        // All five constants are direct big-endian f32 reads of the decrypted ARTIST basefile:
        // 300.0 / 0.5 / 0.0033333334 / 1.0 / 0.0016666667. DWARF gives the return as void; the
        // X360 tail-calls SetFraction and so returns whatever that leaves in r3, which no
        // caller reads.
        // --------------------------------------------------------------------
        void RoundTimeToNetworkAccuracy(CgsSystem::Time* lpTime)
        {
            const f32 KF_NETWORK_TIME_SCALE     = 300.0f;         // flt_8201A258
            const f32 KF_ROUND_HALF_UP_BIAS     = 0.5f;           // flt_82001DA0
            const f32 KF_NETWORK_TIME_ACCURACY  = 0.0033333334f;  // flt_8208B4C4 (1/300 s)
            const f32 KF_FRACTION_LIMIT         = 1.0f;           // flt_82001C98
            const f32 KF_NETWORK_TIME_HALF_STEP = 0.0016666667f;  // flt_82087038 (1/600 s)

            const f32 lfBiased =
                lpTime->GetFraction() * KF_NETWORK_TIME_SCALE + KF_ROUND_HALF_UP_BIAS;  // fmadds
            const f64 lfFloor = std::floor(static_cast<f64>(lfBiased));                 // fsel floor

            f32 lfFraction = static_cast<f32>(lfFloor) * KF_NETWORK_TIME_ACCURACY;      // frsp + fmuls

            if (lfFraction >= KF_FRACTION_LIMIT)
            {
                // Rounded up onto (or past) a whole second -- back off half a quantum so the
                // value stays a legal Time fraction.
                lfFraction -= KF_NETWORK_TIME_HALF_STEP;
            }

            lpTime->SetFraction(lfFraction);
        }
    }
}
