// =================================================================================================
// GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule_wT3_02.cpp
//
// The driver-input producer leg.
//   TrafficEntityModule::UpdateVehicleStuckTimers @0x82708D48 (33 insns)
//   TrafficEntityModule::GenerateDriverInputs     @0x82748E78 (1,439 insns)
//   TrafficEntityModule::CalculateDriverGasBrake  @0x82718CD8
//   TrafficEntityModule::CalculateAndSetSteering  @0x82718E48
//   TrafficEntityModule::DriveTowardsTarget       @0x8273DFC0  PARTIAL
//   TrafficEntityModule::UpdateNormalPhysical     @0x8273EF08
//   TrafficEntityModule::UpdateExtremeSwerving    @0x8273E8D0
// The NORMAL and EXTREME_SWERVE manoeuvre arms are live; the other five are gated at the
// dispatch site.
// =================================================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"      // KU_INVALID_VEHICLE
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"          // Param
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"                       // KU_ENTITYTYPE_TRAFFIC_VEHICLE
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"                            // mpaVehicleTypesUpdate
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"

#include "rw/math/vpu/vector3_operation.h"   // Dot, Cross, IsValid

#include <cmath>     // std::sqrt

namespace BrnTraffic
{
namespace
{
    // UpdateVehicleStuckTimers' own RODATA (headless idat): flt_820BA86C == 2.0f (the value a
    // side timer is RESET to when it starts accumulating) and flt_82004014 == 0.1f (the
    // threshold). THEY ARE NOT PARAMETERS ON THE CONSOLE: the X360 body loads both itself
    // (lfs at 0x82708DA0 / 0x82708D9C) and its one caller GenerateDriverInputs @0x827492F8 sets
    // neither f1 nor f2. BrnTrafficEntityModule.h declares an (f32, f32) pair the console does
    // not have; the body honours the declaration.
    // PARK: retire that pair when the header is next opened.
    const f32 KF_STUCK_SIDE_TIMER_RESET     = 2.0f;
    const f32 KF_STUCK_SIDE_TIMER_THRESHOLD = 0.1f;

    // The two bits of TrafficPhysicsInfo::muContactSideFlags the console tests (`li r5, 1` then
    // `li r5, 2` at 0x82708D94 / 0x82708DB0), paired with mfStuckTimeFront (+0xFCC) and
    // mfStuckTimeBack (+0xFD0) in that order.
    const s32 KI_CONTACT_SIDE_FRONT = 1;
    const s32 KI_CONTACT_SIDE_BACK  = 2;

    // GenerateDriverInputs' own RODATA, dumped headless from the ARTIST image:
    //   flt_820047C8 == 0.05f  stuck-timer threshold that forces gas/brake to zero and re-sends
    //   flt_820BA5E4 == 10.0f  mfTimeNotDriving after which the car gives up
    //   flt_820BA8BC == 0.3f   per-frame gas ramp on the Showtime divergent-behaviour leg
    //   flt_82001C98 == 1.0f (f30)   flt_82001CC0 == 0.0f (f31)
    const f32 KF_STUCK_SEND_THRESHOLD     = 0.05f;
    const f32 KF_GIVE_UP_TIME_NOT_DRIVING = 10.0f;
    const f32 KF_SHOWTIME_GAS_RAMP        = 0.3f;
    const f32 KF_MAX_GAS                  = 1.0f;

    // The record seeds at 0x827492FC..0x82749360. mfBoostMaxSpeedScale takes f30 (1.0f) and
    // miVehicleIDToMerge takes `li r11,-1`; every other float takes f31 (0.0f) and every other
    // bool takes r29 (0). mbToggle (+0x3A) is the one field the console does NOT write.
    const s8 KI8_NO_VEHICLE_TO_MERGE = -1;

    // The queue key at `li r5, 3` (0x82749774 and 0x8274982C). The consumer switch spells it
    // E_DRIVER_TYPE_TRAFFIC (BrnVehicleManager_UpdateDrivers.cpp:152).
    const s32 KI_DRIVER_EVENT_TYPE_TRAFFIC = 3;

    // `li r4, 1` into Vehicle::SetCurrentManoeuvrePhase at 0x827497FC.
    const s8 KI8_GIVE_UP_PHASE = 1;

    // ---- the normal-physical driving leg's own RODATA (headless idat) --------
    // CalculateDriverGasBrake @0x82718CD8
    const f32 KF_DRIVER_OPTIMAL_TIME_TO_TARGET = 1.5f;    // flt_820BA5DC
    const f32 KF_DRIVER_PEDAL_GAIN             = 0.5f;    // flt_820BA62C
    const f32 KF_DRIVER_MIN_CLOSING_SPEED      = 0.1f;    // flt_82004014
    const f32 KF_DRIVER_STALLED_TIME_TO_TARGET = 100.0f;  // flt_820BA5C8
    // CalculateAndSetSteering @0x82718E48. The threshold is lane 3 of unk_8300CBE0, a dyn-init
    // .data vector recovered from its thunk at 0x82C66E50: {0.2, 1.0, 0.94, 0.6}.
    const f32 KF_STEERING_SCALE_THRESHOLD      = 0.6f;
    const f32 KF_STEERING_SCALE_HIGH           = 1.3f;    // flt_820BA554
    // DriveTowardsTarget @0x8273DFC0
    const f32 KF_GIVE_UP_RANDOM_CHANCE             = 0.05f;   // flt_820047C8
    // flt_8300C950, a dyn-init product recovered from its thunk at 0x82C66C30:
    // flt_82F31928 (0.44704, mph->m/s) * flt_820BA5E4 (10.0) == 10 mph in m/s.
    const f32 KF_GIVE_UP_SPEED                     = 4.4704f;
    const f32 KF_DRIVER_FAR_FROM_TARGET_DIST       = 20.0f;   // flt_820BA7E4 @0x8273E2A4
    const f32 KF_DRIVER_MIN_PHYSICAL_TIME_TO_RETURN = 5.0f;   // flt_8200426C
    const f32 KF_DRIVER_RETURN_TO_TRAFFIC_DIST     = 1.5f;    // flt_820BA5DC
    const f32 KF_DRIVER_RETURN_TO_TRAFFIC_DOT      = 0.98f;   // flt_820BA55C @0x8273E440
    const f32 KF_DRIVER_SWERVE_STEERING_TIME       = 3.0f;    // flt_820BA5F4
    const f32 KF_DRIVER_REVERSE_TURN_DIST          = -15.0f;  // folded -15.0

