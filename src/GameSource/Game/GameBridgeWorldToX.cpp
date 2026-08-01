// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeWorldToX.cpp
//
// BrnGame::BrnGameModule::BridgeWorldToDirector  @ X360 0x823E3AB0
//
// ⭐ THE WORLD -> DIRECTOR SEAM. This is the ONLY caller of
// DirectorIO::InputBuffer::SetRaceCarInfo @0x823B2B30 anywhere in the image, i.e. the only
// way a race car's pose ever reaches the director's cameras. Until it existed, the director
// input's mRaceCarInfo[8] stayed at whatever DoUpdate_Director's zero-fill left, so every
// VehicleRef the camera behaviours resolve pointed at a zero transform -- an ORIGIN camera.
//
// SIGNATURE (recovered from the ASM, not from a stub -- the dropped-argument rule):
//   IDA's prototype claims sixteen parameters. The prologue @0x823E3AC8..0x823E3AD8 is
//   `mr r28,r3 / mr r26,r4 / mr r29,r5` and nothing in the body reads r6..r10; a9..a16 are
//   IDA's names for stack slots the body re-uses as locals ("local variable allocation has
//   failed" is printed above its own output). The caller DoUpdate_Director @0x823E8DE0
//   passes (this, a4, a7), and ITS caller DoUpdate @0x823F0AF8 supplies a4 = the
//   DirectorIO::InputBuffer created for this sub-step and a7 = the BrnWorldIO::
//   UpdateOutputBuffer -- which DoUpdate creates ONCE per sub-step and threads through
//   every leg. So: (InputBuffer* lpDirectorInput, const UpdateOutputBuffer* lpWorldOutput).
//
// CONSOLE BODY, in order:
//    1. mDirectorBridgeSerialiser.Lock()
//    2. mbPlayerCarCrashing = active->IsPlayerCarActive() && active->IsPlayerCarCrashing()
//    3. if (!active->IsPlayerCarActive())  { input->SetPlayerCarIndex(INVALID); }
//       else switch on the replay state word:
//         state 0..3 (recording / not replaying) -> THE LIVE PUBLISH (4..13 below)
//         state 4..6 (playback)                  -> replay the serialised snapshot instead
//    4. publish the traffic-director interface (one u16 + a 3588-byte block)
//    5. publish the world-entity status interface (5 bytes)
//    6. NewVehicleEvent<50>::Append(input->GetVehicleInputInterface(),
//                                   world->GetDirectorVehicleInputInterface())
//    7. input->AppendContacts(world->GetContactSpyInterface())
//    8. XMemCpy(input + 16, world->GetRaceCarGlobalOutputInterface(), 2416)
//    9. input->SetPlayerCarIndex(world->GetPlayerActiveRaceCarIndex())
//   10. FOR EACH of the 8 active slots that is active: build a stack VehicleInfo
//       (RaceCarState + AABB + hardest-impact + engine flag) and input->SetRaceCarInfo(i, it)
//   11. FOR EACH of the 8 slots: input->SetCrashingCentreOfMass(i, IDENTITY)
//   12. mfPlayerBoostPercentage = boost.mfBoostAmount / boost.mfMaxBoost  (0 if max ~= 0)
//   13. build the 48-byte PlayerCrashInfo block from the vehicle manager's crash queue
//   14. input->mbWorldWantsDebugControllerFocus = world-entity-status byte
//   15. mDirectorBridgeSerialiser.Unlock()
//
// REPRODUCED HERE: 2, 3, 7, 9, 10, 11, 12.
//
// [FLAG PC bring-up] dropped rather than paraphrased, each for a NAMED reason:
//   1/15 + the whole state-4..6 replay arm -- BrnGameModule has no mDirectorBridgeSerialiser
//        member (the console's is at gm+0x9A1070, inside this layout's omitted range) and
//        DirectorBridgeSerialiser::GetStaticLayout is declaration-only. The PC has no replay
//        path at all, so the live arm is the only one that can run.
//   4/5/8 -- their DESTINATIONS (mMidInterfaceBlock, the tail of
//        mVehicleDriverInputInterface, mGlobalRaceCarInterface) are honest-opaque byte spans
//        in BrnDirectorModuleIO.h. Writing into them by console byte offset is exactly the
//        offset-poke this project forbids; they land when those interface homes do.
//        ⚠️ FINDING for whoever homes mGlobalRaceCarInterface: the console copy is
//        `XMemCpy(input + 16, src, 2416)` and 16 + 2416 == 2432 == mUsedRaceCars, so the real
//        member starts at +16 (16-aligned after the 1-byte IOBuffer base), not at +1 as the
//        current opaque span models it. sizeof(RCEntityGlobalRaceCarOutputInterface) is 2416
//        on x64 too, so retyping that span to the real type reproduces the layout exactly.
//   6  -- BrnDirector::BrnDirectorVehicleInputInterface's NewVehicleEvent<50> queue and the
//        world side's producer are both un-homed; the queue is empty on this build.
//   13 -- BrnDirector::PlayerCrashInfo has no reconstructed home, and its source (the
//        vehicle-manager output's 64-byte crash-event ring) is inside an opaque span.
//   14 -- reads world-entity-status byte +217668, inside an opaque span.
//   the per-car HARDEST-IMPACT leg of 10 -- needs the contact-spy event record layout
//        (96-byte stride, normal at +64 / stress at +48) which is not homed. Left at the
//        cleared value; mfHardestImpact stays 0, which is what "no contact this frame" means.
//   the per-car DEFORMED AABB arm of 10 -- DeformationOutputInterface's DeformationState
//        pointer (+112) is inside an opaque span; the UNDEFORMED arm (the RaceCarState's own
//        mHalfExtent) is reproduced and is the arm a parked, undamaged car takes anyway.
//
// ⚠️ CONSOLE ODDITIES REPRODUCED, NOT FIXED:
//   * step 10 clears mbHasCrashingCenterOfMass in the staged VehicleInfo and step 11 then
//     sets it TRUE for all eight slots (SetCrashingCentreOfMass @0x823B2CA8 stores 1 at
//     element+0x4E5). So the flag is always true and the matrix is always identity unless
//     something else republishes it. That is what the console does.
//   * the console never writes mbHardestImpactIsAgainstWorld (VehicleInfo +0x4E4): the stack
//     VehicleInfo's byte at that offset is left at whatever the frame before put there. An
//     uninitialised read is not reproducible on a different ABI, so it is published as false
//     and flagged here rather than left indeterminate.
//   * the console seeds mHardestNormalStressNormal from an unrecovered .rodata constant at
//     0x82181510 (the IDA export set carries no data, so its 16 bytes cannot be read). It is
//     only meaningful when mfHardestImpact > 0, i.e. inside the gated contact-spy leg;
//     published as zero, flagged, NOT guessed.
//
// ⭐ RaceCarState +4 DRIFT, BOUND TIGHTENED (2026-08-01). This function's thirteen
// rw::math::IsValid asserts name the exact fields they check, so their X360 offsets pin the
// X360-vs-PS3 member drift the pose wave measured:
//     448 mAboveGroundTestResult.mIntersectionPosition   == committed 448  (no drift)
//     464 ...mIntersectionNormal                         == committed 464  (no drift)
//     480 ...mfVerticalDistance                          == committed 480  (no drift)
//     496 mTransform, 816 mLinearVelocity, 832 mAngularVelocity           (no drift)
//     988 mfUpShiftRPM   -> committed  984      \
//     992 mfDownShiftRPM -> committed  988       |
//     972 mfSpeedMPH     -> committed  968       | uniform +4
//     980 mfMaxBoostSpeedMPH -> committed 976    |
//    1020 mfAbsDriftScale -> committed 1016      |
//    1024 mfTimeDrifting -> committed 1020       |
//    1028 mfTimeInAir    -> committed 1024       |
//    1032 mfGas          -> committed 1028       |
//    1036 mfBrake        -> committed 1032       |
//    1044 mfSteering     -> committed 1040      /
// and this function's own `*(state + 968)` entity-id reads (the deformation + contact-spy
// lookups) are committed mEntityId @964, also +4. mHalfExtent @848 is read at 848 -- exact.
// ⇒ the extra four X360 bytes sit between mHalfExtent (848) and mEntityId (964), i.e. inside
// the mComOffset / mSlamEffect / mShuntEffect / mCarAssetAttribKey run. That OVERTURNS the
// pose wave's recorded bound ("between mfBrake @1032 and mi8Gear @1088"), which was only ever
// an upper bound. Nothing here depends on it: every access is by NAME.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeWorldToX.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"           // DirectorIO::InputBuffer
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"                // Camera::VehicleInfo
#include "GameSource/World/BrnWorldModuleIO.h"                                // BrnWorldIO::UpdateOutputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"      // RaceCarState

