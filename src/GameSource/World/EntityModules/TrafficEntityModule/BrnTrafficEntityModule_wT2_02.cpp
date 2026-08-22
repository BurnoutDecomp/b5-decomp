// ============================================================================
// BrnTrafficEntityModule_wT2_02.cpp -- the per-decision-frame param driver and the
// ordered "params on this section" list.
//
//   TrafficEntityModule::UpdateParams                     @0x82744A80  PARTIAL
//   TrafficEntityModule::UpdateParams_UpdateDead          @0x827369A8  PARTIAL
//   TrafficEntityModule::UpdateParams_UpdatePurgatoryList @0x827244E0
//   TrafficEntityModule::UpdateParams_UpdateLinkedList    @0x82739660
//   TrafficEntityModule::UpdateParams_UpdateNeighbours    @0x82708AC8
//   TrafficEntityModule::UpdateParams_TryToReinsertParam  @0x827247F0  GATED
//   TrafficEntityModule::UpdatePressure_Reset             @0x8272BB88
//   TrafficEntityModule::Pressure_PickSplitToTake         @0x8272BC68  PARTIAL
//   BrnTraffic::Neighbour::ConvertOurParameterToTheirs    @0x82705710  (EXPORT HOLE)
//
// Layout is host-native: every member is reached by name; the console displacements in the
// comments only attest which member a line resolves to.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // TrafficData
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // Hull
#include "SharedClasses/Traffic/BrnTrafficSection.h"            // Section, Neighbour

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"

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

    // .rdata literals this TU reads by value.
    const f32 KF_LANE_CHANGE_DICE_ROLL_NUMERATOR = 1275.0f;   // flt_820BFF68 (255 * 5)
}

