// World bridges. BridgeWorldToDirector is recovered from ARTIST823E3AB0.
// Its persistent DirectorBridgeSerialiser is locked across the complete pass.
// Modes0/1/2/3 publish and capture the live world;4/7 only acknowledge real
// world streaming readiness;5/6 restore the static replay frame. The final
// debug-controller-focus transfer and Unlock run on every original branch.
// Other live conversion/diagnostic code below is preserved from the baseline.

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

        mDirectorBridgeSerialiser.Lock();

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

        const BrnReplays::BaseSerialiser::EMode leMode = mDirectorBridgeSerialiser.GetMode();
        if (!lpActiveRaceCars->IsPlayerCarActive())
        {
            lpDirectorInput->SetPlayerCarIndex(E_ACTIVE_RACE_CAR_INDEX_INVALID);
        }
        else if (leMode == BrnReplays::BaseSerialiser::E_MODE_IDLE ||
                 mDirectorBridgeSerialiser.IsRecording())
        {
        // ---- step 9 (the console orders it after 4..8, all of which are gated) ------------
        CGS_ASSERT(lpActiveRaceCars->IsPlayerCarActive(), "Player race car index not active"); // :66
        const EActiveRaceCarIndex lePlayerIndex = lpWorldOutput->GetPlayerActiveRaceCarIndex();

        // ---- step 4: the traffic module's director records ----------------------------------
        // ⭐ RESTORED 2026-09-25 (crash parity FX-DIRECTOR2). 0x823E3F5C..0x823E3F78, straight after
        // the assert above:
        //     r3 = world->GetTrafficDirectorOutputInterface() const        (0x823B6158, read-locked)
        //     lhz r10, 0(r3) ; sth r10, 0x6AC0(input)                     (mu16EntityCount)
        //     sub_823B2368(input + 0x6AD0, r3 + 0x10)                      (the 32-slot entity array:
        //                                                                   32 x 0x70 records, then its
        //                                                                   count word +0xE00)
        // -- the inlined DWARF InputBuffer::SetTrafficOutputInterface (:291), i.e. the interface's
        // member-wise copy. Its one reader is MainDirector::PreSceneQueryUpdate, which hands the
        // array to AllVehicleData::Update as lpTrafficVehicleArray (0x8225BC30..0x8225BC70); the
        // producer is TrafficEntityModule::ProcessNearbyTrafficSceneQueryResults via the world
        // bridge's SetTrafficDirectorOutputInterface (186e27bb). The "dropped" note in the banner
        // above had one reason, an opaque destination, and it expired with the typed member.
        lpDirectorInput->SetTrafficOutputInterface(lpWorldOutput->GetTrafficDirectorOutputInterface());

        // ARTIST823E3F80..3FA4: the actual five status bytes, including
        // WorldEntityIO::StatusInterface::GetAllStreamed at byte4.
        lpDirectorInput->SetWorldEntityStatusInterface(lpWorldOutput->GetWorldEntityStatusInterface());


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
        if (BrnReplays::DirectorBridgeSerialiserStaticLayout* lpFrame = mDirectorBridgeSerialiser.GetStaticLayout())
            lpFrame->muActivePlayerIndex = static_cast<s8>(lePlayerIndex);


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

            const bool lbVehicleActive = lpActiveRaceCars->IsRaceCarActive(leSlot);
            if (BrnReplays::DirectorBridgeSerialiserStaticLayout* lpFrame = mDirectorBridgeSerialiser.GetStaticLayout())
                lpFrame->mabVehicleActive[liSlot] = lbVehicleActive;
            if (!lbVehicleActive)
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
            if (BrnReplays::DirectorBridgeSerialiserStaticLayout* lpFrame = mDirectorBridgeSerialiser.GetStaticLayout())
                lpFrame->maVehicleInfos[liSlot] = lVehicleInfo;


            // [DIAG] NOT IN THE X360 BINARY -- BRN_CAM_INPUT_DIAG (the camera-input gate). The
            // impact-shake input as published: the first 40 non-zero publishes, then every 200th --
            // and at most 60 lines per session (HARD CAP 2026-09-24, crash-parity diag hygiene).
            // sqrt(impact) * 0.0001 is the chase camera's raw shake request
            // (BehaviourGameplayExternal::Update, before its 40 MPH ramp and 0.8 clamp).
            {
                static const bool sbImpactDiag = (getenv("BRN_CAM_INPUT_DIAG") != 0);
                static u32 suNonZeroImpacts = 0;
                static s32 siImpactLinesLeft = 60;
                if (sbImpactDiag && lVehicleInfo.mfHardestImpact > 0.0f
                    && CgsDev::Log::gpDebugPrint != 0)
                {
                    ++suNonZeroImpacts;
                    if (siImpactLinesLeft > 0 && (suNonZeroImpacts <= 40u || (suNonZeroImpacts % 200u) == 0u))
                    {
                        --siImpactLinesLeft;
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
            // publishes, then every 400th -- at most 24 lines per session (HARD CAP 2026-09-24) --
            // with the pristine half-extent box beside it and the largest per-axis difference
            // between the two.
            {
                static const bool sbBoxDiag = (getenv("BRN_CAM_INPUT_DIAG") != 0);
                static u32 suDeformedBoxes = 0;
                static s32 siBoxLinesLeft = 24;
                if (sbBoxDiag && CgsDev::Log::gpDebugPrint != 0
                    && lpDeformationOutputInterface->mpDeformationState != 0
                    && lpDeformationOutputInterface->mpDeformationState->GetCarStateF(lpState->mEntityId.muValue) != 0)
                {
                    ++suDeformedBoxes;
                    if (siBoxLinesLeft > 0 && (suDeformedBoxes <= 12u || (suDeformedBoxes % 400u) == 0u))
                    {
                        --siBoxLinesLeft;
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

            // [DIAG] NOT IN THE X360 BINARY -- bring-up diagnostic: print the pose the cameras will
            // actually frame -- on the FIRST publish (which lands the frame the car is attached,
            // before its physics state has been seeded) and then every 3000th (~40 s), which is the
            // steady state. This is the exact claim the retired fake-car stand-in could not make.
            // GATED 2026-09-24 (crash-parity diag hygiene) behind BRN_CAM_INPUT_DIAG, the file's
            // default-off camera-input switch, and capped at 20 lines: an ungated periodic print is
            // not console behaviour. (Remove when the whole world->director staging is routine.)
            ++suRaceCarInfoPublishCount;
            static const bool sbPublishDiag = (getenv("BRN_CAM_INPUT_DIAG") != 0);
            static s32 siPublishLinesLeft = 20;
            if (sbPublishDiag && siPublishLinesLeft > 0 &&
                (suRaceCarInfoPublishCount == 1u || (suRaceCarInfoPublishCount % 3000u) == 0u))
            {
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    --siPublishLinesLeft;
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
                if (BrnReplays::DirectorBridgeSerialiserStaticLayout* lpFrame = mDirectorBridgeSerialiser.GetStaticLayout())
                    lpFrame->maCentreOfMassTransforms[liSlot] = lIdentity;

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
            if (BrnReplays::DirectorBridgeSerialiserStaticLayout* lpFrame = mDirectorBridgeSerialiser.GetStaticLayout())
                lpFrame->mfPlayerBoostPercentage = lfBoostPercentage;

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
            if (BrnReplays::DirectorBridgeSerialiserStaticLayout* lpFrame = mDirectorBridgeSerialiser.GetStaticLayout())
                lpFrame->mPlayerCrashInfo = lPlayerCrashInfo;


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
        else if (leMode == BrnReplays::BaseSerialiser::E_MODE_RECORDING_PREPARING ||
                 leMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING_PREPARING ||
                 leMode == BrnReplays::BaseSerialiser::E_MODE_RESTORING)
        {
            // ARTIST823E3EC8..3EF8. Preparing/restoring does not publish a live
            // snapshot or overwrite the retained Director input's world status.
            mDirectorBridgeSerialiser.SetDataReady(
                lpWorldOutput->GetWorldEntityStatusInterface()->GetAllStreamed());
            mDirectorBridgeSerialiser.SetDataRestored(
                lpWorldOutput->GetWorldEntityStatusInterface()->GetAllStreamed());
        }
        else if (mDirectorBridgeSerialiser.IsPlaying())
        {
            // ARTIST823E3D2C..3E88: replay data is the sole vehicle/camera input
            // source here. Stalled playback republishes the same static frame.
            lpDirectorInput->SetPlayerCarIndex(static_cast<EActiveRaceCarIndex>(
                mDirectorBridgeSerialiser.GetActivePlayerIndex()));
            for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
            {
                if (mDirectorBridgeSerialiser.GetVehicleActive(liSlot))
                    lpDirectorInput->SetRaceCarInfo(static_cast<u32>(liSlot),
                        *mDirectorBridgeSerialiser.GetVehicleInfo(liSlot));
            }
            for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
            {
                lpDirectorInput->SetCrashingCentreOfMass(static_cast<u32>(liSlot),
                    *mDirectorBridgeSerialiser.GetCentreOfMassTransform(liSlot));
                CGS_ASSERT(liSlot + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
            }
            BrnReplays::DirectorBridgeSerialiserStaticLayout* lpFrame = mDirectorBridgeSerialiser.GetStaticLayout();
            CGS_ASSERT(lpFrame != 0, "lpStatic");
            lpDirectorInput->SetPlayerBoostPercentage(lpFrame->mfPlayerBoostPercentage);
            lpFrame = mDirectorBridgeSerialiser.GetStaticLayout();
            CGS_ASSERT(lpFrame != 0, "lpStatic");
            lpDirectorInput->SetPlayerCrashInfo(&lpFrame->mPlayerCrashInfo);
            mDirectorBridgeSerialiser.SetDataReady(
                lpWorldOutput->GetWorldEntityStatusInterface()->GetAllStreamed());
            mDirectorBridgeSerialiser.SetDataRestored(
                lpWorldOutput->GetWorldEntityStatusInterface()->GetAllStreamed());
        }

        // ARTIST823E5098..50B4: this tail runs even with no active player,
        // and for idle, preparing, stalled and restoring serialiser states.
        lpDirectorInput->SetWorldWantsDebugControllerFocus(
            lpWorldOutput->GetWorldWantsDebugControllerFocus());
        mDirectorBridgeSerialiser.Unlock();

    }

    // ========================================================================================
    // BrnGame::BrnGameModule::BridgeWorldToGameState
    //
    // The world -> game-state hand-off: every per-frame world product the post-world half of the
    // game state reads is copied into the post-world input buffer here. Sole caller:
    // DoUpdate_GameStatePostWorld, inside its LockBuffersForIO bracket (the post-world input
    // write-locked, the world update output read-locked) and only when update-set bit 0x20 is
    // clear. `this` is never read.
    //
    // Ten legs, in console order:
    //    1  the physics race-car crash events (the vehicle-manager output's +0x3A0 queue) appended
    //       onto the buffer's crash-event queue
    //    2  the world's game events appended onto the buffer's game-event queue
    //    3  the traffic-type responses appended onto the buffer's own queue
    //    4  the contact-spy handle copied
    //    5  the active race-car output interface copied (console XMemCpy of 0x28F0 bytes)
    //    6  the global race-car output interface copied (console XMemCpy of 0x970 bytes)
    //    7  the trigger-entity output interface copied (console memcpy of 0x410 bytes)
    //    8  the AI-car output interface copied (console memcpy of 0x14E8 bytes)
    //    9  the vehicle output interface copied with its own operator=
    //   10  every route answer the mode manager asked for becomes game event 174 on the buffer's
    //       game-event queue, after the world's own events from leg 2
    // Legs 4-8 copy by assignment: the console byte counts are console sizes, and the host
    // layouts of the active race-car interface and the contact-spy handle are wider.
    // ========================================================================================
    void BrnGameModule::BridgeWorldToGameState(
            BrnGameState::GameStateModuleIO::PostWorldInputBuffer* lpGameStateInput,
            const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput)
    {
        // ---- leg 1: the race-car crash events ------------------------------------------------
        lpGameStateInput->GetRaceCarCrashEventQueue()->Append(
            *lpWorldOutput->GetVehicleManagerOutputInterface()->GetRaceCarCrashEventQueue());

        // ---- leg 2: the game-event transfer ----------------------------------------------------
        // Source read-locked (`GetGameEventQueue() const`), destination write-locked
        // (`GetGameEventQueue()`). Both ends are CgsModule::VariableEventQueue<1536,16> (the game
        // state's GameEventQueue derives from it), so this is the queue's own bulk Append.
        lpGameStateInput->GetGameEventQueue()->Append(*lpWorldOutput->GetGameEventQueue());

        // ---- leg 3: the traffic-type responses ---------------------------------------------------
        lpGameStateInput->AppendTrafficTypeResponseQueue(lpWorldOutput->GetTrafficTypeResponseQueue());

        // ---- legs 4-7: the contact-spy handle and the race-car / trigger interfaces --------------
        *lpGameStateInput->GetContactSpyInterface()           = *lpWorldOutput->GetContactSpyInterface();
        *lpGameStateInput->GetActiveRaceCarOutputInterface()  = *lpWorldOutput->GetActiveRaceCarOutputInterface();
        *lpGameStateInput->GetGlobalRaceCarOutputInterface()  = *lpWorldOutput->GetRaceCarGlobalOutputInterface();
        *lpGameStateInput->GetTriggerEntityOutputInterface()  = *lpWorldOutput->GetTriggerEntityOutputInterface();

        // ---- leg 8: the AI car output interface ----------------------------------------------------
        // Both ends are the same type (BrnAI::AIModuleIO::AICarOutputInterface -- BrnWorldModuleIO.h
        // and BrnGameStateModuleIO.h both typedef it), so the member-wise copy is exact.
        *lpGameStateInput->GetAICarOutputInterface() = *lpWorldOutput->GetAICarOutputInterface();

        // ---- leg 9: the vehicle output interface -----------------------------------------------------
        *lpGameStateInput->GetVehicleOutputInterface() = *lpWorldOutput->GetVehicleOutputInterface();

        // ---- leg 10: the mode manager's route answers --------------------------------------------
        // The response count is read once; each response record is copied whole, and only those
        // owned by the mode manager (+0x140C == 2) are forwarded as { the response's event id
        // (+0x140E), the route's distance or 0.0f with no nodes }. Same write-locked destination
        // queue as leg 2 (legs 3-9 never touch it), so the world's events come first.
        const BrnAI::RouteMapModuleIO::RouteResponseQueue* lpRouteResponseOutput = lpWorldOutput->GetRouteResponseQueue();
        CGS_ASSERT(lpRouteResponseOutput != 0, "lpRouteResponseOutput");

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
                                                      : 0.0f;
                lpGameStateInput->GetGameEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lRouteInfoEvent),
                    static_cast<s32>(BrnGameState::GameStateModuleIO::E_EVENT_MODE_MANAGER_ROUTE_INFO),
                    static_cast<s32>(sizeof(lRouteInfoEvent)));   // console size 8

                // [FLAG PC witness] BRN_ROUTE_INFO_DIAG: every forwarded answer (first 40).
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

        // [FLAG PC witness] BRN_PROP_DIAG: the `[UI-gate]` ladder's GameState-transport rung, same
        // guard and first-N latch as PropEntityModule_wQ_04.cpp's "[prop-diag] BREAK" line. The
        // count is read after the Append because Append does not drain its source.
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
