// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManager_PerFrameLeaves.cpp
//
// The four SMALL per-frame leaves of BrnPhysics::PhysicsModule::Update @0x825B0640 (home TU
// BrnVehicleManager.cpp -- still unmounted, so this is a slice TU in the established
// BrnVehicleManager_ValidateSimulationContacts.cpp pattern; fold back when the home mounts):
//
//   FreeAllocations           @0x8261BAE0  (21 insns)   DWARF h:647
//   UpdateVehicleEffects      @0x82629E18  (65 insns)   DWARF h:908
//   ReadUpdatedBodyProperties @0x825C5520  (79 insns)   DWARF h:917
//   ProcessDeformationStates  @0x825EA580  (48 insns)   DWARF h:926
//
// Each body is reconstructed from its X360 asm (IDA exports read line by line 2026-08-06 --
// NOT from the Hex-Rays, which mis-renders ReadUpdatedBodyProperties' 64-bit id compare as a
// 32-bit one). Every member access is by name; the seats are pinned by the mounted layout gate
// (_AssertLayoutTuningBank) and by the per-member notes in BrnVehicleManager.h.
//
// The streamed asserts (off_82000D00/D08 + gpcMessageBuffer + BasePriorityQueue::Clear) are
// lowered to CGS_ASSERT with the static message, per the standing project rule.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"               // IOBufferStack::DestroyIOBuffer<T>
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h" // InChangeRigidBodyInertia (80B; mID/mInertia/mu32Flags)
#include "GameSource/Physics/BrnContactGenerationList.h"                  // BrnPhysics::ContactGenList (complete for ~T()/sizeof)
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h" // CgsCollision::CollisionGenerator (complete for ~T()/sizeof)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEffectsInputInterface.h" // the air-ram queue surface
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // mpDeformationState
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h" // DeformationState::GetCarStateF
#include "GameSource/World/BrnEntityTypes.h"                              // BrnWorld::E_ENTITYTYPE_RACECAR / _TRAFFIC_VEHICLE

namespace BrnPhysics
{
namespace Vehicle
{
    // ------------------------------------------------------------------------------------
    // FreeAllocations  @0x8261BAE0  -- end-of-frame teardown of the two contact-generation
    // IO allocations, in the console's order: the generator FIRST (this+172472), then the
    // gen list (this+172468). Each DestroyIOBuffer<T> pops the allocation off the stack and
    // NULLs the member through the passed T**.
    //
    // ⚠️ KNOWN TEMPLATE-SHAPE DIVERGENCE, inherited (not introduced here): the committed
    // CgsIOBufferStack.h DestroyIOBuffer<T> guards a null pointer and calls ~T(), where the
    // console instantiation @0x8259DE50 asserts on null ("Must pass in pointer to pointer to
    // buffer", CgsIOBufferStack.h:180), calls T's two-phase Destruct(), and returns Free's
    // result. That template already has many live instantiation sites (BrnGameModule /
    // CgsSceneManagerModule), so its shape is NOT changed in this wave -- flagged for its own
    // pass. This body is faithful in what IT does: the two calls, in order, on the two named
    // members. (The console DestroyIOBuffer<CollisionGenerator> frees 2171904 bytes ==
    // sizeof(CgsSceneManager::CgsCollision::CollisionGenerator) on the console -- the derived
    // 2 MB generator added to CgsCollisionGenerator.h this wave.)
    // ------------------------------------------------------------------------------------
    void VehicleManager::FreeAllocations(CgsModule::IOBufferStack* lpIOBufferStack)
    {
        lpIOBufferStack->DestroyIOBuffer(&mpContactGenerator);
        lpIOBufferStack->DestroyIOBuffer(&mpContactGenList);
    }

