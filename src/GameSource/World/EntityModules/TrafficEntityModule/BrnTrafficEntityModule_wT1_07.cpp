// ============================================================================
// BrnTrafficEntityModule_wT1_07.cpp -- the SHOWTIME traffic generator and the two
// spacing helpers UpdateDecisionFrame's showtime leg needs.
//
//   TrafficEntityModule::CountParamsOnSection  @0x82723D10  (DWARF :1611)
//   TrafficEntityModule::IsParamTooClose       @0x82726470  (DWARF :1548)
//   TrafficEntityModule::SpawnShowtimeTraffic  @0x82743038  (DWARF :1566)
//
// WHAT THIS LEG IS. UpdateDecisionFrame runs SpawnNewTraffic (the authored generator
// ladder) and then, only while showtime is running, SpawnShowtimeTraffic. The showtime
// spawner is a SECOND, INDEPENDENT source: it walks every section of every active hull
// and tops it up to a density derived from a fixed 20 vehicles-per-minute base rate
// scaled by mfShowtimeTrafficDensityScale -- it does NOT read the section's authored
// SectionFlow::muVehiclesPerMinute rate that FillNewHull / CalcTimeToNextGeneration use.
// The authored rate only acts as an on/off mask (a section whose authored rate is zero is
// skipped entirely). That substitution IS "the logic that increases the traffic for
// showtime": a lane authored at, say, 4 vpm is refilled at 20 vpm while showtime is up.
//
// THE CADENCE. The whole body sits behind a 2.0 s accumulator
// (mfTimeSinceLastShowtimeSpawn, +0x71838) so the top-up runs once every 2 s of sim time,
// not on every decision frame.
//
// THE PLACEMENT RULE (the reason it can top up aggressively without popping). Each
// candidate must survive, against mCameraLastFrame:
//   * 50 m <= distance <= 130 m                      (unk_8300CBA0 / unk_8300C9E0)
//   * NOT (within 20 deg of the view axis AND nearer than 110 m)  (unk_8300CED0/CAE0)
//   * NOT more than 50 deg off the view axis                       (unk_8300CAF0)
// plus IsParamTooClose, which enforces a 15 m gap to the car in front.
//
// ⚠️ Those five vector constants read as ZERO in the image: they are dyn-init splats,
// built by five one-per-constant CRT thunks at 0x82C66A68..0x82C66B2C, each of which is
// `lfs f0, <static .rdata float>` -> stack -> lvlx -> vspltw -> stvx. INIT-ORDER CHECKED:
// every source float is a plain static .rdata constant (0x820BA5C0, 0x8200544C,
// 0x820199F8, 0x820C07F4, 0x820C07F8), NOT another dyn-init global, so no thunk here can
// observe a partially-initialised dependency and the shipped values are the ones below.
//
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h"

#include "SharedClasses/Traffic/BrnTrafficHull.h"               // Hull
#include "SharedClasses/Traffic/BrnTrafficSection.h"            // Section, LaneRung
#include "SharedClasses/Traffic/BrnTrafficSectionFlow.h"        // SectionFlow

#include "rw/math/vpu/vector3_operation.h"                      // Dot / Magnitude / Normalize

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cmath>     // std::floor
#include <cstdlib>   // getenv (the BRN_TRAFFIC_DIAG probe below)

