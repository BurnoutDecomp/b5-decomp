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
//       -- the hardest impact is the car's strongest contact-spy contact this frame (squared
//       |mNormalStress|, its normal and stress), which every gameplay camera shakes by
//   11. FOR EACH of the 8 slots: input->SetCrashingCentreOfMass(i, IDENTITY)
//   12. mfPlayerBoostPercentage = boost.mfBoostAmount / boost.mfMaxBoost  (0 if max ~= 0)
//   13. build the 48-byte PlayerCrashInfo block from the vehicle manager's crash queue
//   14. input->mbWorldWantsDebugControllerFocus = world-entity-status byte
//   15. mDirectorBridgeSerialiser.Unlock()
//
// REPRODUCED HERE: 2, 3, 6, 7, 8, 9, 10, 11, 12, 13.   (6 restored 2026-08-02; 13 and 8 on 2026-09-24.)
//
// [FLAG PC bring-up] dropped rather than paraphrased, each for a NAMED reason:
//   1/15 + the whole state-4..6 replay arm -- BrnGameModule has no mDirectorBridgeSerialiser
//        member (the console's is at gm+0x9A1070, inside this layout's omitted range) and
//        DirectorBridgeSerialiser::GetStaticLayout is declaration-only. The PC has no replay
//        path at all, so the live arm is the only one that can run.
//   4/5 -- their DESTINATIONS (mMidInterfaceBlock, the tail of
//        mVehicleDriverInputInterface) are honest-opaque byte spans
//        in BrnDirectorModuleIO.h. Writing into them by console byte offset is exactly the
//        offset-poke this project forbids; they land when those interface homes do.
//   (8 IS NO LONGER DROPPED -- restored 2026-09-24, crash parity FX-DIRECTOR. Its destination is
//        typed now: InputBuffer::mGlobalRaceCarInterface at +16, exactly as the finding that stood
//        here said -- 16 + 2416 == 2432 == mUsedRaceCars, and the x64 sizeof is 2416 too.)
//   (6 IS NO LONGER DROPPED -- restored 2026-08-02, see the step in the body.)
//   (13 IS NO LONGER DROPPED -- restored 2026-09-24, crash parity FX-BRIDGES CC-6. Both of its
//        reasons had expired: Camera::PlayerCrashInfo is homed (SharedIO/BrnPlayerInfo.h) and so is
//        its source, VehicleManagerOutputInterface::GetRaceCarCrashEventQueue (+0x3A0).)
//   14 -- reads world-entity-status byte +217668, inside an opaque span.
//   (the per-car HARDEST-IMPACT leg of 10 IS NO LONGER DROPPED -- restored 2026-09-24, crash
//        parity FX-BRIDGES CC-5. Its "not homed" reason had expired: BaseContact is homed in
//        Physics/ContactSpies/BrnContactSpyEvents.h (mNormalStress +0x20 / mNormal +0x30 of the
//        96-byte record) and RumbleManager::UpdateImpacts already walks the same run by name.)
//   (the per-car DEFORMED AABB arm of 10 IS NO LONGER DROPPED -- restored 2026-09-24, crash
//        parity FX-BRIDGES CC-7. DeformationOutputInterface::mpDeformationState is homed and written
//        every frame, and DeformationState::GetCarStateF / CarState::mDeformedBBoxMin/Max are named.)
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
//   * the console seeds mHardestNormalStressNormal from the .rdata vector at 0x82181510, which
//     x360rd reads as (0.0, 1.0, 0.0, 0.0) -- world up. (The old note here said the 16 bytes
//     "cannot be read"; they can, from the image, and the seed is now the console's.)
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
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.h"  // EffectsIO::DispatchInputBuffer (BridgeWorldToEffects_Dispatch)
#include "GameSource/World/BrnWorldModuleIO_DispatchOutputBuffer.h"                  // BrnWorldIO::DispatchOutputBuffer (same)
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"                     // Io::RootInputBuffer (BridgeWorldToSound; phase C3b)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"      // RaceCarState

