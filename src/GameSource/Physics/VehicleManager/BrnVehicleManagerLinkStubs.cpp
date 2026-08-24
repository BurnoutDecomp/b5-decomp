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
//   VehicleDriver::UpdateVehicle @0x825D7290 (219) -- ⭐ BODIED 2026-08-11 in BrnVehicleDriver.cpp;
//       the stub AND its (wrong) "driver-controls dispatch" description are gone. See the
//       deletion note below.
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
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "GameSource/AttribSys/Generated/classes/physicssurface.h"
#include "GameSource/AttribSys/Generated/classes/gameplaysurface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags (the boot gates)

#include <cmath>

namespace BrnPhysics
{
namespace Vehicle
{
    // LINK STUB DELETED 2026-08-24 (physics mount wave B3b): BrnVehicleManager.cpp is mounted --
    // the real 923-insn SetRaceCarCrashing @0x82634C90 owns the symbol, exactly the flip this
    // stub's own text prescribed ("mount that chain and DELETE this stub").

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

    // ⭐⭐ 2026-08-11 (lifetime wave): the VehicleManager::EndVehicleTractionLineTests @0x82633CD8
    // LINK STUB THAT STOOD HERE IS DELETED. The real 68-instruction body is in
    // BrnVehicleManager_TractionLineTests.cpp, landed in the SAME commit as
    // StartVehicleTractionLineTests -- which is the whole point: the stub's own banner said "the
    // two halves are lifetime-coupled by the producer: they land together or not at all", and
    // they did. The three blockers that banner named are all retired:
    //   * the triangle-cache FILL half (StartUpdateTriangleCaches / EndUpdateTriangleCaches) --
    //     landed 2026-08-10 and running every frame;
    //   * SimpleVehiclePhysics::GetTractionLine @0x825D85C0 (174, export hole) -- bodied this wave
    //     in BrnSimpleVehiclePhysics.cpp from the image plus the PS3 export;
    //   * the null+0x80 write -- cannot happen now, because Start ALWAYS runs first in the same
    //     frame (PhysicsModule::Update calls Start; UpdateVehiclePhysics calls End) and always
    //     seats the producer.
    // ⚠️ The stub's OTHER claim -- "arity CORRECTED to 1 param" -- is RETRACTED; the caller sets
    // r5 and the PS3 DWARF types it. See BrnVehicleManager.h.

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

    // Breaker @0x825C7BB8; DecFIGS BrnVehicleManager.cpp:9410.
    // A STALE SURFACELIST.BIN makes this walk die in Attrib::Collection::GetData ("Cannot
    // get non-array data from a non-zero index", then an AV): the vault must come from the
    // CURRENT attribsys-vault converter (build_game_data.py --only "SURFACELIST.BIN"
    // --force). Data from the pre-built drop predates it.
    void VehicleManager::ReadSurfaceProperties(u64 luSurfaceListKey)
    {
        Attrib::Gen::surfacelist lSurfaceList;
        lSurfaceList.ChangeWithDefault(luSurfaceListKey);

        // The console sanity-checks surface element 1's leading colour/vector. Its
        // sign bits are cleared, every lane is compared with FLT_EPSILON, and CR6.EQ
        // fires the assert only when NONE of the four lanes is greater than epsilon.
        void* lpSampleRefData = lSurfaceList.Surfaces(1);
        if (!lpSampleRefData)
            lpSampleRefData = Attrib::DefaultDataArea(sizeof(Attrib::RefSpec));

        Attrib::RefSpec* lpSampleRef = static_cast<Attrib::RefSpec*>(lpSampleRefData);
        Attrib::Gen::surface lSampleSurface(
            const_cast<Attrib::Collection*>(lpSampleRef->GetCollection()), nullptr);
        const f32* lpSampleData = static_cast<const f32*>(lSampleSurface.GetAttributeData());
        const f32 KF_EPSILON = 1.1920928955078125e-07f; // stru_8208F620.x
        CGS_ASSERT(std::fabs(lpSampleData[0]) > KF_EPSILON ||
                       std::fabs(lpSampleData[1]) > KF_EPSILON ||
                       std::fabs(lpSampleData[2]) > KF_EPSILON ||
                       std::fabs(lpSampleData[3]) > KF_EPSILON,
                   "Surface list appears to be corrupt");

        KI_NUM_USED_SURFACES = lSurfaceList.Num_Surfaces();
        for (s32 liSurface = 0; liSurface < KI_NUM_USED_SURFACES; ++liSurface)
        {
            void* lpSurfaceRefData = lSurfaceList.Surfaces(static_cast<u32>(liSurface));
            if (!lpSurfaceRefData)
                lpSurfaceRefData = Attrib::DefaultDataArea(sizeof(Attrib::RefSpec));

            Attrib::Gen::surface lSurface(
                *static_cast<Attrib::RefSpec*>(lpSurfaceRefData), nullptr);
            Attrib::Gen::gameplaysurface lGameplaySurface(lSurface.GameplaySurface(), nullptr);
            Attrib::Gen::physicssurface lPhysicsSurface(lSurface.PhysicsSurface(), nullptr);

            const f32 lfRoughness = lPhysicsSurface.Roughness();
            const f32 lfLinearDrag = lPhysicsSurface.LinearDrag();
            const f32 lfGrip = lPhysicsSurface.Grip();
            KAVF_SURFACE_ROUGHNESS[liSurface] =
                VecFloat{lfRoughness, lfRoughness, lfRoughness, lfRoughness};
            KAVF_SURFACE_GRIP[liSurface] = VecFloat{lfGrip, lfGrip, lfGrip, lfGrip};
            KAVF_SURFACE_LINEAR_DRAG[liSurface] =
                VecFloat{lfLinearDrag, lfLinearDrag, lfLinearDrag, lfLinearDrag};
            KAB_SURFACE_IS_WATER[liSurface] = lGameplaySurface.IsWater();
        }

        gbReadSurfaceProperties = true;
    }

