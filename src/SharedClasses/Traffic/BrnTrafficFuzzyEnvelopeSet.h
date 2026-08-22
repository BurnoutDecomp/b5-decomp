#pragma once

// BrnTraffic::FuzzyEnvelopeSet4 -- four trapezoidal fuzzy-membership "envelopes" packed
// into one set of SIMD registers (one lane per envelope). Each envelope is defined by an
// attack ramp (AttackStart -> AttackStop) and a decay ramp (DecayStart -> DecayStop); the
// set stores the start corners and the precomputed (reciprocal) gradients so a score can be
// evaluated with two subtract/multiply/min pairs per lane.
//
// Declaration shape from the DecFIGS DWARF (SharedClasses/Traffic/BrnTrafficFuzzyEnvelopeSet.h),
// gated on the X360 ARTIST ledger (Construct @0x82752570 / SetEnvelope @0x82752598 are attested).
// Bodies live in SharedClasses/Traffic/BrnTrafficFuzzyEnvelopeSet.cpp (landed 2026-08-22).

#include "types.hpp"          // s32
#include "BrnCommonTypes.h"   // Vector4, VecFloat (== rw::math::vpu::Vector4)

namespace BrnTraffic
{
    // KI_MAX_ENVELOPES (DWARF :95) -- four envelopes per set, one per SIMD lane.
    static const s32 KI_MAX_ENVELOPES = 4;

    class FuzzyEnvelopeSet4
    {
    public:
        // Zero all four envelope corners/gradients.
        void Construct();

        // Define envelope liEnvelope (0..3). Stores mAttackStart/mDecayStop in that lane and
        // precomputes mAttackGradient = 1/(attackStop-attackStart),
        // mDecayGradient = 1/(decayStart-decayStop) (negative for a falling ramp).
        void SetEnvelope(s32 liEnvelope, VecFloat lfAttackStart, VecFloat lfAttackStop,
                         VecFloat lfDecayStart, VecFloat lfDecayStop);

        // Evaluate all four envelopes against the broadcast scalar lfValue, returning the four
        // clamped [0,1] membership scores (one per lane):
        //   clamp01( min( (x-AttackStart)*AttackGradient, (x-DecayStop)*DecayGradient ) ).
        Vector4 CalcScores(VecFloat lfValue) const;

        // Evaluate a single envelope (liEnvelope).
        VecFloat CalcScore(s32 liEnvelope, VecFloat lfValue) const;

    private:
        Vector4 mAttackStart;     // 0x00  per-lane attack-ramp start corner
        Vector4 mDecayStop;       // 0x10  per-lane decay-ramp stop corner
        Vector4 mAttackGradient;  // 0x20  1/(attackStop-attackStart)
        Vector4 mDecayGradient;   // 0x30  1/(decayStart-decayStop)
    };
}
