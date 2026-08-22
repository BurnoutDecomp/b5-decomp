// ============================================================================
// BrnTrafficEntityModule_wT2_03.cpp -- per-param behaviour, speed and THE ADVANCE.
//
//   TrafficEntityModule::UpdateParams_UpdatePlan          @0x82737CE8  PARTIAL
//   TrafficEntityModule::UpdateParams_UpdateBehaviour     @0x82716C90
//   TrafficEntityModule::UpdateParams_CalcDesiredSpeed    @0x82717928
//   TrafficEntityModule::UpdateParams_CalcAcceleration    @0x827172B8  PARTIAL
//   TrafficEntityModule::UpdateParams_IncrementParam      @0x82738C80
//   TrafficEntityModule::UpdateParams_HandleLaneChanges   @0x82725880  PARTIAL
//   TrafficEntityModule::UpdateParam_CheckIfInsideParamInFront @0x82717A70  GATED
//   TrafficEntityModule::UpdateParams_PrecalcBehaviourParams   @0x82717C48  PARTIAL
//   TrafficEntityModule::UpdateParam_CheckIfNeedToSlow    @0x82738468  PARTIAL
//   TrafficEntityModule::DoesParamNeedToStopForStopline   @0x827249F8
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
#include "SharedClasses/Traffic/Junctions/BrnTrafficStopLine.h"  // StopLine
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMathsUtils.h" // IsPointWithinSquishedCone
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficPhysicalVehicleInfo.h"

