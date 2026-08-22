// ============================================================================
// BrnTrafficEntityModule_wT2_03.cpp -- per-param behaviour, speed and THE ADVANCE.
//
//   TrafficEntityModule::UpdateParams_UpdatePlan          @0x82737CE8  PARTIAL
//   TrafficEntityModule::UpdateParams_UpdateBehaviour     @0x82716C90  PARTIAL
//   TrafficEntityModule::UpdateParams_CalcDesiredSpeed    @0x82717928
//   TrafficEntityModule::UpdateParams_CalcAcceleration    @0x827172B8  PARTIAL
//   TrafficEntityModule::UpdateParams_IncrementParam      @0x82738C80
//   TrafficEntityModule::UpdateParams_HandleLaneChanges   @0x82725880  PARTIAL
//   TrafficEntityModule::UpdateParam_CheckIfInsideParamInFront @0x82717A70  GATED
//   TrafficEntityModule::UpdateParams_PrecalcBehaviourParams   @0x82717C48  PARTIAL
//   TrafficEntityModule::UpdateParam_CheckIfNeedToSlow    @0x82738468  GATED
//   TrafficEntityModule::DoesParamNeedToStopForStopline   @0x827249F8  GATED
//   TrafficEntityModule::FindNearestParamInFront          @0x82725060
//   TrafficEntityModule::EatParamsNextPlan                @0x827087D0
//   TrafficEntityModule::FindNextParam                    @0x82723A48
//   TrafficEntityModule::FindNextParamRelative            @0x82708400
//   TrafficEntityModule::FindFirstParamAfterPos           @0x82723B80
//
// Layout is host-native: every member is reached by name; the console displacements in the
// comments only attest which member a line resolves to.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTweakConstants.h"

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // TrafficData
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // Hull
#include "SharedClasses/Traffic/BrnTrafficSection.h"            // Section, LaneRung, Neighbour