#include <cstring>   // std::memcpy
#include "rw/math/vpu/vector3_operation.h"       // rw::math::vpu::IsValid / IsZero / operator-
#include "rw/math/vpu/matrix44affine_operation.h"// rw::math::vpu::IsValid(Matrix44Affine)
#include "rw/math/fpu/scalar_operation.h"        // rw::math::fpu::IsValid(float)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                      // CgsDev::Log::gpDebugPrint

namespace BrnGame
{
    // Publish counter for the bring-up diagnostic below (file scope; no console member).
    static u32 suRaceCarInfoPublishCount = 0;

    // ------------------------------------------------------------------------------------
    // The console builds this on the stack once and hands the SAME identity matrix to all
    // eight SetCrashingCentreOfMass calls (its loop writes eight copies of it into a 512-byte
    // stack array first). Rows: X=(1,0,0,0) Y=(0,1,0,0) Z=(0,0,1,0) W=(0,0,0,0).
    // ------------------------------------------------------------------------------------
    static Matrix44Affine MakeIdentityAffine()
    {
        // The console builds the rows literally: X=(1,0,0,0) Y=(0,1,0,0) Z=(0,0,1,0)
        // W=(0,0,0,0) -- which is exactly Matrix44Affine::SetIdentity() (note the affine
        // form's W row is all-zero, not (0,0,0,1)).
        Matrix44Affine lIdentity;
        lIdentity.SetIdentity();
        return lIdentity;
    }

