// =================================================================================================
// BrnVehicleManagerLinkStubs.cpp -- FLAG (big-five #3 UpdateVehiclePhysics mount stubs, 2026-08-06).
//
// The named remainder of VehicleManager::UpdateVehiclePhysics' honest closure: the conductor's
// FULL body is landed (BrnVehicleManager_UpdateVehiclePhysics.cpp) and these are the callees whose
// bodies are NOT -- each one a LOUD CGS_ASSERT(false) trap, per the VehiclePhysicsLinkStubs.cpp
// precedent. Every stub below is DEAD today: the conductor's only caller is
// BrnPhysics::PhysicsModule::Update, which is not landed.
//
// ⭐ 2026-08-09 (conductor wave): PhysicsModule::Update IS LANDED, so this file's census split
// in two. The stubs REACHED UNCONDITIONALLY EVERY FRAME by the landed UpdateVehiclePhysics
// (UpdateVehicleImpacts, EndVehicleTractionLineTests, UpdateAggressiveDriving, UpdateCrashes,
// CrashFatalRaceCars [mbCrashRaceCarWhenFatal is Construct-seeded TRUE], PTM::UpdateTrafficPhysics,
// PTM::PassNearbyCrashingTrafficIdsToRaceCarModule) are converted from CGS_ASSERT(false) traps --
// which would block the sim on frame one -- to the sanctioned LOUD one-shot boot-gate shape:
// one log line per boot naming symbol/address/insns, then inert. The rest stay ASSERT TRAPS
// because their call sites are genuinely input/state-gated on a default run (SetRaceCarCrashing:
// takedown chain; ReadSurfaceProperties: player reset button behind a != -1 guard;
// VehicleDriver::UpdateVehicle + DebugComponent::Update: per-LIVE-car, and the create path is
// still inert so the live set is empty).
//
// ⛔ NEVER make a gate silent. The log-once IS the loudness. Reconstruct the real body in its
// own TU and DELETE the stub (duplicate-definition LNK2005 is the intended tripwire).
//
// Per-symbol status (insn counts from the X360 dossier):
//   SetRaceCarCrashing @0x82634C90 (923)      -- REAL BODY EXISTS in the unmounted
//       BrnVehicleManager.cpp (the takedown chain); mounting it drags the twelve M2-measured
//       externals, which is that chain's own wave. This stub carries the seam until then --
//       the mount of BrnVehicleManager.cpp DELETES this stub (LNK2005 says so loudly).
//   UpdateVehicleImpacts @0x82635C00 (322)    -- not in tree (callee set: HasRaceCarHadRecentImpact
//       [bodied, unmounted PlayerStats TU], VehiclePhysics::AddSlam/AddShunt [mounted],
//       sub_821F0EC8 [unidentified], VariableEventQueue::AddEvent).
//   UpdateAggressiveDriving @0x82640690 (264) -- not in tree (needs HasRaceCarHadRecentImpact +
//       InstantTakedown, both in unmounted TUs).
//   UpdateCrashes (DWARF h:1224)              -- not in tree; the ledger mis-keys it to the
//       CgsBitArray.h TU; address to be re-derived at its wave.
//   EndVehicleTractionLineTests (DWARF h:893) -- .ida-exports HOLE (absent-from-JSON, image-only).
//   CrashFatalRaceCars (DWARF h:1287)         -- .ida-exports HOLE (image-only); its callee, the
//       6-arg ForceRaceCarCrash @0x82635B00, lands with it.
//   ReadSurfaceProperties(u64) @0x825C7BB8 (187) -- AttribSys walk; two unidentified callees
//       (Attrib::FindCollectionWithDefault [external], sub_8227FB58).
//   VehicleDriver::UpdateVehicle              -- recovered-to BrnVehicleDriver.cpp but NOT bodied
//       there; the driver-controls dispatch is its own wave.
//   DebugComponent::Update                    -- recovered-to B5PhysicsHandlingDebugComponent.cpp
//       (unmounted TU, body not reconstructed).
//   PTM::UpdateTrafficPhysics @0x82644418     -- .ida-exports HOLE (image-only; the traffic
//       sibling of the whole conductor -- already flagged in VehiclePhysicsLinkStubs.cpp's banner).
//   PTM::PassNearbyCrashingTrafficIdsToRaceCarModule -- address not yet pinned (recover by
//       caller set at its wave).
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags (the boot gates)