// ----------------------------------------------------------------------------
// BrnTraffic::Neighbour::ConvertOurParameterToTheirs  @0x82705710 (EXPORT HOLE: named bl
// sites in UpdateParams_UpdatePlan @0x82737CE8 and UpdateParams_HandleLaneChanges
// @0x82725880, no per-function JSON). Body from the Feb-2007 original
// (SharedClasses/Traffic/BrnTrafficSection.h:323), whose three asserts are kept.
// ----------------------------------------------------------------------------
f32 Neighbour::ConvertOurParameterToTheirs(f32 lfOurParam) const
{
    CGS_ASSERT(lfOurParam >= static_cast<f32>(muOurStartRung),
               "Parameter has a bad param for lane change");
    CGS_ASSERT(lfOurParam < static_cast<f32>(muOurStartRung + muSharedLength),
               "Parameter has a bad param for lane change");

    const s32 liDelta = static_cast<s32>(muTheirStartRung) - static_cast<s32>(muOurStartRung);
    const f32 lfTheirParam = lfOurParam + static_cast<f32>(liDelta);

    CGS_ASSERT(lfTheirParam >= 0.0f, "Param has negative param after lane change");
    return lfTheirParam;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams  @0x82744A80  (DWARF :1626, .cpp 10021)  PARTIAL
//
// The per-decision-frame driver. TWO stack blocks in the console:
//   * the Array<CrashingThingData,168> at var_15B0 feeds ONLY the two crash arms -- both are
//     wave-3 surface, so the array and its producer are gated together.
//   * the ten-quadword bit set at var_1600 is a FastBitArray<601> intersection built inline
//     from mVehicleSoaData.mAliveVehicles (module +164560) AND .mPhysicalVehicles (+164800),
//     named by DEBUGValidateSoaData @0x82714A60's assert strings. It is passed to
//     UpdateParams_CalcDesiredSpeed and IS built for real -- gating it starves the speed
//     calculation.
//
// GATED, by name + address: UpdateParams_BuildListOfCrashingThings @0x82737270,
// UpdateParams_TryAvoidCrashing @0x82716948, UpdateParams_TryStartSympatheticCrashing
// @0x827165D8 -- crash surface, wave 3.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput)
{
    CgsDev::PerfMonCpu::StartMonitor(miPerfMon_UpdateParam);

    CGS_ASSERT(muLastParamCalculated >= KU_MAX_PARAMS, "muLastParamCalculated >= KU_MAX_PARAMS");

    if (mbNeedToKillAllZombies)              // +0x7180F
    {
        KillAllZombies();
        mbNeedToKillAllZombies = false;
    }

    UpdateParams_UpdateDead();

    // The avoid set the speed calculation reads: alive AND physical.
    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> lPhysicalAliveVehicles;
    lPhysicalAliveVehicles.SetAnd(mVehicleSoaData.mAliveVehicles,
                                  mVehicleSoaData.mPhysicalVehicles);

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
                      "UpdateParams -> UpdateParams_BuildListOfCrashingThings @0x82737270 "
                      "(with its Array<CrashingThingData,168>) -- crash surface, wave 3");
    }
    (void)lpInput;   // forwarded to BuildListOfCrashingThings as its lpInput; gated above.

    const u32 luMaxLaneChangeDiceRoll =
        static_cast<u32>(KF_LANE_CHANGE_DICE_ROLL_NUMERATOR / mfSimTimeSinceLastDecision);

    CGS_ASSERT(IsDecisionFrame(), "IsDecisionFrame()");

    UpdateParams_UpdatePurgatoryList();

    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        Param* lpParam = &maParams[luParam];

        // Where this param started the frame (the scene mover diffs against it).
        lpParam->muStartSectionIndex = lpParam->muSectionIndex;
        lpParam->muStartHullIndex    = lpParam->muHullIndex;

        if (!lpParam->IsAlive())
        {
            continue;
        }
        if ((lpParam->mxEffectAndHistoryState & Param::E_HISTORY_BORN) != 0)
        {
            continue;   // born this frame -- it gets its first update next decision frame
        }
        if ((lpParam->mxFlags & Param::E_FLAG_SHOULD_BE_REMOVED) != 0)
        {
            KillParam(luParam);
            continue;
        }

        const Hull*    lpHull    = GetHull(lpParam->muHullIndex);
        const Section* lpSection = lpHull->GetSection(lpParam->muSectionIndex);

        const u32 luOriginalSection = lpParam->muSectionIndex;

        CGS_ASSERT(lpParam->IsAlive(), "IsAlive()");   // BrnTrafficParam.h 1062

        if ((lpParam->mxEffectAndHistoryState & Param::E_HISTORY_NEEDS_NEW_PLAN) != 0)
        {
            UpdateParams_UpdatePlan(luParam, luMaxLaneChangeDiceRoll);
        }

        UpdateParams_UpdateBehaviour(luParam);
        UpdateParams_CalcDesiredSpeed(luParam, lpSection, lpHull, lPhysicalAliveVehicles);
        UpdateParams_IncrementParam(luParam, &lpHull, &lpSection);

        if (NeedToTakeActionAgainstJunctionFUP())
        {
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                          "UpdateParams -> UpdateParams_TryAvoidCrashing @0x82716948 -- crash "
                          "surface, wave 3 (needs the gated CrashingThingData list)");
        }
        else if (ShouldBeHollywoodAction())
        {
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                          "UpdateParams -> UpdateParams_TryStartSympatheticCrashing @0x827165D8 "
                          "-- crash surface, wave 3 (needs the gated CrashingThingData list)");
        }

        // IncrementParam can kill the param (it runs off the end of a dead-end section).
        if (!lpParam->IsAlive())
        {
            continue;
        }

        UpdateParams_HandleLaneChanges(luParam, lpHull, &lpSection);

        const u16 luParamAsElement = static_cast<u16>(luParam);
        if (mParamsToReinsert.Contains(luParamAsElement))
        {
            UpdateParams_TryToReinsertParam(luParam);
        }

        lpParam->PushHistory(lpParam->muCurrentSegment + lpSection->muRungOffset,
                             lpParam->muHullIndex);

        CGS_ASSERT((luOriginalSection == lpParam->muSectionIndex) ||
                       ((lpParam->mxEffectAndHistoryState & Param::E_HISTORY_CHANGED_SECTION) != 0),
                   "( luOriginalSection == lpParam->muSectionIndex ) || "
                   "( lpParam->HasChangedSection() )");
    }

    UpdatePressure_Reset();
    UpdateParams_UpdateLinkedList();

    mParamsToReinsert.Clear();   // +0x3D7F4 `stwx r17(0)`

    // [T2-param] one-shot: did the driver run, and with what clock?
    {
        static bool sbLogged = false;
        if (!sbLogged)
        {
            if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
            {
                u32 luAlive = 0;
                for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
                {
                    if (maParams[luParam].IsAlive())
                    {
                        ++luAlive;
                    }
                }
                sbLogged = true;
                *lpDiag << "[T2-param] FIRST UpdateParams pass: aliveParams " << luAlive
                        << " decisionFrame " << (mbDecisionFrame ? 1 : 0)
                        << " dtSinceDecision " << mfSimTimeSinceLastDecision
                        << " diceRoll " << luMaxLaneChangeDiceRoll << "\n";
            }
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(miPerfMon_UpdateParam);
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_UpdateDead  @0x827369A8  (.cpp 9772)  PARTIAL
//
// Retires params whose vehicle has already been torn down: dying AND not alive AND without an
// entity AND not collidable. The console builds the set as three inverse-and steps over
// mVehicleSoaData; the ledger files this function under CgsFastBitArray.h (catch-all).
//
// GATED: PutParamInPurgatory @0x82716510 -- declared FLAG in BrnTrafficEntityModule.h, no body
// anywhere in the tree. DELETE-WHEN that body lands.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_UpdateDead()
{
    CGS_ASSERT((meState == E_STATE_TEARING_DOWN) || IsDecisionFrame(),
               "( meState == E_STATE_TEARING_DOWN ) || IsDecisionFrame()");

    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> lNotVehicle;
    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> lDeadParams;

    lNotVehicle.SetInverse(mVehicleSoaData.mAliveVehicles);          // +164560
    lDeadParams.SetAnd(mParamSoaData.mDyingParams, lNotVehicle);

    lNotVehicle.SetInverse(mVehicleSoaData.mVehiclesWithEntities);   // +164640
    lDeadParams.SetAnd(lDeadParams, lNotVehicle);

    lNotVehicle.SetInverse(mVehicleSoaData.mCollidableVehicles);     // +164720
    lDeadParams.SetAnd(lDeadParams, lNotVehicle);

    for (CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>::Iterator lItParam = lDeadParams.Begin();
         lItParam != lDeadParams.End();
         ++lItParam)
    {
        const u32 luParam = static_cast<u32>(lItParam.GetIndex());

        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");   // .cpp 9791

        Param* lpParam = GetParam(luParam);
        CGS_ASSERT(lpParam->IsDying(), "lpParam->IsDying()");             // .cpp 9794

        const Vehicle* lpVehicle = GetVehicle(luParam);
        CGS_ASSERT(!lpVehicle->IsAlive(), "!GetVehicle( luParam )->IsAlive()");        // 9795
        CGS_ASSERT(!lpVehicle->HasEntity(), "!GetVehicle( luParam )->HasEntity()");    // 9796
        // Vehicle has no public IsCollidable(); DEBUGValidateSoaData @0x82714A60 pins the SoA
        // set as the same bit ("GetVehicle( luVehicle )->IsCollidable() == mCollidableVehicles").
        CGS_ASSERT(!mVehicleSoaData.mCollidableVehicles.IsBitSet(luParam),
                   "!GetVehicle( luParam )->IsCollidable()");   // .cpp 9797

        lpParam->ClearDying(luParam, mParamSoaData);

        if (mbAllowDivergentBehaviour)   // +0x717E7
        {
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                          "UpdateParams_UpdateDead -> PutParamInPurgatory @0x82716510 -- "
                          "declared FLAG in BrnTrafficEntityModule.h, no body in the tree");
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_UpdatePurgatoryList  @0x827244E0  (.cpp 10308)
//
// Ticks each purgatoried param down a decision frame and, at zero, returns its id to the free
// stack. Erase-in-place: the index steps back so the compacted slot is revisited.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_UpdatePurgatoryList()
{
    for (u32 luPurgatoryParam = 0;
         luPurgatoryParam < maPurgatoryList.GetLength();
         ++luPurgatoryParam)
    {
        PurgatoryInfo& lrInfo = maPurgatoryList.GetItem(luPurgatoryParam);

        --lrInfo.muDecisionFramesLeft;
        if (lrInfo.muDecisionFramesLeft > 0)
        {
            continue;
        }

        const u32 luParam = lrInfo.muIndex;
        Param* lpParam = GetParam(luParam);

        CGS_ASSERT(!lpParam->IsAlive(), "Param was alive when it came out of purgatory");

        const u16 luParamAsElement = static_cast<u16>(luParam);
        CGS_ASSERT(!mFreeParams.Contains(luParamAsElement),
                   "Trying to put param onto the free list twice");
        mFreeParams.Push(luParamAsElement);

        lpParam->SetInPurgatory(false);

        maPurgatoryList.Erase(luPurgatoryParam);
        --luPurgatoryParam;
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_UpdateLinkedList  @0x82739660  (.cpp 12538..12710)
//
// Re-sorts the per-section doubly-linked param lists once per decision frame. One pass
// classifies every param (died / born / alive / changed-section) and republishes its
// mfParamAlong into maParamListNodes; then remove, bubble-sort by mfParamAlong, and re-insert.
//
// The classification pass also clears the three per-frame history bits (mxEffectAndHistoryState
// &= 0xF1) and rebuilds the per-section-span occupancy counters the pressure system reads.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_UpdateLinkedList()
{
    // The console's own local names (its assert strings cite lauParamSections).
    u8  lauParamSections[KU_MAX_PARAMS];
    u8  labParamInList[KU_MAX_PARAMS];
    u16 lauParamHulls[KU_MAX_PARAMS];

    u16 lauDiedParams[KU_MAX_PARAMS];
    u16 lauAliveParams[KU_MAX_PARAMS];
    u16 lauNewParams[KU_MAX_PARAMS];
    u16 lauChangedSectionParams[KU_MAX_PARAMS];

    u32 luNumDied           = 0;
    u32 luNumAlive          = 0;
    u32 luNumNew            = 0;
    u32 luNumChangedSection = 0;

    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

        Param* lpParam = &maParams[luParam];

        lauParamSections[luParam] = 0;
        lauParamHulls[luParam]    = lpParam->muHullIndex;

        if (lpParam->HasDied())
        {
            lauDiedParams[luNumDied++] = static_cast<u16>(luParam);
            labParamInList[luParam]    = 0;
        }
        else if (lpParam->IsAlive())
        {
            maParamListNodes[luParam].mfParamAlong = lpParam->mfParamAlong;

            lauParamSections[luParam] = lpParam->muSectionIndex;
            labParamInList[luParam]   = 1;

            if ((lpParam->mxEffectAndHistoryState & Param::E_HISTORY_BORN) != 0)
            {
                lauNewParams[luNumNew++] = static_cast<u16>(luParam);
                labParamInList[luParam]  = 0;
            }
            else
            {
                lauAliveParams[luNumAlive++] = static_cast<u16>(luParam);
                if ((lpParam->mxEffectAndHistoryState & Param::E_HISTORY_CHANGED_SECTION) != 0)
                {
                    lauChangedSectionParams[luNumChangedSection++] = static_cast<u16>(luParam);
                    labParamInList[luParam] = 0;
                }
            }

            const Hull* lpHull = GetHull(lpParam->muHullIndex);
            CGS_ASSERT(lpHull != 0, "lpHull");                              // .cpp 12538
            const Section* lpSection = lpHull->GetSection(lpParam->muSectionIndex);
            CGS_ASSERT(lpSection != 0, "lpSection");                        // .cpp 12541
            HullRuntime* lpHullRuntime = GetHullRuntime(lpParam->muHullIndex);
            CGS_ASSERT(lpHullRuntime != 0, "lpHullRuntime");                // .cpp 12544

            lpHullRuntime->IncrementSectionSpanVehicleCount(lpSection->muSpanIndex);
        }
        else
        {
            labParamInList[luParam] = 0;
        }

        // The three per-frame history bits are consumed here and only here.
        lpParam->mxEffectAndHistoryState &= 0xF1u;
    }

    for (u32 luIndex = 0; luIndex < luNumDied; ++luIndex)
    {
        const u32 luParamIndex = lauDiedParams[luIndex];
        CGS_ASSERT(luParamIndex < KU_MAX_STANDARD_TRAFFIC,
                   "luParamIndex < KU_MAX_STANDARD_TRAFFIC");               // .cpp 12557
        RemoveParamFromList(luParamIndex);
    }

    for (u32 luIndex = 0; luIndex < luNumChangedSection; ++luIndex)
    {
        const u32 luParamIndex = lauChangedSectionParams[luIndex];
        CGS_ASSERT(luParamIndex < KU_MAX_STANDARD_TRAFFIC,
                   "luParamIndex < KU_MAX_STANDARD_TRAFFIC");               // .cpp 12597
        RemoveParamFromList(luParamIndex);
    }

    // Bubble pass: any swap restarts the sweep (the console sets the index to -1 then ++s).
    for (u32 luIndex = 0; luIndex < luNumAlive; ++luIndex)
    {
        const u32 luParamIndex = lauAliveParams[luIndex];
        CGS_ASSERT(luParamIndex < KU_MAX_STANDARD_TRAFFIC,
                   "luParamIndex < KU_MAX_STANDARD_TRAFFIC");               // .cpp 12639

        const ParamListNode* lpParamNode = &maParamListNodes[luParamIndex];
        bool lbSwapped = false;

        const u32 luNextParam = lpParamNode->muNextParam;
        if (luNextParam != KU_INVALID_PARAM &&
            lpParamNode->mfParamAlong > maParamListNodes[luNextParam].mfParamAlong)
        {
            SwapParamsInList(luParamIndex, luNextParam);
            lbSwapped = true;
        }

        const u32 luPrevParam = lpParamNode->muPrevParam;
        if (luPrevParam != KU_INVALID_PARAM &&
            lpParamNode->mfParamAlong < maParamListNodes[luPrevParam].mfParamAlong)
        {
            SwapParamsInList(luPrevParam, luParamIndex);
            lbSwapped = true;
        }

        if (lbSwapped)
        {
            luIndex = static_cast<u32>(-1);
        }
    }

    for (u32 luIndex = 0; luIndex < luNumNew; ++luIndex)
    {
        const u32 luParamIndex = lauNewParams[luIndex];
        CGS_ASSERT(luParamIndex < KU_MAX_STANDARD_TRAFFIC,
                   "luParamIndex < KU_MAX_STANDARD_TRAFFIC");               // .cpp 12681

        const ParamListNode* lpParamNode = &maParamListNodes[luParamIndex];
        const u32 luSection = lauParamSections[luParamIndex];

        InsertParamIntoList(luParamIndex, lauParamHulls[luParamIndex], luSection,
                            lpParamNode->mfParamAlong);

        lauAliveParams[luNumAlive + luIndex] = static_cast<u16>(luParamIndex);
        labParamInList[luParamIndex] = 1;

        CGS_ASSERT((lpParamNode->muNextParam == KU_INVALID_PARAM) ||
                       (luSection == lauParamSections[lpParamNode->muNextParam]),
                   "( lpParamNode->muNextParam == KU_INVALID_PARAM ) || "
                   "( lauParamSections[luParamIndex] == lauParamSections[lpParamNode->muNextParam] )");
        CGS_ASSERT((lpParamNode->muPrevParam == KU_INVALID_PARAM) ||
                       (luSection == lauParamSections[lpParamNode->muPrevParam]),
                   "( lpParamNode->muPrevParam == KU_INVALID_PARAM ) || "
                   "( lauParamSections[luParamIndex] == lauParamSections[lpParamNode->muPrevParam] )");
    }

    for (u32 luIndex = 0; luIndex < luNumChangedSection; ++luIndex)
    {
        const u32 luParamIndex = lauChangedSectionParams[luIndex];
        CGS_ASSERT(luParamIndex < KU_MAX_STANDARD_TRAFFIC,
                   "luParamIndex < KU_MAX_STANDARD_TRAFFIC");               // .cpp 12701

        const ParamListNode* lpParamNode = &maParamListNodes[luParamIndex];
        const u32 luSection = lauParamSections[luParamIndex];

        InsertParamIntoList(luParamIndex, lauParamHulls[luParamIndex], luSection,
                            lpParamNode->mfParamAlong);

        labParamInList[luParamIndex] = 1;

        CGS_ASSERT((lpParamNode->muNextParam == KU_INVALID_PARAM) ||
                       (luSection == lauParamSections[lpParamNode->muNextParam]),
                   "( lpParamNode->muNextParam == KU_INVALID_PARAM ) || "
                   "( lauParamSections[luParamIndex] == lauParamSections[lpParamNode->muNextParam] )");
        CGS_ASSERT((lpParamNode->muPrevParam == KU_INVALID_PARAM) ||
                       (luSection == lauParamSections[lpParamNode->muPrevParam]),
                   "( lpParamNode->muPrevParam == KU_INVALID_PARAM ) || "
                   "( lauParamSections[luParamIndex] == lauParamSections[lpParamNode->muPrevParam] )");
    }

    (void)labParamInList;   // the console keeps it only for the two asserts above
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_UpdateNeighbours  @0x82708AC8  (.cpp 10336)
//
// Refreshes the two cached side-neighbour handles when the cache is unknown (KU_UNKNOWN_NEIGHBOUR
// == 0xFFFE) or the param has driven past the cached shared stretch.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_UpdateNeighbours(Param* lpParam,
                                                        const Section* lpSection,
                                                        const Hull* lpHull)
{
    CGS_ASSERT(lpParam != 0, "lpParam");

    const u32 luRung = lpParam->muCurrentSegment;

    for (u32 luSide = E_LEFT; luSide < E_SIDE_COUNT; ++luSide)
    {
        if (lpParam->mauNeighbourData[luSide] != KU_UNKNOWN_NEIGHBOUR &&
            lpParam->mauNeighbourEndRung[luSide] >= (luRung + 1))
        {
            continue;
        }

        const u16 luNeighbour =
            lpSection->FindNeighbourForRung(luRung, static_cast<Side>(luSide), lpHull);
        lpParam->mauNeighbourData[luSide] = luNeighbour;

        if (luNeighbour != KU_INVALID_PARAM)   // 0xFFFF == "no neighbour"
        {
            const Neighbour* lpNeighbour = lpHull->GetNeighbour(luNeighbour);
            lpParam->mauNeighbourEndRung[luSide] =
                static_cast<u8>(lpNeighbour->muOurStartRung + lpNeighbour->muSharedLength);
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_TryToReinsertParam  @0x827247F0  (.cpp 10552)  GATED
//
// Online divergence repair: re-seats a param onto the nearest lane under the vehicle's actual
// transform. Reached only through mParamsToReinsert, which nothing in the offline driving path
// fills, and asserts AllowDivergentBehaviour() on entry.
//
// BLOCKERS (all three): TrafficData::FindNearestLaneForPoint has no declaration in
// SharedClasses/Traffic/BrnTrafficDataResourceType.h; Param::ReinsertInLanes has no declaration
// in BrnTrafficParam.h; muNumTrafficInsertionsThisFrame's <= 1 budget test at +232478 is the
// only other read. DELETE-WHEN those two land.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_TryToReinsertParam(u32 luParam)
{
    CGS_ASSERT(mbAllowDivergentBehaviour, "AllowDivergentBehaviour()");
    (void)luParam;

    static bool sbLogged = false;
    LogMissingLeg(sbLogged,
                  "UpdateParams_TryToReinsertParam @0x827247F0 -- needs "
                  "TrafficData::FindNearestLaneForPoint and Param::ReinsertInLanes, neither "
                  "declared in the tree");
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdatePressure_Reset  @0x8272BB88
//
// Zeroes every active hull's per-section-span occupancy counters before
// UpdateParams_UpdateLinkedList rebuilds them (the console's memset of 512 bytes at
// HullRuntime+0x290 is exactly ResetSectionSpanVehicleCounts).
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdatePressure_Reset()
{
    for (u32 luIndex = 0; luIndex < mActiveHulls.GetLength(); ++luIndex)
    {
        HullRuntime* lpHullRuntime = GetHullRuntime(mActiveHulls.GetItem(luIndex));
        lpHullRuntime->ResetSectionSpanVehicleCounts();
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::Pressure_PickSplitToTake  @0x8272BC68  (.cpp 17230..17233)  PARTIAL
//
// Picks which of a section's three forward splits a param should take: lowest score wins, ties
// keep the earlier direction. The base score is 0 for straight-on and 100 for a left/right
// split whose turn probability is zero; the console then adds the destination span's occupancy
// pressure.
//
// GATED LEG -- the pressure term. It is
//   score += (f32)lpHullRuntime->GetSectionSpanVehicleCount(span) *
//            lpHull->mpaSectionSpans[span].mfMaxVehicleRecip;
// BLOCKER: BrnTraffic::SectionSpan is forward-declared only in SharedClasses/Traffic/
// BrnTrafficHull.h. DWARF BrnTrafficSection.h:260 spells it (u16 muMaxVehicles @+0,
// f32 mfMaxVehicleRecip @+4, record stride 8, plus Hull::GetSectionSpan at BrnTrafficHull.h:94)
// -- home it there and this leg is a two-line edit. Until then the fallback is the console's
// own no-hull-runtime value, 1.0f, so every split scores alike and direction 0 wins.
// DELETE-WHEN SectionSpan is homed.
// ----------------------------------------------------------------------------
void TrafficEntityModule::Pressure_PickSplitToTake(const Section* lpSection,
                                                   u8* lpuOutSection,
                                                   u16* lpuOutHull,
                                                   u8* lpuOutDirection) const
{
    CGS_ASSERT(lpSection != 0, "lpCurrentSection");
    CGS_ASSERT(lpuOutSection != 0, "lpOutSection");
    CGS_ASSERT(lpuOutHull != 0, "lpOutHull");
    CGS_ASSERT(lpuOutDirection != 0, "lpOutDirection");

    u8  luBestSection   = KU_INVALID_SECTION;
    u16 luBestHull      = static_cast<u16>(KU_INVALID_PARAM);
    u8  luBestDirection = 0;
    f32 lfBestScore     = 3.4028235e38f;   // flt_82001C98-adjacent KF_MAX_FLOAT literal

    // Base scores: a split whose change probability is zero starts 100 points down.
    f32 lafScores[3];
    lafScores[0] = 0.0f;
    lafScores[1] = 0.0f;
    lafScores[2] = 0.0f;
    if (lpSection->muChangeLeftProb == 0)      // +0x20
    {
        lafScores[1] = 100.0f;                 // flt_820BA5C8
    }
    if (lpSection->muChangeRightProb == 0)     // +0x21
    {
        lafScores[2] = 100.0f;
    }

    for (u32 luDirection = 0; luDirection < 3; ++luDirection)
    {
        const u8  luSection = lpSection->mauForwardSections[luDirection];   // +0x14
        const u16 luHull    = lpSection->mauForwardHulls[luDirection];      // +0x08

        if (luSection == KU_INVALID_SECTION)
        {
            continue;
        }

        const Hull* lpTargetHull = GetHull(luHull);   // carries the muNumHulls bound

        f32 lfScore = 1.0f;
        if (mauHullRuntimeDataIndices[luHull] != KU_INVALID_HULL_RUNTIME)
        {
            CGS_ASSERT(luSection < lpTargetHull->muNumSections, "luIndex < muNumSections");
            const Section* lpTargetSection = lpTargetHull->GetSection(luSection);
            CGS_ASSERT(lpTargetSection->muSpanIndex < lpTargetHull->muNumSectionSpans,
                       "luIndex < muNumSectionSpans");

            lfScore = lafScores[luDirection];

            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                          "Pressure_PickSplitToTake @0x8272BC68 span-occupancy term -- "
                          "BrnTraffic::SectionSpan is forward-declared only in "
                          "SharedClasses/Traffic/BrnTrafficHull.h (DWARF BrnTrafficSection.h:260)");
        }

        if (lfScore < lfBestScore)
        {
            lfBestScore     = lfScore;
            luBestSection   = luSection;
            luBestHull      = luHull;
            luBestDirection = static_cast<u8>(luDirection);
        }
    }

    *lpuOutSection   = luBestSection;
    *lpuOutHull      = luBestHull;
    *lpuOutDirection = luBestDirection;
}
}