#include "rw/math/vpu/vector3_operation.h"                      // Magnitude
#include "rw/math/vpu/vector4_operation.h"                      // Min/Max on Vector4

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cmath>     // std::floor
#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // NAMED LEG GATE, same shape as the sibling partfiles'. [DIAG] NOT IN THE X360 BINARY.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T2-traffic-leg] TrafficEntityModule leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    // DELETE-WHEN-STABLE bring-up probes. [DIAG] NOT IN THE X360 BINARY.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    CgsDev::Log::DebugPrint* TrafficDiagStream()
    {
        if (!TrafficDiagEnabled() || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }

    // .rdata literals this TU reads by value, each named by the expression it appears in.
    const f32 KF_PARAM_MAX_PARAM_IN_SEGMENT = 0.999f;   // flt_82008984
    const f32 KF_PARAM_PLAN_LOOKAHEAD_DIST  = 80.0f;    // flt_820BA5E8
    const f32 KF_PARAM_DEFAULT_MAX_SPEED    = 500.0f;   // flt_8200A034
    const f32 KF_PARAM_BRAKE_LIGHT_ACCEL    = -0.6f;    // flt_820BC9D4
    const f32 KF_PARAM_BRAKE_LIGHT_SPEED    = 2.5f;     // flt_82005548

    // UpdatePlan's lane-change arm (@0x827381D8 / @0x827382DC) and FindNearestParamInFront
    // (@0x827252C4 / 0x82725840).
    const f32 KF_PARAM_LANE_CHANGE_RUNG_LOOKAHEAD = 2.0f;    // the +2 rungs the carry-out books
    const f32 KF_PARAM_LANE_CHANGE_MIN_ROOM       = 30.0f;
    const f32 KF_FIND_NEAREST_MERGE_MAX_DIST      = 14.0f;
    const f32 KF_FIND_NEAREST_MERGE_MIN_SPEED     = 5.0f;    // flt_8200426C
    const u32 KU_FIND_NEAREST_MAX_EXTRA_SECTIONS   = 9;
    const u32 KU_FIND_NEAREST_MAX_MERGING_SECTIONS = 3;

    // UpdateParams_PrecalcBehaviourParams @0x82717C48. Every literal below is the value IDA
    // folds for the named .rdata symbol in that function's listing.
    const u32 KU_PARAM_NUM_BEHAVIOUR_SCORES     = 6;      // the ProcessParamRules output count
    const u32 KU_PARAM_BUSY_PHYSICAL_TRAFFIC_COUNT = 5;   // 0x827180E8 cmplwi r11, 5
    const f32 KF_PARAM_GIVE_UP_STOP_RADIUS      = 5.0f;   // flt_8200426C
    const f32 KF_PARAM_DRIVE_AROUND_STOP_DIST   = 2.0f;   // flt_820BA86C
    const f32 KF_PARAM_DRIVE_AROUND_SPEED_SCALE = 4.0f;   // flt_820BA8DC
    const f32 KF_PARAM_CRASH_SLIDER_MIN_VALUE   = 0.01f;  // flt_820BA5D4
    const f32 KF_PARAM_ACTION_SCORE_EPSILON     = 0.05f;  // flt_820047C8
    const f32 KF_PARAM_NO_STOP_DIST             = 3.4028235e38f;   // flt_820BA23C

    // mxEffectAndHistoryState bit 0, set/cleared by UpdateParams_CalcDesiredSpeed
    // (`stb r11, 0x1A(r20)` at 0x82717A4C / 0x82717A60): the brake-light effect bit.
    const u8 KX_EFFECT_BRAKING = 0x01;

    inline VecFloat SplatLane(f32 lfValue)
    {
        const VecFloat lLane = { lfValue, lfValue, lfValue, lfValue };
        return lLane;
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_UpdatePlan  @0x82737CE8  (.cpp 10662..10787)  PARTIAL
//
// Fills the param's two-slot plan queue until it has looked KF_PARAM_PLAN_LOOKAHEAD_DIST
// metres down the road. The second argument is the console's luMaxLaneChangeDiceRoll
// (UpdateParams computes 255 * KF_LANE_CHANGE_DICE_ROLL_SCALE / mfSimTimeSinceLastDecision);
// BrnTrafficEntityModule.h spells it luSectionIndex, which is a stale name.
//
// The lane-change arm (@0x82738144..0x8273837C) is LANDED: Section::FindNeighbourForRung
// @0x82752B70 now has a real body in SharedClasses/Traffic/BrnTrafficSection.cpp.
// FLAG (spelling only): the console inlines CgsNumeric::Random::RandomUInt(u32 luRange) --
// draw the OLD seed's high word, step the LCG, reduce unsigned by luRange (0x82738170..A8),
// asserting "luMod > 0" at CgsRandom.h:303. CgsRandom.h declares no single-argument overload
// and is not owned here, so the identical draw is spelled RandomInt(0, luRange - 1), whose
// body is that same expansion (CgsRandom.cpp). DELETE-WHEN CgsRandom.h gains RandomUInt(u32).
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_UpdatePlan(u32 luParam, u32 luMaxLaneChangeDiceRoll)
{
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

    const Param* lpParam = &maParams[luParam];

    f32 lfDistanceSoFar = 0.0f;
    u32 luPlan          = 0;
    u32 luSegmentAlong  = lpParam->muCurrentSegment;
    f32 lfParamAlong    = lpParam->mfParamAlong;
    u32 luHull          = lpParam->muHullIndex;
    u32 luSection       = lpParam->muSectionIndex;

    for (;;)
    {
        ParamPlan* lpPlan = GetParamPlan(luParam, luPlan);

        const Hull* lpHull = GetHull(luHull);              // carries the muNumHulls bound
        CGS_ASSERT(luSection < lpHull->muNumSections, "luIndex < muNumSections");
        const Section* lpSection = lpHull->GetSection(luSection);

        if (lpPlan->muType == ParamPlan::E_TYPE_CHANGE_SECTION)
        {
            const f32 lfDistAlong =
                lpSection->CalcDistanceAlongSection(lfParamAlong, luSegmentAlong,
                                                    lpHull->GetRungLengthsForSection(lpSection));
            lfDistanceSoFar += (lpSection->mfLength - lfDistAlong);

            luSegmentAlong = 0;
            lfParamAlong   = 0.0f;
            luHull         = lpPlan->mChangeSectionData.muNewHull;
            luSection      = lpPlan->mChangeSectionData.muNewSection;
        }
        else if (lpPlan->muType >= ParamPlan::E_TYPES_COUNT)
        {
            CGS_ASSERT(false, "Unknown param plan type");
        }
        else if (lpPlan->muType == ParamPlan::E_TYPE_NONE)
        {
            const f32 lfDistAlong =
                lpSection->CalcDistanceAlongSection(lfParamAlong, luSegmentAlong,
                                                    lpHull->GetRungLengthsForSection(lpSection));
            lfDistanceSoFar += (lpSection->mfLength - lfDistAlong);

            if (lfDistanceSoFar >= KF_PARAM_PLAN_LOOKAHEAD_DIST)
            {
                // 0x82738144..0x8273837C -- roll for a lane change into a neighbour section.
                CGS_ASSERT(luMaxLaneChangeDiceRoll + 1u > 0u, "luMod > 0");
                const u32 luRoll = static_cast<u32>(
                    mRand.RandomInt(0, static_cast<s32>(luMaxLaneChangeDiceRoll)));

                Side leSide         = E_LEFT;
                u8   luPlanDirection = 0;
                if (luRoll < lpSection->muChangeLeftProb)
                {
                    leSide          = E_LEFT;
                    luPlanDirection = 1;
                }
                else if (luRoll < lpSection->muChangeRightProb)
                {
                    leSide          = E_RIGHT;
                    luPlanDirection = 2;
                }

                const u32 luRungToCarryOut =
                    luSegmentAlong + static_cast<u32>(KF_PARAM_LANE_CHANGE_RUNG_LOOKAHEAD);

                if (luPlanDirection != 0 && luRungToCarryOut < lpSection->GetNumSegments())
                {
                    const u16 luNeighbour =
                        lpSection->FindNeighbourForRung(luRungToCarryOut, leSide, lpHull);

                    if (luNeighbour < KU_UNKNOWN_NEIGHBOUR)
                    {
                        const Neighbour* lpNeighbour = lpHull->GetNeighbour(luNeighbour);

                        CGS_ASSERT(lpNeighbour->muSection != luSection,
                                   "Section thinks it's its own neighbour");

                        // 0x827382D4..0x827382F8: the shared stretch must still have
                        // KF_PARAM_LANE_CHANGE_MIN_ROOM of lane left past where we are.
                        const f32* lpafRungLengths = lpHull->GetRungLengthsForSection(lpSection);
                        const f32  lfSharedEnd =
                            lpafRungLengths[lpNeighbour->muOurStartRung +
                                            lpNeighbour->muSharedLength];

                        if ((lfDistAlong + KF_PARAM_LANE_CHANGE_MIN_ROOM) < lfSharedEnd)
                        {
                            lfParamAlong = lpNeighbour->ConvertOurParameterToTheirs(
                                lfParamAlong + KF_PARAM_LANE_CHANGE_RUNG_LOOKAHEAD);

                            lpPlan->muDirection                      = luPlanDirection;
                            lpPlan->mChangeLaneData.muNeighbourData  = luNeighbour;
                            lpPlan->muType                           = ParamPlan::E_TYPE_CHANGE_LANE;
                            lpPlan->mChangeLaneData.muNewSection     = lpNeighbour->muSection;
                            lpPlan->mChangeLaneData.muRungToCarryOut =
                                static_cast<u8>(luRungToCarryOut);

                            CGS_ASSERT(lpPlan->mChangeLaneData.muNewSection != luSection,
                                       "Param came up with a stupid plan");

                            // 0x82738370 fctidz/stfiwx -- truncate, not round.
                            luSegmentAlong = static_cast<u32>(lfParamAlong);
                        }
                    }
                }
            }
            else
            {
                lpPlan->muType = ParamPlan::E_TYPE_CHANGE_SECTION;
                Pressure_PickSplitToTake(lpSection,
                                         &lpPlan->mChangeSectionData.muNewSection,
                                         &lpPlan->mChangeSectionData.muNewHull,
                                         &lpPlan->muDirection);

                CGS_ASSERT(!((lpPlan->mChangeSectionData.muNewHull == lpParam->muHullIndex) &&
                             (lpPlan->mChangeSectionData.muNewSection == lpParam->muSectionIndex)),
                           "Param came up with a stupid plan");

                luSegmentAlong = 0;
                lfParamAlong   = 0.0f;
                luHull         = lpPlan->mChangeSectionData.muNewHull;
                luSection      = lpPlan->mChangeSectionData.muNewSection;
            }
        }

        if (lpPlan->muType == ParamPlan::E_TYPE_NONE ||
            lpPlan->muType == ParamPlan::E_TYPE_CHANGE_LANE)
        {
            return;
        }

        if (lfDistanceSoFar >= KF_PARAM_PLAN_LOOKAHEAD_DIST)
        {
            CGS_ASSERT(maParams[luParam].IsAlive(), "IsAlive()");
            maParams[luParam].mxEffectAndHistoryState &= ~Param::E_HISTORY_NEEDS_NEW_PLAN;
        }

        if (luHull == KU_INVALID_HULL)
        {
            CGS_ASSERT(maParams[luParam].IsAlive(), "IsAlive()");
            maParams[luParam].mxEffectAndHistoryState &= ~Param::E_HISTORY_NEEDS_NEW_PLAN;
            return;
        }

        CGS_ASSERT(luSection != KU_INVALID_SECTION, "luSection != KU_INVALID_SECTION");

        ++luPlan;
        if (luPlan >= KU_PARAM_NUM_PLANS)
        {
            return;
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_UpdateBehaviour  @0x82716C90  (.cpp 10808..10818)
//
// Copies the behaviour the fuzzy pre-pass chose (and its target speed / stop distance) out of
// maParamNeedToSlowData into the Param, then ticks the queueing timer. The copy is gated; the
// timer tick is not.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_UpdateBehaviour(u32 luParam)
{
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

    Param* lpParam = &maParams[luParam];
    const ParamNeedToSlowData* lpParamNeedToSlowData = GetParamNeedToSlowData(luParam);

    // GATE: UpdateParams_UpdateBehaviour @0x82716C90 behaviour copy, plus the console's own
    // `miBehaviour >= 0` assert (.cpp 10808), which would fire on every param every frame.
    // BLOCKER: UpdateParam_CheckIfNeedToSlow @0x82738468 is gated (see _wT2_03 above), so the
    // producer never runs and miBehaviour stays at Clear's -1.
    // DELETE-WHEN CheckIfNeedToSlow lands: arm the assert and drop the branch.
    if (lpParamNeedToSlowData->miBehaviour < 0)
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
                      "UpdateParams_UpdateBehaviour @0x82716C90 behaviour copy -- "
                      "DoTimeSlicedLogic @0x82743FE8 landed but its CheckIfNeedToSlow "
                      "@0x82738468 leg is gated, so miBehaviour stays at Clear's -1");
    }
    else
    {
        CGS_ASSERT(lpParamNeedToSlowData->miBehaviour < Param::KI_BEHAVIOURS_COUNT,
                   "lpParamNeedToSlowData->miBehaviour < Param::E_BEHAVIOURS_COUNT");

        // .cpp 10815 / 10818, message-streamed on console as
        // "Param <n> decided on divergent behaviour <b>".
        CGS_ASSERT(mbAllowDivergentBehaviour || mbAtStartLineSoProtectRaceCarsFromTraffic ||
                       (lpParamNeedToSlowData->miBehaviour != 0 &&
                        lpParamNeedToSlowData->miBehaviour != 1 &&
                        lpParamNeedToSlowData->miBehaviour != 3),
                   "Param decided on divergent behaviour");
        CGS_ASSERT(mbAllowDivergentBehaviour || lpParamNeedToSlowData->miBehaviour != 2,
                   "Param decided on divergent behaviour");

        lpParam->miBehaviour   = lpParamNeedToSlowData->miBehaviour;
        lpParam->mfStopDist    = lpParamNeedToSlowData->mfStopDist;
        lpParam->mfTargetSpeed = lpParamNeedToSlowData->mfTargetSpeed;
    }

    if (lpParam->IsQueueing() && mbAllowDivergentBehaviour)
    {
        lpParam->mfTimeQueueing += mfSimTimeSinceLastDecision;
    }
    else
    {
        lpParam->mfTimeQueueing = 0.0f;
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_CalcDesiredSpeed  @0x82717928  (.cpp 11059)
//
// Integrates one decision frame of the acceleration the arm below picks. lpHull is on the
// console signature but the body never reads it.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_CalcDesiredSpeed(
        u32 luParam,
        const Section* lpSection,
        const Hull* lpHull,
        const CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>& lrAvoidSet)
{
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
    (void)lpHull;

    Param* lpParam = &maParams[luParam];

    CGS_ASSERT(mbAllowDivergentBehaviour || lpParam->miBehaviour != 0,
               "AllowDivergentBehaviour() || "
               "( lpParam->miBehaviour != Param::E_BEHAVIOUR_SLOWING_FOR_CRASH )");

    const f32 lfAcceleration =
        UpdateParams_CalcAcceleration(luParam, lpParam, lpSection, lrAvoidSet);

    lpParam->mfLastSpeed    = lpParam->mfSpeed;
    lpParam->mfAcceleration = lfAcceleration;

    f32 lfSpeed = mfSimTimeSinceLastDecision * lfAcceleration + lpParam->mfSpeed;
    if (lfSpeed < 0.0f)
    {
        lfSpeed = 0.0f;
    }
    if (lfAcceleration <= 0.0f && lfSpeed < mTweakValues.GetMinSpeedForCutoff())
    {
        lfSpeed = 0.0f;
    }
    lpParam->mfSpeed = lfSpeed;

    if (lfAcceleration < KF_PARAM_BRAKE_LIGHT_ACCEL ||
        (lfSpeed < KF_PARAM_BRAKE_LIGHT_SPEED && lfAcceleration <= 0.0f))
    {
        lpParam->mxEffectAndHistoryState |= KX_EFFECT_BRAKING;
    }
    else
    {
        lpParam->mxEffectAndHistoryState &= static_cast<u8>(~KX_EFFECT_BRAKING);
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_CalcAcceleration  @0x827172B8  (.cpp 10998..11030) PARTIAL
//
// The behaviour switch: NORMAL closes on the lane speed limit, the five slowing behaviours
// solve v^2 = u^2 + 2as for the target speed at the stop distance, and behaviour 0
// (SLOWING_FOR_CRASH) is a fixed-rate ramp.
//
// GATED LEGS, all three under the console's own `if (AllowDivergentBehaviour())`, so the
// fallback is the shipped !divergent path (lfMaxSpeed 500, lfSpeedScale 1):
//   * the showtime local-player proximity arm @0x82717370 -- BLOCKER: the unnamed .data
//     vectors it reads and mRaceCarState's active-race-car position lane.
//   * the slam / extreme-swerve arm @0x827174A0 -- BLOCKER: flt_8300CB50 and the
//     unk_8300CB40 / unk_8300CA30 / unk_8300CCA0 / unk_8300CB20 lane block, all dyn-init
//     .data (see scratchpad recovered_constants.md for the thunk-walk recipe).
//   * switch case 0 @0x82717844 -- BLOCKER: flt_8300C958 / flt_8300C95C, same dyn-init class.
// DELETE-WHEN those five globals are recovered.
// ----------------------------------------------------------------------------
f32 TrafficEntityModule::UpdateParams_CalcAcceleration(
        u32 luParam,
        const Param* lpParam,
        const Section* lpSection,
        const CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>& lrAvoidSet) const
{
    (void)luParam;
    (void)lrAvoidSet;

    const f32 lfLaneSpeed  = mfSpeedMultiplier * lpSection->mfSpeed;   // +0x72880 * section+0x24
    const f32 lfMaxSpeed   = KF_PARAM_DEFAULT_MAX_SPEED;
    const f32 lfSpeedScale = 1.0f;

    if (mbAllowDivergentBehaviour)
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
                      "UpdateParams_CalcAcceleration @0x827172B8 divergent arms (showtime "
                      "@0x82717370 / slam-swerve @0x827174A0) -- unnamed dyn-init .data "
                      "vectors flt_8300CB50, unk_8300CB40/CA30/CCA0/CB20");
    }

    f32 lfAcceleration = 0.0f;

    switch (lpParam->miBehaviour)
    {
        case 0:   // E_BEHAVIOUR_SLOWING_FOR_CRASH
        {
            CGS_ASSERT(mbAllowDivergentBehaviour, "AllowDivergentBehaviour()");
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                          "UpdateParams_CalcAcceleration @0x82717844 behaviour 0 arm -- "
                          "flt_8300C958 / flt_8300C95C are unnamed dyn-init .data floats");
            lfAcceleration = 0.0f;
            break;
        }

        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        {
            CGS_ASSERT(mbAllowDivergentBehaviour || lpParam->miBehaviour != 2,
                       "AllowDivergentBehaviour() || "
                       "lpParam->miBehaviour != Param::E_BEHAVIOUR_DRIVING_AROUND_OBSTRUCTION");
            CGS_ASSERT(mbAllowDivergentBehaviour || mbAtStartLineSoProtectRaceCarsFromTraffic ||
                           lpParam->miBehaviour != 1,
                       "AllowDivergentBehaviour() || mbAtStartLineSoProtectRaceCarsFromTraffic || "
                       "lpParam->miBehaviour != Param::E_BEHAVIOUR_STOPPING_FOR_OBSTRUCTION");
            CGS_ASSERT(mbAllowDivergentBehaviour || mbAtStartLineSoProtectRaceCarsFromTraffic ||
                           lpParam->miBehaviour != 3,
                       "AllowDivergentBehaviour() || mbAtStartLineSoProtectRaceCarsFromTraffic || "
                       "lpParam->miBehaviour != Param::E_BEHAVIOUR_FOLLOWING_RACE_CAR");

            if (lpParam->mfStopDist < mTweakValues.GetMinStopDist())
            {
                lfAcceleration = mTweakValues.GetMinAcceleration();
                break;
            }

            f32 lfTargetSpeed = lpParam->mfTargetSpeed * lfSpeedScale;
            if (lfTargetSpeed < 0.0f)
            {
                lfTargetSpeed = 0.0f;
            }
            if (lfTargetSpeed > lfMaxSpeed)
            {
                lfTargetSpeed = lfMaxSpeed;
            }

            lfAcceleration = ((lfTargetSpeed * lfTargetSpeed) -
                              (lpParam->mfSpeed * lpParam->mfSpeed)) /
                             (lpParam->mfStopDist * 2.0f);

            if (lfAcceleration < mTweakValues.GetMinAcceleration())
            {
                lfAcceleration = mTweakValues.GetMinAcceleration();
            }
            if (lfAcceleration > mTweakValues.GetMaxAcceleration())
            {
                lfAcceleration = mTweakValues.GetMaxAcceleration();
            }
            break;
        }

        case 6:   // KI_BEHAVIOUR_NORMAL
        {
            f32 lfDesiredSpeed = lfLaneSpeed * lfSpeedScale;
            if (lfDesiredSpeed < 0.0f)
            {
                lfDesiredSpeed = 0.0f;
            }
            if (lfDesiredSpeed > lfMaxSpeed)
            {
                lfDesiredSpeed = lfMaxSpeed;
            }

            lfAcceleration = lfDesiredSpeed - lpParam->mfSpeed;

            if (lfAcceleration < mTweakValues.GetMinNormalAcceleration())
            {
                lfAcceleration = mTweakValues.GetMinNormalAcceleration();
            }
            if (lfAcceleration > mTweakValues.GetMaxNormalAcceleration())
            {
                lfAcceleration = mTweakValues.GetMaxNormalAcceleration();
            }
            break;
        }

        default:
            CGS_ASSERT(false, "Bad state in param behaviour");
            break;
    }

    return lfAcceleration;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_IncrementParam  @0x82738C80  (.cpp 12164..12290)
//
// THE ADVANCE. Integrates s = ut + at^2/2 (u == mfLastSpeed, a == mfAcceleration, t ==
// mfSimTimeSinceLastDecision), clamped at the stopping distance while decelerating, then walks
// the param forward one lane segment at a time, taking the next planned section when it runs
// off the end. Finishes by resampling the lane frame and republishing the ParamTransform.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_IncrementParam(u32 luParam,
                                                      const Hull** lpapHull,
                                                      const Section** lpapSection)
{
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

    const Hull*    lpHull    = *lpapHull;
    const Section* lpSection = *lpapSection;
    Param*         lpParam   = &maParams[luParam];

    f32 lfParamAlong  = lpParam->mfParamAlong;
    u32 luCurrentRung = lpParam->muCurrentSegment;

    // 0x82738D0C: the console re-resolves the param's own section to take its muRungOffset.
    const Section* lpParamSection = lpHull->GetSection(lpParam->muSectionIndex);
    const f32* lpafRungLengths = lpHull->GetRungLengthsForSection(lpParamSection);

    CGS_ASSERT(lpSection->muNumRungs > 0, "muNumRungs > 0");
    u32 luNumSegments   = lpSection->muNumRungs - 1u;
    f32 lfSegmentLength = lpafRungLengths[luCurrentRung + 1] - lpafRungLengths[luCurrentRung];

    const f32 lfDt = mfSimTimeSinceLastDecision;   // +0x713F8

    f32 lfDistance = ((lfDt * lfDt) * lpParam->mfAcceleration) * 0.5f +
                     (lfDt * lpParam->mfLastSpeed);

    // Under braking, never travel past where the speed reaches zero.
    const f32 lfStopDistance = -((lpParam->mfLastSpeed * lpParam->mfLastSpeed) /
                                 (lpParam->mfAcceleration * 2.0f));
    if (lpParam->mfAcceleration < 0.0f && lfStopDistance >= 0.0f)
    {
        if ((lfDistance - lfStopDistance) >= 0.0f)
        {
            lfDistance = lfStopDistance;
        }
    }

    f32 lfDistanceToTravel = (lfDistance >= 0.0f) ? lfDistance : 0.0f;

    if (mbDEBUGStopTrafficMoving)                                       // +0x727B8
    {
        lfDistanceToTravel = 0.0f;
    }
    if (mbDEBUGPick_StopVehicle && muDEBUGPickedVehicle == luParam)     // +0x7287C / +0x72878
    {
        lfDistanceToTravel = 0.0f;
    }

    const f32 lfParamAlongBefore = lfParamAlong;   // [DIAG] DELETE-WHEN-STABLE

    bool lbChangedRung = false;

    for (;;)
    {
        CGS_ASSERT(luCurrentRung == static_cast<u32>(static_cast<s64>(lfParamAlong)),
                   "Current rung got out of sync");

        const f32 lfDistanceToEndOfSegment =
            (KF_PARAM_MAX_PARAM_IN_SEGMENT - (lfParamAlong - std::floor(lfParamAlong))) *
            lfSegmentLength;

        if (lfDistanceToEndOfSegment > lfDistanceToTravel)
        {
            break;
        }

        lfDistanceToTravel -= lfDistanceToEndOfSegment;
        ++luCurrentRung;
        lfParamAlong = std::floor(lfParamAlong + 1.0f);

        CGS_ASSERT(luCurrentRung == static_cast<u32>(static_cast<s64>(lfParamAlong)),
                   "Current rung got out of sync(2)");

        lbChangedRung = true;

        if (luCurrentRung >= luNumSegments)
        {
            // A queued lane change never survives a section change.
            while (GetParamPlan(luParam, 0)->muType == ParamPlan::E_TYPE_CHANGE_LANE)
            {
                EatParamsNextPlan(luParam);
            }

            ParamPlan* lpPlan = GetParamPlan(luParam, 0);

            u8  luNewSection = KU_INVALID_SECTION;
            u16 luNewHull    = static_cast<u16>(KU_INVALID_HULL);

            if (lpPlan->muType == ParamPlan::E_TYPE_CHANGE_SECTION)
            {
                luNewSection = lpPlan->mChangeSectionData.muNewSection;
                luNewHull    = lpPlan->mChangeSectionData.muNewHull;
                lpParam->muCurrentSectionDirection = lpPlan->muDirection;
                EatParamsNextPlan(luParam);
            }
            else
            {
                CGS_ASSERT(lpPlan->muType == ParamPlan::E_TYPE_NONE,
                           "lpPlan->muType == ParamPlan::E_TYPE_NONE");
                Pressure_PickSplitToTake(lpSection, &luNewSection, &luNewHull,
                                         &lpParam->muCurrentSectionDirection);
            }

            if (luNewSection == KU_INVALID_SECTION)
            {
                KillParam(luParam);
                return;
            }
            if (mActiveHulls.Find(luNewHull) == ActiveHullSet::KU_INVALID)
            {
                KillParam(luParam);
                return;
            }

            const u16 luOldHull = lpParam->muHullIndex;
            lpParam->muSectionIndex = luNewSection;

            CGS_ASSERT(lpParam->IsAlive(), "IsAlive()");   // BrnTrafficParam.h 893

            lpParam->muNextStopLineIndex = static_cast<u8>(KU_UNKNOWN_STOPLINE);   // 0xFE
            lpParam->mxEffectAndHistoryState |= Param::E_HISTORY_CHANGED_SECTION;

            lfParamAlong  = 0.0f;
            luCurrentRung = 0;

            if (luNewHull != luOldHull)
            {
                lpParam->muHullIndex = luNewHull;
                lpHull = GetHull(luNewHull);
                *lpapHull = lpHull;
            }

            CGS_ASSERT(luNewSection < lpHull->muNumSections, "luIndex < muNumSections");
            lpSection = lpHull->GetSection(luNewSection);
            *lpapSection = lpSection;

            lpafRungLengths = lpHull->GetRungLengthsForSection(lpSection);

            CGS_ASSERT(lpSection->muNumRungs > 0, "muNumRungs > 0");
            luNumSegments = lpSection->muNumRungs - 1u;

            lpParam->mauNeighbourData[E_LEFT]  = static_cast<u16>(KU_UNKNOWN_NEIGHBOUR);
            lpParam->mauNeighbourData[E_RIGHT] = static_cast<u16>(KU_UNKNOWN_NEIGHBOUR);
        }

        lpParam->PushHistory(lpSection->muRungOffset + luCurrentRung, lpParam->muHullIndex);
        lfSegmentLength = lpafRungLengths[luCurrentRung + 1] - lpafRungLengths[luCurrentRung];
    }

    const f32 lfNewParamAlong = (lfDistanceToTravel / lfSegmentLength) + lfParamAlong;

    // The third output is the RIGHT axis here (ParamTransform::Update's third argument); the
    // declaration in BrnTrafficSection.h names that parameter for its WorldMap consumer.
    Vector3 lPos;
    Vector3 lDir;
    Vector3 lRight;
    lpSection->CalcTransformAtParameter(lpHull->mpaRungs, SplatLane(lfNewParamAlong),
                                        luCurrentRung, lPos, lDir, lRight);

    CGS_ASSERT(lfNewParamAlong < static_cast<f32>(lpSection->GetNumSegments()),
               "lfParamAlong < (float32_t) lpSection->GetNumSegments()");

    maParamTransforms[luParam].Update(lPos, lDir, lRight,
                                      SplatLane(lpParam->mfSpeed),
                                      SplatLane(lpParam->mfAcceleration));

    CGS_ASSERT(lfNewParamAlong >= 0.0f, "lfParamAlong >= 0.0f");
    lpParam->SetParamAlong(lfNewParamAlong);

    lpParam->PushHistory(lpParam->muCurrentSegment + lpSection->muRungOffset,
                         lpParam->muHullIndex);

    // [T2-move] the go/no-go for the whole round: did a param actually advance?
    {
        static bool sbLogged = false;
        if (!sbLogged && lfNewParamAlong != lfParamAlongBefore)
        {
            if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
            {
                sbLogged = true;
                *lpDiag << "[T2-move] FIRST PARAM ADVANCE: param " << luParam
                        << " mfParamAlong " << lfParamAlongBefore << " -> " << lfNewParamAlong
                        << " delta " << (lfNewParamAlong - lfParamAlongBefore)
                        << " mfSpeed " << lpParam->mfSpeed
                        << " mfAccel " << lpParam->mfAcceleration << "\n";
            }
        }
    }

    if (lbChangedRung)
    {
        UpdateParams_UpdateNeighbours(lpParam, lpSection, lpHull);
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_HandleLaneChanges  @0x82725880  (.cpp 12321..12323) PARTIAL
//
// Carries out a queued E_TYPE_CHANGE_LANE plan once the param reaches the rung it was booked
// for. The outer plan bookkeeping (drop a plan whose rung has already gone past) is real.
//
// GATED LEG -- the carry-out itself (@0x827259A0..0x82725B70). BLOCKER: the local-player
// proximity guard reads unk_8300CBB0, an unnamed dyn-init .data lane block with no recovered
// value. GetParamBehind and Section::CalcDistanceAlongSection have both landed, and
// UpdateParams_UpdatePlan now books CHANGE_LANE plans, so this is the last hole in the chain.
// DELETE-WHEN unk_8300CBB0 is recovered from its 0x82C6xxxx dyn-init thunk.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_HandleLaneChanges(u32 luParam,
                                                         const Hull* lpHull,
                                                         const Section** lpapSection)
{
    CGS_ASSERT(lpHull != 0, "lpHull");
    CGS_ASSERT(lpapSection != 0, "lppSection");
    CGS_ASSERT(*lpapSection != 0, "*lppSection");
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

    const Param* lpParam = &maParams[luParam];
    const ParamPlan* lpPlan = GetParamPlan(luParam, 0);

    if (lpPlan->muType == ParamPlan::E_TYPE_CHANGE_LANE)
    {
        const u32 luRungToCarryOut = lpPlan->mChangeLaneData.muRungToCarryOut;
        if (luRungToCarryOut > lpParam->muCurrentSegment)
        {
            return;   // not there yet
        }
        if (luRungToCarryOut < lpParam->muCurrentSegment)
        {
            EatParamsNextPlan(luParam);   // missed it -- drop the plan
        }
    }

    if (GetParamPlan(luParam, 0)->muType == ParamPlan::E_TYPE_CHANGE_LANE &&
        lpParam->miBehaviour == Param::KI_BEHAVIOUR_NORMAL)
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
                      "UpdateParams_HandleLaneChanges @0x82725880 carry-out -- unk_8300CBB0, the "
                      "unrecovered dyn-init .data lane block the proximity guard reads");
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::EatParamsNextPlan  @0x827087D0  (BrnTrafficParam.h 1092)
//
// Pops plan slot 0, shuffles slot 1 down, empties the tail slot and asks for a fresh plan.
// ----------------------------------------------------------------------------
void TrafficEntityModule::EatParamsNextPlan(u32 luParam)
{
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

    Param* lpParam = &maParams[luParam];

    for (u32 luPlan = 1; luPlan < KU_PARAM_NUM_PLANS; ++luPlan)
    {
        lpParam->maPlans[luPlan - 1] = lpParam->maPlans[luPlan];
    }
    lpParam->maPlans[KU_PARAM_NUM_PLANS - 1].muType = ParamPlan::E_TYPE_NONE;

    CGS_ASSERT(lpParam->IsAlive(), "IsAlive()");
    lpParam->mxEffectAndHistoryState |= Param::E_HISTORY_NEEDS_NEW_PLAN;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::FindNextParam  @0x82723A48  (.cpp 9247 / 9253)
//
// The first argument is the HULL index (it goes straight into GetHullRuntime); the header
// spells it luParam, which is a stale name.
// ----------------------------------------------------------------------------
u32 TrafficEntityModule::FindNextParam(u32 luHull, u32 luSectionIndex, f32 lfParamAlong) const
{
    const HullRuntime* lpHullRuntime = GetHullRuntime(luHull);

    const u32 luFirstParam = lpHullRuntime->GetFirstParamInSection(luSectionIndex);
    if (luFirstParam == KU_INVALID_PARAM)
    {
        return luFirstParam;
    }

    CGS_ASSERT(luFirstParam < KU_MAX_PARAMS, "Out of range param in list");

    const u32 luNextParam = FindNextParamRelative(luFirstParam, lfParamAlong);
    if (luNextParam == KU_INVALID_PARAM)
    {
        return luFirstParam;
    }

    CGS_ASSERT(luNextParam < KU_MAX_PARAMS, "Out of range param in list");
    return luNextParam;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::FindNextParamRelative  @0x82708400  (.cpp 9290..9313)
//
// Walks the section's ordered param list from luCurrParam to the neighbour that straddles
// lfParamAlong. Ties break on the param index, which is what keeps the walk deterministic.
// ----------------------------------------------------------------------------
u32 TrafficEntityModule::FindNextParamRelative(u32 luCurrParam, f32 lfParamAlong) const
{
    CGS_ASSERT(luCurrParam < KU_MAX_PARAMS, "Out of range param in list");

    if (luCurrParam == KU_INVALID_PARAM)
    {
        return KU_INVALID_PARAM;
    }

    const ParamListNode* lpNode = &maParamListNodes[luCurrParam];
    f32 lfNodeParamAlong = lpNode->mfParamAlong;

    if (lfNodeParamAlong > lfParamAlong)
    {
        u32 luPrevParam = lpNode->muPrevParam;
        while (luPrevParam != KU_INVALID_PARAM)
        {
            CGS_ASSERT(luPrevParam < KU_MAX_PARAMS, "Out of range param in list");

            const ParamListNode* lpPrevNode = &maParamListNodes[luPrevParam];
            CGS_ASSERT(lpPrevNode->muNextParam == luCurrParam,
                       "lpPrevNode->muNextParam == luCurrParam");

            const f32 lfPrevNodeParamAlong = lpPrevNode->mfParamAlong;
            CGS_ASSERT(lfPrevNodeParamAlong <= lfNodeParamAlong,
                       "lfPrevNodeParamAlong <= lfNodeParamAlong");

            if (lfParamAlong > lfPrevNodeParamAlong)
            {
                break;
            }
            if (lfParamAlong == lfPrevNodeParamAlong && luCurrParam > luPrevParam)
            {
                break;
            }

            luCurrParam      = luPrevParam;
            luPrevParam      = lpPrevNode->muPrevParam;
            lfNodeParamAlong = lfPrevNodeParamAlong;
        }
        return luCurrParam;
    }

    u32 luNextParam = lpNode->muNextParam;
    while (luNextParam != KU_INVALID_PARAM)
    {
        CGS_ASSERT(luNextParam < KU_MAX_PARAMS, "Out of range param in list");

        const ParamListNode* lpNextNode = &maParamListNodes[luNextParam];
        CGS_ASSERT(lpNextNode->muPrevParam == luCurrParam,
                   "lpNextNode->muPrevParam == luCurrParam");

        const f32 lfNextNodeParamAlong = lpNextNode->mfParamAlong;
        CGS_ASSERT(lfNextNodeParamAlong >= lfNodeParamAlong,
                   "lfNextNodeParamAlong >= lfNodeParamAlong");

        if (lfNextNodeParamAlong > lfParamAlong)
        {
            return luNextParam;
        }
        if (lfNextNodeParamAlong == lfParamAlong && luNextParam > luCurrParam)
        {
            return luNextParam;
        }

        luCurrParam      = luNextParam;
        luNextParam      = lpNextNode->muNextParam;
        lfNodeParamAlong = lfNextNodeParamAlong;
    }
    return luCurrParam;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::FindFirstParamAfterPos  @0x82723B80  (.cpp 9385)
//
// First param on the section whose remaining distance to the end of the lane is at least
// lfDistanceFromEndOfLane, with that remaining distance written back.
// ----------------------------------------------------------------------------
u32 TrafficEntityModule::FindFirstParamAfterPos(u32 luHull, u32 luSectionIndex,
                                                f32 lfDistanceFromEndOfLane,
                                                f32* lpfOutDistance) const
{
    CGS_ASSERT(lfDistanceFromEndOfLane >= 0.0f, "lfDistanceFromEndOfLane >= 0.0f");

    const HullRuntime* lpHullRuntime = GetHullRuntimeSafe(luHull);
    if (lpHullRuntime == 0)
    {
        return KU_INVALID_PARAM;
    }

    u32 luParam = lpHullRuntime->GetFirstParamInSection(luSectionIndex);

    const Hull*    lpHull    = GetHull(luHull);
    const Section* lpSection = lpHull->GetSection(luSectionIndex);

    while (luParam != KU_INVALID_PARAM)
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

        const ParamListNode* lpNode = &maParamListNodes[luParam];

        CGS_ASSERT(luSectionIndex < lpHull->muNumSections, "luIndex < muNumSections");
        const f32 lfDistAlong =
            lpSection->CalcDistanceAlongSection(lpNode->mfParamAlong,
                                                static_cast<u32>(lpNode->mfParamAlong),
                                                lpHull->GetRungLengthsForSection(lpSection));

        const f32 lfDistToEnd = lpSection->mfLength - lfDistAlong;
        if (lfDistToEnd < lfDistanceFromEndOfLane)
        {
            *lpfOutDistance = lfDistToEnd;
            return luParam;
        }

        luParam = lpNode->muNextParam;
    }

    return KU_INVALID_PARAM;
}

// ----------------------------------------------------------------------------
// The behaviour pre-pass and its helpers. PrecalcBehaviourParams and FindNearestParamInFront
// are landed below; UpdateParam_CheckIfNeedToSlow -- the only caller of Precalc -- is still
// gated, so maParamNeedToSlowData keeps Clear()'s miBehaviour -1 in this build and
// UpdateParams_UpdateBehaviour skips its copy, leaving Param::Initialise's
// KI_BEHAVIOUR_NORMAL standing.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_PrecalcBehaviourParams  @0x82717C48 (.cpp 11415..11667)
//
// Runs the fuzzy rule base for one param and turns the winning score into the
// ParamNeedToSlowData that UpdateParams_UpdateBehaviour copies into the Param. The three
// Vector4s are ProcessParamRules' inputs lane for lane (DWARF locals at .cpp 11476/11489/
// 11507/11523 name every lane this switch reads):
//   lConeA = RCDistance, RCHeight, RCClosingSpeed, RCLanePos
//   lConeB = TLDistance, NPDistance, NPClosingSpeed, RCSpeedInOurLane
//   lConeC = TimeQueueing, Obstructedness, DriveAroundStickiness, W
// mTweakValues is at X360 +0x72710, so the three mega-tweek floats this function loads
// (+0x72710 / +0x72714 / +0x72718) are indices 0/1/2: stopline variation, race-car stop
// distance, gap-closing factor.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_PrecalcBehaviourParams(u32 luParam,
                                                              const Section* lpSection,
                                                              const Hull* lpHull,
                                                              Vector4 lConeA,
                                                              Vector4 lConeB,
                                                              Vector4 lConeC)
{
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
    (void)lpHull;   // r6 is on the console signature; the body never reads it

    Param*               lpParam      = &maParams[luParam];
    ParamNeedToSlowData* lpNeedToSlow = &maParamNeedToSlowData[luParam];

    if (lpParam->miBehaviour == 0)
    {
        // GATE: the sympathetic-crash arm @0x82717CE4..0x82717D4C.
        // BLOCKER: GetSympCrashingTargetPos @0x82708C10 is an ARTIST export hole (no body).
        // DELETE-WHEN it lands. This takes its false arm, which is the console's own path
        // for a param with no reachable crash target.
        static bool sbLoggedSymp = false;
        LogMissingLeg(sbLoggedSymp,
                      "UpdateParams_PrecalcBehaviourParams @0x82717C48 sympathetic-crash arm -- "
                      "GetSympCrashingTargetPos @0x82708C10 is an export hole with no body");

        lpNeedToSlow->miBehaviour = Param::KI_BEHAVIOUR_NORMAL;   // 0x82717D50
    }

    if (mbAllowDivergentBehaviour)
    {
        const Vehicle* lpVehicle = GetVehicle(luParam);
        if (lpVehicle->IsAlive() && lpVehicle->IsPhysical())
        {
            const Vehicle::Manoeuvre leManoeuvre = lpVehicle->GetCurrentManoeuvre();

            if (leManoeuvre == Vehicle::E_MANOEUVRE_GIVE_UP)
            {
                // 0x82717DE0..0x82717EB8 -- a given-up car close enough to its param just stops.
                const Vector3 lVehiclePos = GetVehicleTransform(luParam).Pos();
                const Vector3 lDiff       = GetParamTransform(luParam)->GetLerpedPos() - lVehiclePos;

                if (Magnitude(lDiff) <= KF_PARAM_GIVE_UP_STOP_RADIUS)
                {
                    lpNeedToSlow->miBehaviour   = Param::KI_BEHAVIOUR_NORMAL;
                    lpNeedToSlow->mfStopDist    = 0.0f;
                    lpNeedToSlow->mfTargetSpeed = 0.0f;
                    return;
                }
            }
            else if (leManoeuvre == Vehicle::E_MANOEUVRE_STUCK_REVERSE)
            {
                // 0x82717DA8..0x82717DDC
                lpNeedToSlow->miBehaviour   = 2;
                lpNeedToSlow->mfStopDist    = KF_PARAM_DRIVE_AROUND_STOP_DIST;
                lpNeedToSlow->mfTargetSpeed =
                    (lpParam->mfRandomVal + 1.0f) * KF_PARAM_DRIVE_AROUND_SPEED_SCALE;
                return;
            }
        }
    }

    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

    // 0x82717EDC..0x82717F24 -- cache the param position for the debug render, then score.
    mFuzzyBehaviours.DEBUGSetCurrentParamPos(GetParamTransform(luParam)->GetDeterministicPos());

    VecFloat lafScores[KU_PARAM_NUM_BEHAVIOUR_SCORES];
    mFuzzyBehaviours.ProcessParamRules(lafScores, lConeA, lConeB, lConeC);

    {
        // GATE: DEBUG_AddFuzzyLogicData @0x82716040 (0x82717F34).
        // BLOCKER: ARTIST export hole; it only fills the mpaDEBUGVehicleFuzzyLogic ring.
        // DELETE-WHEN it lands. No scoring consequence.
        static bool sbLoggedDebug = false;
        LogMissingLeg(sbLoggedDebug,
                      "UpdateParams_PrecalcBehaviourParams DEBUG_AddFuzzyLogicData @0x82716040 -- "
                      "export hole, debug ring only");
    }

    if (mbAtStartLineSoProtectRaceCarsFromTraffic)
    {
        lafScores[0] = SplatLane(0.0f);   // 0x82717F4C
    }

    // 0x82717F58..0x827180F8 -- the crash-slider cap, or the junction-FUP / physical-traffic
    // suppression of the drive-around score when the slider is not running.
    const bool lbCapScores =
        mbDEBUGTestSympCrash ||
        (mfCrashSliderFinalValue > KF_PARAM_CRASH_SLIDER_MIN_VALUE && mbAllowDivergentBehaviour);

    if (lbCapScores)
    {
        const VecFloat lfCap = SplatLane(1.0f - mfCrashSliderFinalValue);

        lafScores[1] = Min(lfCap, lafScores[1]);
        lafScores[0] = Min(lfCap, lafScores[0]);
        lafScores[2] = Min(lfCap, lafScores[2]);

        if (meGameMode != -1)
        {
            lafScores[3] = SplatLane(0.0f);
        }
    }
    else if (!NeedToTakeActionAgainstJunctionFUP() &&
             maTrafficPhysicsInfoListBits.CountSetBits() >= KU_PARAM_BUSY_PHYSICAL_TRAFFIC_COUNT)
    {
        lafScores[0] = SplatLane(0.0f);
    }

    // 0x827180FC..0x8271816C -- pick the highest score; a winner has to beat the running best
    // by more than KF_PARAM_ACTION_SCORE_EPSILON, which is added to the best as it is taken.
    s32 liNewAction = 0;
    f32 lfBestScore = 0.0f;
    for (u32 luScore = 0; luScore < KU_PARAM_NUM_BEHAVIOUR_SCORES; ++luScore)
    {
        const f32 lfScore = lafScores[luScore].x;
        if (lfScore > lfBestScore)
        {
            liNewAction = static_cast<s32>(luScore);
            lfBestScore = lfScore + KF_PARAM_ACTION_SCORE_EPSILON;
        }
    }

    CGS_ASSERT(liNewAction >= 0, "liNewAction >= 0");

    const f32 lfLaneSpeed = mfSpeedMultiplier * lpSection->mfSpeed;   // +0x72880 * section+0x24

    switch (liNewAction)
    {
    case 0:   // DRIVE_AROUND_OBSTRUCTION (0x827181E8)
        CGS_ASSERT(mbAllowDivergentBehaviour, "Decided to DRIVE_AROUND_OBSTRUCTION online");

        lpNeedToSlow->miBehaviour   = 2;
        lpNeedToSlow->mfStopDist    = KF_PARAM_DRIVE_AROUND_STOP_DIST;
        lpNeedToSlow->mfTargetSpeed =
            (lpParam->mfRandomVal + 1.0f) * KF_PARAM_DRIVE_AROUND_SPEED_SCALE;
        break;

    case 1:   // STOP_FOR_PLAYER (0x8272725C)
    {
        CGS_ASSERT(mbAllowDivergentBehaviour || mbAtStartLineSoProtectRaceCarsFromTraffic,
                   "Decided to STOP_FOR_PLAYER online");

        const f32 lfRCDistance        = lConeA.x;
        const f32 lfStopForPlayerDist = lfRCDistance - mTweakValues.GetRaceCarStopDist();

        lpNeedToSlow->mfTargetSpeed = 0.0f;
        lpNeedToSlow->miBehaviour   = 1;
        lpNeedToSlow->mfStopDist    = (lfStopForPlayerDist > 0.0f) ? lfStopForPlayerDist : 0.0f;
        break;
    }

    case 2:   // FOLLOW_PLAYER (0x827182FC)
    {
        CGS_ASSERT(mbAllowDivergentBehaviour || mbAtStartLineSoProtectRaceCarsFromTraffic,
                   "Decided to FOLLOW_PLAYER online");

        const f32 lfRCDistance       = lConeA.x;
        const f32 lfNPDistance       = lConeB.y;
        const f32 lfRCSpeedInOurLane = lConeB.w;

        const f32 lfRaw              = lfRCDistance - mTweakValues.GetRaceCarStopDist();
        const f32 lfFollowPlayerDist = (lfRaw > 0.0f) ? lfRaw : 0.0f;
        const f32 lfTargetSpeed1     = lfNPDistance * mTweakValues.GetGapClosingFactor();
        const f32 lfTargetSpeed2     = (lpParam->mfSpeed < lfRCSpeedInOurLane)
                                           ? lpParam->mfSpeed
                                           : lfRCSpeedInOurLane;
        const f32 lfTargetSpeed      = (lfTargetSpeed2 > lfTargetSpeed1) ? lfTargetSpeed2
                                                                        : lfTargetSpeed1;

        lpNeedToSlow->miBehaviour   = 3;
        lpNeedToSlow->mfStopDist    = lfFollowPlayerDist;
        lpNeedToSlow->mfTargetSpeed = (lfTargetSpeed < lfLaneSpeed) ? lfTargetSpeed : lfLaneSpeed;
        break;
    }

    case 3:   // STOP AT THE STOPLINE (0x82718494)
    {
        const f32 lfTLDistance = lConeB.x;

        lpNeedToSlow->mfTargetSpeed = 0.0f;
        lpNeedToSlow->miBehaviour   = 4;
        lpNeedToSlow->mfStopDist =
            lfTLDistance - mTweakValues.GetStoplineVariation() * lpParam->mfRandomVal;
        break;
    }

    case 4:   // QUEUE BEHIND THE PARAM IN FRONT (0x82718430)
    {
        const Param* lpNextParam = GetParam(lpNeedToSlow->muParamInFront);

        const f32 lfNPDistance   = lConeB.y;
        const f32 lfTargetSpeed1 = mTweakValues.GetGapClosingFactor() * lfNPDistance;
        const f32 lfTargetSpeed2 = (lpParam->mfSpeed < lpNextParam->mfSpeed)
                                       ? lpParam->mfSpeed
                                       : lpNextParam->mfSpeed;
        const f32 lfTargetSpeed  = (lfTargetSpeed2 > lfTargetSpeed1) ? lfTargetSpeed2
                                                                     : lfTargetSpeed1;

        lpNeedToSlow->miBehaviour   = 5;
        lpNeedToSlow->mfStopDist    = lfNPDistance;
        lpNeedToSlow->mfTargetSpeed = (lfTargetSpeed < lfLaneSpeed) ? lfTargetSpeed : lfLaneSpeed;
        break;
    }

    case 5:   // NORMAL (0x827184D8)
        lpNeedToSlow->miBehaviour   = Param::KI_BEHAVIOUR_NORMAL;
        lpNeedToSlow->mfTargetSpeed = lfLaneSpeed;
        lpNeedToSlow->mfStopDist    = KF_PARAM_NO_STOP_DIST;
        break;

    default:
        CGS_ASSERT(false, "Unknown param action");
        break;
    }
}

// GATE: UpdateParam_CheckIfNeedToSlow @0x82738468 (.cpp 11110..11167), the only caller of
// UpdateParams_PrecalcBehaviourParams. BLOCKERS, all four re-derived from its listing:
//   (1) unk_82CDA3C0 / unk_82CDA400 / unk_8327F140 are vperm CONTROL vectors (0x82738...
//       vperm128 v11,v121,v120,v7 etc). They decide which accumulated scalar lands in which
//       ProcessParamRules input lane, and no lane assignment is derivable without them.
//   (2) CalcRaceCarOnStartGridFuzzyScores @0x82716F10 is exported but unreconstructed and
//       undeclared in this tree (the start-line arm).
//   (3) BrnTraffic::IsPointWithinSquishedCone is declared-only in BrnTrafficMathsUtils.h
//       (inlined at every ARTIST call site), and it is the per-physical-vehicle test.
//   (4) DoesParamNeedToStopForStopline @0x827249F8 below is itself gated on the opaque
//       BrnTraffic::StopLine record.
// DELETE-WHEN all four land. COST: maParamNeedToSlowData keeps Clear()'s miBehaviour -1, so
// no param ever queues, stops at a stopline or slows for the player.
void TrafficEntityModule::UpdateParam_CheckIfNeedToSlow(
        u32 luParam,
        const Hull* lpHull,
        u32 luSectionIndex,
        const Section* lpSection,
        const ::Array<PhysicalVehicleInfo, KU_MAX_PHYSICAL_VEHICLES_TO_CACHE>* lpaPhysicalVehicles)
{
    (void)luParam; (void)lpHull; (void)luSectionIndex; (void)lpSection; (void)lpaPhysicalVehicles;

    static bool sbLogged = false;
    LogMissingLeg(sbLogged,
                  "UpdateParam_CheckIfNeedToSlow @0x82738468 -- the unk_82CDA3C0 / unk_82CDA400 "
                  "/ unk_8327F140 vperm control vectors, CalcRaceCarOnStartGridFuzzyScores "
                  "@0x82716F10 (unreconstructed) and IsPointWithinSquishedCone (declared-only)");
}

// @0x82717A70 (.cpp 11335). BLOCKER: unk_8300CAC0, a three-lane distance-squared threshold
// vector seeded by an unnamed dyn-init thunk.
void TrafficEntityModule::UpdateParam_CheckIfInsideParamInFront(u32 luParam)
{
    (void)luParam;

    static bool sbLogged = false;
    LogMissingLeg(sbLogged,
                  "UpdateParam_CheckIfInsideParamInFront @0x82717A70 -- unk_8300CAC0 lanes 0/1/2 "
                  "are an unrecovered dyn-init .data vector");
}

// @0x827249F8 (.cpp 11433). BLOCKER: the stopline record type is opaque (Hull::GetStopLine
// returns const void* in BrnTrafficHull.h), so the stopline parameter cannot be read by name.
bool TrafficEntityModule::DoesParamNeedToStopForStopline(u32 luParam,
                                                         u32 luSectionIndex,
                                                         const Section* lpSection,
                                                         const Hull* lpHull,
                                                         f32* lpfOutStopDist) const
{
    (void)luParam; (void)luSectionIndex; (void)lpSection; (void)lpHull;

    static bool sbLogged = false;
    LogMissingLeg(sbLogged,
                  "DoesParamNeedToStopForStopline @0x827249F8 -- BrnTraffic::StopLine is "
                  "forward-declared only and Hull::GetStopLine returns const void*");

    if (lpfOutStopDist != 0)
    {
        *lpfOutStopDist = 0.0f;
    }
    return false;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::FindNearestParamInFront  @0x82725060  (.cpp 11855..12070)
//
// Nearest param ahead of luParam within lfMaxDist, in four passes: the param's own list
// successor, every forward section, those sections' own forward sections (the "extra" list,
// up to 9) and their merging (backward) sections (up to 3). Each candidate scores as its
// mfBackDist plus the arc distance to it.
// ----------------------------------------------------------------------------
u32 TrafficEntityModule::FindNearestParamInFront(u32 luParam, f32 lfMaxDist,
                                                 f32* lpfOutDist) const
{
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_STANDARD_TRAFFIC");
    CGS_ASSERT(lpfOutDist != 0, "lpfOutDistance");

    u32 luNearestParam = KU_INVALID_PARAM;
    f32 lfNearestDist  = lfMaxDist;

    const Param*   lpParam   = &maParams[luParam];
    const u32      luHull    = lpParam->muHullIndex;
    const u32      luSection = lpParam->muSectionIndex;
    const Hull*    lpHull    = GetHull(luHull);
    const Section* lpSection = lpHull->GetSection(luSection);

    // 1) the list successor, if it is still on our own section (0x827251B0).
    const u32 luNextParam = maParamListNodes[luParam].muNextParam;
    if (luNextParam != KU_INVALID_PARAM)
    {
        const Param* lpNextParam = &maParams[luNextParam];
        if (lpNextParam->muHullIndex == lpParam->muHullIndex &&
            lpNextParam->muSectionIndex == lpParam->muSectionIndex)
        {
            const f32 lfDistance = lpSection->CalcSignedDistanceAlongSection(
                lpParam->mfParamAlong, lpParam->muCurrentSegment,
                lpNextParam->mfParamAlong, lpNextParam->muCurrentSegment,
                lpHull->GetRungLengthsForSection(lpSection));

            CGS_ASSERT(lfDistance >= 0.0f, "lfDistance >= 0.0f");

            if ((lpNextParam->mfBackDist + lfDistance) < lfMaxDist)
            {
                lfNearestDist  = lpNextParam->mfBackDist + lfDistance;
                luNearestParam = luNextParam;
            }
        }
    }

    // Distance from us to the end of our own lane; every later candidate measures from it.
    const f32 lfDistToEndOfSection =
        lpSection->mfLength -
        lpSection->CalcDistanceAlongSection(lpParam->mfParamAlong, lpParam->muCurrentSegment,
                                            lpHull->GetRungLengthsForSection(lpSection));

    if (lfDistToEndOfSection < lfMaxDist)
    {
        u16 lauExtraHullsToCheck[KU_FIND_NEAREST_MAX_EXTRA_SECTIONS];
        u8  lauExtraSectionsToCheck[KU_FIND_NEAREST_MAX_EXTRA_SECTIONS];
        f32 lafExtraDistances[KU_FIND_NEAREST_MAX_EXTRA_SECTIONS];
        u32 luNumExtraSectionsToCheck = 0;

        u16 lauMergingHullsToCheck[KU_FIND_NEAREST_MAX_MERGING_SECTIONS];
        u8  lauMergingSectionsToCheck[KU_FIND_NEAREST_MAX_MERGING_SECTIONS];
        u32 luNumMergingSectionsToCheck = 0;

        // 2) every forward section of our own (0x82725238..0x827254B0).
        for (u32 luForward = 0; luForward < 3; ++luForward)
        {
            const u32 luNextHull = lpSection->mauForwardHulls[luForward];
            if (luNextHull == KU_INVALID_HULL)
            {
                continue;
            }

            const u32 luNextSection = lpSection->mauForwardSections[luForward];
            CGS_ASSERT(luNextSection != KU_INVALID_SECTION, "luNextSection != KU_INVALID_SECTION");

            const HullRuntime* lpNextHullRuntime = GetHullRuntimeSafe(luNextHull);
            if (lpNextHullRuntime != 0)
            {
                const u32 luFirstParam = lpNextHullRuntime->GetFirstParamInSection(luNextSection);
                if (luFirstParam != KU_INVALID_PARAM)
                {
                    const Hull*    lpFwdHull2    = GetHull(luNextHull);
                    const Section* lpFwdSection2 = lpFwdHull2->GetSection(luNextSection);
                    const Param*   lpFwdParam    = &maParams[luFirstParam];

                    const f32 lfDistance =
                        lpFwdSection2->CalcDistanceAlongSection(
                            lpFwdParam->mfParamAlong, lpFwdParam->muCurrentSegment,
                            lpFwdHull2->GetRungLengthsForSection(lpFwdSection2)) +
                        lfDistToEndOfSection;

                    CGS_ASSERT(lfDistance >= 0.0f, "lfDistance >= 0.0f");

                    if ((lpFwdParam->mfBackDist + lfDistance) < lfNearestDist)
                    {
                        lfNearestDist  = lpFwdParam->mfBackDist + lfDistance;
                        luNearestParam = luFirstParam;
                    }
                }
            }

            const Hull*    lpFwdHull    = GetHull(luNextHull);
            const Section* lpFwdSection = lpFwdHull->GetSection(luNextSection);

            // 2a) book that section's own forward sections for pass 3.
            if ((lpFwdSection->mfLength + lfDistToEndOfSection) < lfMaxDist)
            {
                for (u32 luExtra = 0; luExtra < 3; ++luExtra)
                {
                    if (lpFwdSection->mauForwardHulls[luExtra] == KU_INVALID_HULL)
                    {
                        continue;
                    }

                    CGS_ASSERT(luNumExtraSectionsToCheck < KU_FIND_NEAREST_MAX_EXTRA_SECTIONS,
                               "luNumExtraSectionsToCheck < KU_FIND_NEAREST_MAX_EXTRA_SECTIONS");

                    lauExtraHullsToCheck[luNumExtraSectionsToCheck] =
                        lpFwdSection->mauForwardHulls[luExtra];
                    lauExtraSectionsToCheck[luNumExtraSectionsToCheck] =
                        lpFwdSection->mauForwardSections[luExtra];
                    lafExtraDistances[luNumExtraSectionsToCheck] =
                        lpFwdSection->mfLength + lfDistToEndOfSection;
                    ++luNumExtraSectionsToCheck;
                }
            }

            // 2b) and its merging (backward) sections, but only from close in.
            if (lfDistToEndOfSection < KF_FIND_NEAREST_MERGE_MAX_DIST)
            {
                for (u32 luMerge = 0; luMerge < 3; ++luMerge)
                {
                    const u32 luMergerHull = lpFwdSection->mauBackwardHulls[luMerge];
                    if (luMergerHull == KU_INVALID_HULL)
                    {
                        continue;
                    }

                    const u32 luMergerSection = lpFwdSection->mauBackwardSections[luMerge];
                    CGS_ASSERT(luMergerSection != KU_INVALID_SECTION,
                               "luMergerSection != KU_INVALID_SECTION");
                    CGS_ASSERT(luNumMergingSectionsToCheck < KU_FIND_NEAREST_MAX_MERGING_SECTIONS,
                               "luNumMergingSectionsToCheck < KU_FIND_NEAREST_MAX_MERGING_SECTIONS");

                    if (luMergerSection != luSection || luMergerHull != luHull)
                    {
                        lauMergingHullsToCheck[luNumMergingSectionsToCheck] =
                            static_cast<u16>(luMergerHull);
                        lauMergingSectionsToCheck[luNumMergingSectionsToCheck] =
                            static_cast<u8>(luMergerSection);
                        ++luNumMergingSectionsToCheck;
                    }
                }
            }
        }

        // 3) the booked extra sections (0x827254D0..0x8272573C).
        for (u32 luExtra = 0; luExtra < luNumExtraSectionsToCheck; ++luExtra)
        {
            const u32 luExtraHull    = lauExtraHullsToCheck[luExtra];
            const u32 luExtraSection = lauExtraSectionsToCheck[luExtra];
            const f32 lfDistToThem   = lafExtraDistances[luExtra];

            const HullRuntime* lpExtraHullRuntime = GetHullRuntimeSafe(luExtraHull);
            if (lpExtraHullRuntime == 0)
            {
                continue;
            }

            const u32 luFirstParam = lpExtraHullRuntime->GetFirstParamInSection(luExtraSection);
            if (luFirstParam == KU_INVALID_PARAM)
            {
                continue;
            }

            const Hull*    lpExtraHull2   = GetHull(luExtraHull);
            const Section* lpExtraSection = lpExtraHull2->GetSection(luExtraSection);
            const Param*   lpExtraParam   = &maParams[luFirstParam];

            const f32 lfDistance =
                lpExtraSection->CalcDistanceAlongSection(
                    lpExtraParam->mfParamAlong, lpExtraParam->muCurrentSegment,
                    lpExtraHull2->GetRungLengthsForSection(lpExtraSection)) +
                lfDistToThem;

            CGS_ASSERT(lfDistance >= 0.0f, "lfDistance >= 0.0f");

            if ((lpExtraParam->mfBackDist + lfDistance) < lfNearestDist)
            {
                lfNearestDist  = lpExtraParam->mfBackDist + lfDistance;
                luNearestParam = luFirstParam;
            }
        }

        // 4) the merging sections (0x82725760..0x82725860). A param already stopped at a
        //    light (behaviour 4) below KF_FIND_NEAREST_MERGE_MIN_SPEED does not count.
        for (u32 luMerge = 0; luMerge < luNumMergingSectionsToCheck; ++luMerge)
        {
            const u32 luMergerHull    = lauMergingHullsToCheck[luMerge];
            const u32 luMergerSection = lauMergingSectionsToCheck[luMerge];

            CGS_ASSERT(!(luMergerHull == lpParam->muHullIndex &&
                         luMergerSection == lpParam->muSectionIndex),
                       "!( lauMergingHullsToCheck[luMergingSection] == lpParam->muHullIndex && "
                       "lauMergingSectionsToCheck[luMergingSection] == lpParam->muSectionIndex )");

            f32 lfDistFromEnd = 0.0f;
            const u32 luMergingParam = FindFirstParamAfterPos(luMergerHull, luMergerSection,
                                                              lfDistToEndOfSection,
                                                              &lfDistFromEnd);
            if (luMergingParam == KU_INVALID_PARAM)
            {
                continue;
            }

            const f32 lfDistance = lfDistToEndOfSection - lfDistFromEnd;
            CGS_ASSERT(lfDistance >= 0.0f, "lfDistance >= 0.0f");

            const Param* lpMergingParam = &maParams[luMergingParam];
            if ((lpMergingParam->mfBackDist + lfDistance) < lfNearestDist &&
                (lpMergingParam->miBehaviour != 4 ||
                 lpMergingParam->mfSpeed >= KF_FIND_NEAREST_MERGE_MIN_SPEED))
            {
                lfNearestDist  = lpMergingParam->mfBackDist + lfDistance;
                luNearestParam = luMergingParam;
            }
        }
    }

    *lpfOutDist = lfNearestDist;
    return luNearestParam;
}
}
