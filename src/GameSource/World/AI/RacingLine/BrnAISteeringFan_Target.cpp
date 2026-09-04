#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameSource/World/AI/BrnAICar.h"            // AICar::IsPlayerCar
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cfloat>   // FLT_MAX (the -3.4028235e38 / +3.4028235e38 seeds)

// BrnAI::SteeringFan -- partfile 1 of 2 for the weighting/target half (aiwave R6 lane).
// This TU owns the TARGET SELECTION path plus the kfBias table every contributor is scaled by:
//
//   kfBias                @flt_82F30468 (data; see the table banner below)
//   AccumulateWeightings  @0x82779088
//   BestTargetInArea      @0x82768E50
//   AccumulateInArea      @0x82769000
//   GetMinMaxWeightings   @0x82768AC8
//   GetDrivingTarget      @0x82779C30   <- AIDriver::GetTargetPosition's console arm
//   WriteWeightingValues  @0x827691E0   (debug overlay -- parked, see below)
//   RenderEdge            @0x82779770   (debug render -- parked)
//   RenderContributor     @0x82779830   (debug render -- parked)
//   RenderFanVectors      @0x82779A20   (debug render -- parked)
//
// Fan-vector generation + the 13 contributor rows live in BrnAISteeringFan_Weightings.cpp.
//
// Constants read from the image (scratch/postfx_step9_final/envfix/work/image.bin, big-endian,
// file offset = VA - 0x82000000):
//   flt_82001C98 == 1.0   flt_82001CC0 == 0.0   flt_820C4168 == 0.5   flt_82035570 == -FLT_MAX

