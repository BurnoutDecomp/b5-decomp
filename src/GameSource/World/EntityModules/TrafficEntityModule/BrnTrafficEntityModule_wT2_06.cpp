// ============================================================================
// BrnTrafficEntityModule_wT2_06.cpp -- THE CRASH SURFACE: what traffic reacts to, and
// the two reactions.
//
//   TrafficEntityModule::UpdateParams_BuildListOfCrashingThings   @0x82737270
//   TrafficEntityModule::UpdateParams_TryAvoidCrashing            @0x82716948
//   TrafficEntityModule::UpdateParams_TryStartSympatheticCrashing @0x827165D8
//
// These three were DECLARED in BrnTrafficEntityModule.h and defined NOWHERE; their live call
// sites in _wT2_02.cpp's UpdateParams logged a missing leg instead. The park note there read
// "crash surface, wave 3 (needs the gated CrashingThingData list)" -- STALE on every count:
// CrashingThingData has been homed in BrnTrafficEntityModule.h since wave T2, both
// Array<CrashingThingData,168> accessors have been committed since 2026-07-04
// (Array_CrashingThingData_168.cpp), and IsPointWithinSquishedCone -- the geometric core of
// both consumers -- landed with wave T3. The producer's OWN list is a plain stack local
// UpdateParams clears; nothing was ever gating it but the note.
//
// The producer/consumer contract, straight off the ARTIST asm:
//   BuildListOfCrashingThings fills an Array<CrashingThingData,168> from THREE sources --
//     (1) every alive+physical traffic vehicle that is crashing, or (with the junction-FUP
//         score up) is stopped near the physical centre;
//     (2) every active race car that is crashing (or either debug force);
//     (3) in showtime, every flagged entry of the showtime vehicle list.
//   TryAvoidCrashing then SWERVES a param whose forward cone contains one of them
//     (miBehaviour = 2, DRIVING_AROUND_OBSTRUCTION);
//   TryStartSympatheticCrashing CHAIN-CRASHES a param whose cone contains one
//     (miBehaviour = 0, SLOWING_FOR_CRASH, plus the target id in mSympCrashTarget).
//
// TUNING CONSTANTS. The two .data lane blocks these bodies read are dynamically initialised,
// so their values are not in the pseudocode. Both are recovered from their dyn-init thunks in
// the ARTIST image, and the DecFIGS PS3 build NAMES them (X360 leaves them `unk_`):
//   unk_8300CAD0 == BrnTraffic::KF_JUNCTION_FUP_MAX_RADIUS_SQ
//        dyn-init @0x82C65D08: splat(flt_8200D4E4 == 3600.0f) == 60 m squared.
//   unk_8300CC40 == BrnTraffic::kfSympCrash_MaxDistFromCameraSq_
//                                MaxDistFromCameraShowTimeSq_ZW
//        dyn-init @0x82C66ED8: lane0 = flt_820BA810 == 1600.0f (40 m^2),
//                              lane1 = flt_820BA4B8 == 10000.0f (100 m^2), lanes 2/3 = 0.
//   flt_820BA8F8 == 6.0f, flt_820BA5E4 == 10.0f (both read out of .rdata).
// The X360 lane->case mapping agrees with the PS3 name: showtime takes lane 1.
//
// FLAG (pre-existing, behaviour-neutral, NOT changed here). The ARTIST FastBitArray the SoA
// vehicle sets use reports its capacity as 600, not 601: TryAvoidCrashing's inlined IsBitSet
// range assert streams "max bits: 600" (`li r4, 0x258` @0x82716A3C) and
// BuildListOfCrashingThings' iterator parks at 600 (`li r10, 0x258` @0x82737420). This tree
// models VehicleSoaData::KU_MAX_VEHICLES as 601 (the DecFIGS DWARF value), which is the same
// ten 64-bit fields and the same bytes; only End() differs, and bit 600 is never set because
// the flat vehicle index space is KU_MAX_TOTAL_TRAFFIC == 600. The bound asserts below use
// the console's own 600 (KU_MAX_TOTAL_TRAFFIC) so they fire where the console's fire.
//
// Layout is host-native: every member is reached by name; the console displacements in the
// comments only attest which member a line resolves to.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"   // MakeTrafficEntityId
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMathsUtils.h"  // IsPointWithinSquishedCone

