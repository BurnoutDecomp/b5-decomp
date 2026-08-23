// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManager_ValidateSimulationContacts.cpp
//
// BrnPhysics::Vehicle::VehicleManager::ValidateSimulationContacts  @ X360 0x825C8990
// (DWARF decl BrnVehicleManager.h:667; home TU BrnVehicleManager.cpp -- its own baked assert
// path/lines are BrnVehicleManager.cpp:9877/9899/9908. That TU is still unmounted, so this is
// a slice TU in the established BrnVehicleManager_Construct.cpp / *CrashPrediction.cpp /
// *PlayerStats.cpp pattern. Fold it back when the home TU mounts.)
//
// Reconstructed branch-for-branch from the X360 asm (IDA export 0x825C8990.json; bl target of
// PhysicsModule::ValidateSimulationContacts @0x825A1418). Debug-only validation of the
// outgoing simulation contact queue; called once per frame by the physics module's own
// ValidateSimulationContacts pass. Per queued InAddPotentialContact (80-byte GetEvent copy,
// ctr=10 block move):
//   * If mIDA's high-dword owner byte is E_ENTITYTYPE_TRAFFIC_VEHICLE (2):
//       - traffic index = (idHigh >> 10) & 0x3FFF (`extrwi r29, r28, 14,8` == the packed
//         EntityId::GetEntityIndex field, the same extraction BrnPhysicalTrafficManager.cpp
//         uses throughout);
//       - inlined BitArray bounds tripwire: index < 20 ("invalid index : %d < 20",
//         CgsBitArray.h:203) -- fire-and-continue;
//       - inlined BitArray<20>::IsBitSet against mPhysicalTrafficManager.mUsedTrafficVehicles
//         (the asm's `ldx` at this + 0x24748 == 44768 + 104552, the member's own pinned seat;
//         the friend declaration in BrnPhysicalTrafficManager.h is what lets the read stay
//         BY NAME) -- if the slot is not live, assert
//         "Simulation contact with traffic vehicle A that isn't alive, vehicle index %u,
//          physics ID 0x%X" (BrnVehicleManager.cpp:9899).
//   * Same for mIDB (":9908", "...vehicle B that isn't alive...").
//
// The console's message construction goes through the CgsDev::Assert::gpcMessageBuffer /
// StrStream machinery; lowered to the project CGS_ASSERT with the static message per the
// standing rule (see e.g. CgsScriptedFsm.cpp / CgsTimeManager.cpp banners).
//
// ONE DELIBERATE HOST DIVERGENCE, flagged: when the bounds tripwire fires (index >= 20)
// the console goes on to compute the bit test anyway, reading (index>>6)*8 bytes past the
// 1-word bit array -- an out-of-bounds read whose result only feeds the second (already
// garbage) diagnostic. The host skips the IsBitSet call for out-of-range indices instead of
// reproducing the OOB read; both asserts are non-gating diagnostics, so no live behaviour
// changes.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"  // PhysicsSimulationIO::InAddPotentialContact (+ the InAddContactQueue instantiation)
#include "GameSource/World/BrnEntityTypes.h"                              // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleManager::ValidateSimulationContacts(
        const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddPotentialContact, 1024>* lpContactQueue )
    {
        CGS_ASSERT(lpContactQueue != nullptr, "lpContactQueue != NULL");   // :9877

        for (s32 liIndex = 0; liIndex < lpContactQueue->GetLength(); ++liIndex)
        {
            // The X360 copies the whole 80-byte event to the stack (ctr = 10 ld/std) before
            // reading the two id words; a by-value copy reproduces that.
            const CgsPhysics::PhysicsSimulationIO::InAddPotentialContact lContact =
                lpContactQueue->GetEvent(liIndex);

            const u32 luIdAHigh = static_cast<u32>(lContact.mIDA >> 32);
            const u32 luIdBHigh = static_cast<u32>(lContact.mIDB >> 32);

            if ((luIdAHigh >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE))
            {
                // Packed 14-bit entity index (EntityId::GetEntityIndex -- extrwi 14,8).
                const u32 luTrafficIndexA = (luIdAHigh >> 10) & 0x3FFFu;

                // Inlined BitArray<20> bounds tripwire (CgsBitArray.h:203; streamed
                // "invalid index : %d < 20" lowered to the static message).
                CGS_ASSERT(luTrafficIndexA < 20u, "invalid index : ");

                if (luTrafficIndexA < 20u)   // host bounds guard -- see the banner divergence note
                {
                    CGS_ASSERT(mPhysicalTrafficManager.mUsedTrafficVehicles.IsBitSet(luTrafficIndexA),
                               "Simulation contact with traffic vehicle A that isn't alive, vehicle index ");   // :9899
                }
            }

            if ((luIdBHigh >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE))
            {
                const u32 luTrafficIndexB = (luIdBHigh >> 10) & 0x3FFFu;

                CGS_ASSERT(luTrafficIndexB < 20u, "invalid index : ");

                if (luTrafficIndexB < 20u)
                {
                    CGS_ASSERT(mPhysicalTrafficManager.mUsedTrafficVehicles.IsBitSet(luTrafficIndexB),
                               "Simulation contact with traffic vehicle B that isn't alive, vehicle index ");   // :9908
                }
            }
        }
    }
}
}
