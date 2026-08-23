// ============================================================================
// b5-decomp/src/GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.cpp
//
// WorldModule physics -> entity-module post-physics bridges (X360 TU
// GameSource/Unity/../World/Bridges/WorldBridgePhysicsToEntityModules.cpp).
//
// Bodied here: BridgePhysicsModuleToRaceCarModule_PostPhysics @0x827AE9D0 and
// BridgePhysicsModuleToTrafficModule_PostPhysics @0x827AB910 (banners at each body).
//
// GATE BridgePhysicsModuleToAIModule_PostPhysics @0x827A5680 -- not reconstructed; its boot
// gate lives in WorldLinkStubs.cpp. DELETE-WHEN the body lands. Recovered flow:
//   assert "lpAIModuleInputBuffer_PostPhysics != NULL" (:142) / "lpPhysicsModuleOutputBuffer
//   != NULL" (:143); read-lock check on the physics output (bit 0x10,
//   Physics/BrnPhysicsModuleIO.h:369); *(aiInput + 4) = *(physicsOutput + 0xF3CB0) -- the
//   post-physics AI sub-interface's leading word (the `bl sub_8279F8E0` getter is that
//   read-lock plus the read; the PS3 DecFIGS body @0xA2B84C inlines it).
//
// The prop @0x827AB998 and crash @0x827AB8B0 siblings are declaration-only: their bodies walk
// physics-output sub-interfaces whose accessor band is not homed yet.
// ============================================================================

#include "GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// =================================================================================================
// WorldModule::BridgePhysicsModuleToRaceCarModule_PostPhysics @0x827AE9D0 (39 insns)
//
// The physics->race-car handover: VehicleManager::WriteOutVehicleStats fills the PHYSICS module's
// output buffer, RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics reads the RACE-CAR
// module's post-physics INPUT buffer, and this bridge is the only thing in the XEX that copies one
// into the other (the readback's mUsedRaceCars gate cannot pass without it).
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
// LIVE: legs 1 and 2 -- the car state and the vehicle-manager event queues, i.e. everything the
// pose readback consumes.
// GATE legs 3/4/5 (deformation-for-entity-modules, deformation, scene-update Append). BLOCKER:
//   PhysicsModuleIO::OutputBuffer models those three seats as 1-byte opaque storage
//   (BrnPhysicsModuleIO.h "FLAG (foreign types)") while the RaceCarEntityModuleIO setters take the
//   real BrnPhysics::Deformation::* / OutputBuffer_PreScene::SceneInputInterface types -- bridging
//   today is a 1-byte-onto-multi-KB copy. DELETE-WHEN those seats are promoted to their real types.
// GATE leg 6 (contact spy). BLOCKER: only the NON-const physics accessor exists in this tree and
//   this bridge holds a `const OutputBuffer*`; the console's read-locked const twin is @0x8279F8E0.
//   DELETE-WHEN that const twin is declared and bodied.
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

// =================================================================================================
// WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics @0x827AB910 (33 insns)
//
// The traffic read-back handover: PhysicalTrafficManager::WriteOutVehicleStats fills the PHYSICS
// module's output buffer (mTrafficStateQueue + the vehicle-manager crash/slam queues);
// TrafficEntityModule::HandleExternalResponses reads the TRAFFIC module's post-physics INPUT
// buffer. This bridge is the only thing in the XEX that copies one into the other.
//
// Four accessor->setter pairs, in this asm order (0x827AB92C..0x827AB978), each callee's member
// offset read off its own epilogue:
//   1  OutputBuffer::GetVehicleOutputInterface() const                  @0x8279F598 -> +44128
//        -> InputBuffer_PostPhysics::SetVehicleOutputInterface          @0x827A9F50
//   2  OutputBuffer::GetVehicleManagerOutputInterface() const           @0x8279F4F0 -> +41952
//        -> InputBuffer_PostPhysics::SetVehicleManagerOutputInterface   @0x827AA000
//   3  OutputBuffer::GetDeformationOutputInterfaceForEntityModules() const @0x8279F790 -> +159648
//        -> InputBuffer_PostPhysics::SetDeformationOutputInterfaceForEntityModules @0x827AA0B8
//   4  OutputBuffer::GetContactSpyInterface() const                     @0x8279F8E0 -> +998192
//        -> InputBuffer_PostPhysics::SetContactSpyInterface             @0x827A0778
// r3 (the WorldModule `this`) is never read.
// =================================================================================================

namespace WorldModule
{
    void BridgePhysicsModuleToTrafficModule_PostPhysics(
        void* /*lpWorldModule*/,
        BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInputBuffer_PostPhysics,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer)
    {
        // ⚠️ DIVERGENCE (named): the console has no null guards -- EntityModulePostPhysicsUpdate
        // always passes live buffers. Same bring-up guard the race-car sibling above carries.
        CGS_ASSERT(lpTrafficInputBuffer_PostPhysics != 0, "lpTrafficInputBuffer_PostPhysics != NULL");
        CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL");
        if (lpTrafficInputBuffer_PostPhysics == 0 || lpPhysicsModuleOutputBuffer == 0)
        {
            return;
        }

        // PREREQ, satisfied: both setters run operator=, which Clear()+Append()s every embedded
        // EventQueue and memcpys through mpEvents -- seated only by EventQueue<T,N>::Construct.
        // BrnTrafficEntityModuleIO_InputBuffer_Getters.cpp InputBuffer_PostPhysics::Construct now
        // Constructs mVehicleOutputInterface and mVehicleManagerOutputInterface.

        // Leg 1 -- mTrafficStateQueue (the per-frame physical-traffic pose snapshots).
        lpTrafficInputBuffer_PostPhysics->SetVehicleOutputInterface(
            lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface());

        // Leg 2 -- the vehicle-manager event queues (crashed / slammed traffic, race-car crash).
        lpTrafficInputBuffer_PostPhysics->SetVehicleManagerOutputInterface(
            lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface());

        // Leg 4 -- the contact-spy handle (read-locked const twin @0x8279F8E0).
        lpTrafficInputBuffer_PostPhysics->SetContactSpyInterface(
            lpPhysicsModuleOutputBuffer->GetContactSpyInterface());

        // GATE leg 3 GetDeformationOutputInterfaceForEntityModules @0x8279F790 -> Set... @0x827AA0B8.
        // BLOCKER: PhysicsModuleIO::OutputBuffer models that seat as 1-byte opaque storage
        // (BrnPhysicsModuleIO.h:114) while the traffic setter takes the real
        // BrnPhysics::Deformation::DeformationOutputInterfaceForEntityModules; bridging today is a
        // 1-byte-onto-multi-KB copy. Same blocker as the race-car sibling's leg 3.
        // DELETE-WHEN that seat is promoted to its real type.
        {
            static bool sbLoggedDeformationPark = false;
            if (!sbLoggedDeformationPark && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedDeformationPark = true;
                *CgsDev::Log::gpDebugPrint
                    << "[FLAG PC bring-up] BridgePhysicsModuleToTrafficModule_PostPhysics: legs 1/2/4 LIVE; "
                       "leg 3 (deformation-for-entity-modules @0x8279F790) parked -- "
                       "PhysicsModuleIO::OutputBuffer still models that seat as 1-byte opaque "
                       "storage [FLAG PC partial gate]\n";
            }
        }
    }
}
