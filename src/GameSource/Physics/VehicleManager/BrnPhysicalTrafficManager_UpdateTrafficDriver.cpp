// =================================================================================================
// GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_UpdateTrafficDriver.cpp
//
// ⭐ PhysicalTrafficManager::UpdateTrafficDriver @0x825CA8A0 (169 insns) -- the TRAFFIC arm of
// VehicleManager::UpdateDrivers' four-way driver-control dispatch (case 3 of the jumptable,
// reached with `add r3, r31, r28` where r28 == 0xAEE0 == the embedded mPhysicalTrafficManager, so
// the `this` really is this class). DWARF BrnPhysicalTrafficManager.h:158.
//
// Slice TU: the home BrnPhysicalTrafficManager.cpp is mounted and owns twelve other bodies; this is
// kept separate so the driver-arms wave lands as one reviewable unit alongside
// BrnVehicleManager_DriverArms.cpp (the other four arms). Fold back at will.
//
// The gate for this function in BrnPhysicsConductorGates.cpp is DELETED by this wave.
//
// WHAT IT DOES:
//   1. assert the record names a STANDARD traffic vehicle (species 0). Static street furniture and
//      the trailer slot have no driver;
//   2. build the vehicle's GLOBAL entity id -- owner E_ENTITYTYPE_TRAFFIC_VEHICLE(2), entity index
//      == the record's miVehicleID;
//   3. map global -> LOCAL PHYSICS index through mu8GlobalToPhysicalEntityIndexMap. Traffic is
//      streamed, so most global vehicles have NO physics body: the KU8_INVALID_MAP(127) slot is the
//      normal "this car is not physical right now" answer and the record is dropped SILENTLY;
//   4. install the record into mpaTrafficDrivers[physicsIndex].
//
// ⚠️ IT NEVER TOUCHES THE BitArray<8>. The parameter is declared (DWARF) and passed by the caller,
// and the console's r5 is dead from the prologue to the epilogue -- confirmed by reading the asm,
// not by trusting Hex-Rays (which drops the parameter from its 2-arg prototype entirely). That is
// correct and not an omission: the bitset tracks RACE CARS, and a traffic index is not one.
//
// ⚠️ COST NOTE: only ONE absent callee across the whole body -- BrnTraffic::GetVehicleSpecies
// @0x821F4648 (32 insns, a pure range ladder, xrefs_from == the assert triple). It is LANDED this
// wave as a header-inline in its own console home
// (GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h), not parked. The other
// two non-assert callees, memcpy and VolumeInstanceId::SetEntityIDEntityIndex @0x822B0E70, are
// already real in this tree.
//
// ⚠️ CONSOLE OFFSETS NOT REPRODUCED (this class's standing rule): the map at `this+104688`, the
// driver array behind `this+103600` and the 224-byte driver stride are ALL reached by name. The
// host pointer width alone moves two of the three.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                  // VolumeInstanceId + its two setters
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"     // BrnTraffic::GetVehicleSpecies