    void BrnGameModule::BridgeWorldToDirector(
            BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
            const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput)
    {
        if (lpDirectorInput == 0 || lpWorldOutput == 0)
        {
            return;
        }

        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface
                ActiveRaceCarInterface;

        const ActiveRaceCarInterface* lpActiveRaceCars =
            lpWorldOutput->GetActiveRaceCarOutputInterface();

        // ---- step 2: the module's own "player car is crashing" latch ----------------------
        // X360 `stbx` at gm+0x9A0634, built from the two inlined predicates.
        mbPlayerCarCrashing = lpActiveRaceCars->IsPlayerCarActive()
                           && lpActiveRaceCars->IsPlayerCarCrashing();

        if (!lpActiveRaceCars->IsPlayerCarActive())
        {
            // X360: assert the write lock, then `li r11,-1; stw r11, 0x7AA8(r26)`.
            // Publishing INVALID is what makes MainDirector::GetLivePlayerCarIndex return -1
            // and the director carry its last finalised camera forward.
            lpDirectorInput->SetPlayerCarIndex(E_ACTIVE_RACE_CAR_INDEX_INVALID);
            return;
        }

        // ---- step 9 (the console orders it after 4..8, all of which are gated) ------------
        CGS_ASSERT(lpActiveRaceCars->IsPlayerCarActive(), "Player race car index not active"); // :66
        const EActiveRaceCarIndex lePlayerIndex = lpWorldOutput->GetPlayerActiveRaceCarIndex();

        // ---- step 7: the contact-spy publish ---------------------------------------------
        lpDirectorInput->AppendContacts(lpWorldOutput->GetContactSpyInterface());

        lpDirectorInput->SetPlayerCarIndex(lePlayerIndex);

        // ---- step 10: the per-car VehicleInfo publish ------------------------------------
        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(liSlot);

            // The console tests `maxRaceCarFlags[idx] & 1` inline -- IsRaceCarActive().
            if (!lpActiveRaceCars->IsRaceCarActive(leSlot))
            {
                CGS_ASSERT(liSlot + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
                continue;
            }

            // The console uses the MUTABLE element getter @0x8227D690 on a read-locked
            // buffer (an oddity of its own); the const one is the same address and the same
            // IsRaceCarActive gate.
            const BrnPhysics::Vehicle::RaceCarState* lpState =
                lpActiveRaceCars->GetRaceCarState(leSlot);
            CGS_ASSERT(lpState != 0, "lpState != NULL");                                 // :110
            if (lpState == 0)
            {
                continue;
            }

            // The console's sixteen validity tripwires (:112..:128), BY NAME. Their X360
            // offsets are what pinned the RaceCarState +4 drift documented in this file's
            // header; the committed member NAMES are what is asserted here.
            CGS_ASSERT(rw::math::vpu::IsValid(lpState->mTransform),
                       "rw::math::IsValid(lpState->mTransform)");                        // :112
            CGS_ASSERT(rw::math::vpu::IsValid(lpState->mLinearVelocity),
                       "rw::math::IsValid(lpState->mLinearVelocity)");                   // :113
            CGS_ASSERT(rw::math::vpu::IsValid(lpState->mAngularVelocity),
                       "rw::math::IsValid(lpState->mAngularVelocity)");                  // :114
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mfSpeedMPH),
                       "rw::math::IsValid(lpState->mfSpeedMPH)");                        // :115
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mfMaxBoostSpeedMPH),
                       "rw::math::IsValid(lpState->mfMaxBoostSpeedMPH)");                // :116
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mfAbsDriftScale),
                       "rw::math::IsValid(lpState->mfAbsDriftScale)");                   // :117
            CGS_ASSERT(rw::math::vpu::IsValid(lpState->mAboveGroundTestResult.mIntersectionNormal),
                       "rw::math::IsValid(lpState->mAboveGroundTestResult.mIntersectionNormal)");   // :118
            CGS_ASSERT(rw::math::vpu::IsValid(lpState->mAboveGroundTestResult.mIntersectionPosition),
                       "rw::math::IsValid(lpState->mAboveGroundTestResult.mIntersectionPosition)"); // :119
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mAboveGroundTestResult.mfVerticalDistance),
                       "rw::math::IsValid(lpState->mAboveGroundTestResult.mfVerticalDistance)");    // :120
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mfGas),
                       "rw::math::IsValid(lpState->mfGas)");                             // :122
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mfBrake),
                       "rw::math::IsValid(lpState->mfBrake)");                           // :123
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mfSteering),
                       "rw::math::IsValid(lpState->mfSteering)");                        // :124
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mfTimeInAir),
                       "rw::math::IsValid(lpState->mfTimeInAir)");                       // :125
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mfTimeDrifting),
                       "rw::math::IsValid(lpState->mfTimeDrifting)");                    // :126
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mfDownShiftRPM),
                       "rw::math::IsValid(lpState->mfDownShiftRPM)");                    // :127
            CGS_ASSERT(rw::math::fpu::IsValid(lpState->mfUpShiftRPM),
                       "rw::math::IsValid(lpState->mfUpShiftRPM)");                      // :128

            BrnDirector::Camera::VehicleInfo lVehicleInfo;

            // The RaceCarState payload: the console XMemCpy's all 1120 bytes.
            lVehicleInfo.mRaceCarState = *lpState;

            // The world AABB. [FLAG] the DEFORMED arm -- the console prefers the deformation
            // state's own per-car min/max when DeformationOutputInterface's DeformationState
            // pointer is live AND DeformationState::GetCarStateForCar(mEntityId) resolves --
            // is gated (that pointer sits in an opaque span). The UNDEFORMED arm is the
            // console's own fallback and is exact: max = mHalfExtent, min = -mHalfExtent
            // (asm `vspltisw v0,-1; vslw v0,v0,v0` builds the 0x80000000 sign mask and
            // `vxor` negates), i.e. a symmetric box about the car's origin.
            const Vector3& lHalfExtent = lpState->mHalfExtent;
            lVehicleInfo.mAABB.mMin = Vector3{ -lHalfExtent.x, -lHalfExtent.y, -lHalfExtent.z, 0.0f };
            lVehicleInfo.mAABB.mMax = lHalfExtent;
            CGS_ASSERT(!rw::math::vpu::IsZero(lVehicleInfo.mAABB.mMax - lVehicleInfo.mAABB.mMin),
                       "!rw::math::IsZero(lVehicleInfo.mAABB.Max() - lVehicleInfo.mAABB.Min())"); // :148

            // [FLAG] the hardest-impact triple -- see the header note. Cleared, which is the
            // console's own "no contact this frame" value for the two it does clear.
            lVehicleInfo.mHardestNormalStressNormal.SetZero();
            lVehicleInfo.mHardestNormalStress.SetZero();
            lVehicleInfo.mfHardestImpact            = 0.0f;
            lVehicleInfo.mbHardestImpactIsAgainstWorld = false;   // [FLAG] console leaves it uninitialised

            // The console clears this here and SetCrashingCentreOfMass (below) then sets it.
            lVehicleInfo.mbHasCrashingCenterOfMass = false;

            // The engine flag: the console inlines BOTH engine predicates and ORs them
            // (a non-player slot is always "on"; the player's slot follows its engine state).
            lVehicleInfo.mbEngineOn = lpActiveRaceCars->IsRaceCarEngineOn(leSlot)
                                   || lpActiveRaceCars->IsRaceCarEngineStarting(leSlot);

            lpDirectorInput->SetRaceCarInfo(static_cast<u32>(liSlot), lVehicleInfo);

            // Bring-up diagnostic: print the pose the cameras will actually frame -- on the
            // FIRST publish (which lands the frame the car is attached, before its physics
            // state has been seeded) and then every 3000th (~40 s), which is the steady
            // state. This is the exact claim the retired fake-car stand-in could not make.
            // (Remove when the whole world->director staging is routine.)
            ++suRaceCarInfoPublishCount;
            if (suRaceCarInfoPublishCount == 1u || (suRaceCarInfoPublishCount % 3000u) == 0u)
            {
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    const Vector3& lPos = lVehicleInfo.mRaceCarState.mTransform.Pos();
                    *CgsDev::Log::gpDebugPrint
                        << "[world->director] publish #" << static_cast<s32>(suRaceCarInfoPublishCount)
                        << ": player index "
                        << static_cast<s32>(lePlayerIndex) << ", slot " << liSlot
                        << " at (" << lPos.x << ", " << lPos.y << ", " << lPos.z
                        << "), engine " << (lVehicleInfo.mbEngineOn ? "on" : "off") << "\n";
                }
            }

            CGS_ASSERT(liSlot + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
        }

        // ---- step 11: the crashing centre-of-mass defaults --------------------------------
        {
            const Matrix44Affine lIdentity = MakeIdentityAffine();
            for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
            {
                lpDirectorInput->SetCrashingCentreOfMass(static_cast<u32>(liSlot), lIdentity);
                CGS_ASSERT(liSlot + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
            }
        }

        // ---- step 12: the player's boost percentage --------------------------------------
        // X360: `v311 = boost.mfMaxBoost; v309 = 0; if (v311 > eps || v311 < -eps)
        //        v309 = boost.mfBoostAmount / v311; *(input + 0x325C) = v309;`
        // The epsilon is the same rodata float both comparisons use (0x33800000 == 2^-24;
        // the negative literal IDA prints is -1.1920929e-7 == -2^-23, the sign-flipped pair
        // the compiler emits for `fabs(x) > eps`).
        {
            CGS_ASSERT(lePlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
                       "Player car index hasn't been set");
            const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo* lpBoost =
                lpActiveRaceCars->GetBoostOutputInfoN(lePlayerIndex);
            f32 lfBoostPercentage = 0.0f;
            if (lpBoost != 0 &&
                (lpBoost->mfMaxBoost > 1.1920929e-7f || lpBoost->mfMaxBoost < -1.1920929e-7f))
            {
                lfBoostPercentage = lpBoost->mfBoostAmount / lpBoost->mfMaxBoost;
            }
            lpDirectorInput->SetPlayerBoostPercentage(lfBoostPercentage);
        }
    }
}
