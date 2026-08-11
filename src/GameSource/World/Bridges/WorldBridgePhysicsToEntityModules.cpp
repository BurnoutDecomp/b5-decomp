// ============================================================================
// b5-decomp/src/GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.cpp
//
// WorldModule physics -> entity-module post-physics bridges (X360 TU
// GameSource/Unity/../World/Bridges/WorldBridgePhysicsToEntityModules.cpp).
//
// RETIRED BODY NOTE (world-drive wave 2026-07-27)
// -----------------------------------------------
// This TU previously carried a single reconstruction of
// WorldModule::BridgePhysicsModuleToAIModule_PostPhysics (X360 0x827A5680) written
// against by-name STAND-IN buffer structs, because BrnAI::AIModuleIO::
// InputBuffer_PostPhysics / BrnPhysics::PhysicsModuleIO::OutputBuffer were not
// committed when it landed. The real IO homes exist now and the world-drive spine
// (WorldModule::Update @0x827D63E8 -> EntityModulePostPhysicsUpdate @0x827D3F10)
// calls all five of this TU's bridges with the REAL buffer types, so the header was
// retyped and the stand-in structs retired -- keeping the old body would have
// bound the drive call site to fabricated types.
//
// The X360 data flow it recorded is preserved verbatim for the reconstruction that
// replaces the boot gate (WorldLinkStubs.cpp):
//   BridgePhysicsModuleToAIModule_PostPhysics @0x827A5680
//     if (!aiInput)       assert "lpAIModuleInputBuffer_PostPhysics != NULL"  (:142)
//     if (!physicsOutput) assert "lpPhysicsModuleOutputBuffer != NULL"        (:143)
//     read-lock check on the physics output (bit 0x10, "Not locked for reading",
//       Physics/BrnPhysicsModuleIO.h:369)
//     *(aiInput + 4) = *(physicsOutput + 998192)   ; 0xF3CB0, the post-physics
//                                                  ; AI sub-interface's leading word
//   (the X360 `bl sub_8279F8E0` getter is the read-lock + that direct read; the PS3
//    DecFIGS body @0xA2B84C inlines it.)
//
// The four sibling bridges (@0x827AE9D0 race car, @0x827AB910 traffic,
// @0x827AB998 prop, @0x827AB8B0 crash) are declaration-only here for the same
// reason: their bodies walk physics-output sub-interfaces whose accessor band is
// not homed yet. This TU is therefore intentionally body-free; it stays in the tree
// as the declared home so the reconstruction lands here (and not in the link-stub
// TU) when the physics IO pass runs.
// ============================================================================