namespace BrnPhysics
{
namespace Vehicle
{
    // ==============================================================================================
    // @0x825CA8A0  PhysicalTrafficManager::UpdateTrafficDriver  (169 insns)
    // ==============================================================================================
    void PhysicalTrafficManager::UpdateTrafficDriver(const BrnTrafficDriverControls* lpControls,
                                                     CgsContainers::BitArray<8u>& lrUpdatedCars)
    {
        // The console never reads r5. Named in the signature (DWARF) and deliberately unused, so
        // the divergence is explicit rather than a warning.
        (void)lrUpdatedCars;

        const s32 liVehicleID = lpControls->miVehicleID;

        // ---- 0x825CA8B4..0x825CA944: BrnPhysicalTrafficManager.cpp:1782 (0x6F6). Streamed with the
        //      index ("Driver controls specified for non-standard traffic vehicle " << id).
        CGS_ASSERT(BrnTraffic::GetVehicleSpecies(static_cast<u32>(liVehicleID)) ==
                       BrnTraffic::Vehicle::E_SPECIES_STANDARD,
                   "Driver controls specified for non-standard traffic vehicle ");

        // ---- 0x825CA948..0x825CA95C: the global id. The console materialises the whole 64-bit
        //      word as one `std` of (2 << 56) -- i.e. a zero-initialised VolumeInstanceId whose
        //      owner splice the compiler constant-folded -- then splices the index in through the
        //      out-of-line setter. Reproduced as the two field writes it folded, so no raw packing
        //      constant appears here.
        CgsSceneManager::VolumeInstanceId lGlobalEntityId;
        lGlobalEntityId.muId = 0;
        lGlobalEntityId.SetEntityIDOwner(static_cast<u8>(KU_ENTITYTYPE_TRAFFIC_VEHICLE));
        lGlobalEntityId.SetEntityIDEntityIndex(static_cast<u32>(liVehicleID));

        const u32 luGlobalEntityIndex = lGlobalEntityId.GetEntityIDEntityIndex();

        // ---- 0x825CA96C..0x825CAA08: the "is this vehicle physical at all?" probe. The console
        //      inlines the SAME map lookup TWICE -- once as the bool gate here, once below to
        //      produce the id -- with the BrnPhysicalTrafficManager.h:944 (0x3B0) bound assert
        //      baked into each. Both copies are kept, in order.
        CGS_ASSERT(luGlobalEntityIndex < sizeof(mu8GlobalToPhysicalEntityIndexMap),
                   "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");

        if (mu8GlobalToPhysicalEntityIndexMap[luGlobalEntityIndex] == KU8_INVALID_MAP)
        {
            // The ordinary streamed-out case -- no assert, no log, nothing. A traffic car that is
            // not currently a physics body simply has no driver record to update.
            return;
        }

        // ---- 0x825CAA0C..0x825CAAF8: the same probe again, this time building the physics id.
        //      The `!lbFound` arm cannot be taken (the gate above already proved the slot occupied),
        //      but the console emits it, so the tripwire it carries is emitted too.
        CGS_ASSERT(luGlobalEntityIndex < sizeof(mu8GlobalToPhysicalEntityIndexMap),
                   "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");

        const u8 lu8PhysicalIndex = mu8GlobalToPhysicalEntityIndexMap[luGlobalEntityIndex];
        const bool lbFound = (lu8PhysicalIndex != KU8_INVALID_MAP);

        if (lbFound)
        {
            // CgsEntityId.h:116 (0x74) -- the EntityId ctor bound check; dead for a u8, emitted.
            CGS_ASSERT(static_cast<u32>(lu8PhysicalIndex) < (1u << KU_NUM_BITS_FOR_ENTITY_NUM),
                       "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
        }

        // BrnPhysicalTrafficManager.h:1030 (0x406) -- streamed with the global id.
        CGS_ASSERT(lbFound,
                   "Failed to find local physics ID of traffic vehicle with global entity ID: ");

        EntityId lPhysicsEntityId;
        lPhysicsEntityId.muValue = (static_cast<u32>(lu8PhysicalIndex) << 10)
                                 | (KU_ENTITYTYPE_TRAFFIC_VEHICLE << 24);

        // ---- 0x825CAAFC..0x825CAB38: install. `extrwi r11, r26, 14, 8` is GetEntityIndex(); the
        //      `mulli 0xE0` + the deref of the array pointer is GetTrafficDriver's own arithmetic.
        //      Same four stores as the network arm in BrnVehicleManager_DriverArms.cpp: the driver
        //      type, the 0x48-byte control BASE copy, then the two AI-tail fields the console clears
        //      because a traffic record never carries them (BrnTrafficDriverControls adds NO members
        //      -- sizeof == sizeof(BrnPlayerDriverControls) == 0x48, which is why the copy is 0x48).
        const u32 luPhysicalIndex = (lPhysicsEntityId.muValue >> 10) & 0x3FFFu;
        VehicleDriver& lrDriver = mpaTrafficDrivers[luPhysicalIndex];

        lrDriver.meDriverType = E_DRIVER_TYPE_TRAFFIC;                             // stw 3, +0xD0
        static_cast<BrnPlayerDriverControls&>(lrDriver.mControls) = *lpControls;    // memcpy 0x48
        lrDriver.mControls.mbForceComeOutOfDrift = false;                          // stb 0, +0x4D
        lrDriver.mControls.mfSpeedMatchSpeed     = 0.0f;                           // stfs 0.0f, +0x48
    }
}
}
