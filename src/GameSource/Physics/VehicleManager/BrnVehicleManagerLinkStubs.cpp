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
//   UpdateCrashes (DWARF h:1224)              -- ⭐ ADDRESS RESOLVED 2026-08-10: **0x825EA640,
//       203 insns**. (See the audit note below -- this line used to read "the ledger mis-keys it
//       to the CgsBitArray.h TU; address to be re-derived at its wave". The ledger key is still
//       wrong; the address never needed re-deriving.)
//   EndVehicleTractionLineTests (DWARF h:893) -- ⭐ NOT A HOLE, claim RETRACTED 2026-08-10:
//       **0x82633CD8, 68 insns**, with pseudocode. Body: assert mpContactGenerator != NULL
//       (BrnVehicleManager.cpp:2387); WaitOn(mpTractionLineTestsJob) then null it;
//       DataStreamCommandPoster::End(producer+0x80) and a zero byte at producer+0x100; then the
//       three harvests -- ReadRaceCarTractionLineTestResults @0x82618058 (231),
//       PhysicalTrafficManager::ReadTrafficTractionLineTestResults @0x8262D2B8 (291),
//       ReadPlayerStuckTractionLineTestResults @0x825C3898 (118), each handed an 8-byte value
//       loaded from producer+0x38 -- then DoVehicleTractionLineDecallocations @0x825B5268 (37).
//       ⚠️ ARITY FLAG for whoever bodies it: the emitted body reads r3 and r4 ONLY -- r5 is never
//       touched -- while this stub's declaration takes two parameters. Settle the signature
//       against the DWARF + the PS3 mangle before writing it (the dropped-argument trap).
//   CrashFatalRaceCars (DWARF h:1287)         -- STILL genuinely absent from the export set by
//       name; its callee, the 6-arg ForceRaceCarCrash @0x82635B00, lands with it.
//   ReadSurfaceProperties(u64) @0x825C7BB8 (187) -- AttribSys walk; two unidentified callees
//       (Attrib::FindCollectionWithDefault [external], sub_8227FB58).
//   VehicleDriver::UpdateVehicle              -- recovered-to BrnVehicleDriver.cpp but NOT bodied
//       there; the driver-controls dispatch is its own wave.
//   DebugComponent::Update                    -- recovered-to B5PhysicsHandlingDebugComponent.cpp
//       (unmounted TU, body not reconstructed).
//   PTM::UpdateTrafficPhysics @0x82644418     -- .ida-exports HOLE, RE-CONFIRMED 2026-08-10 (no
//       JSON at that address and nothing of that name in the set). ⚠️ Its sibling
//       UpdateTrafficPhysicsPostSimulation @0x826371D0 (270) IS exported -- do not mistake one
//       for the other when decoding the `bl` at the call site.
//   PTM::PassNearbyCrashingTrafficIdsToRaceCarModule -- ⭐ ADDRESS PINNED 2026-08-10:
//       **0x825EEB70, 256 insns** (was "address not yet pinned; recover by caller set").
//
// ⚠️⚠️ WHY THREE OF THE LINES ABOVE CHANGED -- AN AUDIT WORTH REPEATING ELSEWHERE.
// On 2026-08-10 a name->address index was built over ALL 30,084 X360 export JSONs and every
// "hole"/"unpinned"/"to be re-derived" claim in this banner was looked up again. Three of six
// resolved immediately. This is [[ida-export-set-has-holes]] run in the OTHER direction:
// "absent from the export set" is a claim about a SEARCH, and a search that is never repeated
// after the set grows stops being evidence -- while the note it produced keeps being quoted.
// The audit is discriminating, not permissive: three of the six are still genuinely absent
// (GetUpdatedVehicleBodies @0x82619340, ProcessCrashingNetworkCars @0x8263C7C0,
// PTM::UpdateTrafficPhysics @0x82644418 -- no JSON at any of those addresses either), so the
// index is not simply matching everything.
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
                *CgsDev::Log::gpDebugPrint << "conductor gate: VehicleManager::UpdateCrashes @0x825EA640 (203; address RESOLVED 2026-08-10) inert [FLAG PC boot gate]\n";
        }
    }

    // LINK STUB. ⭐ ARITY CORRECTED 2026-08-10 (ground wave): ONE parameter, not two -- see the
    // declaration in BrnVehicleManager.h for the register proof. The second (interface) argument
    // this stub used to take was fabricated, and the one call site passed it.
    //
    // ⛔ WHY THIS IS STILL A GATE WITH THE WHOLE BODY IN HAND. Its four harvest callees are real as
    // of this wave (BrnVehicleManager_TractionLineTests.cpp), but the body's SECOND act is
    // `DataStreamCommandPoster::End(mpTractionLineStreamProducer + 0x80)` with NO null guard, and
    // mpTractionLineStreamProducer is only ever non-null between DoVehicleTractionLineAllocations
    // and DoVehicleTractionLineDecallocations -- i.e. only if StartVehicleTractionLineTests ran.
    // That one is gated (its command builders dereference an absent TriangleCacheManager), and
    // UpdateVehiclePhysics reaches THIS function unconditionally every frame. Bodying it now is a
    // null+0x80 write per frame. The two halves are lifetime-coupled by the producer: they land
    // together or not at all.
    void VehicleManager::EndVehicleTractionLineTests(CgsModule::IOBufferStack*)
    {
        // BOOT GATE (conductor wave 2026-08-09): reached every frame by the landed
        // UpdateVehiclePhysics. Reconstruct and DELETE this gate.
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "conductor gate: VehicleManager::EndVehicleTractionLineTests @0x82633CD8 (68; NOT an export hole -- claim RETRACTED 2026-08-10; arity CORRECTED to 1 param 2026-08-10) inert -- blocked with StartVehicleTractionLineTests on the absent TriangleCacheManager [FLAG PC boot gate]\n";
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
                *CgsDev::Log::gpDebugPrint << "conductor gate: PhysicalTrafficManager::PassNearbyCrashingTrafficIdsToRaceCarModule @0x825EEB70 (256; address PINNED 2026-08-10) inert [FLAG PC boot gate]\n";
        }
    }
}
}