namespace BrnAI
{

// ========================================================================================
// kfBias -- DWARF BrnAISteeringFan.cpp:68 `float32_t[10][14] kfBias`, image flt_82F30468.
//
// EVIDENCE / HOW THIS TABLE WAS RECOVERED (read this before "correcting" any cell):
//   The static .data image at 0x82F30468 is only HALF the table. 61 of the 140 cells are written
//   at run time by this TU's dynamic initialiser at 0x82C69380..0x82C6946C -- a straight-line
//   lis/addi/lfs/stfs block that copies the file-scope `float32_t KF_*_MAX` constants (which the
//   C++ initialiser list names, so the compiler could not fold them) out of the pool at
//   0x82F30298..0x82F302B4 into kfBias. That initialiser has NO IDA export, which is exactly why
//   a plain image read of 0x82F30468 shows eBiasMode_Race as an all-zero row: it is NOT zero.
//   The pool, decoded store-by-store from 0x82C69380 and matched to the DWARF's file-scope
//   declaration order (BrnAISteeringFan.cpp:33..43):
//     0x82F30298 KF_CENTRE_TRACKING_MAX    =   20      -> column eFan_SteerToCentre
//     0x82F3029C KF_HARD_NO_GO_BAD_MAX     = -300      -> column eFan_ExitHNG
//     0x82F302A0 KF_HARD_NO_GO_GOOD_MAX    =   50      -> column eFan_AvoidHNG
//     0x82F302A4 KF_TRAFFIC_MAX            = -100      -> column eFan_AvoidTraffic
//     0x82F302A8 KF_ONCOMING_TRAFFIC_MAX   = -400      -> column eFan_AvoidOncomingTraffic
//     0x82F302AC KF_EDGE_INTERSECTION_MAX  = -200      -> column eFan_AvoidEdges
//     0x82F302B0 KF_PARALLEL_MAX           = -200      -> column eFan_DriveParallel
//     0x82F302B4 KF_SLAM_PLAYER_MAX        =  300      -> column eFan_SmashIntoPlayer
//   Every other non-zero cell (1 / 5 / 50 / 60 / 90 / 100 / 140 / 500 / 1000) IS present in the
//   static image and is read straight out of image.bin.
//   The table is NOT const on the console (the DWARF spells it `float32_t`, and the dynamic
//   initialiser writes it), so it is not const here either.
// ========================================================================================
const f32 KF_CENTRE_TRACKING_MAX   =   20.0f;   // 0x82F30298
const f32 KF_HARD_NO_GO_BAD_MAX    = -300.0f;   // 0x82F3029C
const f32 KF_HARD_NO_GO_GOOD_MAX   =   50.0f;   // 0x82F302A0
const f32 KF_TRAFFIC_MAX           = -100.0f;   // 0x82F302A4
const f32 KF_ONCOMING_TRAFFIC_MAX  = -400.0f;   // 0x82F302A8
const f32 KF_EDGE_INTERSECTION_MAX = -200.0f;   // 0x82F302AC
const f32 KF_PARALLEL_MAX          = -200.0f;   // 0x82F302B0
const f32 KF_SLAM_PLAYER_MAX       =  300.0f;   // 0x82F302B4

f32 kfBias[E_BIAS_MODE_COUNT][E_FAN_CONTRIBUTORS_COUNT] =
{
    // column order == EFan_Contributors:
    //   0 SteerToCentre  1 AvoidHNG  2 ExitHNG  3 FavourHNGDanger  4 AvoidTraffic
    //   5 AvoidOncomingTraffic  6 AvoidEdges  7 DriveParallel  8 PreferCurrentDirection
    //   9 DriftFinalDirection  10 DriftFinalLocation  11 SmashIntoPlayer
    //  12 DriveCloseToPlayer  13 SmashIntoRivals
    // eBiasMode_Race
    { KF_CENTRE_TRACKING_MAX, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      KF_TRAFFIC_MAX, KF_ONCOMING_TRAFFIC_MAX, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    // eBiasMode_RaceDangerous
    { KF_CENTRE_TRACKING_MAX, 1.0f, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      5.0f, 100.0f, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 500.0f },
    // eBiasMode_Slam
    { 0.0f, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      0.0f, KF_ONCOMING_TRAFFIC_MAX, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, KF_SLAM_PLAYER_MAX, 0.0f, 0.0f },
    // eBiasMode_RoadRage
    { 0.0f, 0.0f, 0.0f, 50.0f,
      KF_TRAFFIC_MAX, KF_ONCOMING_TRAFFIC_MAX, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    // eBiasMode_CloseToPlayer
    { 0.0f, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      KF_TRAFFIC_MAX, KF_ONCOMING_TRAFFIC_MAX, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 60.0f, 0.0f },
    // eBiasMode_SlamRivals
    { KF_CENTRE_TRACKING_MAX, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      KF_TRAFFIC_MAX, KF_ONCOMING_TRAFFIC_MAX, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f },
    // eBiasMode_HitOncoming
    { KF_CENTRE_TRACKING_MAX, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      140.0f, 140.0f, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 90.0f },
    // eBiasMode_SlamDangerous
    { 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, KF_SLAM_PLAYER_MAX, 0.0f, 0.0f },
    // eBiasMode_DontCentreWithinHNG
    { KF_CENTRE_TRACKING_MAX, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      KF_TRAFFIC_MAX, KF_ONCOMING_TRAFFIC_MAX, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    // eBiasMode_VeerAwayFromPlayer
    { 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 50.0f, 0.0f },
};

// ========================================================================================
// AccumulateWeightings @0x82779088   (returns `this`; the X360 threads r3 straight back out)
//
// Fold the 14 contributor rows into one 17-slot column, scaled by the current bias mode's kfBias
// row, then LERP the persistent mfCumulativeWeighting[] halfway toward it.
//   r10 = *(this+0x3B0) == meBiasMode ; r5 = flt_82F30468 + meBiasMode*0x38 (14 floats per mode)
//   r8  = this+0x3B8 / r7 = this+0x3F4 -- mfWeighting[row] walked with the console's 8-wide + 1
//         unrolling (row stride 0x44 == 17 floats); re-rolled here.
//   The `if (bias != 0.0)` skip at 0x827790D0 is the console's own (flt_82001CC0 == 0.0).
//   r11 = this+0x770 == &mfCumulativeWeighting[1] and r10 = &lafAccumulated[1]: the store at
//         -4(r11) is slot 0. flt_820C4168 == 0.5 -> cum = cum + (accum - cum) * 0.5.
// ========================================================================================
SteeringFan* SteeringFan::AccumulateWeightings()
{
    f32 lafAccumulated[KI_FAN_STEPS];
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
        lafAccumulated[liStep] = 0.0f;

    for (s32 liContributor = 0; liContributor < E_FAN_CONTRIBUTORS_COUNT; ++liContributor)
    {
        const f32 lfBias = kfBias[meBiasMode][liContributor];
        if (lfBias != 0.0f)
        {
            for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
                lafAccumulated[liStep] += mfWeighting[liContributor][liStep] * lfBias;
        }
    }

    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        const f32 lfCumulative = mfCumulativeWeighting[liStep];
        mfCumulativeWeighting[liStep] = lfCumulative + (lafAccumulated[liStep] - lfCumulative) * 0.5f;
    }

    return this;
}

// ========================================================================================
// BestTargetInArea @0x82768E50
//
// Index of the largest mfCumulativeWeighting in [liFrom, liTo] INCLUSIVE (the console's
// `4*(liFrom + 475) + this` is &mfCumulativeWeighting[liFrom]; 475*4 == 1900 == 0x76C).
// Seeds with -FLT_MAX (flt_82035570) and returns liFrom when nothing beats it, so an all-equal
// window resolves to its LOW end. Three range asserts, all "Off end of target fan\n"
// (BrnAISteeringFan.cpp:807/808/809).
// ========================================================================================
s32 SteeringFan::BestTargetInArea(s32 liFrom, s32 liTo)
{
    CGS_ASSERT(liTo < KI_FAN_STEPS, "Off end of target fan\n");
    CGS_ASSERT(liFrom >= 0,         "Off end of target fan\n");
    CGS_ASSERT(liFrom <= liTo,      "Off end of target fan\n");

    s32 liBest = liFrom;
    f32 lfBest = -FLT_MAX;
    for (s32 liStep = liFrom; liStep <= liTo; ++liStep)
    {
        if (mfCumulativeWeighting[liStep] > lfBest)
        {
            lfBest = mfCumulativeWeighting[liStep];
            liBest = liStep;
        }
    }
    return liBest;
}

// ========================================================================================
// AccumulateInArea @0x82769000
//
// MEAN of mfCumulativeWeighting over [liFrom, liTo] inclusive: the console sums the slots into
// f31 while adding flt_82001C98 (1.0) into a float counter, then `fdivs f1, f31, count` -- but
// only when the counter is non-zero (flt_82001CC0 == 0.0); otherwise it returns the raw sum
// (0.0 on that path, because the loop never ran). Same three range asserts as BestTargetInArea
// (BrnAISteeringFan.cpp:837/838/839).
// ========================================================================================
f32 SteeringFan::AccumulateInArea(s32 liFrom, s32 liTo)
{
    CGS_ASSERT(liTo < KI_FAN_STEPS, "Off end of target fan\n");
    CGS_ASSERT(liFrom >= 0,         "Off end of target fan\n");
    CGS_ASSERT(liFrom <= liTo,      "Off end of target fan\n");

    f32 lfTotal = 0.0f;
    f32 lfCount = 0.0f;
    for (s32 liStep = liFrom; liStep <= liTo; ++liStep)
    {
        lfTotal += mfCumulativeWeighting[liStep];
        lfCount += 1.0f;
    }

    if (lfCount != 0.0f)
        return lfTotal / lfCount;
    return lfTotal;
}

// ========================================================================================
// GetMinMaxWeightings @0x82768AC8
//
// Walk lpWeightings[0..16] (the console unrolls 8-wide to 16 then a 1-wide tail), writing the
// max to lrBestWeighting (seeded -FLT_MAX) and the min to lrWorstWeighting (seeded +FLT_MAX).
// Returns the index of the max, or -1 when no slot ever beat the seed. Strict `>` / `<`, so
// ties keep the FIRST index.
// ========================================================================================
s32 SteeringFan::GetMinMaxWeightings(f32* lpWeightings, f32& lrBestWeighting, f32& lrWorstWeighting)
{
    s32 liBest = -1;
    lrBestWeighting  = -FLT_MAX;
    lrWorstWeighting =  FLT_MAX;

    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        if (lpWeightings[liStep] > lrBestWeighting)
        {
            lrBestWeighting = lpWeightings[liStep];
            liBest = liStep;
        }
        if (lpWeightings[liStep] < lrWorstWeighting)
            lrWorstWeighting = lpWeightings[liStep];
    }
    return liBest;
}

// ========================================================================================
// GetDrivingTarget @0x82779C30  (sret Vector2)
//
// THE steering target AIDriver::GetTargetPosition @0x8277CBF8 reads every frame.
// Register map (the sret is not shown in the pseudocode's arg list):
//   r3 = &out Vector2, r4 = this, r5 = lpCar, r6 = lpRacingLine, r7 = lpRacingLineGenerator,
//   r8 = lpNearbyTraffic, r9 = lbRenderThinking.
// r6/r7/r8 are NEVER READ by this body -- they exist for the DWARF signature only.
//
//   AccumulateWeightings();
//   liHigh = BestTargetInArea(8, 16);      liLow  = BestTargetInArea(0, 8);
//   lfLow  = AccumulateInArea(liLow, 8);   lfHigh = AccumulateInArea(8, liHigh);
//   pick liHigh when lfHigh > lfLow (fcmpu/ble at 0x82779CC4), else liLow;
//   return mTarget[picked]   (`slwi r11, idx, 4` + `lvx128 v0, r11, r31`: this + idx*16 is
//   mTarget[idx], the FIRST member of the class).
// i.e. the fan is split into the [0..8] half and the [8..16] half, the half with the better MEAN
// cumulative weighting wins, and within it its own best ray is the target. A FLAT fan therefore
// resolves to ray 0 -- see the [FLAG] block in BrnAIDriver.h about why that matters on this host.
// ========================================================================================
Vector2 SteeringFan::GetDrivingTarget(AICar* lpCar, RacingLine* lpRacingLine,
                                      RacingLineGenerator* lpRacingLineGenerator,
                                      const NearbyVehicles* lpNearbyVehicles,
                                      bool lbRenderThinking)
{
    (void)lpRacingLine;             // r6 -- not read by the console body
    (void)lpRacingLineGenerator;    // r7 -- not read by the console body
    (void)lpNearbyVehicles;         // r8 -- not read by the console body

    AccumulateWeightings();

    const s32 liBestHigh = BestTargetInArea(KI_FAN_STEPS / 2, KI_FAN_STEPS - 1);   // (8, 16)
    const s32 liBestLow  = BestTargetInArea(0, KI_FAN_STEPS / 2);                  // (0, 8)

    const f32 lfLowArea  = AccumulateInArea(liBestLow, KI_FAN_STEPS / 2);
    const f32 lfHighArea = AccumulateInArea(KI_FAN_STEPS / 2, liBestHigh);

    if (lbRenderThinking)
        RenderEdge(KI_FAN_STEPS / 2, 0xFFFFFFFFu);          // li r5, -1

    s32 liChosen;
    if (lfHighArea > lfLowArea)
    {
        if (lbRenderThinking && lpCar->IsPlayerCar())        // lbz 0x1549(car)
        {
            WriteWeightingValues(liBestHigh);
            RenderFanVectors(liBestHigh);
            RenderEdge(liBestHigh, 0xFFA0A000u);
            RenderEdge(liBestLow,  0xFFFFA0A0u);            // li r5, -0x5F60 == 0xFFFFA0A0
        }
        liChosen = liBestHigh;
    }
    else
    {
        if (lbRenderThinking && lpCar->IsPlayerCar())
        {
            WriteWeightingValues(liBestLow);
            RenderFanVectors(liBestHigh);                   // the console passes r30 == liBestHigh here
            RenderEdge(liBestHigh, 0xFFA0A0FFu);
            RenderEdge(liBestLow,  0xFFA0A000u);
        }
        liChosen = liBestLow;
    }

    return mTarget[liChosen];
}

// ========================================================================================
// [FLAG PC bring-up] The four DEBUG-OVERLAY members below have real X360 bodies, but each one is
// nothing but calls into the console's on-screen debug layer (CgsDev::DebugInterface /
// CgsDev::DebugRender::Draw2DTextJustified / DrawLine + CgsCore::SPrintf, over the laFanColours /
// lContributorNames tables at 0x82F3042C / 0x82F304xx). None of that layer exists on this host
// build -- exactly as SteeringFan::Prepare already declines to reproduce the six
// CgsDev::PerfMonCpu monitors it registers. They are presentation-only: not one member of
// SteeringFan, AICar, RacingLine or the fan tables is written by any of them, so the driving
// result is bit-identical with them empty. They are BODIED (not omitted) because GetDrivingTarget
// references all four, and the console only reaches them under
// `lbRenderThinking && lpCar->IsPlayerCar()` -- and AIDriver::GetTargetPosition passes
// lbRenderThinking = false, so on this build they are unreachable as well as inert.
// DELETE-WHEN a CgsDev::DebugRender 2D layer exists on the host.
//   WriteWeightingValues @0x827691E0 -- prints the bias-mode name and each non-zero contributor's
//                                       weighting * kfBias down the left of the screen.
//   RenderEdge           @0x82779770 -- one coloured line from mFanOrigin along mUnitDirection[i].
//   RenderContributor    @0x82779830 -- one contributor row drawn as a coloured fan.
//   RenderFanVectors     @0x82779A20 -- every contributor row with a non-zero kfBias, plus the
//                                       chosen ray.
// ========================================================================================
void SteeringFan::WriteWeightingValues(s32 liContributor)
{
    (void)liContributor;
}

void SteeringFan::RenderEdge(s32 liIndex, u32 luColour)
{
    (void)liIndex;
    (void)luColour;
}

void SteeringFan::RenderContributor(s32 liContributor)
{
    (void)liContributor;
}

void SteeringFan::RenderFanVectors(s32 liIndex)
{
    (void)liIndex;
}

}