    // ------------------------------------------------------------------------------------
    // UpdateVehicleEffects  @0x82629E18  -- drain the frame's air-ram events. Per event
    // (64-byte by-value copy, ctr=8 ld/std -- reproduced by the by-value local):
    //   owner RACECAR (1):  maRaceCarVehicles[idx].AddAirRam(...)   (asm: this + 0x740 +
    //                       idx*0x1460 == &maRaceCarVehicles[idx], the VehiclePhysics base)
    //   owner TRAFFIC (2):  mPhysicalTrafficManager.ProcessAddAirRamEvent(&event)  (+44768)
    //   anything else:      the :3544 assert, fire-and-continue.
    // The queue length is re-read from the queue every iteration (asm `lwz 8(r30)` at the
    // loop tail) -- the for-condition below does the same.
    // ------------------------------------------------------------------------------------
    void VehicleManager::UpdateVehicleEffects(const VehicleEffectsInputInterface* lpEffectsInterface)
    {
        const VehicleEffectsInputInterface::CreateAirRamEventQueue* lpQueue =
            lpEffectsInterface->GetAirRamEventQueue();   // inlined on the console (queue @ interface+0)

        for (s32 liIndex = 0; liIndex < lpQueue->GetLength(); ++liIndex)
        {
            const CreateAirRamEvent lEvent = lpQueue->GetEvent(liIndex);

            // The volume id's HIGH dword is the packed EntityId word: owner [31..24],
            // entity index [23..10] (extrwi 14,8).
            const u32 luEntityWord = static_cast<u32>(lEvent.mVolumeId.muId >> 32);
            const u32 luOwner      = luEntityWord >> 24;

            if (luOwner == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR))
            {
                const u32 luIndex = (luEntityWord >> 10) & 0x3FFFu;

                // v1 carries the raw mDirectionAndMagnitude register (w = magnitude lane);
                // reproduced lane-for-lane (GetVector3() would zero the w the console keeps).
                const Vector3 lvDirection{ lEvent.mDirectionAndMagnitude.x,
                                           lEvent.mDirectionAndMagnitude.y,
                                           lEvent.mDirectionAndMagnitude.z,
                                           lEvent.mDirectionAndMagnitude.w };
                maRaceCarVehicles[luIndex].AddAirRam(
                    lEvent.muEffectFlags,                // r4      <- event+0x08
                    lEvent.mDirectionAndMagnitude.w,     // f1      <- event+0x1C (lfFactor)
                    lEvent.mfDecay,                      // f2      <- event+0x0C
                    lvDirection,                         // v1      <- event+0x10
                    lEvent.mPosition,                    // v2      <- event+0x20
                    lEvent.mfStartTime);                 // f3      <- event+0x30
            }
            else if (luOwner == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE))
            {
                mPhysicalTrafficManager.ProcessAddAirRamEvent(&lEvent);
            }
            else
            {
                CGS_ASSERT(false, "Invalid Entity type in air ram UpdateVehicleEffects Effects");   // :3544
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // ReadUpdatedBodyProperties  @0x825C5520  -- apply queued sim inertia changes back onto
    // the race-car bodies. Per event (80-byte by-value copy, ctr=10):
    //   * owner RACECAR (1) only (high dword of the 64-bit mID, owner byte, index [23..10]);
    //   * index bounds tripwire (:4321, streamed "Received invalid entity index for race
    //     car body" -> CGS_ASSERT), fire-and-continue on the console;
    //   * the FULL 64-bit event mID must equal maRaceCarHandlingBodyIDs[idx] -- the asm is
    //     `ldx` + `cmpld` (the Hex-Rays renders a 32-bit compare; the asm is authoritative);
    //   * forward to the car's ExternalPhysicsBody base (asm this + 0x750 + idx*0x1460 ==
    //     &maRaceCarVehicles[idx] + 0x10, the base sub-object -- spelled here as the derived
    //     object's member call, which the compiler seats identically).
    // ⚠️ HOST DIVERGENCE, flagged: for idx >= 8 the console runs the `ldx` compare anyway
    // (an OOB read past the 8-slot id table whose result feeds an id compare that then almost
    // surely fails); the host guards it -- the ValidateSimulationContacts precedent.
    // ------------------------------------------------------------------------------------
    void VehicleManager::ReadUpdatedBodyProperties(
        const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia, 200>* lpInertiaQueue)
    {
        for (s32 liIndex = 0; liIndex < lpInertiaQueue->GetLength(); ++liIndex)
        {
            const CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia lEvent =
                lpInertiaQueue->GetEvent(liIndex);

            const u32 luIdHigh = static_cast<u32>(lEvent.mID >> 32);
            if ((luIdHigh >> 24) != static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR))
            {
                continue;
            }

            const u32 luIndex = (luIdHigh >> 10) & 0x3FFFu;   // extrwi 14,8
            CGS_ASSERT(luIndex < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS),
                       "Received invalid entity index for race car body");   // :4321

            if (luIndex < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS))   // host bounds guard (see banner)
            {
                if (lEvent.mID == maRaceCarHandlingBodyIDs[luIndex])   // asm cmpld -- full 64 bits
                {
                    maRaceCarVehicles[luIndex].ReadPropertiesFromChangeInertiaEvent(&lEvent);
                }
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // ProcessDeformationStates  @0x825EA580  -- in the two SHOWTIME game modes, feed the
    // player car's deformation state record into its showtime bounce modifiers:
    //   * gate: meCurrentGameModeType == 2 || == 16 (asm +172380);
    //   * assert mePlayerActiveRaceCarIndex in [0, 8) (:4642), fire-and-continue (the console
    //     indexes with the out-of-range value anyway; the reads below re-load the member each
    //     time exactly as the asm does, and the arrays are indexed unguarded to match --
    //     the assert is the only tripwire the console has here, and 0 <= index < 8 holds on
    //     every live path);
    //   * GetCarStateF(mpDeformationState, maRaceCarEntityIDs[idx]) -- null means "this car
    //     has no live deformation model", not an error;
    //   * maRaceCarVehicles[idx].UpdateShowtimeBounceModifiers(record) (asm this + 0x740 +
    //     idx*0x1460, the RaceCarPhysics element itself).
    // ------------------------------------------------------------------------------------
    void VehicleManager::ProcessDeformationStates(
        const Deformation::DeformationOutputInterface* lpDeformationInterface)
    {
        if (meCurrentGameModeType == 2 || meCurrentGameModeType == 16)   // the two showtime modes
        {
            CGS_ASSERT(mePlayerActiveRaceCarIndex >= 0
                           && mePlayerActiveRaceCarIndex < KI_MAX_ACTIVE_RACE_CARS,
                       "mePlayerActiveRaceCarIndex >= 0 && mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // :4642

            const Deformation::CarStateRecord* lpCarState =
                lpDeformationInterface->mpDeformationState->GetCarStateF(
                    maRaceCarEntityIDs[mePlayerActiveRaceCarIndex].muValue);

            if (lpCarState != nullptr)
            {
                maRaceCarVehicles[mePlayerActiveRaceCarIndex].UpdateShowtimeBounceModifiers(lpCarState);
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // ⭐ ADDED 2026-08-06 (big-five #2, contact-generation wave).
    // VehicleManager::ValidateTrafficContact @0x825EAC28 (PS3 DecFIGS 0x6E6178; DWARF h:941).
    // Thin forwarder: three tripwires, then the embedded traffic manager's own validation.
    // Register-truth (X360): the wrapper reads muVolumeInstanceIdA's entity word only for the
    // owner tripwire, then tail-forwards (this+44768 == mPhysicalTrafficManager).
    // ------------------------------------------------------------------------------------
    bool VehicleManager::ValidateTrafficContact(
        CgsSceneManager::SceneManagerIO::PotentialContact* lpContact,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCacheInterface,
        f32 lfTimeStep)
    {
        CGS_ASSERT(lpContact != nullptr, "lpContact != NULL");                          // :7727
        CGS_ASSERT(lpTriCacheInterface != nullptr, "lpTriCacheInterface != NULL");      // :7728
        CGS_ASSERT(static_cast<u32>(lpContact->muVolumeInstanceIdA.muId >> 56) == 2u,
                   "lpContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");   // :7730

        return mPhysicalTrafficManager.ValidateTrafficContact(lpContact, lpTriCacheInterface, lfTimeStep);
    }

    // ------------------------------------------------------------------------------------
    // VehicleManager::BridgeSimpleTrafficWithCarContactsToSimulation @0x825C83B0
    //
    // ⚠⚠ TRAP STUB (closure enforcement, 2026-08-06 big-five #2 wave) -- the REAL body (375
    // X360 asm lines / 9 callees; PS3 DecFIGS 0x70DB6C) is NOT reconstructed yet. Dead code
    // today (Update @0x825B0640 is still a link stub; /OPT:REF strips this). RECONSTRUCT-NEXT.
    // ------------------------------------------------------------------------------------
    void VehicleManager::BridgeSimpleTrafficWithCarContactsToSimulation(
        CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddPotentialContact, 1024>* /*lpContactQueue*/,
        const BrnPhysics::PhysicsModuleIO::PotentialContactInterface* /*lpContactInterface*/)
    {
        CGS_ASSERT(false,
                   "TRAP: VehicleManager::BridgeSimpleTrafficWithCarContactsToSimulation @0x825C83B0 "
                   "not reconstructed (big-five #2 closure stub)\n");
    }
}
}
