// =================================================================================================
// BrnTrafficEntityModule_wT3_04.cpp -- wave T3 round 1 (PHYSICAL TRAFFIC), cluster C4.
//
//   BrnTraffic::TrafficEntityModule::HandleExternalResponses @0x82732C68 (1,302 insns, DWARF :1432)
//
// THE FUNCTION THAT MAKES THE HIT CAR MOVE ON SCREEN. PostPhysicsUpdate's RUNNING head leg
// (_wT1_01.cpp) calls it once per frame with the traffic module's post-physics INPUT buffer, which
// WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics @0x827AB910 has just staged from the
// physics module's output buffer.
//
// The Feb-2007 leak has this function at BrnTrafficEntityModule.cpp:3652 and it is the STRUCTURAL
// key only: the ship carries four drain loops where the leak has two, and the ship's
// maTrafficPhysicsInfoList replaces the leak's maCrashingVehiclePartsList. Everything below is read
// off the ARTIST asm/pseudocode; the leak settles the bbox-offset SIGN and nothing else.
//
// THE FOUR LOOPS, in console order:
//   1  VehicleManagerOutputInterface::GetCrashedTrafficEventQueue()  (interface +0)
//        -> RecordTrafficVehicleIsPhysical + the crash-slider score + the parked-car alarm
//   2  VehicleManagerOutputInterface::GetSlammedTrafficEventQueue()  (interface +336 == 0x150)
//        -> the same promotion, carrying the slam's crash type and its two direction floats
//   3  VehicleOutputInterface::GetTrafficStateQueue()                (interface +9760 == 0x2620)
//        -> THE POSE READ-BACK. This is the loop the goal of the wave depends on.
//   4  VehicleManagerOutputInterface::GetRaceCarCrashEventQueue()    (interface +928 == 0x3A0)
//        -> the crash-slider score again, weighted by whether the crasher was AI
//
// THE BBOX ROUND TRIP (loop 3). CalculateInitialPhysicalState (_wT3_00.cpp) promotes with
//     Matrix44AffineFromTranslation( mBBoxOffset ) * lVehicleTransform
// and this reads back with
//     Matrix44AffineFromTranslation( Negate(mBBoxOffset) ) * lEvent.mTransform
// (asm 0x82733E1C `vslw v9,v9,v9` -> 0x80000000 per lane, `vxor v8,v8,v9` == the negate, then the
// generic affine product at 0x82733E60..0x82733EE0). Flip either sign and every promoted car jumps
// by the bbox offset on its first physical frame.
//
// TWO RECOVERED CONSTANTS (headless idat dump of the ARTIST image, this wave):
//   flt_820BA5C0 == 50.0f     flt_820BA5C8 == 100.0f
// Both feed mfCrashSliderCrashScore, whose factor member is mfCrashSliderCrashScoreFactor
// (console +0x72370 / +0x72378 -- anchored by UpdateCrashSlider @0x82715A18, see
// BrnTrafficEntityModule.cpp:195).
//
// NAMED GATES INSIDE THIS FILE (each also listed in the wave report):
//   G-ALARM      (retired: Vehicle::SetAlarmOn @0x8270FC10 landed in BrnTrafficVehicle.cpp) -- this
//                cluster's files and the method is neither declared nor bodied there.
//   G-ARTIC      loops 1's cab/trailer pairing arms + Array<TrafficCrashInfo,160>::Append.
//                Unreachable this round (wave-T2 generation builds InitialiseAsStandard cars only,
//                so muOtherHalfIndex is always KU_INVALID_VEHICLE) and it needs
//                Vehicle::GetTrailerIndex, which does not exist in this tree.
// =================================================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"     // eCrashTrafficType

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/matrix44affine_operation.h"                      // Mult(Matrix44Affine, Matrix44Affine)

