// BrnTraffic::Fuzzy::FuzzyBehaviourLogic -- traffic AI fuzzy-inference engine.
//
// Reconstructed from the X360 ARTIST build (pseudocode + PPC/VMX assembly) as the
// authority for behaviour & calling convention, with declaration shape from the DecFIGS
// DWARF. Member offsets verified against Construct()'s zero-fill (0x10..0x2B0),
// SetupEnvelope()'s category->offset switch, and SetConstantValue()'s 21-case jump table.
//
// The envelope membership scoring (FuzzyEnvelopeSet4::CalcScores) and the fuzzy
// combinators (FuzzyAND/OR/NOT) are folded inline by the X360 compiler; they are
// un-inlined here back into the method/free-function calls the original source wrote.

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficFuzzyLogicBehaviours.h"

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTweakConstants.h" // TweakValues
#include "SharedClasses/Traffic/BrnTrafficFuzzyLogic.h"   // FuzzyAND / FuzzyOR / FuzzyNOT
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "rw/math/vpu/vector4_operation.h"                // Vector4 +/-/*, Min/Max/Clamp/Select/And

namespace BrnTraffic
{
namespace Fuzzy
{
    using rw::math::vpu::Vector4;
    using rw::math::vpu::MaskScalar;
    using rw::math::vpu::Splat;   // broadcast a scalar corner into a VecFloat

    // ---------------------------------------------------------------------------------
    // Construct: zero every envelope set + the cached normal/swerve score vector, latch
    // the tweak-values pointer, run ResetToDefaults to seed the default envelopes, then
    // cache (NormalScore=0.2, ExtremeSwerveStickiness, 0, 0) into mNormalScore.
    // ---------------------------------------------------------------------------------
    void FuzzyBehaviourLogic::Construct(TweakValues* lpTweakValues)
    {
        mbDEBUGRenderParamScores = false;
        mDEBUGLastCameraPos.SetZero();   // (folded into the same zero-fill in the asm)
        mDEBUGCurrentParamPos.SetZero();

        mDistanceEnvelopes.Construct();
        mHeightEnvelopes.Construct();
        mClosingSpeedEnvelopes.Construct();
        mLanePosEnvelopes.Construct();
        mAbsoluteSpeedEnvelopes.Construct();
        mTrafficLightEnvelopes.Construct();
        mNextParamDistEnvelopes.Construct();
        mTimeQueueingEnvelopes.Construct();
        mSwerveDistanceEnvelopes.Construct();
        mSwerveAngleEnvelopes.Construct();

        CGS_ASSERT(lpTweakValues != nullptr, "lpTweakValues");
        mpTweakValues = lpTweakValues;

        ResetToDefaults();

        // mNormalScore lanes: x = base "normal" score (0.2), y = the extreme-swerve
        // stickiness pulled from the tweak block, z = w = 0 (the asm reads +0x2C of
        // mpTweakValues == mfExtremeSwerveStickiness into lane y).
        mNormalScore_ExtremeSwerveStickiness_Z_W =
            Vector4{ 0.2f, mpTweakValues->GetExtremeSwerveStickiness(), 0.0f, 0.0f };
    }