#include <cstring>   // std::memcpy
#include <cmath>     // std::sqrt (the [cam-impact] diagnostic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"          // ContactSpyInterface / RaceCarContactQueue / BaseContact (the hardest-impact leg)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // VehicleManagerOutputInterface::RaceCarCrashEventQueue (step 13)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"          // VolumeInstanceId entity-word geometry (step 13)
#include "GameSource/World/BrnEntityTypes.h"                                  // E_ENTITYTYPE_* (step 13 owner switch)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // mpDeformationState (the deformed-AABB arm)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"          // DeformationState::GetCarStateF / CarState
#include "rw/math/vpu/vector3_operation.h"       // rw::math::vpu::IsValid / IsZero / operator- / MagnitudeSquared
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

        // ---- step 8: the global race-car table ---------------------------------------------
        // ⭐ RESTORED 2026-09-24 (crash parity FX-DIRECTOR). 0x823E3FD8..0x823E3FEC:
        // `XMemCpy(input + 0x10, world->GetRaceCarGlobalOutputInterface() (0x823B5AC8), 0x970)` --
        // the inlined DWARF InputBuffer::SetGlobalRaceCarInterface. MainDirector::ProcessInputQueue
        // converts the global race-car indices in actions 113 (a car reached a checkpoint) and 223
        // (a car joined) to active indices through it; unpublished, both conversions read nothing.
        lpDirectorInput->SetGlobalRaceCarInterface(lpWorldOutput->GetRaceCarGlobalOutputInterface());

        lpDirectorInput->SetPlayerCarIndex(lePlayerIndex);

        // ---- the frame's race-car contacts (DWARF GameBridgeWorldToX.cpp:87 lpCarContacts) --
        // X360 0x823E4010..0x823E4034: the local starts NULL and is filled only when the world's
        // contact spy is bound (`lwz r11, 0(GetContactSpy()) ; beq`), through
        // ContactSpyInterface::GetRaceCarContacts @0x82277900 (asserts mpData, returns mpData+0 ==
        // ContactSpyData::mRaceCarContactQueue). The per-car hardest-impact leg below reads it.
        // lpContactSpy only names the accessor result the console re-fetches at each use.
        const BrnPhysics::ContactSpy::ContactSpyInterface* lpContactSpy =
            lpWorldOutput->GetContactSpyInterface();
        const BrnPhysics::ContactSpy::ContactSpyData::RaceCarContactQueue* lpCarContacts = 0;
        if (lpContactSpy->IsValid())
        {
            lpCarContacts = lpContactSpy->GetRaceCarContacts();
        }

        // ---- the deformation output (DWARF GameBridgeWorldToX.cpp:94 lpDeformationOutputInterface)
        // X360 0x823E403C..0x823E4050: GetDeformationOutputInterface() @0x823B6350, kept in var_828
        // for the per-car deformed-AABB arm below.
        const BrnWorldIO::UpdateOutputBuffer::DeformationOutputInterface* lpDeformationOutputInterface =
            lpWorldOutput->GetDeformationOutputInterface();

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

            // The world AABB (X360 0x823E4924..0x823E49B4; DWARF :137 `AxisAlignedBox lBox`). The
            // console prefers the deformation state's own per-car box: when the output interface's
            // mpDeformationState (`lwz r3, 0x70(r31)`) is live AND DeformationState::GetCarStateF
            // @0x822CC340 (the DWARF's GetCarStateFromEntityId) resolves this car's mEntityId
            // (`lwz r4, 0x3C8`), it calls the lookup a SECOND time and copies the 32 bytes at
            // CarState +0x640 -- mDeformedBBoxMin / mDeformedBBoxMax (0x823E4954..0x823E4978). Only
            // otherwise does it fall back to the pristine box: max = mHalfExtent (+0x350), min = max
            // with the sign bit flipped in ALL FOUR lanes (`vspltisw v0,-1 ; vslw v0,v0,v0` builds
            // the 0x80000000 mask, `vxor` applies it -- w included).
            if (lpDeformationOutputInterface->mpDeformationState != 0
                && lpDeformationOutputInterface->mpDeformationState->GetCarStateF(lpState->mEntityId.muValue) != 0)
            {
                const BrnPhysics::Deformation::CarState* lpCarState =
                    lpDeformationOutputInterface->mpDeformationState->GetCarStateF(lpState->mEntityId.muValue);
                lVehicleInfo.mAABB.mMin = lpCarState->mDeformedBBoxMin;
                lVehicleInfo.mAABB.mMax = lpCarState->mDeformedBBoxMax;
            }
            else
            {
                const Vector3& lHalfExtent = lpState->mHalfExtent;
                lVehicleInfo.mAABB.mMin = Vector3{ -lHalfExtent.x, -lHalfExtent.y, -lHalfExtent.z, -lHalfExtent.w };
                lVehicleInfo.mAABB.mMax = lHalfExtent;
            }
            CGS_ASSERT(!rw::math::vpu::IsZero(lVehicleInfo.mAABB.mMax - lVehicleInfo.mAABB.mMin),
                       "!rw::math::IsZero(lVehicleInfo.mAABB.Max() - lVehicleInfo.mAABB.Min())"); // :148

            // ---- the hardest-impact leg (X360 0x823E4884..0x823E489C seed, 0x823E4A18..0x823E4B24
            //      walk) -- the only producer of VehicleInfo::mfHardestImpact, which is what the
            //      gameplay / bumper / bystander cameras scale their impact shake by.
            // Seed: the normal is the .rdata vector at 0x82181510 = (0, 1, 0, 0) (x360rd; `lvx128`
            // through r25 = unk_82181510), the stress is `vspltisw128 v127, 0`, the impact is f31 =
            // flt_82001CC0 = 0.0f.
            lVehicleInfo.mHardestNormalStressNormal = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
            lVehicleInfo.mHardestNormalStress.SetZero();
            lVehicleInfo.mfHardestImpact            = 0.0f;
            // [FLAG] the console never stores +0x4E4 (the stack byte keeps whatever the previous
            // car left); an uninitialised read is not reproducible, so it is published false.
            lVehicleInfo.mbHardestImpactIsAgainstWorld = false;

            // Walk: only while the spy is bound (`lwz r11, 0(GetContactSpy()) ; beq 0x823E4B28`),
            // this car's run in the race-car run list (GetRaceCarContactRunList @0x82355BF0 ->
            // ContactSpyRunList<8>::GetRunDataWithEntityID @0x82373C38 keyed on mEntityId, `lwz
            // 0x3C8`). The magnitude is the SQUARED length of mNormalStress (`vmsum3fp128 v0,v0,v0`
            // @0x823E4ADC, no square root: the camera takes the root itself), and only a STRICTLY
            // greater one replaces the running best (`vcmpgtfp128` @0x823E4AE4 + three vsel), so the
            // normal / stress pair is the first contact to reach the maximum. An empty run publishes
            // the zero accumulator (`stvx128 v127` @0x823E4A60 -> `lfs` @0x823E4B1C).
            if (lpContactSpy->IsValid())
            {
                const BrnPhysics::ContactSpy::ContactSpyRunData* lpRunData =
                    lpContactSpy->GetRaceCarContactRunList()->GetRunDataWithEntityID(lpState->mEntityId);
                if (lpRunData != 0)
                {
                    f32 lfHardestImpact = 0.0f;   // DWARF VecFloat, a splat accumulator (v127)
                    for (s32 liIndex = 0; liIndex < lpRunData->GetRunLength(); ++liIndex)
                    {
                        const BrnPhysics::ContactSpy::BaseContact* lpContact =
                            lpCarContacts->GetBaseContact(lpRunData->GetStartIndex() + liIndex);
                        const f32 lfImpact = rw::math::vpu::MagnitudeSquared(lpContact->mNormalStress);
                        if (lfImpact > lfHardestImpact)
                        {
                            lVehicleInfo.mHardestNormalStressNormal = lpContact->mNormal;
                            lfHardestImpact                         = lfImpact;
                            lVehicleInfo.mHardestNormalStress       = lpContact->mNormalStress;
                        }
                    }
                    lVehicleInfo.mfHardestImpact = lfHardestImpact;
                }
            }

            // The console clears this here and SetCrashingCentreOfMass (below) then sets it.
            lVehicleInfo.mbHasCrashingCenterOfMass = false;

            // The engine flag: the console inlines BOTH engine predicates and ORs them
            // (a non-player slot is always "on"; the player's slot follows its engine state).
            lVehicleInfo.mbEngineOn = lpActiveRaceCars->IsRaceCarEngineOn(leSlot)
                                   || lpActiveRaceCars->IsRaceCarEngineStarting(leSlot);

            lpDirectorInput->SetRaceCarInfo(static_cast<u32>(liSlot), lVehicleInfo);

            // [DIAG] NOT IN THE X360 BINARY -- BRN_CAM_INPUT_DIAG (the camera-input gate). The
            // impact-shake input as published: the first 40 non-zero publishes, then every 200th.
            // sqrt(impact) * 0.0001 is the chase camera's raw shake request
            // (BehaviourGameplayExternal::Update, before its 40 MPH ramp and 0.8 clamp).
            {
                static const bool sbImpactDiag = (getenv("BRN_CAM_INPUT_DIAG") != 0);
                static u32 suNonZeroImpacts = 0;
                if (sbImpactDiag && lVehicleInfo.mfHardestImpact > 0.0f
                    && CgsDev::Log::gpDebugPrint != 0)
                {
                    ++suNonZeroImpacts;
                    if (suNonZeroImpacts <= 40u || (suNonZeroImpacts % 200u) == 0u)
                    {
                        const Vector3& lN = lVehicleInfo.mHardestNormalStressNormal;
                        const Vector3& lS = lVehicleInfo.mHardestNormalStress;
                        *CgsDev::Log::gpDebugPrint
                            << "[cam-impact] #" << static_cast<s32>(suNonZeroImpacts)
                            << " slot " << liSlot << (leSlot == lePlayerIndex ? " (player)" : "")
                            << " mfHardestImpact " << lVehicleInfo.mfHardestImpact
                            << " shake-request " << std::sqrt(lVehicleInfo.mfHardestImpact) * 0.0001f
                            << " normal (" << lN.x << ", " << lN.y << ", " << lN.z
                            << ") stress (" << lS.x << ", " << lS.y << ", " << lS.z << ")\n";
                    }
                }
            }

            // [DIAG] NOT IN THE X360 BINARY -- BRN_CAM_INPUT_DIAG. The camera box as published when
            // it came from the DEFORMATION state (the arm CC-7 restored): the first 12 such
            // publishes, then every 400th, with the pristine half-extent box beside it and the
            // largest per-axis difference between the two.
            {
                static const bool sbBoxDiag = (getenv("BRN_CAM_INPUT_DIAG") != 0);
                static u32 suDeformedBoxes = 0;
                if (sbBoxDiag && CgsDev::Log::gpDebugPrint != 0
                    && lpDeformationOutputInterface->mpDeformationState != 0
                    && lpDeformationOutputInterface->mpDeformationState->GetCarStateF(lpState->mEntityId.muValue) != 0)
                {
                    ++suDeformedBoxes;
                    if (suDeformedBoxes <= 12u || (suDeformedBoxes % 400u) == 0u)
                    {
                        const Vector3& lMin = lVehicleInfo.mAABB.mMin;
                        const Vector3& lMax = lVehicleInfo.mAABB.mMax;
                        const Vector3& lHalf = lpState->mHalfExtent;
                        f32 lfWorst = 0.0f;
                        const f32 lafDelta[6] = { lMax.x - lHalf.x, lMax.y - lHalf.y, lMax.z - lHalf.z,
                                                  lMin.x + lHalf.x, lMin.y + lHalf.y, lMin.z + lHalf.z };
                        for (s32 liAxis = 0; liAxis < 6; ++liAxis)
                        {
                            const f32 lfAbs = lafDelta[liAxis] < 0.0f ? -lafDelta[liAxis] : lafDelta[liAxis];
                            if (lfAbs > lfWorst) lfWorst = lfAbs;
                        }
                        *CgsDev::Log::gpDebugPrint
                            << "[cam-aabb] #" << static_cast<s32>(suDeformedBoxes)
                            << " slot " << liSlot << (leSlot == lePlayerIndex ? " (player)" : "")
                            << " deformed box min (" << lMin.x << ", " << lMin.y << ", " << lMin.z
                            << ") max (" << lMax.x << ", " << lMax.y << ", " << lMax.z
                            << ") halfExtent (" << lHalf.x << ", " << lHalf.y << ", " << lHalf.z
                            << ") worst-axis delta " << lfWorst << "\n";
                    }
                }
            }

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
                        << "), halfExtent (" << lpState->mHalfExtent.x << ", " << lpState->mHalfExtent.y
                        << ", " << lpState->mHalfExtent.z
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

        // ---- step 13: the player's crash record (X360 0x823E4DF0..0x823E4FE4) ----------------
        // DWARF GameBridgeWorldToX.cpp:216..:231 (lPlayerCrashInfo / luLoop / lpCrashEventQueue /
        // luNumCrashEvents / lCrashEvent). The director reads it through ArbStateSharedInfo::
        // mpPlayerCrashInfo: mbWrecked picks the "Wrecked" crash treatment and the wrecked exit
        // (ArbStateCrashing), mbHitWater the drowning fade (Arbitrator), the two hard-stop flags
        // the crash moment selector.
        //   Construct             0x823E4DF4..0x823E4E20  zero vectors, mfSpeedMPH = f31 (0.0), bools
        //   mbWrecked             0x823E4E08 / 0x823E4E14 `lbz 0x28E0` = IsPlayerWrecked()
        //   mbHitWater            0x823E4E44..0x823E4E94  HasCrashedIntoWater(GetPlayerActiveRaceCarIndex())
        //                         (the inlined getter's line-980 assert, then `lbz 0x2810(iface + idx)`)
        //   crash queue           GetVehicleManagerOutputInterface() + 0x3A0, length `lwz 8`, stride 0x40
        //   match                 `ld 0 ; srdi 32` -- the entity word of mRaceCarVolumeInstanceID --
        //                         against GetRaceCarState(player)->mEntityId (`lwz 0x3C8`), UNSIGNED loop
        //   copy                  mfSpeedMPH +0x34, mCollisionNormal +0x10, mContactPoint +0x20: the LAST
        //                         matching event wins; the two flags only ever set, so they ACCUMULATE
        //   owner                 `lbz 8` = the high byte of mCrasherEntityID: 0 (WORLD) -> hard stop vs
        //                         wall; 1 (RACECAR) or 2 (TRAFFIC_VEHICLE) -> hard stop vs AI; else none
        //   publish               0x823E4FC4..0x823E4FE4, 6 x ld/std into input + 0x78E0
        {
            BrnDirector::Camera::PlayerCrashInfo lPlayerCrashInfo;
            lPlayerCrashInfo.Construct();
            lPlayerCrashInfo.mbWrecked  = lpActiveRaceCars->IsPlayerWrecked();
            lPlayerCrashInfo.mbHitWater =
                lpActiveRaceCars->HasCrashedIntoWater(lpActiveRaceCars->GetPlayerActiveRaceCarIndex());

            const BrnPhysics::Vehicle::VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpCrashEventQueue =
                lpWorldOutput->GetVehicleManagerOutputInterface()->GetRaceCarCrashEventQueue();
            const u32 luNumCrashEvents = static_cast<u32>(lpCrashEventQueue->GetLength());
            for (u32 luLoop = 0; luLoop < luNumCrashEvents; ++luLoop)
            {
                const BrnPhysics::Vehicle::RaceCarCrashEvent& lCrashEvent =
                    lpCrashEventQueue->GetEvent(static_cast<s32>(luLoop));

                // VolumeInstanceId::GetEntityId (DWARF call list): the HIGH dword of the 64-bit id.
                const u32 luCrashedEntityId = static_cast<u32>(
                    lCrashEvent.mRaceCarVolumeInstanceID.muId
                    >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX);
                if (luCrashedEntityId == lpActiveRaceCars->GetRaceCarState(lePlayerIndex)->mEntityId.muValue)
                {
                    lPlayerCrashInfo.mfSpeedMPH        = lCrashEvent.mfSpeedMPH;
                    lPlayerCrashInfo.mvCollisionNormal = lCrashEvent.mCollisionNormal;
                    lPlayerCrashInfo.mvContactPoint    = lCrashEvent.mContactPoint;

                    // The owner byte of the crasher's entity word (bits 24..31).
                    switch (lCrashEvent.mCrasherEntityID.muValue >> CgsSceneManager::VolumeInstanceId::KU_OWNER_BASE)
                    {
                    case BrnWorld::E_ENTITYTYPE_WORLD:
                        lPlayerCrashInfo.mbHardstopVsWall = true;
                        break;
                    case BrnWorld::E_ENTITYTYPE_RACECAR:
                    case BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE:
                        lPlayerCrashInfo.mbHardStopVsAI = true;
                        break;
                    default:
                        break;
                    }
                }
            }

            lpDirectorInput->SetPlayerCrashInfo(&lPlayerCrashInfo);

            // [DIAG] NOT IN THE X360 BINARY -- BRN_CRASHCAM_DIAG (the crash-camera gate). The
            // record as published, on every CHANGE of its four flags or of a non-zero speed (at
            // most 60 lines): the witness that ArbStateCrashing / the moment selector / the water
            // fade now receive what the world says instead of a zeroed slot.
            {
                static const bool sbCrashInfoDiag = (getenv("BRN_CRASHCAM_DIAG") != 0);
                static s32 siLinesLeft = 60;
                static u32 suLastKey = 0;
                const u32 luKey = (lPlayerCrashInfo.mbHardstopVsWall ? 1u : 0u)
                                | (lPlayerCrashInfo.mbHardStopVsAI   ? 2u : 0u)
                                | (lPlayerCrashInfo.mbWrecked        ? 4u : 0u)
                                | (lPlayerCrashInfo.mbHitWater       ? 8u : 0u)
                                | (lPlayerCrashInfo.mfSpeedMPH != 0.0f ? 16u : 0u);
                if (sbCrashInfoDiag && luKey != suLastKey && siLinesLeft > 0
                    && CgsDev::Log::gpDebugPrint != 0)
                {
                    --siLinesLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[crash-info] player slot " << static_cast<s32>(lePlayerIndex)
                        << " wrecked " << (lPlayerCrashInfo.mbWrecked ? 1 : 0)
                        << " hitWater " << (lPlayerCrashInfo.mbHitWater ? 1 : 0)
                        << " hardstopVsWall " << (lPlayerCrashInfo.mbHardstopVsWall ? 1 : 0)
                        << " hardStopVsAI " << (lPlayerCrashInfo.mbHardStopVsAI ? 1 : 0)
                        << " speedMPH " << lPlayerCrashInfo.mfSpeedMPH
                        << " normal (" << lPlayerCrashInfo.mvCollisionNormal.x << ", "
                        << lPlayerCrashInfo.mvCollisionNormal.y << ", "
                        << lPlayerCrashInfo.mvCollisionNormal.z << ")"
                        << " crash events " << static_cast<s32>(luNumCrashEvents) << "\n";
                }
                suLastKey = luKey;
            }
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
    //   * leg 10 -- ⭐ LANDED 2026-09-24 (crash parity FX-BRIDGES CC-11) through
    //     BridgeWorldToGameState_RouteInfo; the note below is kept as the record of what it needed.
    //     Everything on the SOURCE side exists (BrnRouteMapModuleIO.h ::
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
    // ========================================================================================
    // [FX-BRIDGES CC-11, 2026-09-24] BridgeWorldToGameState_RouteInfo -- LEG 10 of
    // BridgeWorldToGameState @0x823E5368, de-inlined (see GameBridgeWorldToX.h for why).
    //   0x823E5494..0x823E54C4  lpRouteResponseOutput = worldOut->GetRouteResponseQueue() const
    //                           (0x823B5F60); assert it (GameBridgeWorldToX.cpp:505, `li r5, 0x1F9`)
    //   0x823E54C8..0x823E54D4  the count (`lwz r28, 8(r31)`), read once; f31 = flt_82001CC0 (0.0f)
    //   0x823E54E0..0x823E54F8  per response: GetEvent(i), and the whole 0x1410-byte record memcpy'd
    //                           to the stack
    //   0x823E54FC..0x823E5504  only muOwnerId (+0x140C) == 2 (E_OWNER_MODE_MANAGER) is forwarded
    //   0x823E5508..0x823E551C  distance = node count (+0x1400) > 0 ? Route::GetDistance() (+0x8) : 0.0f
    //   0x823E5520..0x823E5540  { (s32) muEventId (+0x140E), distance } -> AddEvent(record, 174, 8)
    //                           onto the post-world input's game-event queue
    // ========================================================================================
    void BridgeWorldToGameState_RouteInfo(CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
                                          const BrnWorldIO::UpdateOutputBuffer*     lpWorldOutput)
    {
        const BrnAI::RouteMapModuleIO::RouteResponseQueue* lpRouteResponseOutput = lpWorldOutput->GetRouteResponseQueue();
        CGS_ASSERT(lpRouteResponseOutput != 0, "lpRouteResponseOutput");   // GameBridgeWorldToX.cpp:505

        const s32 liNumResponses = lpRouteResponseOutput->GetLength();
        for (s32 liResponse = 0; liResponse < liNumResponses; ++liResponse)
        {
            const BrnAI::RouteMapModuleIO::RouteResponse lRouteResponse = lpRouteResponseOutput->GetEvent(liResponse);
            if (lRouteResponse.GetOwnerId() == BrnAI::RouteMapModuleIO::E_OWNER_MODE_MANAGER)
            {
                BrnGameState::GameStateModuleIO::ModeManagerRouteInfoEvent lRouteInfoEvent;
                lRouteInfoEvent.miEventId       = static_cast<s32>(lRouteResponse.GetEventId());
                lRouteInfoEvent.mfRouteDistance = (lRouteResponse.GetRoute()->GetNodeCount() > 0)
                                                      ? lRouteResponse.GetRoute()->GetDistance()
                                                      : 0.0f;   // f31 = flt_82001CC0
                lpGameEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRouteInfoEvent),
                                           static_cast<s32>(BrnGameState::GameStateModuleIO::E_EVENT_MODE_MANAGER_ROUTE_INFO),
                                           static_cast<s32>(sizeof(lRouteInfoEvent)));   // li r5, 0xAE ; li r6, 8

                // [DIAG] NOT IN THE X360 BINARY -- BRN_ROUTE_INFO_DIAG: every forwarded answer (first 40).
                static const bool sbRouteDiag = (getenv("BRN_ROUTE_INFO_DIAG") != 0);
                static s32 siRouteLinesLeft = 40;
                if (sbRouteDiag && siRouteLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
                {
                    --siRouteLinesLeft;
                    *CgsDev::Log::gpDebugPrint << "[route-info] BridgeWorldToGameState leg 10: event 174 leg "
                                               << lRouteInfoEvent.miEventId << " nodes "
                                               << lRouteResponse.GetRoute()->GetNodeCount() << " distance "
                                               << lRouteInfoEvent.mfRouteDistance << "\n";
                }
            }
        }
    }

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

        // ---- leg 10: the mode manager's route answers (@0x823E5494..0x823E554C) -------------
        // [FX-BRIDGES CC-11, 2026-09-24] Landed through the de-inlined helper above, onto the same
        // write-locked destination queue leg 2 appends into (legs 3..9 never touch it, so the
        // queue's content order -- world events, then the answers -- is the console's).
        BridgeWorldToGameState_RouteInfo(lpGameStateInput->GetGameEventQueue(), lpWorldOutput);

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
    // @ 0x823CD580 (bodied 2026-08-25, faithful-audio-engine phase C3b). The
    // world -> sound input bridge, console step-for-step (the caller holds the
    // world output's read lock + the sound input's write lock):
    //   [1] SetVehicleData from the ACTIVE race-car output interface -- or the
    //       REPLAY one when the update set carries bit 0x100 (replay playback).
    //   [2] the seven interface installs (contact spy / traffic / the physical-
    //       traffic block at vehicle-output +9760 / deformation / world-load /
    //       AI car).
    //   [3] the four queue appends (audio-car-loaded, the 1536 game events, the
    //       prop-became-physical events, the prop-update notifications).
    // The cross-namespace casts view each opaque record through its twin: both
    // sides model the SAME console record (the spans match member-for-member);
    // the typed side wins where one exists.
    void BrnGameModule::BridgeWorldToSound(BrnSound::Module::Io::RootInputBuffer* lpSoundInputBuffer,
                                           const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                                           BrnUpdateSet leUpdateSet)
    {
        typedef BrnSound::Module::Io::RootInputBuffer RootIn;

        // [1] the vehicle data (bit 0x100 == the replay-source select).
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicleData =
            ((leUpdateSet & 0x100) != 0)
                ? lpWorldOutputBuffer->GetReplayActiveRaceCarOutputInterface()
                : lpWorldOutputBuffer->GetActiveRaceCarOutputInterface();
        lpSoundInputBuffer->SetVehicleData(lpVehicleData);

        // [2] the interface installs (each opaque view cast onto its twin).
        lpSoundInputBuffer->SetContactSpyQueueInterface(
            lpWorldOutputBuffer->GetContactSpyInterface());
        lpSoundInputBuffer->SetTrafficOutputInterface(
            reinterpret_cast<const RootIn::TrafficSoundOutputInterface*>(
                lpWorldOutputBuffer->GetTrafficSoundOutputInterface()));
        // The physical-traffic block rides INSIDE the vehicle output interface at +0x2620 --
        // mTrafficStateQueue, the member the console reaches inline here and at the traffic
        // entity module's own consumer. Read it through the typed accessor, exactly as the
        // effects path does; the cast is only the cross-namespace view onto the sound twin.
        lpSoundInputBuffer->SetPhysicalTrafficStates(
            reinterpret_cast<const RootIn::PhysicalTrafficStateQueue*>(
                lpWorldOutputBuffer->GetVehicleOutputInterface()->GetTrafficStateQueue()));
        lpSoundInputBuffer->SetDeformationInterface(
            reinterpret_cast<const RootIn::DeformationInterface*>(
                lpWorldOutputBuffer->GetDeformationOutputInterface()));
        lpSoundInputBuffer->SetWorldLoadInterface(
            reinterpret_cast<const RootIn::SoundWorldLoadInterface*>(
                lpWorldOutputBuffer->GetSoundWorldLoadInterface()));
        lpSoundInputBuffer->SetAICarOutputInterface(
            reinterpret_cast<const RootIn::AICarOutputInterface*>(
                lpWorldOutputBuffer->GetAICarOutputInterface()));

        // [3] the queue appends.
        lpSoundInputBuffer->GetAudioCarDataLoadedQueue()->Append(
            *lpWorldOutputBuffer->GetAudioCarLoadedDataQueue());
        // Both game-event ends are VariableEventQueue<1536,16> (the console append
        // symbol pins both); the root side's nominal 4-byte tag is viewed through
        // the real queue type over its attested +0x6494..+0x6AB0 span.
        reinterpret_cast<CgsModule::VariableEventQueue<1536, 16>*>(
            lpSoundInputBuffer->GetGameEventQueue())->Append(
                *reinterpret_cast<const CgsModule::VariableEventQueue<1536, 16>*>(
                    lpWorldOutputBuffer->GetGameEventQueue()));
        // The prop pairs: the world side owns the typed queues; the root side's
        // attested-width storage is viewed through the same types.
        reinterpret_cast<BrnWorldIO::UpdateOutputBuffer::PropBecamePhysicalEventQueue*>(
            lpSoundInputBuffer->GetPropBecamePhysicalEventQueue())->Append(
                *lpWorldOutputBuffer->GetPropBecamePhysicalEventQueue());
        reinterpret_cast<BrnWorldIO::UpdateOutputBuffer::PropUpdateNotificationQueue*>(
            lpSoundInputBuffer->GetPropUpdateNotificationQueue())->Append(
                *lpWorldOutputBuffer->GetPropUpdateNotificationQueue());
    }

