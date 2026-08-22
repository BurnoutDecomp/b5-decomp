#pragma once

// BrnTraffic::Fuzzy::FuzzyBehaviourLogic -- the traffic AI's fuzzy-inference engine.
//
// Each traffic car evaluates a bank of trapezoidal membership functions (the
// FuzzyEnvelopeSet4 "envelope sets", four envelopes packed per SIMD register) over a
// handful of scalar inputs (distance to the race car, height, closing speed, lane
// position, etc.), then combines the resulting category scores with fuzzy AND/OR/NOT
// to derive a small set of behaviour scores (stop, follow, drive-around, swerve...).
//
// Layout + declaration shape recovered from the DecFIGS DWARF
// (GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficFuzzyLogicBehaviours.h),
// gated on the X360 ARTIST ledger. Member offsets verified against the ARTIST
// Construct() zero-fill (0x10..0x2B0) and the per-category stores in ResetToDefaults /
// SetupEnvelope (mDistanceEnvelopes @0x30, +0x40 per set; mNormalScore @0x230;
// mSwerveDistanceEnvelopes @0x240; mpTweakValues @0x2C0).

#include "types.hpp"          // u32/s32/f32
#include "BrnCommonTypes.h"   // Vector3, Vector4, VecFloat (== rw::math::vpu::Vector4)
#include "SharedClasses/Traffic/BrnTrafficFuzzyEnvelopeSet.h"   // BrnTraffic::FuzzyEnvelopeSet4

namespace BrnTraffic
{
    class TweakValues;   // BrnTrafficTweakConstants.h -- pointer-only here (mpTweakValues)

namespace Fuzzy
{
    // Forward only; the debug-render path (DEBUGRenderScore) takes a DebugRender* but is
    // not in the X360 ledger for this TU, so no header dependency is pulled in.
    class FuzzyBehaviourLogic
    {
    public:
        // Zero every envelope set + the cached normal/swerve score vector, latch the
        // tweak-values pointer, then seed the default envelopes/constants.
        void Construct(TweakValues* lpTweakValues);

        // Evaluate the fuzzy rule base for one traffic car. Reads three packed input
        // vectors (lane-named below) and writes six behaviour scores to lpafOutputs[0..5].
        // const: the rule base only reads the envelope sets it owns.
        void ProcessParamRules(VecFloat* lpafOutputs,
                               Vector4 lf_RCDistance_RCHeight_RCClosingSpeed_RCLanePos,
                               Vector4 lf_TLDistance_NPDistance_NPClosingSpeed_RCSpeedInOurLane,
                               Vector4 lf_TimeQueueing_Obstructedness_DriveAroundStickiness_W) const;

        // Re-read the tunable behaviour file at runtime (debug tool). In the X360 ledger
        // but lives in a sibling TU; declared here for completeness, not defined here.
        void ReloadBehaviours();

        // Debug accessors (const; cache camera / param-eval positions for on-screen render).
        void DEBUGSetLastCameraPos(Vector3 lCameraPos) const;
        void DEBUGSetCurrentParamPos(Vector3 lParamPos) const;

    private:
        // Seed all envelope sets + mega-tweek constants with their hard-coded defaults.
        void ResetToDefaults();

        // Set one envelope (liEnvelope) of one category's envelope set (liCategory,
        // 0..9 -> the ten FuzzyEnvelopeSet4 members in declaration order).
        void SetupEnvelope(s32 liCategory, s32 liEnvelope,
                           f32 lfAttackStart, f32 lfAttackStop,
                           f32 lfDecayStart, f32 lfDecayStop);

        // Write one of the 21 "mega-tweek" tunable constants (liValueIndex 0..20) into
        // the shared TweakValues block.
        void SetConstantValue(s32 liValueIndex, f32 lfValue);

        // Debug-only rendering helpers (not in the X360 ledger for this TU; declared to
        // preserve the class shape from the DWARF -- intentionally not defined here).
        void DEBUGRenderParamScores(const VecFloat* lpafScores) const;

        // --- members (verified offsets in parentheses) -----------------------------------
        bool    mbDEBUGRenderParamScores;        // 0x00
        // mutable: the DWARF marks both setters const and the console stores straight
        // through them (UpdateParams_PrecalcBehaviourParams @0x82717F1C
        // `stvx128 v0, r31, 465024` == this + 0x20).
        mutable Vector3 mDEBUGLastCameraPos;     // 0x10
        mutable Vector3 mDEBUGCurrentParamPos;   // 0x20

        FuzzyEnvelopeSet4 mDistanceEnvelopes;    // 0x30  (category 0)
        FuzzyEnvelopeSet4 mHeightEnvelopes;      // 0x70  (category 1)
        FuzzyEnvelopeSet4 mClosingSpeedEnvelopes;// 0xB0  (category 2)
        FuzzyEnvelopeSet4 mLanePosEnvelopes;     // 0xF0  (category 3)
        FuzzyEnvelopeSet4 mAbsoluteSpeedEnvelopes;//0x130 (category 4)
        FuzzyEnvelopeSet4 mTrafficLightEnvelopes;// 0x170 (category 5)
        FuzzyEnvelopeSet4 mNextParamDistEnvelopes;//0x1B0 (category 6)
        FuzzyEnvelopeSet4 mTimeQueueingEnvelopes;// 0x1F0 (category 7)

        // Cached output of the swerve/normal-score path. The y lane holds
        // TweakValues::GetExtremeSwerveStickiness() (latched in Construct/ReloadBehaviours).
        Vector4 mNormalScore_ExtremeSwerveStickiness_Z_W; // 0x230

        FuzzyEnvelopeSet4 mSwerveDistanceEnvelopes;// 0x240 (category 8)
        FuzzyEnvelopeSet4 mSwerveAngleEnvelopes;   // 0x280 (category 9)

        TweakValues* mpTweakValues;              // 0x2C0
    };
}
}