    // ---------------------------------------------------------------------------------
    // SetupEnvelope: route (liCategory 0..9) to the matching FuzzyEnvelopeSet4 member and
    // forward the four corner values. Category index follows declaration order; categories
    // 8/9 are the swerve sets (mNormalScore at 0x230 is skipped, hence the +0x10 gap).
    // ---------------------------------------------------------------------------------
    void FuzzyBehaviourLogic::SetupEnvelope(s32 liCategory, s32 liEnvelope,
                                            f32 lfAttackStart, f32 lfAttackStop,
                                            f32 lfDecayStart, f32 lfDecayStop)
    {
        FuzzyEnvelopeSet4* lpSet = nullptr;
        switch (liCategory)
        {
            case 0: lpSet = &mDistanceEnvelopes;      break;   // this+0x30
            case 1: lpSet = &mHeightEnvelopes;        break;   // this+0x70
            case 2: lpSet = &mClosingSpeedEnvelopes;  break;   // this+0xB0
            case 3: lpSet = &mLanePosEnvelopes;       break;   // this+0xF0
            case 4: lpSet = &mAbsoluteSpeedEnvelopes; break;   // this+0x130
            case 5: lpSet = &mTrafficLightEnvelopes;  break;   // this+0x170
            case 6: lpSet = &mNextParamDistEnvelopes; break;   // this+0x1B0
            case 7: lpSet = &mTimeQueueingEnvelopes;  break;   // this+0x1F0
            case 8: lpSet = &mSwerveDistanceEnvelopes;break;   // this+0x240
            case 9: lpSet = &mSwerveAngleEnvelopes;   break;   // this+0x280
            default:
                CGS_ASSERT(false, "Unknown category");
                return;
        }
        // The corners are scalar floats here but the envelope set works in broadcast
        // VecFloat lanes (the asm vspltw's each scalar before the SetEnvelope call).
        lpSet->SetEnvelope(liEnvelope, Splat(lfAttackStart), Splat(lfAttackStop),
                           Splat(lfDecayStart), Splat(lfDecayStop));
    }

    // ---------------------------------------------------------------------------------
    // SetConstantValue: write one of the 21 "mega-tweek" constants. The X360 jump table
    // stores to mpTweakValues +0x00..+0x50 (index*4); modelled as the index-addressed
    // write on TweakValues, with the same out-of-range assert as the default case.
    // ---------------------------------------------------------------------------------
    void FuzzyBehaviourLogic::SetConstantValue(s32 liValueIndex, f32 lfValue)
    {
        CGS_ASSERT(mpTweakValues != nullptr, "mpTweakValues");
        if (!mpTweakValues->SetByIndex(liValueIndex, lfValue))
        {
            CGS_ASSERT(false, "Unknown mega-tweek value");
        }
    }

