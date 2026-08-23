// ============================================================================
// BrnTrafficEntityModule_wT2_05.cpp
//
//   TrafficEntityModule::UpdateParams_DoTimeSlicedLogic @0x82743FE8  (asm, hole closed wave3 r2)
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficPhysicalVehicleInfo.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

#include "SharedClasses/Traffic/BrnTrafficHull.h"     // Hull::GetSection
#include "SharedClasses/Traffic/BrnTrafficSection.h"  // Section

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
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

    // PhysicalVehicleInfo importance (lane 3 of mPositionAndImportance). CheckIfNeedToSlow
    // keeps the SMALLEST Dot(diff,diff) * importance, so the smaller weight wins.
    // 0x827441CC `vcfsx v0, <1>, 1` == 0.5 (player); 0x827441C4 `vcfsx v0, <1>, 0` == 1.0.
    const f32 KF_IMPORTANCE_PLAYER = 0.5f;
    const f32 KF_IMPORTANCE_OTHER  = 1.0f;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_DoTimeSlicedLogic  @ 0x82743FE8..0x82744A7C
//
// EXPORT HOLE CLOSED: dumped straight from the ARTIST .i64 with headless
// idat -- see wave3r2/A/fix/dump1.txt. Every line below is now asm-attested; the previous
// DWARF-shaped reconstruction had the importance weight INVERTED, was missing the rival gate
// and the mbAllowDivergentBehaviour gate, and called an unbodied predicate.
//
// It caches every "physical vehicle" the params should react to (the active race cars, then
// the physical traffic) into one Array<PhysicalVehicleInfo,33>, then runs
// UpdateParam_CheckIfNeedToSlow @0x82738468 over its slice of the param pool.
//
// The slot is cleared through maParamNeedToSlowData directly, not through
// GetParamNeedToSlowData: that accessor asserts muLastParamCalculated >= KU_MAX_PARAMS, which
// is false by construction inside the slicer, and CheckIfNeedToSlow likewise indexes inline
// (asm `16*(luParam+13528)`).
//
// muLastParamCalculated advance: 0x82744A4C..0x82744A70 stores luEndParam back to +0x71830.
//
// PhysicalVehicleInfo field assignments, now asm-attested (race-car block 0x82744190..
// 0x827441F8, traffic block 0x827444F0..0x82744544): lane 0..2 of the first quadword is the
// position (RaceCarState +0x220 / transform +0x30), lane 3 the importance; the second quadword
// is the linear velocity (RaceCarState +0x330 / Vehicle::GetLinearVelocity) and the third the
// body's right axis (RaceCarState +0x1F0 / transform +0x00).
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_DoTimeSlicedLogic(
    u32 luBeginParam,
    u32 luEndParam,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        lpActiveRaceCarInterface)
{
    CGS_ASSERT(luBeginParam < KU_MAX_PARAMS, "luBeginParam < KU_MAX_PARAMS");  // 0x82744024
    CGS_ASSERT(luEndParam <= KU_MAX_PARAMS, "luEndParam <= KU_MAX_PARAMS");    // 0x82744054
    CGS_ASSERT(lpActiveRaceCarInterface != 0, "lpActiveRaceCarInterface");     // 0x82744078

    // .cpp 9778 -- the shared physical-vehicle cache.
    ::Array<PhysicalVehicleInfo, KU_MAX_PHYSICAL_VEHICLES_TO_CACHE> lPhysicalVehicleInfo;
    lPhysicalVehicleInfo.Construct();

    // 0x827440AC..0x827440C0 -- the WHOLE cache build is skipped when divergent behaviour is
    // off (online, non-Showtime): the fuzzy pre-pass then sees no physical vehicles at all and
    // cannot pick a divergent behaviour. Offline this is always true
    // (mbAllowDivergentBehaviour = !mbIsOnlineGameMode || mbPlayingShowtimeMode, _wT1_01:208).
    if (mbAllowDivergentBehaviour)
    {
        // 0x82744100..0x82744228 -- every active race car. The importance scalar multiplies the
        // squared distance in CheckIfNeedToSlow's pick and the SMALLEST score wins
        // (_wT2_03.cpp:1309), so the PLAYER's 0.5 beats everyone else's 1.0 at equal range.
        // The weights are `vcfsx v0, <int 1>, 1` = 0.5 (player, 0x827441CC) and
        // `vcfsx v0, <int 1>, 0` = 1.0 (0x827441C4) -- both asm-attested.
        for (s32 liRaceCar = 0; liRaceCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liRaceCar)
        {
            const EActiveRaceCarIndex leRaceCar = static_cast<EActiveRaceCarIndex>(liRaceCar);

            // 0x82744140: the console tests `maxRaceCarFlags[i] & 1` inline (IsRaceCarActive).
            if (!lpActiveRaceCarInterface->IsRaceCarActive(leRaceCar))
            {
                continue;
            }

            // 0x82744158..0x8274417C -- a RIVAL only counts as something to react to while it
            // is crashing (`lbz r11, 0x44A(state)` == RaceCarState::mbCrashing @1098).
            if (lpActiveRaceCarInterface->IsRaceCarRival(leRaceCar)
                && !lpActiveRaceCarInterface->GetRaceCarState(leRaceCar)->mbCrashing)
            {
                continue;
            }

            const BrnPhysics::Vehicle::RaceCarState* const lpRaceCarState =
                lpActiveRaceCarInterface->GetRaceCarState(leRaceCar);

            // The console calls IsRaceCarPlayer @0x82681DF0 (`maxRaceCarFlags[idx] >> 1 & 1`,
            // E_RACE_CAR_OUTPUT_FLAG_PLAYER). That method is declared-only in this tree and its
            // own TU is another owner's file, so calling it is a LINK HOLE; maxRaceCarFlags is
            // private, so the bit cannot be read here either. Stand-in: the bodied public
            // GetPlayerActiveRaceCarIndex @0x82277BF8 -- the same slot, set from the same
            // muType == E_RACE_CAR_TYPE_PLAYER producer arm.
            // DELETE-WHEN BrnRCEntityActiveRaceCarOutputInterface.cpp bodies IsRaceCarPlayer.
            const bool lbIsPlayer =
                (leRaceCar == lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex());

            PhysicalVehicleInfo lInfo;
            lInfo.mPositionAndImportance.SetVector3(lpRaceCarState->mTransform.Pos());
            lInfo.mPositionAndImportance.SetPlus(lbIsPlayer ? KF_IMPORTANCE_PLAYER
                                                            : KF_IMPORTANCE_OTHER);
            lInfo.mLinearVelocity = lpRaceCarState->mLinearVelocity;
            lInfo.mRight          = lpRaceCarState->mTransform.Right();

            // FLAG (host guard): the console Appends unconditionally -- 8 race cars can never
            // overflow the 33-slot array. Kept so a corrupt count cannot smash the stack.
            if (lPhysicalVehicleInfo.GetLength() < KU_MAX_PHYSICAL_VEHICLES_TO_CACHE)
            {
                lPhysicalVehicleInfo.Append(lInfo);
            }
        }

        // 0x827443EC..0x82744544 -- then every alive+physical traffic vehicle, all weighing 1.0
        // (`vcsxwfp128 v125, <int 1>, 0` hoisted at 0x827443BC). The console asserts IsAlive()
        // and IsPhysical() per element (0x82744490 / 0x827444B8); the intersection makes both
        // true by construction here.
        CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> lVehiclesAlive_And_Physical;
        lVehiclesAlive_And_Physical.SetAnd(mVehicleSoaData.mAliveVehicles,
                                           mVehicleSoaData.mPhysicalVehicles);

        for (CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>::Iterator lItVehicle =
                 lVehiclesAlive_And_Physical.Begin();
             lItVehicle != lVehiclesAlive_And_Physical.End();
             ++lItVehicle)
        {
            // FLAG (host guard): no console equivalent -- 601 physical vehicles would overrun
            // the 33-slot array, which on console is an Append assert. Bounded here instead.
            if (lPhysicalVehicleInfo.GetLength() >= KU_MAX_PHYSICAL_VEHICLES_TO_CACHE)
            {
                break;
            }

            const u32 luVehicle = static_cast<u32>(lItVehicle.GetIndex());
            const Vehicle* const lpVehicle = GetVehicle(luVehicle);
            const Matrix44Affine lVehicleTransform = GetVehicleTransform(luVehicle);

            PhysicalVehicleInfo lInfo;
            lInfo.mPositionAndImportance.SetVector3(lVehicleTransform.Pos());
            lInfo.mPositionAndImportance.SetPlus(KF_IMPORTANCE_OTHER);
            lInfo.mLinearVelocity = lpVehicle->GetLinearVelocity();
            lInfo.mRight          = lVehicleTransform.Right();

            lPhysicalVehicleInfo.Append(lInfo);
        }
    }

    // 0x827448C4..0x82744A48 -- the slice itself.
    for (u32 luParam = luBeginParam; luParam < luEndParam && luParam < KU_MAX_PARAMS; ++luParam)
    {
        maParamNeedToSlowData[luParam].Clear();

        // 0x82744968 (`lbz 0x3E(param+2)` & 1 == mxFlags @0x40 & E_FLAG_ALIVE) and 0x82744978
        // (`lbz 0x18(param+2)` & 2 == mxEffectAndHistoryState @0x1A & E_HISTORY_BORN). Both
        // gates are the console's, and both leave the slot on Clear()'s miBehaviour == -1.
        const Param* const lpParam = &maParams[luParam];
        if ((lpParam->mxFlags & Param::E_FLAG_ALIVE) == 0)
        {
            continue;
        }
        if ((lpParam->mxEffectAndHistoryState & Param::E_HISTORY_BORN) != 0)
        {
            continue;
        }

        const Hull* const    lpHull    = GetHull(lpParam->muHullIndex);
        const Section* const lpSection = lpHull->GetSection(lpParam->muSectionIndex);

        UpdateParam_CheckIfNeedToSlow(luParam, lpHull, lpParam->muSectionIndex, lpSection,
                                      &lPhysicalVehicleInfo);
        UpdateParam_CheckIfInsideParamInFront(luParam);   // the console's second per-param call
    }

    muLastParamCalculated = luEndParam;

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T3-behaviour] first param that reaches DRIVE_AROUND_OBSTRUCTION (miBehaviour 2 --
        // the ONLY value UpdateVehiclesJob::CalcSwerveAmount @0x8291CF18 turns into a
        // normal-physical promotion), plus a ~5 s histogram of every behaviour value.
        // DELETE-WHEN-STABLE.
        static bool sbFirstBehaviour2 = true;
        static u32  sauBehaviourCounts[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        static f32  sfHistogramTimer = 0.0f;

        for (u32 luParam = luBeginParam; luParam < luEndParam && luParam < KU_MAX_PARAMS; ++luParam)
        {
            const s8 liBehaviour = maParamNeedToSlowData[luParam].miBehaviour;
            if (liBehaviour >= 0 && liBehaviour < 8)
            {
                ++sauBehaviourCounts[liBehaviour];
            }

            if (sbFirstBehaviour2 && liBehaviour == 2)
            {
                sbFirstBehaviour2 = false;
                *lpDiag << "[T3-behaviour] param " << static_cast<s32>(luParam)
                        << " reached miBehaviour 2 (DRIVE_AROUND_OBSTRUCTION); stopDist "
                        << maParamNeedToSlowData[luParam].mfStopDist
                        << " targetSpeed " << maParamNeedToSlowData[luParam].mfTargetSpeed
                        << "\n";
            }
        }

        sfHistogramTimer += mfSimTimeStep;
        if (sfHistogramTimer >= 5.0f)
        {
            sfHistogramTimer = 0.0f;
            *lpDiag << "[T3-behaviour] histogram";
            for (u32 luBehaviour = 0; luBehaviour < 8; ++luBehaviour)
            {
                *lpDiag << " [" << static_cast<s32>(luBehaviour) << "]="
                        << static_cast<s32>(sauBehaviourCounts[luBehaviour]);
                sauBehaviourCounts[luBehaviour] = 0;
            }
            // physSlots is the module's own 25-slot TrafficPhysicsInfo list, which
            // StopVehicleBeingPhysical is the ONLY thing that ever frees. A monotonic
            // physSlots means demotion is unreachable.
            *lpDiag << " physSlots="
                    << static_cast<s32>(maTrafficPhysicsInfoListBits.CountSetBits())
                    << "\n";
        }
    }
}

}