#include "rw/math/vpu/vector3_operation.h"                      // Magnitude
#include "rw/math/vpu/vector4_operation.h"                      // Min/Max on Vector4

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cmath>     // std::floor, std::sqrt
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

    // UpdateParam_CheckIfNeedToSlow @0x82738468 / DoesParamNeedToStopForStopline @0x827249F8.
    const f32 KF_STOP_LINE_REACTION_DISTANCE      = 40.0f;   // flt_820BA590
    const f32 KF_SECTION_POSITION_TOLERANCE       = -0.1f;   // the "negative position" asserts
    const f32 KF_PARAM_NEXT_PARAM_TIME_THRESHOLD  = 2.0f;    // flt_820BA86C
    const f32 KF_PARAM_MIN_NEXT_PARAM_DIST        = 25.0f;   // flt_820BA4F0
    const f32 KF_PARAM_LOOKAHEAD_SCALE            = 1.5f;    // flt_820BA5DC
    const f32 KF_PARAM_AVOIDANCE_BIAS             = 3.0f;    // flt_820BA5F4 (also the space factor)
    const f32 KF_PARAM_TIME_QUEUEING_RANDOM_SCALE = 2.5f;    // flt_82005548
    const f32 KF_PARAM_DRIVE_AROUND_STICKINESS    = 0.15f;   // flt_820BA8D4
    // The "which physical vehicle is interesting to me" cone (DWARF locals
    // KF_PHYSICAL_INTEREST_CONE_ANGLE / _LENGTH / _RECIP_Y at .cpp 11061..11063).
    const f32 KF_PHYSICAL_INTEREST_CONE_ANGLE   = 0.70709997f; // flt_8200D514
    const f32 KF_PHYSICAL_INTEREST_CONE_LENGTH  = 30.0f;       // flt_820BA5E8
    const f32 KF_PHYSICAL_INTEREST_CONE_RECIP_Y = 2.5f;        // flt_82005548

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
// maParamNeedToSlowData into the Param, then ticks the queueing timer.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_UpdateBehaviour(u32 luParam)
{
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

    Param* lpParam = &maParams[luParam];
    const ParamNeedToSlowData* lpParamNeedToSlowData = GetParamNeedToSlowData(luParam);

    // 0x82716CE8 -- the console assert is `(u8)miBehaviour < 0x80`, i.e. miBehaviour >= 0. It
    // is REACHABLE here: UpdateParams_DoTimeSlicedLogic @0x82743FE8 leaves Clear()'s -1 in the
    // slot for any param that was dead or E_HISTORY_BORN when its 100-param slice ran
    // (0x82744968 / 0x82744978), and nothing in this tree clears E_HISTORY_BORN yet. Demoted
    // to a one-shot report + skip so it cannot break the round's 0-assert boot baseline; the
    // console would store the -1 through.
    // DELETE-WHEN the E_HISTORY_BORN clear lands and slice coverage is provable.
    if (lpParamNeedToSlowData->miBehaviour < 0)
    {
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            static bool sbWarnedNoBehaviour = false;
            if (!sbWarnedNoBehaviour)
            {
                sbWarnedNoBehaviour = true;
                *lpDiag << "[T3-behaviour] param " << static_cast<s32>(luParam)
                        << " reached UpdateBehaviour with miBehaviour -1 (its time slice has "
                           "not run since it became alive) -- console asserts here\n";
            }
        }
        return;
    }

    CGS_ASSERT(lpParamNeedToSlowData->miBehaviour < Param::KI_BEHAVIOURS_COUNT,
               "lpParamNeedToSlowData->miBehaviour < Param::E_BEHAVIOURS_COUNT"); // 0x82716D18

    // .cpp 10815 / 10818, message-streamed on console as
    // "Param <n> decided on divergent behaviour <b>".
    CGS_ASSERT(mbAllowDivergentBehaviour || mbAtStartLineSoProtectRaceCarsFromTraffic ||
                   (lpParamNeedToSlowData->miBehaviour != 0 &&
                    lpParamNeedToSlowData->miBehaviour != 1 &&
                    lpParamNeedToSlowData->miBehaviour != 3),
               "Param decided on divergent behaviour");
    CGS_ASSERT(mbAllowDivergentBehaviour || lpParamNeedToSlowData->miBehaviour != 2,
               "Param decided on divergent behaviour");

    lpParam->miBehaviour   = lpParamNeedToSlowData->miBehaviour;              // 0x82716EA8
    lpParam->mfStopDist    = lpParamNeedToSlowData->mfStopDist;               // 0x82716EB0
    lpParam->mfTargetSpeed = lpParamNeedToSlowData->mfTargetSpeed;            // 0x82716EB8

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
// The behaviour pre-pass and its helpers. PrecalcBehaviourParams, FindNearestParamInFront,
// UpdateParam_CheckIfNeedToSlow and DoesParamNeedToStopForStopline are all landed below, so
// maParamNeedToSlowData carries a real behaviour and UpdateParams_UpdateBehaviour copies it.
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

    // [T3-behaviour] DIAG. NOT IN THE X360 BINARY. Opt-in, first four params only: the six
    // fuzzy scores and the cone inputs behind one action pick. This is the probe that names
    // the culprit when the histogram goes one-sided -- an all-zero score vector elects action 0
    // by default (best starts at 0.0 and only `>` replaces it). DELETE-WHEN-STABLE.
    {
        static s32 siPrinted = 0;
        if (siPrinted < 4)
        {
            if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
            {
                ++siPrinted;
                *lpDiag << "[T3-behaviour] scores param " << static_cast<s32>(luParam)
                        << " action " << liNewAction
                        << " s=";
                for (u32 luScore = 0; luScore < KU_PARAM_NUM_BEHAVIOUR_SCORES; ++luScore)
                {
                    *lpDiag << " " << lafScores[luScore].x;
                }
                *lpDiag << " | coneA " << lConeA.x << " " << lConeA.y << " " << lConeA.z
                        << " " << lConeA.w
                        << " | coneB " << lConeB.x << " " << lConeB.y << " " << lConeB.z
                        << " " << lConeB.w
                        << " | coneC " << lConeC.x << " " << lConeC.y << " " << lConeC.z
                        << "\n";
            }
        }
    }

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

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParam_CheckIfNeedToSlow  @0x82738468  (.cpp 10979..11175)
//
// Builds the three fuzzy input vectors for one param and hands them to
// UpdateParams_PrecalcBehaviourParams. Lane names are the DWARF locals at .cpp 11028..11030.
//
// The three vperm control vectors the console uses are recovered, not guessed:
//   unk_82CDA3C0 = 00 01 02 03 | 00 01 02 03 | 00 01 02 03 | 14 15 16 17
//   unk_82CDA400 = 08 09 0A 0B | 1C 1D 1E 1F | 00 01 02 03 | 00 01 02 03
// with `vsldoi128 v126, v11, v13, 8` those two assemble
// {RCDistance, RCHeight, RCClosingSpeed, RCLanePos} -- i.e. cone A lane for lane.
// unk_8327F140 is the ENGINE-WIDE SetLane permute table. INFERRED, not read: the table is
// all zeros in the image (dyn-init, written at 0x82C741C8), so the lane mapping comes from
// FuzzyEnvelopeSet4::SetEnvelope @0x827526C0's `slwi r9, r28, 6` stride, not from the data.
// On that inference the +0x00 / +0x40 / +0x80 loads are SetLane<0/1/2> on cone C.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParam_CheckIfNeedToSlow(
        u32 luParam,
        const Hull* lpHull,
        u32 luSectionIndex,
        const Section* lpSection,
        const ::Array<PhysicalVehicleInfo, KU_MAX_PHYSICAL_VEHICLES_TO_CACHE>* lpaPhysicalVehicles)
{
    CGS_ASSERT(lpaPhysicalVehicles != 0, "lpPhysicalVehicleArray");      // .cpp 11110
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");      // .h 2350

    Param* const lpParam = &maParams[luParam];

    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");      // .h 2387
    const ParamTransform* const lpParamTransform = GetParamTransform(luParam);
    const Vector3 lParamPos = lpParamTransform->GetDeterministicPos();

    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");      // .cpp 11115
    ParamNeedToSlowData* const lpNeedToSlow = &maParamNeedToSlowData[luParam];

    // --- cone B lane 0: how far to the next red stop line -----------------------------
    f32 lfStoplineStopDist = KF_PARAM_NO_STOP_DIST;                      // seeded @0x8273857C
    DoesParamNeedToStopForStopline(luParam, luSectionIndex, lpSection, lpHull,
                                   &lfStoplineStopDist);

    // --- who is in front, and how far --------------------------------------------------
    const f32 lfSpeedLookahead = lpParam->mfSpeed * KF_PARAM_NEXT_PARAM_TIME_THRESHOLD;
    const f32 lfParamLookaheadDist =
        ((lfSpeedLookahead >= KF_PARAM_MIN_NEXT_PARAM_DIST) ? lfSpeedLookahead
                                                            : KF_PARAM_MIN_NEXT_PARAM_DIST) *
        KF_PARAM_LOOKAHEAD_SCALE;

    f32 lfNextParamDist = 0.0f;
    u32 luNextParam = FindNearestParamInFront(luParam, lfParamLookaheadDist, &lfNextParamDist);

    if (mbAllowDivergentBehaviour)
    {
        // 0x827385CC..0x82738728 -- a car that is recovering from a slam is something you
        // drive AROUND, not something you queue behind; likewise the debug-picked vehicle.
        if (luNextParam != KU_INVALID_PARAM &&
            mVehicleSoaData.mPhysicalVehiclesTryingToRecover.IsBitSet(luNextParam))
        {
            luNextParam = KU_INVALID_PARAM;
        }

        if (mbDEBUGPick_DontStopForPickedVehicle && luNextParam == muDEBUGPickedVehicle)
        {
            luNextParam = KU_INVALID_PARAM;
        }
    }

    lpNeedToSlow->muParamInFront  = static_cast<u16>(luNextParam);        // 0x82738738
    lpNeedToSlow->mfNextParamDist = lfNextParamDist;                      // 0x82738740

    f32 lfNPDistance     = KF_PARAM_NO_STOP_DIST;
    f32 lfNPClosingSpeed = KF_PARAM_NO_STOP_DIST;

    if (luNextParam != KU_INVALID_PARAM)
    {
        // 0x82738754..0x8273878C. One constant serves as both the avoidance bias and the
        // per-car random spacing factor.
        const f32 lfRaw = ((lfNextParamDist - lpParam->mfFrontDist) - KF_PARAM_AVOIDANCE_BIAS) -
                          lpParam->mfRandomVal * KF_PARAM_AVOIDANCE_BIAS;
        lfNPDistance     = (lfRaw >= 0.0f) ? lfRaw : 0.0f;
        lfNPClosingSpeed = lpParam->mfSpeed - GetParam(luNextParam)->mfSpeed;
    }

    // --- the race-car ("physical vehicle") scalars -------------------------------------
    // All five default to KF_MAX_FLOAT (the console loads this+0x72600 into v121/v120/v126/
    // v122/v125 at 0x82738794).
    f32 lfRCDistance       = KF_PARAM_NO_STOP_DIST;
    f32 lfRCHeight         = KF_PARAM_NO_STOP_DIST;
    f32 lfRCClosingSpeed   = KF_PARAM_NO_STOP_DIST;
    f32 lfRCLanePos        = KF_PARAM_NO_STOP_DIST;
    f32 lfRCSpeedInOurLane = KF_PARAM_NO_STOP_DIST;

    if (mbAllowDivergentBehaviour || mbAtStartLineSoProtectRaceCarsFromTraffic)
    {
        if (mbAtStartLineSoProtectRaceCarsFromTraffic)
        {
            // GATE: CalcRaceCarOnStartGridFuzzyScores @0x82716F10 (0x82738B18), the start-grid
            // replacement for the cone scan below. BLOCKER: 233 insns, unreconstructed, and it
            // only runs while the race is still on the grid.
            // DELETE-WHEN it lands. COST: on the grid the five race-car lanes stay KF_MAX_FLOAT,
            // so traffic scores NORMAL instead of protecting the grid.
            static bool sbLoggedStartGrid = false;
            LogMissingLeg(sbLoggedStartGrid,
                          "UpdateParam_CheckIfNeedToSlow @0x82738468 start-grid arm -- "
                          "CalcRaceCarOnStartGridFuzzyScores @0x82716F10 is unreconstructed");
        }
        else if (!(GetVehicle(luParam)->IsAlive() && GetVehicle(luParam)->IsExtremeSwerving()))
        {
            // 0x827388AC..0x82738AFC -- pick the physical vehicle inside our interest cone
            // with the smallest importance-weighted squared distance, then measure it.
            const VecFloat lfConeAngle  = SplatLane(KF_PHYSICAL_INTEREST_CONE_ANGLE);
            const VecFloat lfConeLength = SplatLane(KF_PHYSICAL_INTEREST_CONE_LENGTH);
            const VecFloat lfConeRecipY = SplatLane(KF_PHYSICAL_INTEREST_CONE_RECIP_Y);

            u32 luBestVehicle = 0xFFFFFFFFu;
            f32 lfBestScore   = KF_PARAM_NO_STOP_DIST;

            for (u32 luPhysicalVehicle = 0;
                 luPhysicalVehicle < lpaPhysicalVehicles->GetLength();
                 ++luPhysicalVehicle)
            {
                const PhysicalVehicleInfo& lInfo = (*lpaPhysicalVehicles)[luPhysicalVehicle];

                Vector3 lTargetPos;
                lTargetPos.x = lInfo.mPositionAndImportance.x;
                lTargetPos.y = lInfo.mPositionAndImportance.y;
                lTargetPos.z = lInfo.mPositionAndImportance.z;
                lTargetPos.w = 0.0f;

                if (!IsPointWithinSquishedCone(lpParamTransform->GetDeterministicPos(),
                                               lpParamTransform->GetDirection(),
                                               lfConeAngle, lfConeLength, lfConeRecipY,
                                               lTargetPos))
                {
                    continue;
                }

                const Vector3 lDiff = lTargetPos - lpParamTransform->GetDeterministicPos();
                const f32 lfImportance = lInfo.mPositionAndImportance.w;     // vspltw v0,v0,3
                const f32 lfScore = rw::math::vpu::Dot(lDiff, lDiff) * lfImportance;

                if (lfBestScore > lfScore)
                {
                    lfBestScore   = lfScore;
                    luBestVehicle = luPhysicalVehicle;
                }
            }

            if (luBestVehicle != 0xFFFFFFFFu)
            {
                const PhysicalVehicleInfo& lInfo = (*lpaPhysicalVehicles)[luBestVehicle];

                Vector3 lInfoPos;
                lInfoPos.x = lInfo.mPositionAndImportance.x;
                lInfoPos.y = lInfo.mPositionAndImportance.y;
                lInfoPos.z = lInfo.mPositionAndImportance.z;
                lInfoPos.w = 0.0f;

                const Vector3 lDiff = lInfoPos - lpParamTransform->GetDeterministicPos();

                lfRCDistance = rw::math::vpu::Dot(lDiff, lpParamTransform->GetDirection());
                lfRCHeight   = rw::math::vpu::Dot(lDiff, lpParamTransform->CalcUp());

                // 0x82738A48..0x82738A84 -- the lateral offset, scaled up as the race car's
                // own right vector lines up with ours: 0.5 + 0.5 * |alignment|.
                const f32 lfAlignment =
                    rw::math::vpu::Dot(lpParamTransform->GetRight(), lInfo.mRight);
                const f32 lfAlignScale =
                    0.5f + 0.5f * ((lfAlignment < 0.0f) ? -lfAlignment : lfAlignment);
                lfRCLanePos =
                    rw::math::vpu::Dot(lDiff, lpParamTransform->GetRight()) * lfAlignScale;

                // 0x82738A94..0x82738AF0 -- closing speed along the unit separation.
                const Vector3 lParamLinearVel =
                    lpParamTransform->GetDirection() * lpParamTransform->GetSpeed();
                const Vector3 lClosingVel = lParamLinearVel - lInfo.mLinearVelocity;

                // FLAG (host guard, no console equivalent): the console does the raw rsqrt and
                // lets a coincident vehicle produce a NaN closing speed. Zero kept here instead.
                const f32 lfDistSq = rw::math::vpu::Dot(lDiff, lDiff);
                if (lfDistSq > 0.0f)
                {
                    const Vector3 lUnitDiff = lDiff * (1.0f / std::sqrt(lfDistSq));
                    lfRCClosingSpeed = rw::math::vpu::Dot(lUnitDiff, lClosingVel);
                }

                lfRCSpeedInOurLane =
                    rw::math::vpu::Dot(lpParamTransform->GetDirection(), lInfo.mLinearVelocity);
            }
        }
    }

    // --- cone C: queueing / obstructedness / drive-around stickiness -------------------
    const s8 liBehaviour = lpParam->miBehaviour;                              // Param+0x1B

    // 0x82738B54 -- the raw queueing timer, biased by the car's own random value.
    const f32 lfTimeQueueing =
        lpParam->mfTimeQueueing + lpParam->mfRandomVal * KF_PARAM_TIME_QUEUEING_RANDOM_SCALE;

    // 0x82738BD0 -- SetLane<2>: a car already driving around an obstruction wants to keep
    // doing so.
    const f32 lfDriveAroundStickiness =
        (liBehaviour == 2) ? KF_PARAM_DRIVE_AROUND_STICKINESS : 0.0f;

    // 0x82738BF0..0x82738C40 -- SetLane<1>: we count as obstructed while we are stopping for
    // or following the player, or while queueing behind someone who is.
    bool lbObstructed = (liBehaviour == 1) || (liBehaviour == 3);
    if (!lbObstructed && liBehaviour == 5 && luNextParam != KU_INVALID_PARAM)
    {
        const s32 liNextParamBehaviour = GetParam(luNextParam)->miBehaviour;
        lbObstructed = (liNextParamBehaviour == 1) || (liNextParamBehaviour == 3);
    }

    Vector4 lConeA;
    lConeA.x = lfRCDistance;
    lConeA.y = lfRCHeight;
    lConeA.z = lfRCClosingSpeed;
    lConeA.w = lfRCLanePos;

    Vector4 lConeB;
    lConeB.x = lfStoplineStopDist;
    lConeB.y = lfNPDistance;
    lConeB.z = lfNPClosingSpeed;
    lConeB.w = lfRCSpeedInOurLane;

    Vector4 lConeC;
    lConeC.x = lfTimeQueueing;
    lConeC.y = lbObstructed ? 1.0f : 0.0f;
    lConeC.z = lfDriveAroundStickiness;
    lConeC.w = KF_PARAM_NO_STOP_DIST;   // lane 3 of KF_MAX_FLOAT survives the three SetLanes

    (void)lParamPos;   // the console loads it (0x82738530) but only the transform is read on

    UpdateParams_PrecalcBehaviourParams(luParam, lpSection, lpHull, lConeA, lConeB, lConeC);
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

// ----------------------------------------------------------------------------
// TrafficEntityModule::DoesParamNeedToStopForStopline  @0x827249F8  (.cpp 11690..11840)
//
// True when a RED stop line lies within KF_STOP_LINE_REACTION_DISTANCE ahead of the param,
// with the distance to it in *lpfOutStopDist. The param caches its next stop line
// (muNextStopLineIndex / mfNextStopLineParam) and only re-scans once it has driven past it.
// Structure matches the Feb-2007 leak (BrnTrafficEntityModule.cpp:6116) except for the
// divergent-behaviour head arm, which retail adds.
// ----------------------------------------------------------------------------
bool TrafficEntityModule::DoesParamNeedToStopForStopline(u32 luParam,
                                                         u32 luSectionIndex,
                                                         const Section* lpSection,
                                                         const Hull* lpHull,
                                                         f32* lpfOutStopDist) const
{
    CGS_ASSERT(lpHull->GetSection(luSectionIndex) == lpSection,
               "lpHull->GetSection( luSection ) == lpSection");             // .cpp 11698
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");         // .h 2365

    Param* const lpParam = const_cast<Param*>(&maParams[luParam]);

    // 0x82724A90..0x82724AE8 -- a slammed or extreme-swerving car ignores stop lines.
    if (mbAllowDivergentBehaviour)
    {
        const Vehicle* const lpVehicle = GetVehicle(luParam);
        if (lpVehicle->IsAlive() &&
            (lpVehicle->IsRecoveringFromSlam() || lpVehicle->IsExtremeSwerving()))
        {
            return false;
        }
    }

    u32 luStopline      = lpParam->muNextStopLineIndex;
    f32 lfStoplineParam = lpParam->mfNextStopLineParam;
    u32 luStoplineSegment;

    if (luStopline == KU_UNKNOWN_STOPLINE || lfStoplineParam <= lpParam->mfParamAlong)
    {
        luStopline = lpSection->FindNextStopLineIndex(lpParam->mfParamAlong, lpHull);

        if (luStopline == KU_INVALID_STOPLINE)
        {
            lfStoplineParam   = KF_PARAM_NO_STOP_DIST;
            luStoplineSegment = 0xFFFFFFFFu;
        }
        else
        {
            const StopLine* const lpStopline = lpHull->GetStopLine(luStopline);
            lfStoplineParam   = StopLine::ConvertToFloat(lpStopline->GetParameterAlongSection());
            luStoplineSegment = lpStopline->GetSegmentAlongSection();
        }

        lpParam->mfNextStopLineParam = lfStoplineParam;
        lpParam->muNextStopLineIndex = static_cast<u8>(luStopline);
    }
    else
    {
        // 0x82724B70 -- the cached param is the 8.8 value in float form, so its integer part
        // is the segment (the console re-reads the same stack slot's high word).
        luStoplineSegment = static_cast<u32>(lfStoplineParam);
    }

    CGS_ASSERT(lpHull->GetSection(luSectionIndex) == lpSection,
               "lpHull->GetSection( luSection ) == lpSection");             // .cpp 11751

    const f32* const lpafRungLengths = lpHull->GetRungLengthsForSection(lpSection);

    f32 lfStopLineAlongSection = 0.0f;

    if (luStopline != KU_INVALID_STOPLINE)
    {
        CGS_ASSERT(luStopline != KU_UNKNOWN_STOPLINE, "luStopline != KU_UNKNOWN_STOPLINE");

        const HullRuntime* const lpHullRuntime = GetHullRuntime(lpParam->muHullIndex);
        if (!lpHullRuntime->IsStoplineRed(luStopline))
        {
            return false;
        }

        lfStopLineAlongSection =
            lpSection->CalcDistanceAlongSection(lfStoplineParam, luStoplineSegment, lpafRungLengths);
        CGS_ASSERT(lfStopLineAlongSection >= KF_SECTION_POSITION_TOLERANCE,
                   "Stopline is at a negative position along is section");  // .cpp 11764
    }
    else
    {
        // 0x82724D04 -- nothing on this section, so look one section down each queued plan.
        const f32 lfDistFromEndOfSection =
            lpSection->mfLength -
            lpSection->CalcDistanceAlongSection(lpParam->mfParamAlong, lpParam->muCurrentSegment,
                                                lpafRungLengths);
        CGS_ASSERT(lfDistFromEndOfSection >= KF_SECTION_POSITION_TOLERANCE,
                   "Param is well beyond the end of its section");          // .cpp 11770

        if (lfDistFromEndOfSection >= KF_STOP_LINE_REACTION_DISTANCE)
        {
            return false;
        }

        for (u32 luPlan = 0; luPlan < KU_PARAM_NUM_PLANS; ++luPlan)
        {
            const ParamPlan* const lpPlan = GetParamPlan(luParam, luPlan);
            CGS_ASSERT(lpPlan != 0, "lpPlan");                              // .cpp 11781

            if (lpPlan->muType != ParamPlan::E_TYPE_CHANGE_SECTION ||
                lpPlan->mChangeSectionData.muNewHull == KU_INVALID_HULL)
            {
                if (lpPlan->muType == ParamPlan::E_TYPE_NONE)
                {
                    return false;
                }
                continue;
            }

            const Hull* const    lpNextHull    = GetHull(lpPlan->mChangeSectionData.muNewHull);
            const Section* const lpNextSection =
                lpNextHull->GetSection(lpPlan->mChangeSectionData.muNewSection);

            luStopline = lpNextSection->FindNextStopLineIndex(0.0f, lpNextHull);
            if (luStopline == KU_INVALID_STOPLINE)
            {
                continue;
            }

            const HullRuntime* const lpNextHullRuntime =
                GetHullRuntimeSafe(lpPlan->mChangeSectionData.muNewHull);
            if (lpNextHullRuntime == 0 || !lpNextHullRuntime->IsStoplineRed(luStopline))
            {
                return false;
            }

            const StopLine* const lpStopline = lpNextHull->GetStopLine(luStopline);
            lfStoplineParam = StopLine::ConvertToFloat(lpStopline->GetParameterAlongSection());

            lfStopLineAlongSection = lpNextSection->CalcDistanceAlongSection(
                lfStoplineParam, lpStopline->GetSegmentAlongSection(),
                lpNextHull->GetRungLengthsForSection(lpNextSection));
            CGS_ASSERT(lfStopLineAlongSection >= KF_SECTION_POSITION_TOLERANCE,
                       "Stopline is at a negative position along is section");   // .cpp 11805

            lfStopLineAlongSection += lpSection->mfLength;
            break;
        }

        if (luStopline == KU_INVALID_STOPLINE)
        {
            return false;
        }
    }

    CGS_ASSERT(lpHull->GetSection(luSectionIndex) == lpSection,
               "lpHull->GetSection( luSection ) == lpSection");             // .cpp 11827

    const f32 lfParamAlongSection =
        lpSection->CalcDistanceAlongSection(lpParam->mfParamAlong, lpParam->muCurrentSegment,
                                            lpafRungLengths);
    CGS_ASSERT(lfParamAlongSection >= KF_SECTION_POSITION_TOLERANCE,
               "Param is at a negative position along is section");         // .cpp 11829

    const f32 lfCarFromStopLine = lfStopLineAlongSection - lfParamAlongSection;
    CGS_ASSERT(lfCarFromStopLine >= 0.0f, "Next stopline was behind param"); // .cpp 11832

    if (lfCarFromStopLine >= KF_STOP_LINE_REACTION_DISTANCE)
    {
        return false;
    }

    *lpfOutStopDist = lfCarFromStopLine;
    return true;
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