#include <cmath>     // std::sqrt (the [T3-apply] delta only)
#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // The traffic owner byte MakeTrafficEntityId packs (BrnTrafficConstants.cpp: owner(2) << 24)
    // and the 14-bit index field above the 10-bit part field. The console reads them back inline
    // as `HIBYTE(id)` and `(id >> 10) & 0x3FFF`; spelled here through the same geometry rather
    // than re-derived, because ::EntityId in this tree is the raw packed word.
    const u32 KU_TRAFFIC_ENTITY_OWNER      = 2u;
    const u32 KU_ENTITY_OWNER_SHIFT        = 24u;
    const u32 KU_ENTITY_INDEX_SHIFT        = 10u;
    const u32 KU_ENTITY_INDEX_MASK         = 0x3FFFu;

    inline u32 EntityOwnerOf(EntityId lId) { return lId.muValue >> KU_ENTITY_OWNER_SHIFT; }
    inline u32 EntityIndexOf(EntityId lId)
    {
        return (lId.muValue >> KU_ENTITY_INDEX_SHIFT) & KU_ENTITY_INDEX_MASK;
    }

    // The 64-bit VolumeInstanceId's embedded entity word lives in its HIGH dword
    // (CgsVolumeInstanceId.h KU_ENTITY_ID_START_INDEX == 32); on the big-endian console that word
    // is what a 32-bit load of the id's first four bytes yields, which is what the crashed-traffic
    // loop reads. DWARF CgsVolumeInstanceId.h:85 names this accessor `EntityId GetEntityId() const`
    // but it is not declared in this tree, so the geometry is spelled locally through the class's
    // own named constant rather than adding an accessor to a header this cluster does not own.
    inline EntityId EntityIdOfVolumeInstance(const CgsSceneManager::VolumeInstanceId& lrId)
    {
        EntityId lResult;
        lResult.muValue = static_cast<u32>(
            lrId.muId >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX);
        return lResult;
    }

    // flt_820BA5C0 / flt_820BA5C8, dumped out of the ARTIST image with headless IDA 9.3
    // (BE u32 0x42480000 / 0x42C80000).
    const f32 KF_CRASH_SLIDER_TRAFFIC_CRASH_SCORE = 50.0f;   // flt_820BA5C0
    const f32 KF_CRASH_SLIDER_PLAYER_CRASH_SCORE  = 100.0f;  // flt_820BA5C8

    // The distance a read-back has to move a car before the one-shot [T3-apply] diag re-latches.
    const f32 KF_T3_APPLY_REPORT_DELTA = 0.05f;

    // `vmsum3fp128 v1, v0, v126` -- the 3-lane dot the speed publish uses.
    inline f32 Dot3(const Vector3& lvA, const Vector3& lvB)
    {
        return lvA.x * lvB.x + lvA.y * lvB.y + lvA.z * lvB.z;
    }

    void LogGateOnce(bool& lrbLogged, const char* lpcText)
    {
        if (!lrbLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            lrbLogged = true;
            *CgsDev::Log::gpDebugPrint << "[T3-gate] HandleExternalResponses: " << lpcText << "\n";
        }
    }
}