    // ⭐⭐ 2026-08-11 (driving-path wave): the VehicleDriver::UpdateVehicle @0x825D7290 LINK STUB
    // THAT STOOD HERE IS DELETED -- the real 219-instruction body is in BrnVehicleDriver.cpp.
    // ⚠️ AND ITS COMMENT WAS WRONG, which is worth keeping on the record. It said "the driver-type
    // dispatch into the four Update(controls) overloads". The asm contains no meDriverType read, no
    // switch, and no call to any Update overload: the function is the network catch-up SLERP
    // APPLIER -- gated on mi8NumOfInterpSteps (+0xD4) > 0, it concatenates mSlerpTransform (+0x90)
    // into the vehicle's own mTransform (unless the vehicle is frozen), runs the two
    // "Slerped race car transform is not normalised/orthogonal" dev tripwires
    // (BrnVehicleDriver.cpp:219/:220) and decrements the counter. Construct seeds the counter to 0,
    // so it is a NO-OP on every non-networked frame -- the stub's own claim that it was
    // "per-LIVE-car" gated was right for the wrong reason.
    // LESSON: a stub's prose is a HYPOTHESIS, not a finding. This one had been quoted forward
    // through three banners without anyone reading the 219 instructions it was describing.

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet.
    // ⚠️ DEGRADED trap -> log-once gate (conductor, 2026-08-11, create-drain wave): the hard
    // CGS_ASSERT(false) was written when no car existed and the stub was unreachable. The create
    // drain went live this wave, so this is now reached PER LIVE CAR PER FRAME -- the hard trap
    // halted the boot once a second (measured). Loudness preserved via the standard one-shot log.
    // Reconstruct with the B5PhysicsHandlingDebugComponent pass and DELETE this gate.
    void DebugComponent::Update(f32)
    {
        static bool sbLogged = false;
        if (!sbLogged)
        {
            sbLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: BrnPhysics::Vehicle::DebugComponent::Update -- per-car "
                       "debug tick inert (reconstruct with the B5PhysicsHandlingDebugComponent "
                       "pass) [FLAG PC boot gate]\n";
        }
    }

    // GATE RETIRED 2026-08-22 (traffic wave T3): PhysicalTrafficManager::UpdateTrafficPhysics
    // @0x82644418 is REAL in BrnPhysicalTrafficManager_UpdateTrafficPhysics.cpp (export hole closed
    // with headless idat).

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