    // ---- UpdateExtremeSwerving @0x8273E8D0's own RODATA (headless idat) ----
    //   flt_8200473C  == 0.4f    the crash-slider level below which the arm just drives
    //   unk_8300C9F0  == splat(1600.0f), dyn-init thunk 0x82C66C50 from flt_820BA810 (1600) --
    //                            a SQUARED distance, so 40 m to the local player's car
    //   flt_820BA5B4  == 0.8f    per-frame roll that keeps a far car driving instead of crashing
    //   flt_82009D58  == 35.0f / flt_820BA5E8 == 30.0f  the (slider*35 + 30) percentage split
    //                            between the two sympathetic-crash styles
    const f32 KF_EXTREME_SWERVE_CRASH_SLIDER_MIN   = 0.4f;
    const f32 KF_EXTREME_SWERVE_PLAYER_DIST_SQ     = 1600.0f;
    const f32 KF_EXTREME_SWERVE_KEEP_DRIVING_ROLL  = 0.8f;
    const f32 KF_SYMP_CRASH_STYLE_SLIDER_SCALE     = 35.0f;
    const f32 KF_SYMP_CRASH_STYLE_BASE_PERCENT     = 30.0f;
    // `mulhwu 0x446F8657` + shifts at 0x8273EACC..0x8273EAF8 == unsigned % 101.
    const u32 KU_SYMP_CRASH_PERCENT_MODULUS        = 101u;
    // The race-car EntityId UpdateExtremeSwerving packs at 0x8273EA50 (`slwi r11,r29,10` then
    // `oris r11,r11,0x100`). Same 14/10 split as MakeTrafficEntityId, owner byte 1.
    const u32 KU_RACE_CAR_PART_INDEX_SHIFT         = 10;
    const u32 KU_RACE_CAR_OWNER_PACKED             = 0x01000000u;
    const u32 KU_NUM_BITS_FOR_ENTITY_NUM_LOCAL     = 14;

    inline VecFloat SplatDrive(f32 lfValue)
    {
        const VecFloat lLane = { lfValue, lfValue, lfValue, lfValue };
        return lLane;
    }

    inline Vector3 ZeroVector3()
    {
        Vector3 lZero;
        lZero.SetZero();
        return lZero;
    }

    // One-shot gate banner -- NOT IN THE X360 BINARY. Retire with the last gate below.
    void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndAddress)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T3-drive] GenerateDriverInputs leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndAddress << "\n";
        }
    }
}

// -------------------------------------------------------------------------------------------
// TrafficEntityModule::UpdateVehicleStuckTimers   @0x82708D48   (33 insns)
//
// Advance one physical traffic car's two "wedged against something" timers, one per contact
// side. The whole body is two calls to the header-inline UpdateVehicleStuckSideTime, differing
// only in the mask and the timer they point at:
//   0x82708D88  lbz  r4, 0x1008(info)      == muContactSideFlags
//   0x82708D90  addi r8, info, 0xFCC       == &mfStuckTimeFront   , mask 1
//   0x82708DA8  addi r8, info, 0xFD0       == &mfStuckTimeBack    , mask 2
// Both offsets are reached BY NAME here; the console record is 0x100C bytes and the host one is
// not (the embedded DetachedPartRenderQueue widens), so an offset transcription would have
// written past the front timer into the skinning scratch.
//
// muContactSideFlags IS RE-READ between the two calls (`lbz r4, 0x1008(r31)` at 0x82708DAC).
// UpdateVehicleStuckSideTime writes only through lpfTimer, so the two reads cannot differ --
// reproduced as two reads anyway, because that is what the console emits.
// -------------------------------------------------------------------------------------------
void TrafficEntityModule::UpdateVehicleStuckTimers(void* lpPhysicsInfo, f32 lfReset, f32 lfThreshold)
{
    CGS_ASSERT(lpPhysicsInfo != 0, "lpPhysicsInfo");        // .cpp:17412

    TrafficPhysicsInfo* const lpInfo = static_cast<TrafficPhysicsInfo*>(lpPhysicsInfo);

    UpdateVehicleStuckSideTime(static_cast<s32>(lpInfo->muContactSideFlags), KI_CONTACT_SIDE_FRONT,
                               lfReset, lfThreshold, &lpInfo->mfStuckTimeFront);

    UpdateVehicleStuckSideTime(static_cast<s32>(lpInfo->muContactSideFlags), KI_CONTACT_SIDE_BACK,
                               lfReset, lfThreshold, &lpInfo->mfStuckTimeBack);
}