// -------------------------------------------------------------------------------------------------
// @0x82732C68  TrafficEntityModule::HandleExternalResponses
//   DWARF spelling at the boundary: void HandleExternalResponses(const InputBuffer_PostPhysics*)
//   The three `luVehicle >= KU_MAX_TOTAL_TRAFFIC` continues are HOST OOB NETS, not console
//   branches (0x8273392C `blt` skips only the assert; the console indexes anyway). Unreachable
//   today -- every entity id the physics side publishes is in range.
// -------------------------------------------------------------------------------------------------
void TrafficEntityModule::HandleExternalResponses(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput)
{
    CGS_ASSERT(lpInput != 0, "lpInput != NULL");                                        // :6215
    if (lpInput == 0)
    {
        return;
    }

    const BrnPhysics::Vehicle::VehicleManagerOutputInterface* lpManagerOutput =
        lpInput->GetVehicleManagerOutputInterface();
    const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutput =
        lpInput->GetVehicleOutputInterface();
    if (lpManagerOutput == 0 || lpVehicleOutput == 0)
    {
        return;
    }

    // =============================================================================================
    // LOOP 1 -- the CRASHED-traffic queue (interface +0). 0x82732D80..0x827334xx.
    // =============================================================================================
    {
        const BrnPhysics::Vehicle::VehicleManagerOutputInterface::TrafficCrashedEventQueue*
            lpCrashedQueue = lpManagerOutput->GetCrashedTrafficEventQueue();

        for (s32 liEvent = 0; liEvent < lpCrashedQueue->GetLength(); ++liEvent)
        {
            const BrnPhysics::Vehicle::TrafficCrashedEvent& lrEvent =
                lpCrashedQueue->GetEvent(liEvent);

            const EntityId lTrafficId = EntityIdOfVolumeInstance(lrEvent.mTrafficVolumeInstanceID);
            const EntityId lCrasherId = lrEvent.mCrasherEntityID;

            CGS_ASSERT(lTrafficId.muValue != lCrasherId.muValue,
                       "Vehicle crashed into itself");                                  // :6227
            CGS_ASSERT(EntityOwnerOf(lTrafficId) == KU_TRAFFIC_ENTITY_OWNER,
                       "Crashed traffic event not referring to traffic");                // :6230

            const u32 luVehicle = EntityIndexOf(lTrafficId);
            CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC,
                       "Crashed traffic event referring to invalid traffic vehicle");    // :6233
            if (luVehicle >= KU_MAX_TOTAL_TRAFFIC)
            {
                continue;
            }

            const Vehicle* lpVehicle = GetVehicle(luVehicle);

            // `(*(v40 + 5) & 1) == 0` -- physics crashed a vehicle the world already reaped.
            // The console logs and skips (message filter bit 0); non-fatal either way.
            if (!lpVehicle->IsAlive())
            {
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "TRAF WARNING: Physics told us that vehicle "
                        << static_cast<s32>(luVehicle) << " crashed, but it isn't even alive!\n";
                }
                continue;
            }

            RecordTrafficVehicleIsPhysical(luVehicle, lTrafficId, lCrasherId,
                                           BrnPhysics::Vehicle::eCrashTrafficType_Standard,
                                           0.0f, 0.0f);

            mfCrashSliderCrashScore +=
                mfCrashSliderCrashScoreFactor * KF_CRASH_SLIDER_TRAFFIC_CRASH_SCORE;

            // 0x827330D8: species == E_SPECIES_STATIC (a PARKED car) and bit 1 of the vehicle
            // index set -- the console's own "every other parked car has an alarm" selector.
            if (GetVehicleSpecies(luVehicle) == Vehicle::E_SPECIES_STATIC
                && (luVehicle & 2u) == 2u)
            {
                GetVehicle(luVehicle)->SetAlarmOn(true);   // @0x8270FC10
            }

            // GATE G-ARTIC: the cab/trailer pairing arms and their
            // Array<TrafficCrashInfo,160>::Append. BLOCKER: needs Vehicle::GetTrailerIndex, absent
            // here; and the standard-species arm early-outs on muOtherHalfIndex == KU_INVALID_VEHICLE,
            // which every wave-T2-generated car has. DELETE-WHEN trailers land.
            {
                static bool sbLoggedArticGate = false;
                LogGateOnce(sbLoggedArticGate,
                            "loop 1 cab/trailer pairing + Array<TrafficCrashInfo,160>::Append "
                            "parked -- Vehicle::GetTrailerIndex is absent and no generated car "
                            "carries a trailer this round");
            }
        }
    }

    // =============================================================================================
    // LOOP 2 -- the SLAMMED-traffic queue (interface +336). 0x827335xx..0x82733Cxx.
    // =============================================================================================
    {
        const BrnPhysics::Vehicle::VehicleManagerOutputInterface::TrafficSlammedEventQueue*
            lpSlammedQueue = lpManagerOutput->GetSlammedTrafficEventQueue();

        for (s32 liEvent = 0; liEvent < lpSlammedQueue->GetLength(); ++liEvent)
        {
            const BrnPhysics::Vehicle::TrafficSlammedEvent& lrEvent =
                lpSlammedQueue->GetEvent(liEvent);

            CGS_ASSERT(lrEvent.meCrashTrafficType == BrnPhysics::Vehicle::eCrashTrafficType_Slammed,
                       "leCrashTrafficType == BrnPhysics::Vehicle::eCrashTrafficType_Slammed"); // :6341
            CGS_ASSERT(EntityOwnerOf(lrEvent.mTrafficId) == KU_TRAFFIC_ENTITY_OWNER,
                       "Slammed traffic event not referring to traffic");                       // :6344

            const u32 luVehicle = EntityIndexOf(lrEvent.mTrafficId);
            CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC,
                       "Slammed traffic event referring to invalid traffic vehicle");           // :6347
            if (luVehicle >= KU_MAX_TOTAL_TRAFFIC)
            {
                continue;
            }

            const Vehicle* lpVehicle = GetVehicle(luVehicle);
            if (!lpVehicle->IsAlive())
            {
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "TRAF WARNING: Physics told us that vehicle "
                        << static_cast<s32>(luVehicle) << " slammed, but it isn't even alive!\n";
                }
                continue;
            }

            if (GetVehicleSpecies(luVehicle) == Vehicle::E_SPECIES_STATIC
                && (luVehicle & 2u) == 2u)
            {
                GetVehicle(luVehicle)->SetAlarmOn(true);   // @0x8270FC10 (slam site)
            }

            // `(v143[5] & 8) == 0` -- only promote a car that is not already physical.
            if (!lpVehicle->IsPhysical())
            {
                RecordTrafficVehicleIsPhysical(luVehicle, lrEvent.mTrafficId,
                                               lrEvent.mEntityThatSlammedIt,
                                               lrEvent.meCrashTrafficType,
                                               lrEvent.mfSteeringDirection,
                                               lrEvent.mfDriveDirection);
            }
        }
    }

    // =============================================================================================
    // LOOP 3 -- THE POSE READ-BACK. mTrafficStateQueue (interface +9760). 0x82733Cxx..0x82734010.
    // =============================================================================================
    {
        const BrnPhysics::Vehicle::VehicleOutputInterface::PhysicalTrafficStateQueue*
            lpStateQueue = lpVehicleOutput->GetTrafficStateQueue();

        for (s32 liEvent = 0; liEvent < lpStateQueue->GetLength(); ++liEvent)
        {
            const BrnPhysics::Vehicle::PhysicalTrafficState& lrState =
                lpStateQueue->GetEvent(liEvent);

            CGS_ASSERT(EntityOwnerOf(lrState.mEntityID) == KU_TRAFFIC_ENTITY_OWNER,
                       "Crashed traffic state not referring to traffic");                // :6396

            const u32 luVehicle = EntityIndexOf(lrState.mEntityID);
            CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC,
                       "Crashed traffic update referring to invalid traffic vehicle");    // :6399
            if (luVehicle >= KU_MAX_TOTAL_TRAFFIC)
            {
                continue;
            }

            Vehicle* lpVehicle = GetVehicle(luVehicle);

            // `(v143[5] & 1) != 0` gates the whole body; a dead vehicle is silently skipped.
            if (!lpVehicle->IsAlive())
            {
                continue;
            }

            // NON-GATING tripwire (:6405): the physics side published a state for a vehicle the
            // world does not think is physical. Both paths reach the transform apply --
            // 0x827339DC rlwinm r11,r11,0,28,28 (vehicle+5 & 8 == IsPhysical), 0x827339E4 bne
            // SKIPS the assert when physical, and the not-physical arm falls out at 0x82733B00
            // into the SAME 0x82733B04 / 0x82733B0C GetTrafficPhysicsInfoForVehicl.
            // FIX ROUND: an invented `continue` used to sit here; it froze a promoted car at its
            // promotion pose whenever the world flag lagged the publish by a frame.
            CGS_ASSERT(lpVehicle->IsPhysical(), "Vehicle is alive but not physical");     // :6405

            // 0x82733B0C. The accessor never returns null (it always hands back
            // &maTrafficPhysicsInfoList[partsIndex]); this guard is a never-taken bring-up
            // net, NOT a console branch. Do not read it as one.
            TrafficPhysicsInfo* lpPhysicsInfo = GetTrafficPhysicsInfoForVehicl(luVehicle);
            if (lpPhysicsInfo == 0)
            {
                continue;
            }

            const VehicleTypeRuntime* lpVehicleTypeRuntime =
                GetVehicleTypeRuntime(lpVehicle->GetVehicleType());

            // ---- the bbox round trip: Translate(-mBBoxOffset) * lrState.mTransform -------------
            // Same shape as CalculateInitialPhysicalState's outbound product (_wT3_00.cpp), with
            // the offset negated. Only the translation row changes; rows 0-2 pass straight through.
            Matrix44Affine lBBoxTranslate;
            lBBoxTranslate.SetIdentity();
            {
                const Vector3 lvOffset = lpVehicleTypeRuntime->GetBBoxOffset();
                lBBoxTranslate.wAxis.x = -lvOffset.x;
                lBBoxTranslate.wAxis.y = -lvOffset.y;
                lBBoxTranslate.wAxis.z = -lvOffset.z;
                lBBoxTranslate.wAxis.w = lvOffset.w;
            }
            const Matrix44Affine lTransform =
                rw::math::vpu::Mult(lBBoxTranslate, lrState.mTransform);

            // [T3-apply] the first read-back this build applies, plus a value-latched repeat the
            // first time a car actually MOVES. DELETE-WHEN-STABLE.
            f32 lfDeltaLength = 0.0f;
            {
                const Matrix44Affine lPrevious = GetVehicleTransform(luVehicle);
                const f32 lfDX = lTransform.wAxis.x - lPrevious.wAxis.x;
                const f32 lfDY = lTransform.wAxis.y - lPrevious.wAxis.y;
                const f32 lfDZ = lTransform.wAxis.z - lPrevious.wAxis.z;
                lfDeltaLength = std::sqrt(lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ);
            }

            SetVehicleTransform(luVehicle, lTransform);

            // `memcpy(info + 3376, event + 544, 256)` -- maWheelTransforms[4], whole-array copy.
            for (s32 liWheel = 0; liWheel < TrafficPhysicsInfo::KU_NUM_WHEELS; ++liWheel)
            {
                lpPhysicsInfo->maWheelTransforms[liWheel] = lrState.maWheelTransforms[liWheel];
            }

            // The road-test normal, then its .w height corrected for the bbox shift the transform
            // above just applied (`vsubfp` of the two translation rows, lane .y only).
            lpPhysicsInfo->mvRoadTestNormal_HeightAboveRoad.x = lrState.mvRoadTestNormal_HeightAboveRoad.x;
            lpPhysicsInfo->mvRoadTestNormal_HeightAboveRoad.y = lrState.mvRoadTestNormal_HeightAboveRoad.y;
            lpPhysicsInfo->mvRoadTestNormal_HeightAboveRoad.z = lrState.mvRoadTestNormal_HeightAboveRoad.z;
            lpPhysicsInfo->mvRoadTestNormal_HeightAboveRoad.w =
                lrState.mvRoadTestNormal_HeightAboveRoad.w
                - (lrState.mTransform.wAxis.y - lTransform.wAxis.y);

            // `*(info + 4069) = info->mbIsDeforming || event.mbIsDeforming` -- an OR-accumulate,
            // not a copy: the flag is cleared elsewhere in the deformation pass.
            if (lpPhysicsInfo->mbIsDeforming || lrState.mbIsDeforming)
            {
                lpPhysicsInfo->mbIsDeforming = true;
            }

            lpVehicle->SetFrozen(lrState.mbFrozen);
            lpVehicle->SetLinearVelocity(lrState.mLinearVelocity);

            // `vmsum3fp128 v1, v0, v126` with v126 == the RESULT transform's row 2 (the At axis):
            // the world-side speed is the forward component of the physics velocity, NOT the
            // event's own mfSpeed field, which this function never reads.
            {
                const f32 lfForwardSpeed = Dot3(lrState.mLinearVelocity, lTransform.zAxis);
                lpVehicle->SetSpeed(VecFloat{ lfForwardSpeed, lfForwardSpeed,
                                              lfForwardSpeed, lfForwardSpeed });
            }

            lpVehicle->SetSteering(lrState.mfSteering);

            // ---- [T3-apply] ----------------------------------------------------------------
            {
                static const bool skbTrafficDiag = (std::getenv("BRN_TRAFFIC_DIAG") != 0);
                if (skbTrafficDiag && CgsDev::Log::gpDebugPrint != 0)
                {
                    static bool sbLoggedFirstApply = false;
                    static bool sbLoggedFirstMove  = false;
                    const bool  lbMoved = (lfDeltaLength > KF_T3_APPLY_REPORT_DELTA);

                    if (!sbLoggedFirstApply || (lbMoved && !sbLoggedFirstMove))
                    {
                        if (lbMoved)
                        {
                            sbLoggedFirstMove = true;
                        }
                        const char* lpcWhich = sbLoggedFirstApply ? "FIRST MOVE" : "FIRST";
                        sbLoggedFirstApply = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[T3-apply] " << lpcWhich << " physical-traffic read-back: vehicle "
                            << static_cast<s32>(luVehicle)
                            << " |delta| " << lfDeltaLength
                            << " speed " << Dot3(lrState.mLinearVelocity, lTransform.zAxis)
                            << " at (" << lTransform.wAxis.x << ", " << lTransform.wAxis.y
                            << ", " << lTransform.wAxis.z << ")\n";
                    }
                }
            }
        }
    }

    // =============================================================================================
    // LOOP 4 -- the RACE-CAR crash queue (interface +928). 0x82733Fxx..0x827340A0.
    // Nothing but the crash slider: a primary crash scores 50 when the crasher was AI and 100
    // when it was not (`lbz +0x38` == mbIsPrimaryCrash, `lbz +0x3A` == mbCarIsAI).
    // =============================================================================================
    {
        const BrnPhysics::Vehicle::VehicleManagerOutputInterface::RaceCarCrashEventQueue*
            lpRaceCarCrashQueue = lpManagerOutput->GetRaceCarCrashEventQueue();

        for (s32 liEvent = 0; liEvent < lpRaceCarCrashQueue->GetLength(); ++liEvent)
        {
            const BrnPhysics::Vehicle::RaceCarCrashEvent& lrEvent =
                lpRaceCarCrashQueue->GetEvent(liEvent);

            if (lrEvent.mbIsPrimaryCrash)
            {
                const f32 lfScore = lrEvent.mbCarIsAI ? KF_CRASH_SLIDER_TRAFFIC_CRASH_SCORE
                                                      : KF_CRASH_SLIDER_PLAYER_CRASH_SCORE;
                mfCrashSliderCrashScore += mfCrashSliderCrashScoreFactor * lfScore;
            }
        }
    }
}

}  // namespace BrnTraffic