namespace BrnTraffic
{
namespace
{
    // [DIAG] NOT IN THE X360 BINARY. Same BRN_TRAFFIC_DIAG switch every sibling partfile
    // uses; capped so a live showtime session costs one line per top-up, not per candidate.
    CgsDev::Log::DebugPrint* ShowtimeDiagStream()
    {
        static const bool skbOn = (getenv("BRN_TRAFFIC_DIAG") != 0);
        if (!skbOn || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }

    // ---- IsParamTooClose tuning (rodata, read per call site) -------------------------
    // 0x82726574 flt_820BA2A8. Minimum clear road in front of a candidate param.
    const f32 KF_PARAM_TOO_CLOSE_AHEAD  = 15.0f;
    // 0x827265FC flt_820BA7E4. The wider gap demanded once the car in front has a car
    // behind it on the same section (i.e. the candidate would land in a queue).
    const f32 KF_PARAM_TOO_CLOSE_QUEUED = 20.0f;

    // ---- SpawnShowtimeTraffic tuning -------------------------------------------------
    // 0x827430F4 flt_820BA86C. Sim seconds between showtime top-ups.
    const f32 KF_SHOWTIME_SPAWN_INTERVAL = 2.0f;
    // 0x82743300 flt_820BA7E4. The showtime base generation rate. The section's own
    // authored SectionFlow::muVehiclesPerMinute is NOT used -- only tested for zero.
    const f32 KF_SHOWTIME_VEHICLES_PER_MINUTE = 20.0f;
    // 0x82743120 flt_82001CC0. The per-hull leftover-vehicle accumulator's seed.
    const f32 KF_SHOWTIME_CARRY_SEED = 0.0f;

    // The five dyn-init splats, resolved through their CRT thunks (see the banner).
    const f32 KF_SHOWTIME_SPAWN_MIN_RANGE      =  50.0f;  // unk_8300CBA0 <- 0x820BA5C0
    const f32 KF_SHOWTIME_SPAWN_MAX_RANGE      = 130.0f;  // unk_8300C9E0 <- 0x8200544C
    const f32 KF_SHOWTIME_SPAWN_NEAR_RANGE     = 110.0f;  // unk_8300CAE0 <- 0x820199F8
    const f32 KF_SHOWTIME_SPAWN_NEAR_CONE_COS  = 0.9397000074386597f;  // unk_8300CED0, cos(20 deg)
    const f32 KF_SHOWTIME_SPAWN_CONE_COS       = 0.642799973487854f;   // unk_8300CAF0, cos(50 deg)
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::CountParamsOnSection  @ 0x82723D10   (.cpp 9435 / 9436)
//
// Walk the section's ordered param list and count it. The two asserts are the list's
// standing integrity pair: an out-of-range link, and a cycle (the counter can never
// legitimately reach the pool size).
// ----------------------------------------------------------------------------
u32 TrafficEntityModule::CountParamsOnSection(u32 luHull, u32 luSection) const
{
    u32 luCount = 0;

    // 0x82723D2C/0x82723D34: GetHullRuntime(luHull)->GetFirstParamInSection(luSection).
    u32 luParam = GetHullRuntime(luHull)->GetFirstParamInSection(luSection);

    while (luParam != KU_INVALID_PARAM)
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "Out of range param in list: ");   // .cpp 9435
        CGS_ASSERT(luCount < KU_MAX_PARAMS, "Param Loop!");                    // .cpp 9436

        ++luCount;

        // 0x82723E7C: `(luParam + 0x6CD0) * 8` indexed off `this` is maParamListNodes
        // (stride 8) and its leading u16 is muNextParam.
        luParam = maParamListNodes[luParam].muNextParam;
    }