// -------------------------------------------------------------------------------------------
// TrafficEntityModule::GenerateDriverInputs   @0x82748E78   (1,439 insns)   DWARF :1356
//
// The producer of BrnTrafficDriverControls. Structure read off the ASM; the Hex-Rays view is
// the degenerate "local variable allocation has failed" form and was not used.
//
// THE ITERATION SET IS AN INTERSECTION OF TWO SoA BIT SETS, built on the stack at
// 0x82748F0C..0x82748F40 as ten doublewords of `this + 8*(0x5078+i) & this + 8*(0x505A+i)`.
// The four console byte bases resolve against mVehicleSoaData at module +164560 (attested
// independently by _wT1_01.cpp:876 and _wT2_02.cpp:100, both mounted), each FastBitArray<601>
// being 80 bytes:
//   164560 (+0)   mAliveVehicles                    164800 (+240) mPhysicalVehicles
//   165040 (+480) mPhysicalVehiclesFarFromPlayer    165120 (+560) mPhysicalVehiclesTryingToRecover
// The last two are the pair CLEARED in place at 0x82748F48/0x82748F68. Every access below is by
// name; no console displacement reaches the host.
//
// The SEND label at 0x82749814 is taken whenever GetPhysicalReason() == E_PHYSICALREASON_CRASHED
// (0x827493A4 `lbz r11, 0x39 ; cmplwi 0 ; beq`): a traffic car promoted because the player
// crashed into it gets a ZERO-CONTROL record and never reaches a manoeuvre arm at all.
// -------------------------------------------------------------------------------------------
void TrafficEntityModule::GenerateDriverInputs(BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput)
{
    CGS_ASSERT(lpOutput != 0, "lpOutputBuffer != NULL");                       // .cpp:15794

    BrnPhysics::Vehicle::VehicleDriverInputInterface* const lpDriverInputInterface =
        lpOutput->GetVehicleDriverInterface();                                 // 0x82748EDC

    CGS_ASSERT(lpDriverInputInterface != 0, "lpDriverInputInterface != NULL"); // .cpp:15801

    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> lPhysicalAliveVehicles;
    lPhysicalAliveVehicles.SetAnd(mVehicleSoaData.mAliveVehicles,
                                  mVehicleSoaData.mPhysicalVehicles);

    mVehicleSoaData.mPhysicalVehiclesFarFromPlayer.UnSetAll();
    mVehicleSoaData.mPhysicalVehiclesTryingToRecover.UnSetAll();

    for (CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>::Iterator lIterator =
             lPhysicalAliveVehicles.Begin();
         lIterator != lPhysicalAliveVehicles.End();
         ++lIterator)
    {
        const s32 liVehicle = lIterator.GetIndex();

        // 0x827491C0 / 0x82749838 -- the console range-checks the index at BOTH ends of the
        // body (CgsFastBitArray.h:235, "Index has gone out of range").
        CGS_ASSERT(liVehicle >= 0 && liVehicle < static_cast<s32>(KU_MAX_TOTAL_TRAFFIC),
                   "Index has gone out of range");

        // GATE TrafficEntityModule (second section) @0x82749B48 -- the >= 400 static/parked pool
        // arm, ~600 insns with its own TryClearupOffscreenTraffic @0x8274A1E8.
        // Blocker: parked cars are never promoted on this build (only standard traffic is).
        // DELETE-WHEN the static pool gains physical promotion.
        if (liVehicle >= static_cast<s32>(KU_MAX_STANDARD_TRAFFIC))            // 0x82749224
        {
            static bool sbLoggedStaticPool = false;
            LogMissingLeg(sbLoggedStaticPool, "the static/parked pool section @0x82749B48");
            continue;
        }

        // GATE TrafficEntityModule::TryClearupOffscreenTraffic @0x8273C4C8 (453) -- removal
        // polish; the console `continue`s when it returns true. BLOCKER: unreconstructed, and it
        // is the only demotion route for an outrun car. DELETE-WHEN it lands. Taken as false.
        {
            static bool sbLoggedClearup = false;
            LogMissingLeg(sbLoggedClearup, "TryClearupOffscreenTraffic @0x8273C4C8");
        }

        Vehicle* const lpVehicle = GetVehicle(static_cast<u32>(liVehicle));    // 0x82749244

        CGS_ASSERT(lpVehicle->IsAlive(), "lpVehicle->IsAlive()");              // .cpp:15833
        CGS_ASSERT(lpVehicle->IsPhysical(), "lpVehicle->IsPhysical()");        // .cpp:15834

        // 0x827492A8..0x827492D4. DriveTowardsTarget reads mfPhysicalTime against 5.0 and 3.0,
        // so both timers are armed now that BrnTrafficVehicle.h carries the accessors.
        lpVehicle->AddPhysicalTime(mfSimTimeStep);
        lpVehicle->AddManoeuvreTime(mfSimTimeStep);

        TrafficPhysicsInfo* const lpInfo =
            GetTrafficPhysicsInfoForVehicl(static_cast<u32>(liVehicle));       // 0x827492D8

        lpInfo->mfTimeNotDriving += mfSimTimeStep;                             // 0x827492F4

        UpdateVehicleStuckTimers(lpInfo, KF_STUCK_SIDE_TIMER_RESET,
                                 KF_STUCK_SIDE_TIMER_THRESHOLD);              // 0x827492F8

        // The record seeds, in the console's own field order.
        BrnPhysics::Vehicle::BrnTrafficDriverControls lControls;
        lControls.miVehicleID               = liVehicle;
        lControls.mfGas                     = 0.0f;
        lControls.mfBrake                   = 0.0f;
        lControls.mfHandBrake               = 0.0f;
        lControls.mfSteering                = 0.0f;
        lControls.mfForwardSteering         = 0.0f;
        lControls.mfSpin                    = 0.0f;
        lControls.mfRequestedGas            = 0.0f;
        lControls.mfAftertouchLevel         = 0.0f;
        lControls.mfXSensor                 = 0.0f;
        lControls.mfYSensor                 = 0.0f;
        lControls.mfZSensor                 = 0.0f;
        lControls.mfGSensor                 = 0.0f;
        lControls.mfBoostMaxSpeedScale      = KF_MAX_GAS;
        lControls.miVehicleIDToMerge        = KI8_NO_VEHICLE_TO_MERGE;
        lControls.mbReset                   = false;
        lControls.mbBoost                   = false;
        lControls.mbIsInvulnerableToVehicles = false;
        lControls.mbIsInvulnerableToWorld   = false;
        lControls.mbForceDrift              = false;
        lControls.mbBoostBounce             = false;
        lControls.mbIsOnStartLine           = false;
        lControls.mbIsSteeringWheel         = false;
        lControls.mbHorn                    = false;

        bool lbSend = true;                                                    // `li r23, 1`

        const Vehicle::Manoeuvre leManoeuvre = lpVehicle->GetCurrentManoeuvre();

        // 0x8274936C / 0x827493A4 -- the two early-SEND predicates. mbIsFatallyCrashing is the
        // console's `lbz r11, 0xFE6(info)`: 0xFE4..0xFE7 are miNumLightLocators / mbIsDeforming /
        // mbIsFatallyCrashing / mu8RenderDamageFlags, pinned by mfStuckTimeFront @0xFCC and
        // muContactSideFlags @0x1008 on either side.
        const bool lbEarlySend = lpInfo->mbIsFatallyCrashing
                              || lpVehicle->GetPhysicalReason() == E_PHYSICALREASON_CRASHED;

        if (!lbEarlySend)
        {
            // 0x827493B0..0x827496AC -- the console's manoeuvre dispatch, in its own order and
            // with its own lbSend joins. Two arms REACH `lbSend = 0` (`mr r23, r29` @0x827496AC,
            // r29 == 0) and send NOTHING that frame; STUCK_REVERSE branches over the join
            // (0x827493DC `b loc_827496B0`) and is always sent.
            if (leManoeuvre == Vehicle::E_MANOEUVRE_STUCK_REVERSE)              // 0x827493B0
            {
                // GATE UpdateStuckReverseManoeuvre @0x82719430 -- exported, no body.
                // DELETE-WHEN it lands. The car keeps the zero-control record AND is sent.
                static bool sbLoggedStuckArm = false;
                LogMissingLeg(sbLoggedStuckArm,
                              "GenerateDriverInputs arm UpdateStuckReverseManoeuvre "
                              "@0x82719430 -- no body");
            }
            else if (lpVehicle->IsExtremeSwerving())                            // 0x82749478
            {
                // 0x8274949C, with the console's own argument set (r4 vehicle, r5 arg_1C ==
                // lpOutput, r6 &lControls). Every promotion on this build arrives as reason
                // SWERVING, so this is THE arm.
                UpdateExtremeSwerving(static_cast<u32>(liVehicle), lpOutput, &lControls);
            }
            else if (leManoeuvre == Vehicle::E_MANOEUVRE_NONE)                  // 0x827494B4
            {
                if (lpVehicle->IsSympatheticallyCrashing())                     // 0x82749584
                {
                    // GATE UpdateSympatheticCrashing @0x8273D378 -- exported, no body; its
                    // target argument also needs Vehicle::GetSympatheticCrashTarget @0x82705450.
                    static bool sbLoggedSympArm = false;
                    LogMissingLeg(sbLoggedSympArm,
                                  "GenerateDriverInputs arm UpdateSympatheticCrashing "
                                  "@0x8273D378 -- no body");
                }
                else if (lpVehicle->IsRecoveringFromSlam())                     // 0x827495DC
                {
                    // GATE UpdateRecoveringFromSlam @0x8273E778 -- exported, no body.
                    static bool sbLoggedSlamArm = false;
                    LogMissingLeg(sbLoggedSlamArm,
                                  "GenerateDriverInputs arm UpdateRecoveringFromSlam "
                                  "@0x8273E778 -- no body");
                }
                else if (lpVehicle->IsNormalPhysical())                         // 0x82749618
                {
                    UpdateNormalPhysical(static_cast<u32>(liVehicle), &lControls); // 0x82749638
                }
                else if (lpVehicle->IsBeingChecked())                           // 0x82749654
                {
                    // 0x82749664 -- the console FIRES a streamed assert here and still sends
                    // the zero record (lbSend stays 1). Reported once instead of aborting the
                    // boot: a checked car is a normal in-game state on this build.
                    static bool sbLoggedChecked = false;
                    LogMissingLeg(sbLoggedChecked,
                                  "GenerateDriverInputs: a BeingChecked vehicle with manoeuvre "
                                  "NONE reached 0x82749664, where the console fires a streamed "
                                  "assert and still sends the zero-control record");
                }
                else
                {
                    lbSend = false;                                             // 0x827496AC
                }
            }
            else if (leManoeuvre == Vehicle::E_MANOEUVRE_EXTREME_SWERVE)        // jpt case 0
            {
                // Manoeuvre EXTREME_SWERVE without IsExtremeSwerving() sends nothing at all.
                lbSend = false;                                                 // 0x827496AC
            }
            else if (leManoeuvre == Vehicle::E_MANOEUVRE_3_POINT_TURN)          // jpt case 1
            {
                // GATE Update3PointTurnManoeuvre @0x827190B0 -- exported, no body. Reached by
                // DriveTowardsTarget's reverse-turn leg (SetCurrentManoeuvre(2) @0x8273E6F0).
                static bool sbLogged3PtArm = false;
                LogMissingLeg(sbLogged3PtArm,
                              "GenerateDriverInputs arm Update3PointTurnManoeuvre @0x827190B0 "
                              "-- no body");
            }
            else if (leManoeuvre == Vehicle::E_MANOEUVRE_GIVE_UP)               // jpt case 2
            {
                // GATE UpdateGiveUpManoeuvre @0x8273EB60 -- exported, no body. Reached by the
                // ten-second no-driving latch at the tail of this loop.
                static bool sbLoggedGiveUpArm = false;
                LogMissingLeg(sbLoggedGiveUpArm,
                              "GenerateDriverInputs arm UpdateGiveUpManoeuvre @0x8273EB60 "
                              "-- no body");
            }
            else
            {
                CGS_ASSERT(false, "Unknown manoeuvre");                          // 0x82749540
            }

            // GATE TrafficEntityModule::DEBUG_ValidateEmDriverControls @0x82708FF8 (204) --
            // debug-only validator the console runs after every arm and before every AddEvent.
            // DELETE-WHEN it lands; it has no effect on the record.

            // 0x827496B0 -- an arm can kill the vehicle, so the console re-checks liveness.
            if (!lpVehicle->IsAlive())
            {
                continue;
            }

            // 0x827496C8 -- a wedged car has its gas and brake forced off and is always sent.
            if (leManoeuvre != Vehicle::E_MANOEUVRE_STUCK_REVERSE
                && (lpInfo->mfStuckTimeFront > KF_STUCK_SEND_THRESHOLD
                    || lpInfo->mfStuckTimeBack > KF_STUCK_SEND_THRESHOLD))
            {
                lControls.mfGas   = 0.0f;
                lControls.mfBrake = 0.0f;
                lbSend            = true;
            }

            if (lbSend)
            {
                // 0x8274971C -- the Showtime scatter leg: ramp the gas by 0.3f per frame,
                // clamped to 1.0f (`fsubs` + `fsel` against f30), and hold the brake off.
                if (mbPlayingShowtimeMode && mbAllowDivergentBehaviour)
                {
                    const f32 lfRampedGas = lControls.mfGas + KF_SHOWTIME_GAS_RAMP;
                    lControls.mfBrake = 0.0f;
                    lControls.mfGas   = (lfRampedGas >= KF_MAX_GAS) ? KF_MAX_GAS : lfRampedGas;
                }

                lpDriverInputInterface->GetUpdateDriverQueue()
                    ->AddEvent<BrnPhysics::Vehicle::BrnTrafficDriverControls>(
                        &lControls, KI_DRIVER_EVENT_TYPE_TRAFFIC);             // 0x8274977C

                if (lControls.mfGas > 0.0f || lControls.mfBrake > 0.0f)
                {
                    lpInfo->mfTimeNotDriving = 0.0f;
                }
            }

            // 0x8274979C -- ten seconds of no driving and the car gives up. The console open-
            // codes Vehicle::SetCurrentManoeuvre (phase 0 on change, mfManoeuvreTime 0, then the
            // manoeuvre byte) and follows it with SetCurrentManoeuvrePhase(1).
            if (lpVehicle->GetCurrentManoeuvre() != Vehicle::E_MANOEUVRE_GIVE_UP
                && lpInfo->mfTimeNotDriving >= KF_GIVE_UP_TIME_NOT_DRIVING)
            {
                lpVehicle->SetCurrentManoeuvre(Vehicle::E_MANOEUVRE_GIVE_UP);
                lpVehicle->SetCurrentManoeuvrePhase(KI8_GIVE_UP_PHASE);
                lpInfo->mfTimeNotDriving = 0.0f;
            }
        }
        else
        {
            // SEND @0x82749814 -- DEBUG_ValidateEmDriverControls (gated above) then the same
            // AddEvent, with none of the post-dispatch work.
            lpDriverInputInterface->GetUpdateDriverQueue()
                ->AddEvent<BrnPhysics::Vehicle::BrnTrafficDriverControls>(
                    &lControls, KI_DRIVER_EVENT_TYPE_TRAFFIC);                 // 0x82749834
        }
    }
}