    // ---------------------------------------------------------------------------------
    // ResetToDefaults: seed every envelope + every mega-tweek constant with the hard-coded
    // defaults baked into the build. Envelope corner values and constant values are read
    // verbatim from the ARTIST data (resolved float literals).
    // ---------------------------------------------------------------------------------
    void FuzzyBehaviourLogic::ResetToDefaults()
    {
        // -- Distance envelopes (category 0, via direct FuzzyEnvelopeSet4::SetEnvelope) --
        // SetEnvelope(envelope, attackStart, attackStop, decayStart, decayStop); the corners
        // are broadcast into VecFloat lanes (asm vspltw's each scalar before the call).
        mDistanceEnvelopes.SetEnvelope(0, Splat(-3.5f), Splat(-1.5f), Splat(7.0f),  Splat(11.0f));
        mDistanceEnvelopes.SetEnvelope(1, Splat( 6.0f), Splat(12.0f), Splat(14.0f), Splat(16.0f));
        mDistanceEnvelopes.SetEnvelope(2, Splat(15.0f), Splat(18.0f), Splat(20.0f), Splat(30.0f));

        // -- Height envelopes (category 1) --
        mHeightEnvelopes.SetEnvelope(0, Splat(-2.0f), Splat(-1.0f), Splat(3.0f), Splat(5.0f));

        // -- ClosingSpeed envelopes (category 2) --
        mClosingSpeedEnvelopes.SetEnvelope(0, Splat(-1000.1f), Splat(-1000.0f), Splat(-4.0f),   Splat(-3.0f));
        mClosingSpeedEnvelopes.SetEnvelope(1, Splat(-3.5f),    Splat(-2.0f),    Splat(-0.5f),   Splat(0.5f));
        mClosingSpeedEnvelopes.SetEnvelope(2, Splat(0.0f),     Splat(3.0f),     Splat(1000.0f), Splat(1000.1f));
        mClosingSpeedEnvelopes.SetEnvelope(3, Splat(10.0f),    Splat(20.0f),    Splat(75.0f),   Splat(95.0f));

        // -- The remaining categories go through SetupEnvelope(category, envelope, ...) --
        SetupEnvelope(3, 0, -2.5f, -1.5f, 1.5f, 2.5f);          // LanePos
        SetupEnvelope(3, 1, -8.5f, -7.0f, 7.0f, 8.5f);
        SetupEnvelope(3, 2, -3.5f, -2.0f, 2.0f, 3.5f);

        SetupEnvelope(4, 0, -0.5f, 1.0f, 1000.0f, 1000.1f);     // AbsoluteSpeed
        SetupEnvelope(4, 1, -1000.1f, -1000.0f, -0.5f, 0.5f);

        SetupEnvelope(5, 0, -0.1f, 0.0099999998f, 3.0f, 30.0f); // TrafficLight

        SetupEnvelope(6, 0, -0.1f, 0.0f, 15.0f, 35.0f);         // NextParamDist

        SetupEnvelope(8, 0, -25.0f, -15.0f, 80.0f, 110.0f);     // SwerveDistance
        SetupEnvelope(8, 1, -3.0f, 0.0f, 50.0f, 65.0f);

        SetupEnvelope(9, 0, -10.0f, -1.0f, -0.96499997f, -0.90600002f); // SwerveAngle

        SetupEnvelope(7, 0, 2.0f, 8.0f, 1000.0f, 1000.1f);      // TimeQueueing

        // -- Mega-tweek constants (indices 0..20). The asm seeds index 0 and 1 via a direct
        // mpTweakValues store (mfStoplineVariation=1.0, mfRaceCarStopDist=10.0); the rest
        // through SetConstantValue. Reproduce all 21 via the same path for clarity. --
        CGS_ASSERT(mpTweakValues != nullptr, "mpTweakValues");
        mpTweakValues->SetByIndex(0, 1.0f);     // mfStoplineVariation
        mpTweakValues->SetByIndex(1, 10.0f);    // mfRaceCarStopDist
        SetConstantValue(2,  0.64999998f);      // mfGapClosingFactor
        SetConstantValue(3, -8.0f);             // mfMinNormalAcceleration
        SetConstantValue(4,  8.0f);             // mfMaxNormalAcceleration
        SetConstantValue(5, -20.0f);            // mfMinAcceleration
        SetConstantValue(6,  8.0f);             // mfMaxAcceleration
        SetConstantValue(7,  0.050000001f);     // mfMinSpeedForCutoff
        SetConstantValue(8,  0.050000001f);     // mfMinStopDist
        SetConstantValue(9,  4.0f);             // mfSwerveScoreScale
        SetConstantValue(10, 1.0f);             // mfExtremeSwerveScoreScale
        SetConstantValue(11, 0.30000001f);      // mfExtremeSwerveStickiness
        SetConstantValue(12, 1.0f);             // mfExtremeSwerveMinTime
        SetConstantValue(13, 0.0049999999f);    // mfSpinAirRamMagMin
        SetConstantValue(14, 0.02f);            // mfSpinAirRamMagMax
        SetConstantValue(15, 0.15000001f);      // mfSpinAirRamDecay
        SetConstantValue(16, 10.0f);            // mfSpinAirRamZDist
        SetConstantValue(17, 0.025f);           // mfRollAirRamMagMin
        SetConstantValue(18, 0.039999999f);     // mfRollAirRamMagMax
        SetConstantValue(19, 0.1f);             // mfRollAirRamDecay
        SetConstantValue(20, 4.0f);             // mfRollAirRamSideDist
    }

    // ---------------------------------------------------------------------------------
    // ProcessParamRules: evaluate the fuzzy rule base for one traffic car.
    //
    // Each input scalar is scored against its category's envelope set (CalcScores returns
    // the four per-envelope membership values, one per SIMD lane), then specific lanes are
    // combined with fuzzy AND (vminfp) / OR (vmaxfp) / NOT (1-x, vsubfp) into the behaviour
    // outputs and a final Select. The X360 body is fully VMX-vectorised and lane-shuffled;
    // the per-category CalcScores evaluations, the lane picks (vperm/vspltw), and the
    // combine-with-Fuzzy{AND,OR,NOT}/Select tree below are re-derived directly from the RAW
    // The two debug position caches. Console-inlined at every call site (the caller stores
    // the quadword itself), which is why neither has its own X360 symbol.
    void FuzzyBehaviourLogic::DEBUGSetLastCameraPos(Vector3 lCameraPos) const
    {
        mDEBUGLastCameraPos = lCameraPos;
    }