// =================================================================================================
// BrnGameModule::BridgeWorldToEffects_Dispatch  @ X360 0x823C11F8
//
// THE WORLD -> EFFECTS DISPATCH BRIDGE. Four legs, all read out of the world's dispatch OUTPUT
// buffer and written into the effects dispatch INPUT buffer: key-light direction, key-light
// colour, average irradiance colour, and the WHITE LEVEL.
//
// ⛔⛔ THIS IS THE FUNCTION THREE COMMITS CALLED "BridgeRendererToEffects @0x823C1168", AND THE
// DISTINCTION IS THE WHOLE POINT. The two are adjacent in the image and both are called exactly
// once, from DoDispatch @0x823DC458, but they carry different payloads:
//     0x823C1168 BridgeRendererToEffects        -- GetDispatchFrame -> in+0x10, GetBaseEffectsFrame,
//                                                  GetFXEventsEffectsFrame(0..1), SetEnvironmentMap.
//                                                  22 instructions, NOT ONE OF THEM A FLOAT.
//     0x823C11F8 BridgeWorldToEffects_Dispatch   -- THIS. Its last two calls are
//                                                     bl BrnWorldIO::DispatchOutputBuffer::GetWhiteLevel
//                                                     bl BrnEffects::EffectsIO::DispatchInputBuffer::SetWhiteLevel
// and SetWhiteLevel @0x823BAB40's xrefs_to list has exactly ONE entry: this function. So this is
// the only path by which a white level ever reaches the effects side, i.e. the only producer of
// the particle pass's colourScale. (See the banner in EffectsModule::GenerateDispatchLists for
// what that cost: the particles ran at 1.0 while the rest of the frame ran at 0.5.)
//
// SIGNATURE, from the asm rather than the pseudocode. The prologue is
//     0x823C120C  mr r30, r5      ; the world DispatchOutputBuffer
//     0x823C1210  mr r31, r4      ; the effects DispatchInputBuffer
// and r3 (`this`) is overwritten by the very next instruction (`addi r3, r1, var_40`, the first
// getter's return slot) and never read again. So the method genuinely does not touch its own
// object -- it is a static-shaped member, exactly like its BridgeRendererToEffects neighbour.
//
// THE THREE VECTOR LEGS ARE BARE STORES, NOT SETTER CALLS:
//     li r10, 0x20 / lvx128 v0, r0, r11 / stvx128 v0, r31, r10      -- mKeyLightDirection
//     li r10, 0x30 / lvx128 v0, r0, r11 / stvx128 v0, r31, r10      -- mKeyLightColour
//     li r10, 0x40 / lvx128 v0, r0, r11 / stvx128 v0, r31, r10      -- mAverageIrradianceColour
// with no write-lock assert, while the fourth leg goes through the out-of-line SetWhiteLevel
// (which DOES assert the lock). That is the compiler inlining three header setters and not the
// fourth -- and the X360 ledger agrees: SetWhiteLevel has a row, the three vector setters do not.
// They are therefore spelled as the inline setters added to the effects header, not as offset
// pokes. (The getters take a 16-byte return slot in r3 and the buffer in r4; the `lvx128` reads
// that slot straight back, so by value here.)
//
// ⚠ NO CALLER ON THIS BUILD, AND THE REASON IS AN OBJECT, NOT A FUNCTION. DoDispatch would call
// this immediately after BridgeRendererToEffects, but neither IO buffer exists on PC: no
// BrnEffects::EffectsIO::DispatchInputBuffer is ever created, and the only
// BrnWorldIO::DispatchOutputBuffer in the tree is the file-static bring-up stand-in
// gWorldDispatchOutputBringUp inside BrnWorldModule.cpp. Until that buffer set is real,
// EffectsModule::GenerateDispatchLists' documented null arm stands in for this function's WHITE
// LEVEL leg, reading the same number out of gBrnWorldShaderConstantsFrameBringUp -- which
// WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410 fills from the same environment
// manager, in the same call, as the DispatchOutputBuffer word this reads (BrnWorldModule.cpp:4958
// vs :4970). DELETE-WHEN the dispatch IO buffer set is real: wire this in DoDispatch and drop
// that arm.
// =================================================================================================
void BrnGameModule::BridgeWorldToEffects_Dispatch(
        BrnEffects::EffectsIO::DispatchInputBuffer* lpEffectsInputBuffer,
        const BrnWorldIO::DispatchOutputBuffer*     lpWorldOutputBuffer )
{
    lpEffectsInputBuffer->SetKeyLightDirection( lpWorldOutputBuffer->GetKeyLightDirection() );
    lpEffectsInputBuffer->SetKeyLightColour( lpWorldOutputBuffer->GetKeyLightColour() );
    lpEffectsInputBuffer->SetAverageIrradianceColour(
        lpWorldOutputBuffer->GetAverageIrradianceColour() );
    lpEffectsInputBuffer->SetWhiteLevel( lpWorldOutputBuffer->GetWhiteLevel() );
}

}