// ============================================================================================
// The normal-physical driving leg. UpdateNormalPhysical is the manoeuvre arm GenerateDriverInputs
// dispatches to for a car promoted with E_PHYSICALREASON_NORMAL; everything below it is the
// shared "drive at Vehicle::GetTargetPos" machinery.
// ============================================================================================

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::CalculateDriverGasBrake  @0x82718CD8  (.cpp 16109..16130)
//
// One signed pedal value in [-1, 1]: positive is gas, negative is brake. It is a time-to-target
// controller -- (distance / closing speed - the optimal 1.5 s) * 0.5, clamped -- so a car that
// would arrive too soon lifts off and a car that is falling behind accelerates.
// Every constant is the value IDA folds for the named .rdata symbol in the listing.
// --------------------------------------------------------------------------------------------
VecFloat TrafficEntityModule::CalculateDriverGasBrake(u32 luVehicle, VecFloat lfDistToTarget,
                                                      Vector3 lParamLinearVelocity)
{
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");  // .h 2467

    const f32 lfOptimalTime = KF_DRIVER_OPTIMAL_TIME_TO_TARGET;   // flt_820BA5DC 1.5
    const f32 lfGain        = KF_DRIVER_PEDAL_GAIN;               // flt_820BA62C 0.5
    const f32 lfMinSpeed    = KF_DRIVER_MIN_CLOSING_SPEED;        // flt_82004014 0.1
    const f32 lfStalledTime = KF_DRIVER_STALLED_TIME_TO_TARGET;   // flt_820BA5C8 100.0

    const Vector3 lRelativeVelocity =
        lParamLinearVelocity - GetVehicle(luVehicle)->GetLinearVelocity();

    // 0x82718DC4..0x82718DFC -- |relative velocity|, with the exactly-zero case selected to 0.
    const f32 lfSpeedSq = rw::math::vpu::Dot(lRelativeVelocity, lRelativeVelocity);
    const f32 lfSpeed   = (lfSpeedSq == 0.0f) ? 0.0f : std::sqrt(lfSpeedSq);

    // 0x82718E00..0x82718E18 -- below the epsilon the divide is meaningless, so the console
    // substitutes a "practically stationary" time of 100 s.
    const f32 lfTimeToTarget =
        (lfSpeed > lfMinSpeed) ? (lfDistToTarget.x / lfSpeed) : lfStalledTime;

    // 0x82718E1C..0x82718E28
    f32 lfPedal = (lfTimeToTarget - lfOptimalTime) * lfGain;
    lfPedal = (lfPedal < -1.0f) ? -1.0f : lfPedal;
    lfPedal = (lfPedal >  1.0f) ?  1.0f : lfPedal;

    return SplatDrive(lfPedal);
}

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::CalculateAndSetSteering  @0x82718E48  (.cpp 16189..16220)
//
// Steer onto lTargetDirection. The turn magnitude is |cross(target, forward)| -- the sine of the
// angle between them -- signed by the cross product's Y lane, rate-limited by
// KF_VEHICLE_MAX_STEERING_DELTA and clamped to KF_VEHICLE_SIN_MAX_STEERING_ANGLE. The car keeps
// the clamped ABSOLUTE steer; the driver record gets the raw DELTA, scaled by 1.3 when the
// caller's lvfScale reaches 0.6 (unk_8300CBE0 lane 3, recovered from its dyn-init thunk at
// 0x82C66E50: {0.2, 1.0, 0.94, 0.6}).
// FLAG (VMX->portable): vrsqrtefp + two Newton steps -> exact 1/sqrt.
// --------------------------------------------------------------------------------------------
void TrafficEntityModule::CalculateAndSetSteering(u32 luVehicle, Vector3 lTargetDirection,
                                                  BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls,
                                                  VecFloat lvfScale)
{
    CGS_ASSERT(lpControls != 0, "lpOutControls");                                   // .cpp 16189
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC"); // .h 2459

    Vehicle* const lpVehicle = GetVehicle(luVehicle);
    CGS_ASSERT(lpVehicle != 0, "lpVehicle");                                        // .cpp 16192

    const Matrix44Affine lTransform = GetVehicleTransform(luVehicle);

    // 0x82718F14..0x82718F4C -- the VMX three-instruction cross product (yzx permutes).
    const Vector3 lCross = rw::math::vpu::Cross(lTargetDirection, lTransform.At());

    const f32 lfCrossLenSq = rw::math::vpu::Dot(lCross, lCross);
    const f32 lfSinAngle   = (lfCrossLenSq == 0.0f) ? 0.0f : std::sqrt(lfCrossLenSq);

    // 0x82718F8C..0x82718FD4 -- Sgn(lCross.y) built from two compares and two vsels. Matches
    // rw::math::fpu::Sgn<VecFloat> @0x825BC920 exactly: ==0 -> 0.0, >=0 -> 1.0, else -1.0.
    const f32 lfSign = (lCross.y > 0.0f) ? 1.0f : ((lCross.y < 0.0f) ? -1.0f : 0.0f);

    const f32 lfCurrentSteering = lpVehicle->GetSteering().x;
    const f32 lfDelta           = lfSinAngle * lfSign - lfCurrentSteering;

    const f32 lfMaxDelta = KF_VEHICLE_MAX_STEERING_DELTA.x;          // this+0x72670
    const f32 lfMaxSin   = KF_VEHICLE_SIN_MAX_STEERING_ANGLE.x;      // this+0x72680

    f32 lfClamped = (lfDelta < -lfMaxDelta) ? -lfMaxDelta : lfDelta;
    lfClamped     = (lfClamped > lfMaxDelta) ? lfMaxDelta : lfClamped;

    f32 lfNewSteering = lfCurrentSteering + lfClamped;
    lfNewSteering = (lfNewSteering < -lfMaxSin) ? -lfMaxSin : lfNewSteering;
    lfNewSteering = (lfNewSteering >  lfMaxSin) ?  lfMaxSin : lfNewSteering;

    lpVehicle->SetSteering(lfNewSteering);

    // 0x82719030..0x82719090 -- the record carries the DELTA, not the absolute.
    const f32 lfRecordScale =
        (lvfScale.x >= KF_STEERING_SCALE_THRESHOLD) ? KF_STEERING_SCALE_HIGH : 1.0f;
    lpControls->mfSteering = lfDelta * lfRecordScale;

    // GATE: DEBUG_ValidateEmDriverControls @0x82708FF8 (0x82719094) -- debug-only validator.
    // DELETE-WHEN it lands; no effect on the record.
}

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::DriveTowardsTarget  @0x8273DFC0  (.cpp 16594..16700)   PARTIAL
//
// The shared driving body: give up if a slammed car is still moving, hand the car back to the
// param sim once it is on top of its target and pointing the right way, then steer, pedal and
// (when reversing) flip the steering sign.
// --------------------------------------------------------------------------------------------
void TrafficEntityModule::DriveTowardsTarget(u32 luVehicle, bool lbAllowReturnToTraffic,
                                             BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls)
{
    CgsDev::PerfMonCpu::StartMonitor(miPerfMon_Driving);            // this+0x72A00

    CGS_ASSERT(lpControls != 0, "lpOutControls");                                   // .cpp 16594
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC"); // .h 2459

    Vehicle* const lpVehicle = GetVehicle(luVehicle);

    CGS_ASSERT(lpVehicle->IsPhysical(), "lpVehicle->IsPhysical()");                 // .cpp 16597
    CGS_ASSERT(lpVehicle->IsOfStandardSpecies(), "lpVehicle->IsOfStandardSpecies()"); // .cpp 16598

    // 0x8273E0AC..0x8273E144 -- a slammed standard car that is still rolling gives up, 5% of
    // the time. KF_GIVE_UP_SPEED is flt_8300C950, a dyn-init product recovered from its thunk
    // at 0x82C66C30: 0.44704 (mph->m/s) * 10 == 10 mph.
    if (lpVehicle->IsRecoveringFromSlam() && lpVehicle->IsOfStandardSpecies() &&
        lpVehicle->GetRandomVal() < KF_GIVE_UP_RANDOM_CHANCE &&
        lpVehicle->GetSpeed().x > KF_GIVE_UP_SPEED)
    {
        lpVehicle->StartGiveUpManoeuvre();
        CgsDev::PerfMonCpu::StopMonitor(miPerfMon_Driving);
        return;
    }

    // GATE: CheckIfPhysicalVehicleIsStuck @0x8272C010 (0x8273E150), 141 insns, unreconstructed.
    // BLOCKER: needs the TrafficPhysicsInfo stuck-timer block and three module bools this
    // cluster has not homed. DELETE-WHEN it lands. COST: a wedged physical car is not detected
    // here (GenerateDriverInputs' own mfStuckTime send still catches it).
    {
        static bool sbLoggedStuck = false;
        LogMissingLeg(sbLoggedStuck,
                      "DriveTowardsTarget's CheckIfPhysicalVehicleIsStuck @0x8272C010 test -- "
                      "unreconstructed; taken as not-stuck");
    }

    const Vector3 lTargetPos = lpVehicle->GetTargetPos();
    CGS_ASSERT(rw::math::vpu::IsValid(lTargetPos), "IsValid( lpVehicle->GetTargetPos() )"); // 16618

    const Matrix44Affine lTransform = GetVehicleTransform(luVehicle);
    const Vector3 lDiff  = lTargetPos - lTransform.Pos();

    // The distance guard is the console's own (0x8273E258 vcmpeqfp + 0x8273E288 vsel).
    // FLAG (host guard): only the unit vector is unguarded on the console; zero vector here.
    const f32 lfDistSq = rw::math::vpu::Dot(lDiff, lDiff);
    const f32 lfDist   = (lfDistSq == 0.0f) ? 0.0f : std::sqrt(lfDistSq);
    const Vector3 lUnitDiff = (lfDistSq == 0.0f) ? ZeroVector3() : lDiff * (1.0f / lfDist);

    const ParamTransform* const lpParamTransform = GetParamTransform(luVehicle);

    // 0x8273E2D8..0x8273E3CC -- 20 m off its own param and the car is TRYING TO RECOVER. The
    // console base is `addis r26,r23,3 ; addi r26,r26,-0x7B00` == this + 0x28500 == 165120 ==
    // mVehicleSoaData + 560 == mPhysicalVehiclesTryingToRecover (FarFromPlayer is +480 ==
    // 165040, which TryClearupOffscreenTraffic @0x8273C4C8 reads instead). This is the ONLY
    // writer of the bit in the image; its only reader is UpdateParam_CheckIfNeedToSlow
    // @0x82738468, which drops such a car as a queueing target so the param drives AROUND it.
    if (lfDist >= KF_DRIVER_FAR_FROM_TARGET_DIST)
    {
        mVehicleSoaData.mPhysicalVehiclesTryingToRecover.SetBit(luVehicle);
    }

    // 0x8273E27C..0x8273E32C -- sitting on its target, pointing the same way, and physical for
    // more than five seconds: hand it back to the param sim.
    if (lbAllowReturnToTraffic &&
        lpVehicle->GetPhysicalTime() > KF_DRIVER_MIN_PHYSICAL_TIME_TO_RETURN &&
        KF_DRIVER_RETURN_TO_TRAFFIC_DIST > lfDist &&
        rw::math::vpu::Dot(lUnitDiff, lpParamTransform->GetDirection()) >
            KF_DRIVER_RETURN_TO_TRAFFIC_DOT)
    {
        CGS_ASSERT(!lpVehicle->IsOfTrailerSpecies(), "!lpVehicle->IsOfTrailerSpecies()"); // 16648

        // 0x8273E4B4 the call, 0x8273E4B8 the `b loc_8273E75C` that RETURNS out of the driver
        // for this frame -- the car is no longer physical, so steering, pedals and the
        // reverse-turn test below must not run on it. The perf-mon stop is the console's tail.
        ReturnPhysicalVehicleToTraffic(luVehicle);
        CgsDev::PerfMonCpu::StopMonitor(miPerfMon_Driving);
        return;
    }

    // 0x8273E634..0x8273E664 -- forward distance to the target, along the car's own At axis.
    const f32 lfForwardDist = rw::math::vpu::Dot(lDiff, lTransform.At());

    if (lpVehicle->IsExtremeSwerving() &&
        lpVehicle->GetPhysicalTime() < KF_DRIVER_SWERVE_STEERING_TIME)
    {
        CalculateAndSetSteering(luVehicle, lUnitDiff, lpControls, SplatDrive(0.0f));
    }
    else
    {
        // GATE: CalculateAndSetSteeringUsingAvoidance @0x8273D258 (0x8273E6A0) -- unreconstructed
        // (VMX feeler pipeline + the mbDEBUGEnableAvoidance block). FALLBACK: the console's own
        // direct-target steering with lvfScale 0, i.e. avoidance disabled, not steering disabled.
        // DELETE-WHEN it lands; it also outputs the score the gated handbrake leg reads.
        static bool sbLoggedAvoid = false;
        LogMissingLeg(sbLoggedAvoid,
                      "DriveTowardsTarget's CalculateAndSetSteeringUsingAvoidance @0x8273D258 -- "
                      "unreconstructed; falls back to CalculateAndSetSteering @0x82718E48");

        CalculateAndSetSteering(luVehicle, lUnitDiff, lpControls, SplatDrive(0.0f));
    }

    // 0x8273E6C4..0x8273E70C -- one signed pedal split into gas and brake.
    const Vector3 lParamLinearVelocity =
        lpParamTransform->GetDirection() * lpParamTransform->GetSpeed();
    const f32 lfPedal =
        CalculateDriverGasBrake(luVehicle, SplatDrive(lfForwardDist), lParamLinearVelocity).x;

    const f32 lfGas   = (lfPedal > 0.0f) ? ((lfPedal > 1.0f) ? 1.0f : lfPedal) : 0.0f;
    const f32 lfBrake = (lfPedal < 0.0f) ? ((-lfPedal > 1.0f) ? 1.0f : -lfPedal) : 0.0f;

    lpControls->mfGas   = lfGas;
    lpControls->mfBrake = lfBrake;

    // 0x8273E5B8..0x8273E624 -- the target is more than 15 m BEHIND us and we are not already
    // in a manoeuvre: start the three-point turn.
    if (KF_DRIVER_REVERSE_TURN_DIST > lfForwardDist &&
        lpVehicle->GetCurrentManoeuvre() == Vehicle::E_MANOEUVRE_NONE &&
        0.0f > rw::math::vpu::Dot(lpParamTransform->GetDirection(), lTransform.At()))
    {
        lpVehicle->SetCurrentManoeuvre(Vehicle::E_MANOEUVRE_3_POINT_TURN);
    }

    // 0x8273E6F8..0x8273E71C -- reversing flips the steering sign.
    const f32 lfSpeed = lpVehicle->GetSpeed().x;
    const f32 lfSpeedSign = (lfSpeed > 0.0f) ? 1.0f : ((lfSpeed < 0.0f) ? -1.0f : 0.0f);
    lpControls->mfSteering = lpControls->mfSteering * lfSpeedSign;

    // GATE: the handbrake leg @0x8273E71C. It tests the avoidance score the gated
    // CalculateAndSetSteeringUsingAvoidance writes back against unk_8300CEE0 (recovered from
    // its dyn-init thunk at 0x82C66990: splat(0.7)), and sets mfHandBrake 0.5.
    // DELETE-WHEN the avoidance leg lands.

    // GATE: DEBUG_ValidateEmDriverControls @0x82708FF8 (0x8273E748) -- debug-only validator.

    CgsDev::PerfMonCpu::StopMonitor(miPerfMon_Driving);
}

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::UpdateExtremeSwerving  @0x8273E8D0  (164 insns, .cpp 16770..)
//
// The arm GenerateDriverInputs takes for ANY physical car whose reason is SWERVING (4), which
// on this build is every promotion. Two outcomes:
//   * crash slider < 0.4, or no local player, or the player is >40 m away AND this car's own
//     mfRandomVal is above 0.8
//       -> DriveTowardsTarget, i.e. the car actually gets gas/brake/steer. lbAllowReturnToTraffic
//          is `manoeuvre != EXTREME_SWERVE` (0x8273EB38..0x8273EB50: cntlzw(m-1), extract bit 26,
//          xori 1), so a swerving car whose manoeuvre has already wound back to NONE is the one
//          allowed to hand its physical slot back.
//   * otherwise -> flip the car into a sympathetic crash aimed at the local player's race car.
// The console's r5 (lpOutput) is saved by neither the prologue nor the body: this arm never
// posts an event. Kept in the signature because the declaration and the call site carry it.
// --------------------------------------------------------------------------------------------
void TrafficEntityModule::UpdateExtremeSwerving(
        u32 luVehicle,
        BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
        BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls)
{
    (void)lpOutput;   // console r5 -- passed in, never read by this arm

    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");   // .h 2459

    Vehicle* const lpVehicle = GetVehicle(luVehicle);                                 // 0x8273E910

    CGS_ASSERT(lpVehicle->IsExtremeSwerving(), "lpVehicle->IsExtremeSwerving()");      // .cpp 16770
    CGS_ASSERT(lpVehicle->IsOfStandardSpecies(), "lpVehicle->IsOfStandardSpecies()");  // .cpp 16771

    // 0x8273E980..0x8273EA20 -- the three tests that decide whether this frame turns into a
    // sympathetic crash. Written in the console's own short-circuit order: a low crash slider
    // or no local player skips the transform fetch entirely.
    bool lbStartSympatheticCrash = false;

    if (mfCrashSliderFinalValue >= KF_EXTREME_SWERVE_CRASH_SLIDER_MIN &&
        meLocalPlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        const Matrix44Affine lTransform = GetVehicleTransform(luVehicle);

        // GetSympCrashingTargetPos @0x82708C10 reads the same table for a race-car target:
        // mabRaceCarActive @+0x716C0 then maActiveRaceCarPositions @+0x716D0, both indexed by
        // EActiveRaceCarIndex. This site reads the position WITHOUT the active test.
        const Vector3 lDiff =
            mRaceCarState.maActiveRaceCarPositions[static_cast<s32>(meLocalPlayerIndex)]
            - lTransform.Pos();

        const f32 lfDistSq = rw::math::vpu::Dot(lDiff, lDiff);

        // 0x8273E9F4 `vcmpgtfp. v0, dist2, splat(1600)` -> the CR6 "all lanes greater" bit.
        // NEAR the player: crash. FAR: the car's OWN fixed mfRandomVal decides, and only a
        // value ABOVE 0.8 keeps it driving (so ~20% of cars, permanently, not per frame).
        // Polarity checked against the two branches at 0x8273EA0C/0x8273EA20.
        const bool lbPlayerIsFar = (lfDistSq > KF_EXTREME_SWERVE_PLAYER_DIST_SQ);

        lbStartSympatheticCrash =
            !lbPlayerIsFar ||
            !(lpVehicle->GetRandomVal() > KF_EXTREME_SWERVE_KEEP_DRIVING_ROLL);
    }

    if (!lbStartSympatheticCrash)
    {
        // 0x8273EB30..0x8273EB54 -- the ONLY route from a swerving car back into the shared
        // driving body, and (through it) into ReturnPhysicalVehicleToTraffic.
        const bool lbAllowReturnToTraffic =
            (lpVehicle->GetCurrentManoeuvre() != Vehicle::E_MANOEUVRE_EXTREME_SWERVE);

        DriveTowardsTarget(luVehicle, lbAllowReturnToTraffic, lpControls);
        return;
    }

    // 0x8273EA24..0x8273EA58 -- the target id is built BEFORE the already-crashing test, so
    // the bound assert fires either way. Owner byte 1 == race car, same 14/10 split as
    // MakeTrafficEntityId.
    const u32 luPlayerEntityIndex = static_cast<u32>(meLocalPlayerIndex);
    CGS_ASSERT(luPlayerEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM_LOCAL),
               "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");   // CgsEntityId.h:116

    EntityId lTargetEntityId;
    lTargetEntityId.muValue =
        (luPlayerEntityIndex << KU_RACE_CAR_PART_INDEX_SHIFT) | KU_RACE_CAR_OWNER_PACKED;

    if (lpVehicle->IsSympatheticallyCrashing())    // 0x8273EA5C -- already going: nothing to do
    {
        return;
    }

    lpVehicle->SetPhysicalReason(
        static_cast<s8>(E_PHYSICALREASON_SYMPATHETIC_CRASHING));                     // 0x8273EA74
    lpVehicle->SetSympatheticCrashTarget(lTargetEntityId);                           // 0x8273EA80

    // 0x8273EA84..0x8273EB24 -- one mEffectRand LCG step (the module's SECOND Random, seed at
    // X360 +0x1380 == mEffectRand.muSeed, mRand's being +0x1350), reduced mod 101 and compared
    // against a signed (slider*35 + 30) truncated to int. Roll under the threshold accelerates
    // into the player; otherwise it is a head-on. mfSympCrashTime is zeroed either way.
    const s32 liRoll = static_cast<s32>(mEffectRand.RandomUInt() % KU_SYMP_CRASH_PERCENT_MODULUS);
    const s32 liAcceleratePercent = static_cast<s32>(
        mfCrashSliderFinalValue * KF_SYMP_CRASH_STYLE_SLIDER_SCALE
        + KF_SYMP_CRASH_STYLE_BASE_PERCENT);

    lpVehicle->SetSympCrashTime(0.0f);
    lpVehicle->SetSympCrashState(liRoll < liAcceleratePercent
                                     ? Vehicle::E_SYMPATHETIC_ACCELERATE
                                     : Vehicle::E_SYMPATHETIC_HEADON);
}

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::UpdateNormalPhysical  @0x8273EF08  (.cpp 17068..17073)
//
// The whole arm: three asserts and a DriveTowardsTarget with the return-to-traffic test armed.
// --------------------------------------------------------------------------------------------
void TrafficEntityModule::UpdateNormalPhysical(u32 luVehicle,
                                               BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls)
{
    CGS_ASSERT(lpControls != 0, "lpDriverControls");                                // .cpp 17068
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC"); // .h 2459

    const Vehicle* const lpVehicle = GetVehicle(luVehicle);
    CGS_ASSERT(lpVehicle->IsNormalPhysical(), "lpVehicle->IsNormalPhysical()");     // .cpp 17073

    DriveTowardsTarget(luVehicle, true, lpControls);
}