namespace BrnPhysics
{
namespace Vehicle
{
    // LINK STUB -- real body in the unmounted BrnVehicleManager.cpp takedown chain (see banner).
    void VehicleManager::SetRaceCarCrashing(
        EntityId /*lVictimEntityId*/, EntityId /*lAggressorEntityId*/,
        Vector3 /*lCollisionNormal*/, Vector3 /*lContactPoint*/,
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* /*lpRequestOutputInterface*/,
        VehicleManagerOutputInterface* /*lpManagerOutputInterface*/,
        BrnGameState::GameStateModuleIO::VehicleOutputInterface* /*lpVehicleOutputInterface*/,
        BrnPhysics::Deformation::DeformationInputInterface* /*lpDeformationInterface*/,
        BrnGameState::ETakedownType /*leTakedownType*/)
    {
        CGS_ASSERT(false, "VehicleManager::SetRaceCarCrashing: link stub -- the real 923-insn body "
                          "lives in the unmounted BrnVehicleManager.cpp; mount that chain and "
                          "DELETE this stub (X360 @0x82634C90)");
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet.
    void VehicleManager::UpdateVehicleImpacts(
        const CgsModule::EventQueue<ImpactEvent, 16>*,
        VehicleOutputInterface*,
        BrnPhysics::Vehicle::VehicleOutputRequestInterface*,
        VehicleManagerOutputInterface*,
        BrnPhysics::Deformation::DeformationInputInterface*)
    {
        // BOOT GATE (conductor wave 2026-08-09): reached every frame by the landed
        // UpdateVehiclePhysics. Reconstruct and DELETE this gate.
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "conductor gate: VehicleManager::UpdateVehicleImpacts @0x82635C00 (322) inert [FLAG PC boot gate]\n";
        }
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet.
    void VehicleManager::UpdateAggressiveDriving(
        f32, BrnPhysics::Vehicle::VehicleOutputRequestInterface*,
        VehicleManagerOutputInterface*, VehicleOutputInterface*,
        BrnPhysics::Deformation::DeformationInputInterface*)
    {
        // BOOT GATE (conductor wave 2026-08-09): reached every frame by the landed
        // UpdateVehiclePhysics. Reconstruct and DELETE this gate.
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "conductor gate: VehicleManager::UpdateAggressiveDriving @0x82640690 (264) inert [FLAG PC boot gate]\n";
        }
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet.
    void VehicleManager::UpdateCrashes(f32)
    {
        // BOOT GATE (conductor wave 2026-08-09): reached every frame by the landed
        // UpdateVehiclePhysics. Reconstruct and DELETE this gate.
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "conductor gate: VehicleManager::UpdateCrashes (address to re-derive; ledger mis-key) inert [FLAG PC boot gate]\n";
        }
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet (.ida-exports hole).
    void VehicleManager::EndVehicleTractionLineTests(CgsModule::IOBufferStack*,
                                                     const VehicleInputInterface*)
    {
        // BOOT GATE (conductor wave 2026-08-09): reached every frame by the landed
        // UpdateVehiclePhysics. Reconstruct and DELETE this gate.
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "conductor gate: VehicleManager::EndVehicleTractionLineTests (export hole; the traction harvest) inert [FLAG PC boot gate]\n";
        }
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet (.ida-exports hole).
    void VehicleManager::CrashFatalRaceCars(
        BrnPhysics::Vehicle::VehicleOutputRequestInterface*,
        VehicleManagerOutputInterface*, VehicleOutputInterface*,
        BrnPhysics::Deformation::DeformationInputInterface*, CgsSceneManager::EntityId)
    {
        // BOOT GATE (conductor wave 2026-08-09): reached every frame by the landed
        // UpdateVehiclePhysics. Reconstruct and DELETE this gate.
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "conductor gate: VehicleManager::CrashFatalRaceCars (export hole; mbCrashRaceCarWhenFatal seeds TRUE) inert [FLAG PC boot gate]\n";
        }
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet.
    void VehicleManager::ReadSurfaceProperties(u64)
    {
        CGS_ASSERT(false, "VehicleManager::ReadSurfaceProperties(Attribute::Key): link stub -- "
                          "reconstruct from X360 @0x825C7BB8 (187 insns; AttribSys walk)");
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet. The per-car driver
    // dispatch (BrnVehicleDriver.h declares it; BrnVehicleDriver.cpp does not body it).
    void VehicleDriver::UpdateVehicle(VehiclePhysics*)
    {
        CGS_ASSERT(false, "VehicleDriver::UpdateVehicle: link stub -- the driver-type dispatch "
                          "into the four Update(controls) overloads; its own wave");
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet.
    void DebugComponent::Update(f32)
    {
        CGS_ASSERT(false, "BrnPhysics::Vehicle::DebugComponent::Update: link stub -- per-car debug "
                          "tick; reconstruct with the B5PhysicsHandlingDebugComponent pass");
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet (.ida-exports hole;
    // X360 @0x82644418 -- the traffic-side conductor).
    void PhysicalTrafficManager::UpdateTrafficPhysics(f32, f32, const Matrix44Affine*, bool, bool)
    {
        // BOOT GATE (conductor wave 2026-08-09): reached every frame by the landed
        // UpdateVehiclePhysics. Reconstruct and DELETE this gate.
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "conductor gate: PhysicalTrafficManager::UpdateTrafficPhysics @0x82644418 (export hole) inert [FLAG PC boot gate]\n";
        }
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet.
    void PhysicalTrafficManager::PassNearbyCrashingTrafficIdsToRaceCarModule(
        VehicleManagerOutputInterface*, Vector3)
    {
        // BOOT GATE (conductor wave 2026-08-09): reached every frame by the landed
        // UpdateVehiclePhysics. Reconstruct and DELETE this gate.
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "conductor gate: PhysicalTrafficManager::PassNearbyCrashingTrafficIdsToRaceCarModule (address unpinned) inert [FLAG PC boot gate]\n";
        }
    }
}
}
