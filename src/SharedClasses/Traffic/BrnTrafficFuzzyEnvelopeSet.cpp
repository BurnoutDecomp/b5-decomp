// BrnTraffic::FuzzyEnvelopeSet4 -- the four trapezoidal fuzzy-membership envelopes packed one
// per SIMD lane. Construct @0x82752570 and SetEnvelope @0x82752598 are real X360 exports;
// CalcScores / CalcScore have no out-of-line body in either build (fully folded at every call
// site, and the PS3 DWARF for this .cpp lists only the other two), so they were header-inline
// originals -- recovered here from their inline expansions.

#include "SharedClasses/Traffic/BrnTrafficFuzzyEnvelopeSet.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/vector4_operation.h"           // Min / Max / Splat / GetComponent / operators

namespace BrnTraffic
{
    namespace
    {
        // rw::math::vpu::Vector4::SetComponent (DWARF BrnTrafficFuzzyEnvelopeSet.cpp:60, and the
        // console's vperm-with-a-lane-mask). The committed PC Vector4 lives in the vendor header
        // rw/math/vpu/types.h and carries no SetComponent; that header is not this file's to grow.
        void SetLane(Vector4& lrVector, s32 liLane, f32 lfValue)
        {
            switch (liLane)
            {
            case 0:  lrVector.x = lfValue; break;
            case 1:  lrVector.y = lfValue; break;
            case 2:  lrVector.z = lfValue; break;
            default: lrVector.w = lfValue; break;
            }
        }
    }

    // @0x82752570 (DWARF BrnTrafficFuzzyEnvelopeSet.cpp:39). Four Vector4::SetZero.
    void FuzzyEnvelopeSet4::Construct()
    {
        mAttackStart.SetZero();
        mDecayStop.SetZero();
        mAttackGradient.SetZero();
        mDecayGradient.SetZero();
    }

    // @0x82752598 (DWARF BrnTrafficFuzzyEnvelopeSet.cpp:60). Lane liEnvelope keeps the two ramp
    // corners the score subtracts from -- mAttackStart (0x827526EC) and mDecayStop (0x82752708),
    // note it is the STOP corner of the decay ramp -- plus the two reciprocal gradients
    // 1/(attackStop-attackStart) (0x827526C4) and 1/(decayStart-decayStop) (0x827526D4).
    // FLAG (VMX->portable): vrefp + two Newton steps (0x8275270C..0x82752740) -> exact division.
    void FuzzyEnvelopeSet4::SetEnvelope(s32 liEnvelope, VecFloat lfAttackStart, VecFloat lfAttackStop,
                                        VecFloat lfDecayStart, VecFloat lfDecayStop)
    {
        CGS_ASSERT(liEnvelope >= 0, "liEnvelope >= 0");
        CGS_ASSERT(liEnvelope < KI_MAX_ENVELOPES, "Tried to set out-of-range envelope ");

        SetLane(mAttackStart, liEnvelope, lfAttackStart.x);
        SetLane(mDecayStop,   liEnvelope, lfDecayStop.x);
        SetLane(mAttackGradient, liEnvelope, 1.0f / (lfAttackStop.x  - lfAttackStart.x));
        SetLane(mDecayGradient,  liEnvelope, 1.0f / (lfDecayStart.x - lfDecayStop.x));
    }

    // Inline expansion: UpdateVehiclesJob::ProcessSwervingRules @0x82918828..0x829188EC evaluates
    // four sets this way. Both ramps are (value - startCorner) * gradient; the decay gradient is
    // negative, so its term falls as the value passes mDecayStop.
    Vector4 FuzzyEnvelopeSet4::CalcScores(VecFloat lfValue) const
    {
        const Vector4 lAttack = (lfValue - mAttackStart) * mAttackGradient;
        const Vector4 lDecay  = (lfValue - mDecayStop)   * mDecayGradient;

        return rw::math::vpu::Clamp(rw::math::vpu::Min(lAttack, lDecay),
                                    rw::math::vpu::Splat(0.0f),
                                    rw::math::vpu::GetVector4_One());
    }

    // Inline expansion: UpdateVehiclesJob::CalcSwerveAmount @0x8291D14C..0x8291D168 -- CalcScores
    // followed by a vperm that broadcasts lane liEnvelope.
    VecFloat FuzzyEnvelopeSet4::CalcScore(s32 liEnvelope, VecFloat lfValue) const
    {
        return rw::math::vpu::Splat(
                   rw::math::vpu::GetComponent(CalcScores(lfValue), liEnvelope));
    }
}