    void FuzzyBehaviourLogic::DEBUGSetCurrentParamPos(Vector3 lParamPos) const
    {
        mDEBUGCurrentParamPos = lParamPos;
    }

    // PPC/VMX assembly at 0x82750C28..0x82751080. The six VecFloat outputs are written to
    // lpafOutputs[0..5] (stores at r30 offsets 0x00/0x10/0x20/0x30/0x40/0x50).
    //
    // Envelope sets are loaded by member offset (verified against the class layout): Distance
    // @0x30, Height @0x70, ClosingSpeed @0xB0, LanePos @0xF0, TrafficLight @0x170,
    // NextParamDist @0x1B0, TimeQueueing @0x1F0, mNormalScore @0x230. mAbsoluteSpeedEnvelopes
    // (@0x130) is NOT referenced by this function. mClosingSpeedEnvelopes is evaluated TWICE:
    // once on lRC.z (RC closing speed) and once on lTLNP.z (next-param / NP closing speed).
    // ---------------------------------------------------------------------------------
    void FuzzyBehaviourLogic::ProcessParamRules(
            VecFloat* lpafOutputs,
            Vector4 lf_RCDistance_RCHeight_RCClosingSpeed_RCLanePos,
            Vector4 lf_TLDistance_NPDistance_NPClosingSpeed_RCSpeedInOurLane,
            Vector4 lf_TimeQueueing_Obstructedness_DriveAroundStickiness_W) const
    {
        CGS_ASSERT(lpafOutputs != nullptr, "lpafOutputs");

        const Vector4& lRC   = lf_RCDistance_RCHeight_RCClosingSpeed_RCLanePos;
        const Vector4& lTLNP = lf_TLDistance_NPDistance_NPClosingSpeed_RCSpeedInOurLane;
        const Vector4& lTQ   = lf_TimeQueueing_Obstructedness_DriveAroundStickiness_W;

        // --- per-category membership scores (each broadcast input lane fed to its envelope
        //     set; Splat is the asm's vspltw/vperm broadcasting one input lane). Each
        //     CalcScores returns the four per-envelope memberships in lanes x/y/z/w.
        const Vector4 lafRCDistanceScores     = mDistanceEnvelopes.CalcScores(Splat(lRC.x));      // @0x30, lRC.x
        const Vector4 lafRCHeightScores       = mHeightEnvelopes.CalcScores(Splat(lRC.y));        // @0x70, lRC.y
        const Vector4 lafRCClosingSpeedScores = mClosingSpeedEnvelopes.CalcScores(Splat(lRC.z));  // @0xB0, lRC.z
        const Vector4 lafRCLanePosScores      = mLanePosEnvelopes.CalcScores(Splat(lRC.w));       // @0xF0, lRC.w
        const Vector4 lafTrafLightDistScores  = mTrafficLightEnvelopes.CalcScores(Splat(lTLNP.x));// @0x170, lTLNP.x
        const Vector4 lafNPDistScores         = mNextParamDistEnvelopes.CalcScores(Splat(lTLNP.y));//@0x1B0, lTLNP.y
        const Vector4 lafNPClosingSpeedScores = mClosingSpeedEnvelopes.CalcScores(Splat(lTLNP.z));// @0xB0 (2nd), lTLNP.z
        const Vector4 lafTimeQueueingScores   = mTimeQueueingEnvelopes.CalcScores(Splat(lTQ.x));  // @0x1F0, lTQ.x

        // --- behaviour rules (fuzzy combination of specific category-score lanes) ---------
        // The exact lane picks (.x/.y/.z) below are the vperm lane selections in the asm.

        // Output 1 (r30+0x10, follow score):
        //   MIN( Height[x], MIN( LanePos[x],
        //        MAX( MIN(Dist[y], RCClosingSpeed[z]),
        //             MIN(Dist[x], 1 - RCClosingSpeed[x]) ) ) )
        const VecFloat lfFollowPlayerScore =
            FuzzyAND(Splat(lafRCHeightScores.x),
                     FuzzyAND(Splat(lafRCLanePosScores.x),
                              FuzzyOR(FuzzyAND(Splat(lafRCDistanceScores.y),
                                               Splat(lafRCClosingSpeedScores.z)),
                                      FuzzyAND(Splat(lafRCDistanceScores.x),
                                               FuzzyNOT(Splat(lafRCClosingSpeedScores.x))))));

        // Output 2 (r30+0x20, drive-around-obstruction score):
        //   MIN( Height[x],
        //        MIN( MAX(Dist[y], Dist[z]),
        //             MIN(LanePos[x], 1 - RCClosingSpeed[z]) ) )
        const VecFloat lfDriveAroundObstructionScore =
            FuzzyAND(Splat(lafRCHeightScores.x),
                     FuzzyAND(FuzzyOR(Splat(lafRCDistanceScores.y),
                                      Splat(lafRCDistanceScores.z)),
                              FuzzyAND(Splat(lafRCLanePosScores.x),
                                       FuzzyNOT(Splat(lafRCClosingSpeedScores.z)))));

        // Output 3 (r30+0x30): a single TrafficLight envelope membership, no combinator
        //   (asm stores the clamped TrafficLight(lTLNP.x) lane0 broadcast directly).
        const VecFloat lfQueueingScore = Splat(lafTrafLightDistScores.x);

        // Output 4 (r30+0x40): MIN( NextParamDist[x], 1 - NPClosingSpeed[x] ).
        const VecFloat lfNextParamScore =
            FuzzyAND(Splat(lafNPDistScores.x),
                     FuzzyNOT(Splat(lafNPClosingSpeedScores.x)));

        // Output 5 (r30+0x50): the cached normal-score vector, lane0 (0.2) broadcast.
        const VecFloat lfNormalScore = Splat(mNormalScore_ExtremeSwerveStickiness_Z_W.x);

        // --- Output 0 (r30+0x00): the Select at the tail -------------------------------
        // The asm computes maxOutputs = MAX(out1, out2), gates a "stickiness" fallback by
        // a mask, and selects between 1.0 (when the mask fires) and that fallback:
        //   false branch = MIN( MAX(Obstructedness(lTQ.y), maxOutputs), TimeQueueing[x] )
        //                  + DriveAroundStickiness(lTQ.z)
        //   mask         = (maxOutputs > 0) AND (DriveAroundStickiness(lTQ.z) > 0)
        //   output0      = mask ? 1.0 : false branch
        // (vcmpgtfp v6,maxOutputs,0; vcmpgtfp v13,lTQ.z,0; vand; vsel(false,1.0,mask).)
        const VecFloat lfMaxOutputs = FuzzyOR(lfFollowPlayerScore, lfDriveAroundObstructionScore);
        const VecFloat lfDriveAroundStickiness = Splat(lTQ.z);

        const VecFloat lfStickyFallback =
            FuzzyAND(FuzzyOR(Splat(lTQ.y), lfMaxOutputs),
                     Splat(lafTimeQueueingScores.x))
            + lfDriveAroundStickiness;

        const Vector4 lZero = Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };
        const MaskScalar lMaskOutputs    = rw::math::vpu::IsGreater(lfMaxOutputs, lZero);
        const MaskScalar lMaskStickiness = rw::math::vpu::IsGreater(lfDriveAroundStickiness, lZero);
        const MaskScalar lMaskCombined   = rw::math::vpu::And(lMaskOutputs, lMaskStickiness);

        const VecFloat lfSelectedScore =
            rw::math::vpu::Select(/*false:*/ lfStickyFallback,
                                  /*true :*/ rw::math::vpu::GetVector4_One(),
                                  lMaskCombined);

        // --- store the six behaviour scores -------------------------------------------
        lpafOutputs[0] = lfSelectedScore;                 // r30 + 0x00
        lpafOutputs[1] = lfFollowPlayerScore;             // r30 + 0x10
        lpafOutputs[2] = lfDriveAroundObstructionScore;   // r30 + 0x20
        lpafOutputs[3] = lfQueueingScore;                 // r30 + 0x30
        lpafOutputs[4] = lfNextParamScore;                // r30 + 0x40
        lpafOutputs[5] = lfNormalScore;                   // r30 + 0x50
    }
}
}