#include "GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// =================================================================================================
// ⭐⭐ WorldModule::BridgePhysicsModuleToRaceCarModule_PostPhysics @0x827AE9D0 (39 insns)
//     -- landed 2026-08-11 (physics->output publish wave). Its boot gate in WorldLinkStubs.cpp is
//     DELETED in the same commit.
//
// THIS IS THE HANDOVER. VehicleManager::WriteOutVehicleStats fills the PHYSICS module's output
// buffer; RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics reads the RACE-CAR module's
// post-physics INPUT buffer. Nothing joined them: this bridge is the only thing in the XEX that
// copies one into the other, and while it was inert the readback's mUsedRaceCars gate could never
// pass no matter what the physics side published.
//
// The console body is six accessor->setter pairs, in this exact order (asm at 0x827AE9E4..0x827AEA60,
// each callee's return offset read off its own epilogue):
//   1  OutputBuffer::GetVehicleOutputInterface() const        @0x8279F598 -> +44128
//        -> InputBuffer_PostPhysics::SetVehicleOutputInterface                @0x827ACBC8
//   2  OutputBuffer::GetVehicleManagerOutputInterface() const @0x8279F4F0 -> +41952
//        -> InputBuffer_PostPhysics::SetVehicleManagerOutputInterface         @0x827ACC78
//   3  OutputBuffer::GetDeformationOutputInterfaceForEntityModules() const @0x8279F790 -> +159648
//        -> SetDeformationOutputInterfaceForEntityModules                    @0x827A99B0
//   4  OutputBuffer::GetDeformationOutputInterface() const    @0x8279F6E8 -> +148656
//        -> SetDeformationOutputInterface                                     @0x827A9A68
//   5  OutputBuffer::GetSceneInputInterface() const           @0x8279F838 -> +179424
//        -> InSceneUpdateInterface::Append(InputBuffer_PostPhysics::GetSceneInputInterface(), ...)
//           (@0x8279E310 -> input+0x74A0, then @0x827A9340)
//   6  OutputBuffer::GetContactSpyInterface() const           @0x8279F8E0 -> +998192
//        -> InputBuffer_PostPhysics::SetContactSpyInterface                   @0x8279E3B8
// (@0x8279F4F0 was an .ida-exports hole; pulled headless -- 42 insns, `addis r3,r28,1 ;
//  addi r3,r3,-0x5C20` == +41952, assert cite BrnPhysicsModuleIO.h:351. It is the const twin of
//  the already-declared GetVehicleManagerOutputInterface, so no new declaration was needed.)
//
// ⭐ LANDED HERE: legs 1 and 2 -- the two that carry the car state and the vehicle-manager event
//    queues, i.e. everything the pose readback consumes.
// ⛔ PARKED, LOUDLY (one log-once for the set, listing each blocker):
//    legs 3, 4 and 5 -- BrnPhysics::PhysicsModuleIO::OutputBuffer models
//    mDeformationOutputInterface, mDeformationOutputInterfaceForEntityModules and
//    mSceneInputInterface as ONE-BYTE OPAQUE STORAGE structs (BrnPhysicsModuleIO.h's own
//    "FLAG (foreign types)" block), while the RaceCarEntityModuleIO setters take the REAL
//    BrnPhysics::Deformation::* / OutputBuffer_PreScene::SceneInputInterface types. Bridging them
//    today would mean reinterpret_cast'ing a 1-byte placeholder onto a multi-kilobyte interface and
//    copy-assigning through it -- a guaranteed live corruption, and precisely the bug class this
//    project keeps paying for. DELETE-WHEN those three seats are promoted to their real types (the
//    same promotion the vehicle trio already had, BrnPhysicsModuleIO.h 2026-08-09).
//    leg 6 -- the physics-side accessor exists only in its NON-const form in this tree
//    (`ContactSpy::ContactSpyInterface* GetContactSpyInterface()`), and this bridge holds a
//    `const OutputBuffer*`. The console's read-locked const twin is @0x8279F8E0; adding and bodying
//    it is a two-line follow-up, deliberately not bundled into this wave. Nothing in the pose path
//    reads the contact-spy interface.
// =================================================================================================

namespace WorldModule
{
    void BridgePhysicsModuleToRaceCarModule_PostPhysics(
        void* /*lpWorldModule*/,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpRaceCarInputBuffer_PostPhysics,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer)
    {
        // ⚠️ DIVERGENCE (named): the console has no null guards -- WorldModule::
        // EntityModulePostPhysicsUpdate always passes live buffers. Kept as asserts + early-out
        // because this runs every frame during bring-up.
        CGS_ASSERT(lpRaceCarInputBuffer_PostPhysics != 0,
                   "lpRaceCarInputBuffer_PostPhysics != NULL");
        CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL");
        if (lpRaceCarInputBuffer_PostPhysics == 0 || lpPhysicsModuleOutputBuffer == 0)
        {
            return;
        }

        // Leg 1 -- the per-race-car physics snapshots (mUsedRaceCars + maRaceCarStates[8]).
        lpRaceCarInputBuffer_PostPhysics->SetVehicleOutputInterface(
            lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface());

        // Leg 2 -- the vehicle-manager event queues (crash/reset/create-result/traffic).
        lpRaceCarInputBuffer_PostPhysics->SetVehicleManagerOutputInterface(
            lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface());

        // Legs 3-6: parked. See this file's banner for the per-leg blocker.
        {
            static bool sbLoggedBridgeParks = false;
            if (!sbLoggedBridgeParks)
            {
                sbLoggedBridgeParks = true;
                *CgsDev::Log::gpDebugPrint
                    << "[FLAG PC bring-up] BridgePhysicsModuleToRaceCarModule_PostPhysics: legs "
                       "1-2 (vehicle output + vehicle-manager output) are LIVE; legs 3-5 "
                       "(deformation-for-entity-modules @0x8279F790, deformation @0x8279F6E8, "
                       "scene-update Append @0x827A9340) are parked because "
                       "PhysicsModuleIO::OutputBuffer still models those three seats as 1-byte "
                       "opaque storage; leg 6 (contact spy @0x8279F8E0) is parked because only "
                       "the non-const physics accessor exists in this tree.\n";
            }
        }
    }
}