#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Dot, operator-

#include <cstdlib>   // getenv (the BRN_TRAFFIC_DIAG probes)


namespace BrnTraffic
{
namespace
{
    // ---- recovered .data / .rdata constants (see the file banner for provenance) ---------

    // unk_8300CAD0, PS3-named BrnTraffic::KF_JUNCTION_FUP_MAX_RADIUS_SQ. Splat of 3600.0f.
    const f32 KF_JUNCTION_FUP_MAX_RADIUS_SQ = 3600.0f;

    // unk_8300CC40 lanes 0/1, PS3-named
    // BrnTraffic::kfSympCrash_MaxDistFromCameraSq_MaxDistFromCameraShowTimeSq_ZW.
    const f32 KF_SYMP_CRASH_MAX_DIST_FROM_CAMERA_SQ           = 1600.0f;
    const f32 KF_SYMP_CRASH_MAX_DIST_FROM_CAMERA_SHOWTIME_SQ  = 10000.0f;

    // flt_820BA8F8. A physical traffic car that has not driven for this long is something the
    // rest of the traffic has to get around, exactly like a crashing one. The same literal and
    // the same pairing with mbIsFatallyCrashing drive
    // JunctionFUP_TryClearupNonMovingPhysical @0x8273F2E8.
    const f32 KF_NOT_DRIVING_TIME_TO_COUNT_AS_A_CRASH = 6.0f;

    // flt_820BA5E4 (the module's shared 10.0f). Below this speed a param does not start a
    // sympathetic crash -- there is nothing to chain off.
    const f32 KF_MIN_SPEED_TO_START_SYMPATHETIC_CRASH = 10.0f;

    // `subfic r11, r11, 0x19` then `cmplwi r11, 4 ; blt` -- both consumers bail unless at
    // least this many of the 25 physical-traffic slots are still free. Reacting means possibly
    // becoming a physics body, so a full pool means no reaction.
    const u32 KU_MIN_FREE_PHYSICAL_SLOTS_TO_REACT = 4;

    // The race-car EntityId BuildListOfCrashingThings packs at 0x82737B78 (`slwi r11,r30,10`
    // then `oris r11,r11,0x100`). Same 14/10 split as MakeTrafficEntityId, owner byte 1 --
    // the same pair UpdateExtremeSwerving uses in _wT3_02.cpp.
    const u32 KU_RACE_CAR_PART_INDEX_SHIFT = 10;
    const u32 KU_RACE_CAR_OWNER_PACKED     = 0x01000000u;
    const u32 KU_NUM_BITS_FOR_ENTITY_NUM   = 14;

    // A showtime list entry only becomes a crash magnet with this bit set
    // (`lbz r11, 4(r29) ; rlwinm r11,r11,0,30,30` @0x82737C40). The producer,
    // SpawnShowtimeTraffic @0x82743038, has no body in this tree yet, and nothing else in the
    // image names the bit -- so it stays the console's own literal rather than an invented
    // enumerator. DebugComponent::DrawShowtime @0x8275CA58 tests the same bit.
    const u8 KU_SHOWTIME_INFO_FLAG_CRASH_MAGNET = 0x02u;

    // Unpack a traffic/race-car EntityId back to its entity index (the inverse of the 14/10
    // split above): `extrwi r5, r11, 14, 8` @0x827168CC.
    inline u32 EntityIdToEntityIndex(EntityId lEntityId)
    {
        return (lEntityId.muValue >> KU_RACE_CAR_PART_INDEX_SHIFT) &
               ((1u << KU_NUM_BITS_FOR_ENTITY_NUM) - 1u);
    }

