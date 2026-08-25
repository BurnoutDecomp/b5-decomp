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
// REPRODUCED HERE: 2, 3, 6, 7, 9, 10, 11, 12.   (6 restored 2026-08-02.)
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
//   (6 IS NO LONGER DROPPED -- restored 2026-08-02, see the step in the body.)
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
// ✅ RaceCarState +4 DRIFT -- SETTLED AND FIXED (2026-08-01, physics wave 1). The extra four
// bytes were mCarAssetAttribKey: it is EIGHT bytes, not four. Proof and the full corroborating
// field table live in BrnVehicleEvents.h's RaceCarState banner (the producer,
// VehicleOutputInterface::UpdateRaceCarState @0x825EC808, stores it with `std` at state+960 and
// then lands all fourteen following scalars exactly where the committed offsets now say).
// The committed member offsets below are therefore CONSOLE-EXACT as of that fix, and this
// function's own `*(state + 968)` entity-id reads are now literally mEntityId @968.
// The historical measurement that bounded it is kept for provenance:
//
// (HISTORICAL) This function's thirteen
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
// (END HISTORICAL -- the bound was correct and the culprit was the last name in that run.)
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
// [gateui] BridgeWorldToGameState's destination buffer + the AI-car interface it copies.
#include "GameSource/GameState/BrnGameStateModuleIO.h"                          // GameStateModuleIO::PostWorldInputBuffer
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"               // BrnAI::AIModuleIO::AICarOutputInterface
#include <stdlib.h>                                                             // getenv (the [UI-gate] diag guard)

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

        // [crash-probe] witness. NOT X360. Inert unless BRN_CRASH_PLAYER is set. Latches the
        // 0 -> 1 EDGE only, so it proves the crash state reached the WORLD->DIRECTOR bridge --
        // i.e. that the commit is not physics-local the way the [showtime-probe] path is.
        {
            static const char* const kspW = getenv("BRN_CRASH_PLAYER");
            static bool sbSeen = false;
            if (kspW != 0 && mbPlayerCarCrashing && !sbSeen)
            {
                sbSeen = true;
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[crash-probe] BridgeWorldToDirector: mbPlayerCarCrashing=1"
                        << " (IsPlayerCarActive=" << (lpActiveRaceCars->IsPlayerCarActive() ? 1 : 0)
                        << " IsPlayerCarCrashing=" << (lpActiveRaceCars->IsPlayerCarCrashing() ? 1 : 0)
                        << ")\n";
                }
            }
        }

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

        // ---- step 6: the NEW-VEHICLE queue merge ------------------------------------------
        // ⭐ RESTORED 2026-08-02 (camera parameter-chain wave). The console's own step, in the
        // console's own position (immediately before the contact-spy publish). The X360 body
        // inlines BrnDirectorVehicleInputInterface::Append into
        // DirectorIO::InputBuffer::SetVehicleInputInterface @0x827AD1A0's mirror on this side:
        // clear the destination queue, then EventQueue<NewVehicleEvent,50>::Append @0x823C2CB8
        // the source's live events.
        //
        // ⚠️ THE DROP NOTE THIS RETIRES WAS PART TRUE AND PART EXPIRED. It read:
        // "BrnDirector::BrnDirectorVehicleInputInterface's NewVehicleEvent<50> queue and the
        // world side's producer are both un-homed; the queue is empty on this build."
        //   * "un-homed" is FALSE and was false when written: the interface has had a real
        //     home (SharedIO/BrnDirectorVehicleInputInterface.h, the DWARF layout with
        //     Append) since before this file's banner. Only the DESTINATION was opaque
        //     (DirectorIO::InputBuffer's +0x6780 span), and it is typed as of this wave.
        //   * "the queue is empty" was TRUE and is the part that had to be fixed: the
        //     console's producer chain starts in the physics VehicleManager, which this build
        //     does not have. See RaceCarEntityModule::
        //     PublishNewVehicleToDirectorWithoutPhysicsBringUp for the flagged stand-in.
        // This merge is what makes MainDirector::ProcessNewVehicleEvents see anything at all,
        // and that function is the ONLY primary writer of the two shared gameplay cameras'
        // Parameters::mbIsValid. Full map in BrnBehaviourGameplayExternal.h's Update FLAG.
        lpDirectorInput->GetVehicleInputInterface()->Append(
            lpWorldOutput->GetDirectorVehicleInputInterface());

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
                    // [BRING-UP MEASUREMENT, physics wave 1] the WRITE end of the half-extent
                    // transfer; BehaviourRotateAboutVehicle::Update prints the READ end.
                    *CgsDev::Log::gpDebugPrint
                        << "[world->director] publish #" << static_cast<s32>(suRaceCarInfoPublishCount)
                        << ": player index "
                        << static_cast<s32>(lePlayerIndex) << ", slot " << liSlot
                        << " at (" << lPos.x << ", " << lPos.y << ", " << lPos.z
                        << "), halfExtent (" << lHalfExtent.x << ", " << lHalfExtent.y
                        << ", " << lHalfExtent.z
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

    // ========================================================================================
    // ⭐⭐ [gateui] BrnGame::BrnGameModule::BridgeWorldToGameState  @ X360 0x823E5368
    //
    // THE WORLD -> GAME-STATE SEAM. Its game-event leg is the hop that carries the world's
    // per-frame game events -- including event 111 E_EVENT_RECORD_PROP_HIT, which
    // WorldModule::BridgeEntityModulesToOutput_PostPhysics (@0x827AEEB0 leg 10) posts from
    // PropEntityIO's mRecordHitPropQueue -- into the GameState post-world input buffer, from
    // where GameStateModule::PostWorldUpdate @0x8238F358 carries it to PreWorldUpdate
    // @0x823A5328 and ProcessGameEvents @0x823A0A18 case 111 ->
    // StuntManager::OnPropHit @0x8236EE18.
    //
    // SIGNATURE from the ASM prologue (@0x823E537C..0x823E5380): r3 = BrnGameModule* (never
    // dereferenced in the body), r4 = GameStateModuleIO::PostWorldInputBuffer* (the WRITE
    // side), r5 = const BrnWorldIO::UpdateOutputBuffer* (the READ side). Home + line proven by
    // the body's own assert ("lpRouteResponseOutput", GameBridgeWorldToX.cpp:505).
    // Sole caller: BrnGameModule::DoUpdate_GameStatePostWorld @0x823E92A8, which brackets it
    // with sub_823B7620 == LockBuffersForIO(postWorldInput, /*sources*/ a4..a8) -- destination
    // write-locked, every source read-locked -- and gates it on `(luUpdateSet & 0x20) == 0`.
    //
    // THE CONSOLE'S TEN LEGS, in body order, each with its measured accessor pair:
    //    1  RaceCarCrashEvent_::Append( postWorldIn->GetRaceCarCrashEventQueue() /*W,
    //         0x823B9258 -> +0x10, BrnGameStateModuleIO.h:199*/,
    //         worldOut->GetVehicleManagerOutputInterface() /*R, 0x823B5978 -> +27680*/ + 0x3A0 )
    //                                                                       @0x823E539C
    //  ⭐2  Append<1536,16>( postWorldIn->GetGameEventQueue() /*W, 0x823B91B0 -> +0xA4B0*/,
    //         worldOut->GetGameEventQueue() const /*R, 0x823B62A8 -> +216116*/ )
    //                                                                       @0x823E53B8
    //    3  postWorldIn->AppendTrafficTypeResponseQueue( worldOut->
    //         GetTrafficTypeResponseQueue() const /*R, 0x823B63F8 -> +155616,
    //         BrnWorldModuleIO.h:585*/ )                                    @0x823E53CC
    //    4  *postWorldIn-><+0x6E30> = *worldOut->GetContactSpyInterface()    (one 4-byte word)
    //         /*W 0x823B93A8 -> +0x6E30 :205; R 0x823B5D68 -> +146656*/     @0x823E53F0
    //    5  XMemCpy( postWorldIn->GetActiveRaceCarOutputInterface() /*W, 0x823B94F8 -> +0x7250
    //         :211*/, worldOut->GetActiveRaceCarOutputInterface() const /*R, 0x823B5C18 ->
    //         +29856*/, 10480 )                                             @0x823E540C
    //    6  XMemCpy( postWorldIn-><+0x9B40> /*W, 0x823B95A0 :214*/,
    //         worldOut->GetRaceCarGlobalOutputInterface() const /*R, 0x823B5AC8 -> +52672*/,
    //         2416 )                                                        @0x823E542C
    //    7  memcpy( postWorldIn->GetTriggerEntityOutputInterface() /*W, 0x823B9450 -> +0x6E34
    //         :208*/, worldOut->GetTriggerEntityOutputInterface() const /*R, 0x823B5B70 ->
    //         +50816*/, 1040 )                                              @0x823E5450
    //  ⭐8  memcpy( postWorldIn->GetAICarOutputInterface() /*W, 0x823B9648 -> +0xAAC0 :217*/,
    //         worldOut->GetAICarOutputInterface() const /*R, 0x823B5E10 -> +55088*/, 5352 )
    //                                                                       @0x823E5474
    //    9  VehicleOutputInterface::operator=( postWorldIn->GetVehicleOutputInterface() /*W,
    //         0x823B9300 -> +0x220 :202*/, worldOut->GetVehicleOutputInterface() const
    //         /*R, 0x823B58D0 -> +16*/ )                                    @0x823E5490
    //   10  assert(lpRouteResponseOutput);  for each of worldOut->GetRouteResponseQueue()
    //         const /*R, 0x823B5F60 -> +60440*/ entries: copy the 5136-byte RouteResponse and,
    //         when its muOwnerId (+0x140C) == 2 (E_OWNER_MODE_MANAGER), AddEvent(
    //           { u32 muEventId(+0x140E); f32 (nodeCount > 0 ? route->GetDistance() : 0.0f) },
    //           /*type*/ 174, /*size*/ 8 ) onto postWorldIn->GetGameEventQueue()
    //                                                                       @0x823E5540
    //
    // ⭐ LANDED HERE: legs 2 and 8 -- the only two whose SOURCE and DESTINATION are both real,
    // complete, correctly-locked types on this build. Leg 2 is the wave's load-bearing hop.
    //
    // [FLAG] PARKED, each with the precise blocker -- every one is named here, none is dropped
    // without a reason and an address:
    //   * leg 1  -- GameStateModuleIO::PostWorldInputBuffer has no write-lock
    //     GetRaceCarCrashEventQueue() (only the const half, X360 0x8231D170), and its member is
    //     still the named-opaque `RaceCarCrashEventQueue { u8 maOpaque[0x210]; }`, so the
    //     console's EventQueue<RaceCarCrashEvent,8>::Append has nothing to bind to. It also
    //     needs the VehicleManagerOutputInterface member at its +0x3A0, which is inside an
    //     opaque span. Owner: gsm (BrnGameStateModuleIO.h).
    //   * leg 3  -- BrnWorldIO::UpdateOutputBuffer declares only the WRITE-side
    //     AppendTrafficTypeResponseQueue (:586); the const READ getter the console calls
    //     (X360 0x823B63F8, BrnWorldModuleIO.h:585, returns +155616) is not declared in-tree.
    //     Owner: wire (BrnWorldModuleIO.h). Filed as a shared_header_request.
    //   * leg 4  -- the destination word at PostWorldInputBuffer +0x6E30 (X360 0x823B93A8,
    //     :205) has no accessor and no named member; it sits inside
    //     mVehicleOutputInterfaceStorage. Owner: gsm.
    //   * leg 5  -- no write-lock GetActiveRaceCarOutputInterface() on PostWorldInputBuffer,
    //     AND ⚠️ A MEASURED SIZE TRAP: the destination member is
    //     `u8 mActiveRaceCarOutputInterfaceStorage[0x2890]` == 10384 bytes, while the console
    //     copies 10480 (0x28F0). Landing this leg as a literal byte copy would overrun the
    //     member by 96 bytes. It must be retyped to the real
    //     RCEntityActiveRaceCarOutputInterface and copied BY ASSIGNMENT (the same correction
    //     GameStateModule::PostWorldUpdateStuntBringUp already documents for its own copy of
    //     this transfer). Owner: gsm.
    //   * leg 6  -- the destination at PostWorldInputBuffer +0x9B40 (X360 0x823B95A0, :214) has
    //     no accessor and no named member. Owner: gsm.
    //   * leg 7  -- the destination accessor exists, but its type is the named-opaque
    //     `TriggerEntityModuleOutputInterface { u8 maOpaque[16]; }` -- 16 bytes for a 1040-byte
    //     console copy. Owner: gsm.
    //   * leg 9  -- GameStateModuleIO::VehicleOutputInterface is a FORWARD-DECLARED class with
    //     no definition anywhere in the tree, so its operator= (X360 0x823C89C8) cannot be
    //     named. Owner: gsm.
    //   * leg 10 -- everything on the SOURCE side exists (BrnRouteMapModuleIO.h ::
    //     RouteResponse::GetOwnerId/GetEventId/GetRoute, BrnRoute.h :: GetNodeCount/
    //     GetDistance -- the console's `lfs` at RouteResponse+8 IS maNodes[0].z, i.e.
    //     Route::GetDistance()), and so does the destination queue. What is missing is the
    //     EVENT: game event id 174 and its 8-byte payload have no home in
    //     GameSource/GameState/BrnGameEvents.h. ProcessGameEvents @0x823A0A18 names the
    //     consumer's local "lpRouteInfoEvent", so the type wants to be
    //     `E_EVENT_ROUTE_INFO = 174` + `struct RouteInfoEvent { u32 muEventId; f32 mfDistance; }`.
    //     Owner: gsm (BrnGameEvents.h). Filed as a shared_header_request; the world side is
    //     then a ~12-line loop, spelled out above.
    //
    // ⚠️ NO CALL SITE YET, and that is deliberate. Nothing on this build creates a
    // GameStateModuleIO::PostWorldInputBuffer (DoUpdate_GameStatePostWorld @0x823E92A8, which
    // CreateIOBuffer<PostWorldInputBuffer>s it, is not reconstructed), so this function has no
    // buffer to be handed. The live PC feed of the SAME two legs is the direct one:
    // BrnGameModule::Update calls GameStateModule::PostWorldUpdateStuntBringUp with the world
    // output's GetActiveRaceCarOutputInterface() + GetGameEventQueue() -- ONE feed, not two
    // (see the banner at that call site in BrnGameModule.cpp). This body is the console shape,
    // homed where the console homes it, ready for the day the buffer exists.
    // DELETE-WHEN DoUpdate_GameStatePostWorld lands: then this becomes the only feed and the
    // bring-up entry point retires.
    // ========================================================================================
    void BrnGameModule::BridgeWorldToGameState(
            BrnGameState::GameStateModuleIO::PostWorldInputBuffer* lpGameStateInput,
            const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput)
    {
        CGS_ASSERT(lpGameStateInput != 0, "lpGameStateInput != NULL");
        CGS_ASSERT(lpWorldOutput    != 0, "lpWorldOutput != NULL");
        if (lpGameStateInput == 0 || lpWorldOutput == 0)
        {
            return;
        }

        // ---- leg 2: the game-event transfer (@0x823E53A4..0x823E53B8) -----------------------
        // Source read-locked (`GetGameEventQueue() const`), destination write-locked
        // (`GetGameEventQueue()`) -- the LockBuffersForIO discipline the caller sets up. Both
        // ends are CgsModule::VariableEventQueue<1536,16>: the world's by typedef
        // (BrnWorldModuleIO.h:556) and the game state's by the DWARF-attested derivation
        // (BrnGameEvents.h:314, `GameEventQueue : public VariableEventQueue<1536,16>`), so this
        // is the console's own bulk Append<1536,16>, unchanged.
        lpGameStateInput->GetGameEventQueue()->Append(*lpWorldOutput->GetGameEventQueue());

        // ---- leg 8: the AI car output interface (@0x823E5460..0x823E5474) -------------------
        // ⚠️ COPIED BY ASSIGNMENT, NEVER AT THE CONSOLE'S LITERAL 5352 BYTES. Both ends are the
        // SAME real type (BrnAI::AIModuleIO::AICarOutputInterface -- BrnWorldModuleIO.h:504 and
        // BrnGameStateModuleIO.h:169 both typedef it), so a member-wise copy is exact and stays
        // exact if the host layout ever widens; a literal `memcpy(dst, src, 5352)` would be a
        // console byte count carried onto the x64 host, which is this tree's most-repeated bug.
        {
            const BrnAI::AIModuleIO::AICarOutputInterface* lpAICarSource =
                lpWorldOutput->GetAICarOutputInterface();
            BrnGameState::GameStateModuleIO::AICarOutputInterface* lpAICarDest =
                lpGameStateInput->GetAICarOutputInterface();
            if (lpAICarSource != 0 && lpAICarDest != 0)
            {
                *lpAICarDest = *lpAICarSource;
            }
        }

        // [DIAG] NOT IN THE X360 BINARY -- the `[UI-gate]` ladder's GameState-transport rung.
        // Same logger, same env guard (BRN_PROP_DIAG) and same first-N latch as
        // PropEntityModule_wQ_04.cpp's "[prop-diag] BREAK" line. The count is read AFTER the
        // Append because Append does not drain its source.
        {
            static const bool sbPropDiag     = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static s32        siDiagLinesLeft = 8;

            const s32 liQueued = lpWorldOutput->GetGameEventQueue()->GetLength();
            if ( sbPropDiag && liQueued > 0 && siDiagLinesLeft > 0
                 && CgsDev::Log::gpDebugPrint != 0 )
            {
                --siDiagLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[UI-gate] world->gamestate events n=" << liQueued << "\n";
            }
        }
    }
}