    return luCount;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::IsParamTooClose  @ 0x82726470
//
// Would a car placed at lfParamAlong on (luHull, luSection) land on top of the traffic
// already there? Measured in METRES along the lane, not in parameter units, so the answer
// is independent of how long the section's segments happen to be.
//
// ⚠️⚠️ CONSOLE BUG REPRODUCED, NOT "FIXED": the second test is supposed to measure the
// param BEHIND the one in front, and it re-measures the one IN FRONT. The asm is explicit
// -- 0x827265E0 `lfs f1, 4(r29)` and 0x827265E4 `lbz r5, 3(r29)` read r29, which is still
// the GetParam(luNextParam) pointer from 0x82726510; GetParamBehind's own result lives in
// r3 and is used for the hull/section identity checks at 0x827265B0/0x827265BC and then
// discarded. Hex-Rays agrees (it renders both as `v14`). The observable effect is that a
// car in front which itself has a car behind it raises the required gap from 15 m to 20 m,
// rather than measuring the trailing car. Do NOT "correct" this: the shipped spacing that
// the rest of the traffic system is tuned against is the 15/20 m ladder below.
// ----------------------------------------------------------------------------
bool TrafficEntityModule::IsParamTooClose(u32 luHull, u32 luSection, f32 lfParamAlong)
{
    const Hull*    lpHull    = GetHull(luHull);
    const Section* lpSection = lpHull->GetSection(luSection);

    const u32 luNextParam = FindNextParam(luHull, luSection, lfParamAlong);

    // 0x827264C8..0x827264F4. The candidate's own arc-length from the section start; the
    // segment index is the parameter truncated toward zero (fctidz).
    const f32* lpafRungLengths = lpHull->GetRungLengthsForSection(lpSection);
    const f32  lfOurDistance   = lpSection->CalcDistanceAlongSection(
                                     lfParamAlong,
                                     static_cast<u32>(static_cast<s32>(lfParamAlong)),
                                     lpafRungLengths);

    if (luNextParam == KU_INVALID_PARAM)
    {
        return false;
    }

    const Param* lpNextParam = GetParam(luNextParam);

    // 0x82726514..0x82726528. A param in front that is on some OTHER section does not
    // constrain us; the end of our own section does.
    f32 lfGapInFront;
    if (lpNextParam->muHullIndex == luHull && lpNextParam->muSectionIndex == luSection)
    {
        lfGapInFront = lpSection->CalcDistanceAlongSection(lpNextParam->mfParamAlong,
                                                           lpNextParam->muCurrentSegment,
                                                           lpafRungLengths)
                       - lfOurDistance;
    }
    else
    {
        lfGapInFront = lpSection->mfLength - lfOurDistance;
    }

    if (lfGapInFront < KF_PARAM_TOO_CLOSE_AHEAD)
    {
        return true;
    }

    // 0x82726590..0x82726608. Is the car in front itself queued behind something?
    const u32 luParamBehind = GetParamBehind(luNextParam);
    if (luParamBehind == KU_INVALID_PARAM)
    {
        return false;
    }

    const Param* lpParamBehind = GetParam(luParamBehind);
    if (lpParamBehind->muHullIndex != luHull || lpParamBehind->muSectionIndex != luSection)
    {
        return false;
    }

    // See the banner: the console re-measures lpNextParam here, NOT lpParamBehind.
    const f32 lfQueuedGap = lpSection->CalcDistanceAlongSection(lpNextParam->mfParamAlong,
                                                                lpNextParam->muCurrentSegment,
                                                                lpafRungLengths)
                            - lfOurDistance;

    return (lfQueuedGap < KF_PARAM_TOO_CLOSE_QUEUED);
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::SpawnShowtimeTraffic  @ 0x82743038   (.cpp 8468 / 8469 / 8508)
//
// The showtime top-up. Structure, per the asm:
//
//   assert(IsDecisionFrame());                              ; .cpp 8468
//   assert(IsPlayingShowtimeGameMode());                    ; .cpp 8469
//   if (!mbAllowDivergentBehaviour) return;                 ; 0x827430B8, +0x717E7
//   mfTimeSinceLastShowtimeSpawn += mfSimTimeSinceLastDecision;  ; +0x71838 += +0x713F8
//   if (mfTimeSinceLastShowtimeSpawn <= 2.0f) return;
//
// ⚠️ The accumulator advances by mfSimTimeSinceLastDecision (+0x713F8), the 0.1 s decision
// period -- NOT by mfSimTimeStep, which is the next slot along at +0x713FC. This function
// only ever runs on a decision frame, so a frame-step accumulator would under-count by 5x.
//   mfTimeSinceLastShowtimeSpawn = 0.0f;
//   for each hull in mActiveHulls: for each of its sections: top up to the showtime rate
//
// ⚠️ THE THIRD GATE IS mbAllowDivergentBehaviour (+0x717E7), NOT a second showtime flag.
// EnterStartingUpState @0x827080C8 is its only writer and ResetEventData seeds it true, so
// offline (mbAllowDivergentBehaviour = !mbIsOnlineGameMode || mbPlayingShowtimeMode) it is
// always true and the gate is transparent; online it is what stops an unsynchronised
// client minting cars of its own.
//
// ⚠️ THE CARRY IS ASYMMETRIC AND THAT IS THE CONSOLE'S OWN SHAPE. lfCarry lives in f31.
// The compiler puts floorf(lfVehiclesToSpawn) in that same register for the division, and
// `fmr f31, f30` -- the assignment that makes the carry the FRACTION -- sits at 0x827435B0,
// i.e. INSIDE the arm the `ble` at 0x827433B0 skips. So a section that is already full
// enough carries floorf(total) into the next section, not the fraction. FillNewHull
// @0x82743600 does the same computation with the assignment unconditional. Reproduced as
// found; do not "tidy" it.
// ----------------------------------------------------------------------------
void TrafficEntityModule::SpawnShowtimeTraffic()
{
    CGS_ASSERT(IsDecisionFrame(), "IsDecisionFrame()");                       // .cpp 8468
    CGS_ASSERT(IsPlayingShowtimeGameMode(), "IsPlayingShowtimeGameMode()");   // .cpp 8469

    if (!mbAllowDivergentBehaviour)
    {
        return;
    }

    mfTimeSinceLastShowtimeSpawn += mfSimTimeSinceLastDecision;   // +0x713F8, not +0x713FC
    if (mfTimeSinceLastShowtimeSpawn <= KF_SHOWTIME_SPAWN_INTERVAL)
    {
        return;
    }
    mfTimeSinceLastShowtimeSpawn = KF_SHOWTIME_CARRY_SEED;

    u32 luDiagCandidates = 0;
    u32 luDiagSpawned    = 0;

    for (u32 luActiveHull = 0; luActiveHull < mActiveHulls.GetLength(); ++luActiveHull)
    {
        const u32   luHull = mActiveHulls[luActiveHull];
        const Hull* lpHull = GetHull(luHull);

        // 0x82743240 `fmr f31, f23`: the leftover-vehicle carry restarts at each hull.
        f32 lfCarry = KF_SHOWTIME_CARRY_SEED;

        for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
        {
            // Hull::GetFlowData, inlined (0x8274327C, the +0x28 array at stride 4).
            const SectionFlow* lpFlow = &lpHull->mpaSectionFlows[luSection];

            // 0x827432AC. An authored rate of zero masks the section out even in showtime.
            if (lpFlow->muVehiclesPerMinute == 0)
            {
                continue;
            }

            const Section* lpSection = lpHull->GetSection(luSection);

            CGS_ASSERT(lpSection->muNumRungs > 0, "muNumRungs > 0");   // BrnTrafficSection.h 368

            // 0x827432E0..0x8274331C. GetNumSegments() as a float, via fcfid/frsp.
            const f32 lfNumSegments = static_cast<f32>(lpSection->GetNumSegments());

            const f32 lfTimeToDrive = lpSection->mfLength / lpSection->mfSpeed;

            // 0x82743300..0x8274330C. THE SHOWTIME SUBSTITUTION: a fixed base rate scaled
            // by the showtime density, in place of the section's authored rate.
            const f32 lfVehiclesPerMinute =
                mfShowtimeTrafficDensityScale * KF_SHOWTIME_VEHICLES_PER_MINUTE;
            CGS_ASSERT(lfVehiclesPerMinute > 0.0f, "lfVehiclesPerMinute > 0.0f");   // .cpp 8508

            const f32 lfSecondsPerVehicle = KF_SECONDS_PER_MINUTE / lfVehiclesPerMinute;

            // 0x82743354..0x82743398. Two separate inlined floorf()s, exactly as the
            // console emits them (the fsel +/- 2^52 round, then the fsel 0/1 correction).
            const f32 lfVehiclesToSpawn = lfCarry + lfTimeToDrive / lfSecondsPerVehicle;
            const f32 lfWholeVehicles   = std::floor(lfVehiclesToSpawn);
            const f32 lfLeftOver        = lfVehiclesToSpawn - std::floor(lfVehiclesToSpawn);

            const u32 luVehiclesWanted = static_cast<u32>(lfWholeVehicles);

            // 0x827433A4..0x827433B0. Top up, never thin out.
            if (luVehiclesWanted <= CountParamsOnSection(luHull, luSection))
            {
                lfCarry = lfWholeVehicles;   // see the banner -- the console's own shape
                continue;
            }

            // 0x827433B4..0x827433C0. Space them evenly across the section's segments and
            // phase the first one by the fraction carried in from the previous section.
            const f32 lfParamStep = lfNumSegments / lfWholeVehicles;
            f32       lfParam     = lfParamStep * lfLeftOver;

            for (u32 luRemaining = luVehiclesWanted; luRemaining != 0; --luRemaining)
            {
                // 0x827433CC..0x82743414. One RandomFloat() draw whose RESULT IS DISCARDED
                // -- the asm keeps the ring-slot refill, the LCG step and the cursor
                // advance, and never loads the value back. It is the sequence step that is
                // load-bearing, so the call stays.
                mRand.RandomFloat();

                if (IsParamTooClose(luHull, luSection, lfParam))
                {
                    lfParam += lfParamStep;
                    continue;
                }

                ++luDiagCandidates;

                // 0x82743428..0x82743450. The full parameter goes in the vector lane, its
                // truncation in the segment slot -- CalcPositionAtParameter asserts they
                // agree.
                const VecFloat lvParam = { lfParam, lfParam, lfParam, lfParam };
                Vector3        lSpawnPosition;
                lpSection->CalcPositionAtParameter(lpHull->mpaRungs,
                                                   lvParam,
                                                   static_cast<u32>(static_cast<s32>(lfParam)),
                                                   lSpawnPosition);

                // 0x82743458..0x82743568. The placement rule, against the camera transform
                // latched last frame (+0x728C0 is its Pos row, +0x728B0 its At row).
                const Vector3 lToSpawn  = lSpawnPosition - mCameraLastFrame.GetPosition();
                const f32     lfRange   = rw::math::vpu::Magnitude(lToSpawn);

                bool lbCanSpawn = true;
                if (lfRange < KF_SHOWTIME_SPAWN_MIN_RANGE || lfRange > KF_SHOWTIME_SPAWN_MAX_RANGE)
                {
                    lbCanSpawn = false;
                }

                if (lbCanSpawn)
                {
                    const Vector3 lDirectionToSpawn = rw::math::vpu::Normalize(lToSpawn);
                    const f32     lfCosToViewAxis   =
                        rw::math::vpu::Dot(mCameraLastFrame.GetDirection(), lDirectionToSpawn);

                    if (lfCosToViewAxis >= KF_SHOWTIME_SPAWN_NEAR_CONE_COS
                        && lfRange < KF_SHOWTIME_SPAWN_NEAR_RANGE)
                    {
                        // Straight ahead and close enough to be watched arriving.
                        lbCanSpawn = false;
                    }
                    else if (lfCosToViewAxis <= KF_SHOWTIME_SPAWN_CONE_COS)
                    {
                        // Outside the 50 deg cone the player is heading into.
                        lbCanSpawn = false;
                    }
                }

                if (lbCanSpawn)
                {
                    GenerateNewVehicle(PickVehicleToSpawn(lpFlow->muFlowTypeId),
                                       luHull,
                                       luSection,
                                       lfParam);
                    ++luDiagSpawned;
                }

                lfParam += lfParamStep;
            }

            lfCarry = lfLeftOver;   // 0x827435B0 `fmr f31, f30`
        }
    }

    // [DIAG] NOT IN THE X360 BINARY. One line per 2 s top-up. DELETE-WHEN-STABLE.
    if (CgsDev::Log::DebugPrint* lpDiag = ShowtimeDiagStream())
    {
        *lpDiag << "[T1-showtime] top-up hulls=" << static_cast<s32>(mActiveHulls.GetLength())
                << " densityScale=" << mfShowtimeTrafficDensityScale
                << " vpm=" << (mfShowtimeTrafficDensityScale * KF_SHOWTIME_VEHICLES_PER_MINUTE)
                << " candidates=" << static_cast<s32>(luDiagCandidates)
                << " spawned=" << static_cast<s32>(luDiagSpawned)
                << " [DELETE-WHEN-STABLE]\n";
    }
}

}