    // The cone tuning members are Vector4 {cos(half-angle), length, recip-Y-scale, w}; the
    // console splats lanes 0/1/2 into IsPointWithinSquishedCone's three VecFloat arguments
    // (`vspltw128 v125/v126/v127, v0, 0/1/2`).
    inline VecFloat SplatLane(f32 lfValue)
    {
        const VecFloat lLane = { lfValue, lfValue, lfValue, lfValue };
        return lLane;
    }

    // ---- BRN_TRAFFIC_DIAG probes. NOT IN THE X360 BINARY. ------------------------------
    // Capped or rate-limited, so steady state costs one env lookup per call. These are how the
    // wave measured the reaction counts; DELETE-WHEN-STABLE.
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
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_BuildListOfCrashingThings  @0x82737270
//   (DWARF :1631; .cpp 10179..10270)
//
// The shared producer. Three appends, in the console's order.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_BuildListOfCrashingThings(
        ::Array<CrashingThingData, 168u>* lpaOutCurrentCrashingThings,
        const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
        const CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>& lrVehicles_Alive_And_Physical)
{
    CGS_ASSERT(lpaOutCurrentCrashingThings != 0, "lpOutCurrentCrashingThings");   // .cpp 10179
    CGS_ASSERT(lpInput != 0, "lpInput");                                          // .cpp 10180

    if (!mbAllowDivergentBehaviour)   // +0x717E7
    {
        return;
    }

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars =
        lpInput->GetActiveRaceCarOutputInterface();   // 0x82711850

    if (!lpActiveRaceCars->IsPlayerCarActive())
    {
        return;   // nobody to react around
    }

    u32 luDiagPhysical = 0;
    u32 luDiagRaceCars = 0;
    u32 luDiagShowtime = 0;

    // --- source 1: the alive AND physical traffic vehicles -------------------------------
    // The caller hands us the intersection it already built for UpdateParams_CalcDesiredSpeed.
    for (CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>::Iterator lItVehicle =
             lrVehicles_Alive_And_Physical.Begin();
         lItVehicle != lrVehicles_Alive_And_Physical.End();
         ++lItVehicle)
    {
        const u32 luVehicle = static_cast<u32>(lItVehicle.GetIndex());

        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC,
                   "Index is out of range (max bits: 600)");            // CgsFastBitArray.h 235

        // The console's second bound assert here (.h 2459, "luIndex < KU_MAX_TOTAL_TRAFFIC"
        // @0x82737524) is GetVehicle's own, inlined -- GetVehicle carries it verbatim, so
        // calling it reproduces the assert rather than adding one.
        const Vehicle* lpTargetVehicle = GetVehicle(luVehicle);
        CGS_ASSERT(lpTargetVehicle->IsAlive(), "lpTargetVehicle->IsAlive()");        // .cpp 10203
        CGS_ASSERT(lpTargetVehicle->IsPhysical(), "lpTargetVehicle->IsPhysical()");  // .cpp 10204

        bool lbIsACrashingThing = false;

        if (lpTargetVehicle->IsCrashing())
        {
            lbIsACrashingThing = true;
        }
        else if (NeedToTakeActionAgainstJunctionFUP())
        {
            // 0x82737664..0x827376F8. A physical car that is not crashing still counts when the
            // junction-fouling-up score is high AND it is sitting near the physical centre and
            // either deforming fatally or simply not driving.
            const Matrix44Affine lTargetTransform = GetVehicleTransform(luVehicle);
            const Vector3 lToCentre = mAveragePhysicalCentre - lTargetTransform.Pos();  // +0x725D0

            if (KF_JUNCTION_FUP_MAX_RADIUS_SQ >= rw::math::vpu::Dot(lToCentre, lToCentre))
            {
                const TrafficPhysicsInfo* lpPhysInfo = GetTrafficPhysicsInfoForVehicl(luVehicle);
                CGS_ASSERT(lpPhysInfo != 0, "lpPhysInfo");                          // .cpp 10222

                if (lpPhysInfo->mbIsFatallyCrashing ||                              // +0xFE6
                    lpPhysInfo->mfTimeNotDriving >=                                 // +0xFD8
                        KF_NOT_DRIVING_TIME_TO_COUNT_AS_A_CRASH)
                {
                    lbIsACrashingThing = true;
                }
            }
        }

        if (lbIsACrashingThing)
        {
            CrashingThingData lThing;
            lThing.mEntityId             = MakeTrafficEntityId(luVehicle);
            lThing.mbShowtimeCrashMagnet = false;
            lThing.mPosition             = GetVehicleTransform(luVehicle).Pos();
            lpaOutCurrentCrashingThings->Append(lThing);
            ++luDiagPhysical;
        }
    }