// =================================================================================================
// THE DEMOTION CHAIN. Three functions, in call order. The console path is
//   DriveTowardsTarget @0x8273DFC0 (car on its target, pointing the right way, physical > 5 s)
//     -> ReturnPhysicalVehicleToTraffic @0x8273DCD0
//       -> StopVehicleBeingPhysical @0x8271FED0  (frees the MODULE's TrafficPhysicsInfo slot and
//                                                 queues the index in maNewRemovedVehicles)
//   PrePhysicsUpdate @0x8274C690
//     -> CleanUpCrashedVehiclePhysics @0x82720960 (drains that array into the physics
//                                                  RemoveTrafficEvent queue, then clears it)
//   -> PhysicalTrafficManager::ProcessRemoveEvents frees the PHYSICS slot.
//
// WHICH array is maNewRemovedVehicles (:682), not maRecentlyRecoveredSlammedTraffic (:683):
// StopVehicleBeingPhysical's base is this+0x57F7C and CleanUpCrashedVehiclePhysics reads its
// count at +0x140 (160 u16 elements). 0x57F7C + 324 + 324 + 12 bytes of padding == 0x58210 ==
// maTrafficPhysicsInfoList, so exactly one Array<u16,160> sits between them.
// =================================================================================================

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::StopVehicleBeingPhysical  @0x8271FED0  (88 insns)
// --------------------------------------------------------------------------------------------
void TrafficEntityModule::StopVehicleBeingPhysical(u32 luVehicle, bool lbSuppressPhysicsRemoval)
{
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");   // .h 2459

    Vehicle* const lpVehicle = GetVehicle(luVehicle);

    CGS_ASSERT(lpVehicle->IsPhysical(), "lpVehicle->IsPhysical()");   // .cpp baked 0x11F2
    CGS_ASSERT(lpVehicle->HasEntity(),  "lpVehicle->HasEntity()");    // .cpp baked 0x11F3

    // 0x8271FF7C..0x8271FF98 -- ONLY when the flag byte is zero. This is the half that reaches
    // physics; everything below is module-local bookkeeping.
    if (!lbSuppressPhysicsRemoval)
    {
        maNewRemovedVehicles.Append(static_cast<u16>(luVehicle));
    }

    // GATE: EnsureVehicleRemovedFromCrashModule (0x8271FFA4) -- no body in this tree (KillParam
    // @_wT2_01.cpp:656 and StaticVehicles_KillParam already gate the same callee).
    // COST: a car demoted mid-crash keeps its crash-module entry. DELETE-WHEN it lands.
    {
        static bool sbLoggedCrashModule = false;
        LogMissingLeg(sbLoggedCrashModule,
                      "StopVehicleBeingPhysical's EnsureVehicleRemovedFromCrashModule "
                      "(0x8271FFA4) -- no body; the crash module keeps its entry");
    }

    // 0x8271FFAC..0x82720010 -- free the module-side physics record. The parts index is read
    // BEFORE SetNotPhysical (which stores -1 over it); the console's `extsb` only serves the
    // 25-bound compare.
    const s32 liPartsIndex = lpVehicle->GetPhysicalPartsIndex();
    CGS_ASSERT(static_cast<u32>(liPartsIndex) < KU_MAX_PHYSICAL_TRAFFIC_VEHICLES,
               "luIndex < NUMBITS");                                   // CgsBitArray.h:241

    maTrafficPhysicsInfoListBits.UnSetBit(static_cast<u32>(liPartsIndex));
    maTrafficPhysicsInfoList[liPartsIndex].Destruct();

    // 0x82720024 -- clears E_FLAG_PHYSICAL, the mPhysicalVehicles bit, the parts index, the
    // physical reason and the crash-traffic type.
    lpVehicle->SetNotPhysical(luVehicle, mVehicleSoaData);
}

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::ReturnPhysicalVehicleToTraffic  @0x8273DCD0  (188 insns)
//
// Re-seat the car's axles on the transform physics left it at, stop it being physical, and put
// its param back on a normal behaviour. Recurses once into the articulated other half.
// --------------------------------------------------------------------------------------------
void TrafficEntityModule::ReturnPhysicalVehicleToTraffic(u32 luVehicle)
{
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");   // .h 2459

    Vehicle* const lpVehicle = GetVehicle(luVehicle);
    CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");                    // BrnTrafficVehicle.h 786

    // 0x8273DD58..0x8273DDB4 -- the two per-type records SetFromVehicleTransform needs. The
    // update record is TrafficData +0x30 (`lwz r10,0x30(r3)`, stride 20 == mpaVehicleTypesUpdate).
    const u32 luVehicleType = lpVehicle->GetVehicleType();
    const VehicleTypeUpdateData* const lpVehicleTypeUpdate =
        &mpData->mpaVehicleTypesUpdate[luVehicleType];
    const VehicleTypeRuntime* const lpVehicleTypeRuntime = GetVehicleTypeRuntime(luVehicleType);

    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");   // .h 2475

    // 0x8273DDE4..0x8273DE00 -- the axles are re-derived from where PHYSICS left the car, so the
    // param sim picks it up in place instead of snapping it back.
    const Matrix44Affine lTransform = GetVehicleTransform(luVehicle);
    GetVehicleAxles(luVehicle)->SetFromVehicleTransform(lTransform, lpVehicleTypeRuntime,
                                                        lpVehicleTypeUpdate);

    StopVehicleBeingPhysical(luVehicle, false);                        // 0x8273DE10

    CGS_ASSERT(luVehicle < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");  // .h 2350

    Param* const lpParam = GetParam(luVehicle);
    CGS_ASSERT(lpParam != 0, "Failed to GetParam for vehicle: ");      // streamed on console

    // 0x8273DED4..0x8273DF14 -- a param that was driving around this car's obstruction has
    // nothing to drive around any more. KI_BEHAVIOUR_NORMAL is 6, not 0.
    if (lpParam->miBehaviour == 2)
    {
        CGS_ASSERT(mbAllowDivergentBehaviour, "AllowDivergentBehaviour()");
        lpParam->miBehaviour = Param::KI_BEHAVIOUR_NORMAL;
    }

    // 0x8273DF18..0x8273DF7C -- the articulated other half comes back with it. The species test
    // is what stops the recursion (the trailer half never recurses into its cab).
    if (lpVehicle->GetOtherHalfIndex() != static_cast<u16>(KU_INVALID_VEHICLE) &&
        !lpVehicle->IsOfTrailerSpecies())
    {
        CGS_ASSERT(lpVehicle->IsOfStandardSpecies(), "IsOfStandardSpecies()");
        ReturnPhysicalVehicleToTraffic(lpVehicle->GetOtherHalfIndex());
    }

    // 0x8273DF80..0x8273DFB4 -- a car whose param died while it was physical is deleted, not
    // handed back.
    if (!lpVehicle->IsOfTrailerSpecies() && !GetParam(luVehicle)->IsAlive())
    {
        // GATE: RemoveVehicle @0x8272E370 (499) -- unreconstructed. BLOCKER: GetVehicleSpecies /
        // Vehicle::DetachArticulation / StaticTrafficParam::SetShouldBeRemoved are not bodied.
        // COST: the record survives with a dead param. DELETE-WHEN RemoveVehicle lands.
        static bool sbLoggedRemoveVehicle = false;
        LogMissingLeg(sbLoggedRemoveVehicle,
                      "ReturnPhysicalVehicleToTraffic's RemoveVehicle @0x8272E370 -- "
                      "unreconstructed; the demoted car keeps its (dead) param record");
    }
}

// --------------------------------------------------------------------------------------------
// TrafficEntityModule::CleanUpCrashedVehiclePhysics  @0x82720960  (76 insns)
//
// Drain maNewRemovedVehicles into the physics RemoveTrafficEvent queue, then clear it. The
// console re-reads the array length every iteration (`lwz 0x140(r26)` inside the loop).
// --------------------------------------------------------------------------------------------
void TrafficEntityModule::CleanUpCrashedVehiclePhysics(
        BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");   // .cpp baked 0x167E

    for (u32 luEntry = 0; luEntry < maNewRemovedVehicles.GetLength(); ++luEntry)
    {
        const u16 lu16Vehicle = maNewRemovedVehicles.GetItem(luEntry);

        CGS_ASSERT(lu16Vehicle < 0x4000u,
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_INDEX)");

        // 0x82720A18..0x82720A58 -- the folded 0x0200000000000000 owner splat plus the
        // index<<10 splice, spelled as the two field writes (the same idiom
        // AddVehicleToPhysics uses in _wT3_01.cpp).
        CgsSceneManager::VolumeInstanceId lVolumeInstanceId;
        lVolumeInstanceId.muId = 0;
        lVolumeInstanceId.SetEntityIDOwner(
            static_cast<u8>(BrnPhysics::Vehicle::KU_ENTITYTYPE_TRAFFIC_VEHICLE));
        lVolumeInstanceId.SetEntityIDEntityIndex(lu16Vehicle);

        lpOutput->GetVehicleInputInterface()->RemovePhysicalTraffic(lVolumeInstanceId);
    }

    maNewRemovedVehicles.Clear();   // 0x82720A78..0x82720A84 (`stwx 0` into the count word)
}

}