    // --- source 2: the active race cars ---------------------------------------------------
    for (EActiveRaceCarIndex leRaceCar = E_ACTIVE_RACE_CAR_INDEX_0;
         leRaceCar < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leRaceCar++)
    {
        CGS_ASSERT(leRaceCar >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");   // OutputInterface.h 854
        CGS_ASSERT(leRaceCar < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");// OutputInterface.h 855

        // 0x82737AF0 -- `lhzx r11, 0x2780(iface) ; clrlwi r11,r11,31` == maxRaceCarFlags & 1.
        if (!lpActiveRaceCars->IsRaceCarActive(leRaceCar))
        {
            continue;
        }

        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
            lpActiveRaceCars->GetRaceCarState(leRaceCar);
        CGS_ASSERT(lpRaceCarState != 0, "lpRaceCarState");                          // .cpp 10247

        // 0x82737B34..0x82737B54. Either debug force, or the car really is crashing.
        if (!mbDEBUGTestSympCrash &&                       // +0x7286A
            !lpRaceCarState->mbCrashing &&                 // state +0x44A
            !mbDEBUGFakeShowtime)                          // +0x72876
        {
            continue;
        }

        CGS_ASSERT(static_cast<u32>(leRaceCar) < (1u << KU_NUM_BITS_FOR_ENTITY_NUM),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");   // CgsEntityId.h 116

        CrashingThingData lThing;
        lThing.mEntityId.muValue = (static_cast<u32>(leRaceCar) << KU_RACE_CAR_PART_INDEX_SHIFT) |
                                   KU_RACE_CAR_OWNER_PACKED;
        lThing.mbShowtimeCrashMagnet = false;
        lThing.mPosition             = lpRaceCarState->mTransform.Pos();   // state +0x220
        lpaOutCurrentCrashingThings->Append(lThing);
        ++luDiagRaceCars;
    }

    // --- source 3: the showtime crash magnets ---------------------------------------------
    if (!mbPlayingShowtimeMode)   // +0x717DD
    {
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            static u32 suDiagFrame = 0;
            if ((suDiagFrame++ % 10u) == 0u)   // decision frames are 0.1 s -> ~1 line/s
            {
                *lpDiag << "[T5-crash] things=" << static_cast<s32>(luDiagPhysical + luDiagRaceCars)
                        << " physical=" << static_cast<s32>(luDiagPhysical)
                        << " racecars=" << static_cast<s32>(luDiagRaceCars)
                        << " showtime=0 [DELETE-WHEN-STABLE]\n";
            }
        }
        return;
    }

    for (u32 luShowtime = 0; luShowtime < muShowtimeVehicleInfoCount; ++luShowtime)   // +0x72480
    {
        const ShowtimeVehicleInfo* lpShowtimeInfo = &maShowtimeVehicleInfoList[luShowtime]; // +0x72380
        CGS_ASSERT(lpShowtimeInfo != 0, "lpShowtimeInfo");                          // .cpp 10270

        if ((lpShowtimeInfo->muFlags & KU_SHOWTIME_INFO_FLAG_CRASH_MAGNET) == 0)
        {
            continue;
        }

        const u32 luVehicle = lpShowtimeInfo->muVehicleIndex;
        CGS_ASSERT(luVehicle < (1u << KU_NUM_BITS_FOR_ENTITY_NUM),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");   // CgsEntityId.h 116

        CrashingThingData lThing;
        lThing.mEntityId = MakeTrafficEntityId(luVehicle);

        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC,
                   "luIndex < KU_MAX_TOTAL_TRAFFIC");                       // .h 2483

        lThing.mbShowtimeCrashMagnet = true;
        lThing.mPosition             = maVehicleTransforms[luVehicle].Pos();   // +0x1ECB0 + 64*i
        lpaOutCurrentCrashingThings->Append(lThing);
        ++luDiagShowtime;
    }

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        static u32 suDiagFrameShowtime = 0;
        if ((suDiagFrameShowtime++ % 10u) == 0u)
        {
            *lpDiag << "[T5-crash] things="
                    << static_cast<s32>(luDiagPhysical + luDiagRaceCars + luDiagShowtime)
                    << " physical=" << static_cast<s32>(luDiagPhysical)
                    << " racecars=" << static_cast<s32>(luDiagRaceCars)
                    << " showtime=" << static_cast<s32>(luDiagShowtime)
                    << " [DELETE-WHEN-STABLE]\n";
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_TryAvoidCrashing  @0x82716948  (.cpp 10489)
//
// This is the function that makes traffic swerve. A param whose forward cone holds a crashing
// thing other than itself drives AROUND it instead of through it. The cone is the member
// kfParamAvoidCrashCone_CosAngle_Length_RecipYScale_W @+0x727A0, seeded by Construct as
// { cos(10 deg), 30.0f, 0.25f, 0.0f } -- a 10-degree half-angle, 30 m long, with the vertical
// axis squashed to a quarter before the test.
//
// The three early-outs are the console's, in order: divergent behaviour off; the param has
// already been promoted to a physics body (a body is steered by physics, not by this); and the
// physical-traffic pool has fewer than four free slots.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_TryAvoidCrashing(
        u32 luParam,
        const ::Array<CrashingThingData, 168u>* lpaCrashingThings)
{
    CGS_ASSERT(lpaCrashingThings != 0, "lpCurrentCrashingThings");   // .cpp 10489

    if (!mbAllowDivergentBehaviour)   // +0x717E7
    {
        return;
    }

    CGS_ASSERT(luParam < KU_MAX_TOTAL_TRAFFIC,
               "Index is out of range (max bits: 600)");             // CgsFastBitArray.h 396

    if (mVehicleSoaData.mPhysicalVehicles.IsBitSet(luParam))   // +164800
    {
        return;
    }

    if ((KU_MAX_PHYSICAL_TRAFFIC_VEHICLES - maTrafficPhysicsInfoListBits.CountSetBits()) <
        KU_MIN_FREE_PHYSICAL_SLOTS_TO_REACT)
    {
        return;
    }

    Param* const lpParam = GetParam(luParam);
    const ParamTransform* const lpParamTransform = GetParamTransform(luParam);
    const EntityId lOurEntityId = MakeTrafficEntityId(luParam);

    // 0x82716C10..0x82716C30 -- the cone comes from the member, one splat per lane.
    const VecFloat lfConeCosAngle =
        SplatLane(kfParamAvoidCrashCone_CosAngle_Length_RecipYScale_W.x);   // +0x727A0
    const VecFloat lfConeLength =
        SplatLane(kfParamAvoidCrashCone_CosAngle_Length_RecipYScale_W.y);
    const VecFloat lfConeRecipYScale =
        SplatLane(kfParamAvoidCrashCone_CosAngle_Length_RecipYScale_W.z);

    for (u32 luThing = 0; luThing < lpaCrashingThings->GetLength(); ++luThing)
    {
        const CrashingThingData& lrThing = (*lpaCrashingThings)[luThing];

        if (lrThing.mEntityId.muValue == lOurEntityId.muValue)
        {
            continue;   // never swerve around yourself
        }

        if (!IsPointWithinSquishedCone(lpParamTransform->GetDeterministicPos(),
                                       lpParamTransform->GetDirection(),
                                       lfConeCosAngle, lfConeLength, lfConeRecipYScale,
                                       lrThing.mPosition))
        {
            continue;
        }

        // 0x82716C78: `li r11, 2 ; stb r11, 0x1B(param)`.
        lpParam->miBehaviour = Param::KI_BEHAVIOUR_DRIVING_AROUND_OBSTRUCTION;

        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            static u32 suAvoidLogged = 0;
            const u32 KU_AVOID_LOG_CAP = 64;
            if (suAvoidLogged < KU_AVOID_LOG_CAP)
            {
                ++suAvoidLogged;
                const Vector3 lParamPos = lpParamTransform->GetDeterministicPos();
                const Vector3 lToThing  = lrThing.mPosition - lParamPos;
                *lpDiag << "[T5-avoid] param=" << static_cast<s32>(luParam)
                        << " thing=" << static_cast<s32>(luThing)
                        << " ofNThings=" << static_cast<s32>(lpaCrashingThings->GetLength())
                        << " distSq=" << rw::math::vpu::Dot(lToThing, lToThing)
                        << " behaviour=DRIVING_AROUND_OBSTRUCTION"
                        << " [DELETE-WHEN-STABLE]\n";
            }
        }
        return;
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_TryStartSympatheticCrashing  @0x827165D8  (.cpp 10375)
//
// The chain-reaction half. A moving param close enough to the camera, with a crashing thing in
// its cone, drops into E_BEHAVIOUR_SLOWING_FOR_CRASH and latches the thing's id as its
// sympathetic-crash target (UpdateSympatheticCrashing @0x8273D378 is what consumes that).
//
// The camera proximity test is done on the VEHICLE transform (not the param's deterministic
// pos), and it is two-part: within the squared radius AND in front of the camera. The showtime
// case takes the wider radius (100 m vs 40 m) and the wider cone (cos 20 deg / 50 m).
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_TryStartSympatheticCrashing(
        u32 luParam,
        const ::Array<CrashingThingData, 168u>* lpaCrashingThings)
{
    CGS_ASSERT(lpaCrashingThings != 0, "lpCurrentCrashingThings");   // .cpp 10375

    if (!mbAllowDivergentBehaviour)   // +0x717E7
    {
        return;
    }

    if ((KU_MAX_PHYSICAL_TRAFFIC_VEHICLES - maTrafficPhysicsInfoListBits.CountSetBits()) <
        KU_MIN_FREE_PHYSICAL_SLOTS_TO_REACT)
    {
        return;
    }

    Param* const lpParam = GetParam(luParam);

    // 0x827166F0: a param already in behaviour 0 (SLOWING_FOR_CRASH) has nothing to start.
    if (lpParam->miBehaviour == Param::KI_BEHAVIOUR_SLOWING_FOR_CRASH)
    {
        return;
    }

    if (lpParam->mfSpeed < KF_MIN_SPEED_TO_START_SYMPATHETIC_CRASH)   // param +0x14
    {
        return;
    }

    // 0x8271673C..0x82716764 -- distance and in-front-ness relative to LAST FRAME's camera.
    const Matrix44Affine lVehicleTransform = GetVehicleTransform(luParam);
    const Vector3 lFromCamera = lVehicleTransform.Pos() - mCameraLastFrame.GetPosition(); // +0x728C0
    const f32 lfAlongCamera =
        rw::math::vpu::Dot(mCameraLastFrame.GetDirection(), lFromCamera);                 // +0x728B0
    const f32 lfDistFromCameraSq = rw::math::vpu::Dot(lFromCamera, lFromCamera);
    const bool lbBehindCamera = !(lfAlongCamera >= 0.0f);

    Vector4 lCone;
    if (mbPlayingShowtimeMode)   // +0x717DD
    {
        if (lfDistFromCameraSq >= KF_SYMP_CRASH_MAX_DIST_FROM_CAMERA_SHOWTIME_SQ)  // lane 1
        {
            return;
        }
        if (lbBehindCamera)
        {
            return;
        }
        lCone = kfParamSympatheticConeShowTime_CosAngle_Length_RecipYScale_W;   // +0x726F0
    }
    else
    {
        if (lfDistFromCameraSq >= KF_SYMP_CRASH_MAX_DIST_FROM_CAMERA_SQ)        // lane 0
        {
            return;
        }
        if (lbBehindCamera)
        {
            return;
        }
        lCone = kfParamSympatheticCone_CosAngle_Length_RecipYScale_W;           // +0x726E0
    }

    const VecFloat lfConeCosAngle    = SplatLane(lCone.x);
    const VecFloat lfConeLength      = SplatLane(lCone.y);
    const VecFloat lfConeRecipYScale = SplatLane(lCone.z);

    const ParamTransform* const lpParamTransform = GetParamTransform(luParam);
    const EntityId lOurEntityId = MakeTrafficEntityId(luParam);

    for (u32 luThing = 0; luThing < lpaCrashingThings->GetLength(); ++luThing)
    {
        const CrashingThingData& lrThing = (*lpaCrashingThings)[luThing];

        if (lrThing.mEntityId.muValue == lOurEntityId.muValue)
        {
            continue;
        }

        if (!IsPointWithinSquishedCone(lpParamTransform->GetDeterministicPos(),
                                       lpParamTransform->GetDirection(),
                                       lfConeCosAngle, lfConeLength, lfConeRecipYScale,
                                       lrThing.mPosition))
        {
            continue;
        }

        // 0x827168A8..0x82716904. Outside showtime, and for any non-magnet entry, the cone hit
        // is enough. A showtime CRASH MAGNET additionally has to be travelling roughly the way
        // we are (`vcmpgtfp128. 0, dot` -> take the crash only when the dot is NOT negative),
        // so the chain follows the flow of traffic instead of firing at oncoming cars.
        if (mbPlayingShowtimeMode && lrThing.mbShowtimeCrashMagnet)
        {
            const Matrix44Affine lTargetTransform =
                GetVehicleTransform(EntityIdToEntityIndex(lrThing.mEntityId));
            const f32 lfFacing = rw::math::vpu::Dot(lTargetTransform.At(),
                                                    lpParamTransform->GetDirection());
            if (0.0f > lfFacing)
            {
                continue;
            }
        }

        // 0x8271692C: `stb 0, 0x1B(param)` then `stw thingId, 0x48(param)`.
        lpParam->miBehaviour     = Param::KI_BEHAVIOUR_SLOWING_FOR_CRASH;
        lpParam->mSympCrashTarget = lrThing.mEntityId;

        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            static u32 suSympLogged = 0;
            const u32 KU_SYMP_LOG_CAP = 64;
            if (suSympLogged < KU_SYMP_LOG_CAP)
            {
                ++suSympLogged;
                const Vector3 lParamPos = lpParamTransform->GetDeterministicPos();
                const Vector3 lToThing  = lrThing.mPosition - lParamPos;
                *lpDiag << "[T5-symp] param=" << static_cast<s32>(luParam)
                        << " target=" << static_cast<s32>(EntityIdToEntityIndex(lrThing.mEntityId))
                        << " magnet=" << (lrThing.mbShowtimeCrashMagnet ? 1 : 0)
                        << " distSq=" << rw::math::vpu::Dot(lToThing, lToThing)
                        << " camDistSq=" << lfDistFromCameraSq
                        << " [DELETE-WHEN-STABLE]\n";
            }
        }
        return;
    }
}
}
