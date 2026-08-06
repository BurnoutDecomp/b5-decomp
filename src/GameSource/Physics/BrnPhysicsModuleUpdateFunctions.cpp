// ============================================================================
// GameSource/Physics/BrnPhysicsModuleUpdateFunctions.cpp
//
// BrnPhysics::PhysicsModule -- the per-frame Update helper functions' home TU
// (the console path baked into this TU's asserts is
// "...\gamesource\unity\../Physics/BrnPhysicsModuleUpdateFunctions.cpp").
// ⚠️ The dossier keys this function to BrnPhysicsModule.cpp; the baked :915 path
// above is the byte-grounded correction.
//
// This slice: FixUpVehicleContacts @ 0x825A6010 (1067 insns) -- the big-five
// opener of the PhysicsModule::Update subtree. Reconstructed from the
// BURNOUT_X360_ARTIST.XEX asm; the PS3 DecFIGS out-of-line build (@0x699058)
// corroborates the structure call-for-call (it keeps GetPhysicsEntityIDFrom-
// GlobalEntityID and the queue GetEvent out-of-line where the X360 inlines /
// out-of-lines differently).
//
// Caller: PhysicsModule::Update @0x825B0640 -- STILL A LINK STUB, so nothing
// reaches this body at runtime yet; /OPT:REF strips it (unchanged exe size is
// the expected result). Mounted anyway: link-closure enforcement is the point.
// ============================================================================

#include "GameSource/Physics/BrnPhysicsModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // PotentialContact (muVolumeInstanceIdA/B)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"  // the three custom-queue accessors
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"            // VehicleManager::GetPhysicsEntityIDFromGlobalEntityID
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"    // DeformationManager::FixUpVehicleContact[ByInterpolation]

namespace BrnPhysics
{
    namespace
    {
        // Owner byte of a packed 64-bit VolumeInstanceId (entity word == the HIGH dword).
        inline u32 GetVolumeInstanceOwner(const CgsSceneManager::VolumeInstanceId& lrId)
        {
            return static_cast<u32>(lrId.muId >> 56) & 0xFFu;
        }

        // Splice a rewritten 32-bit physics entity id into the HIGH dword of a packed
        // 64-bit volume-instance id, preserving the low dword (X360 @0x825A6620:
        // `sldi rewritten,32 ; clrldi low,32 ; or`).
        inline CgsSceneManager::VolumeInstanceId SpliceEntityWord(
            const CgsSceneManager::VolumeInstanceId& lrId, CgsSceneManager::EntityId lNewEntityId)
        {
            CgsSceneManager::VolumeInstanceId lResult;
            lResult.muId = (static_cast<u64>(static_cast<u32>(lNewEntityId)) << 32)
                         | (lrId.muId & 0x00000000FFFFFFFFull);
            return lResult;
        }
    }

    // ==========================================================================================
    // FixUpVehicleContacts @ 0x825A6010
    //
    // Deform-fix every vehicle-vs-vehicle potential contact this frame:
    //   1) the racecar-vs-traffic queue ([8]): assert the (RACECAR, TRAFFIC_VEHICLE) owner pair,
    //      rewrite the traffic B id GLOBAL->PHYSICAL, then FixUpVehicleContactByInterpolation;
    //   2) the racecar-vs-racecar queue ([7]): assert (RACECAR, RACECAR), no rewrite,
    //      FixUpVehicleContactByInterpolation;
    //   3) the scene-manager contact queue ([0]): FILTER (no assert) for (TRAFFIC, TRAFFIC)
    //      pairs, rewrite BOTH ids GLOBAL->PHYSICAL, then FixUpVehicleContact.
    // Queue lengths are re-read every iteration, as the console does (`lwz 8(queue)` inside the
    // loop). The contacts are mutated IN PLACE through the queues' storage -- the X360 walks the
    // event array directly and the DeformationManager callees write the records; GetEvent's
    // const overload is cast away exactly once per loop to reproduce that in-place write.
    // ==========================================================================================
    void PhysicsModule::FixUpVehicleContacts(
        PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface)
    {
        CGS_ASSERT(lpPotentialContactsInterface != nullptr, "lpPotentialContactsInterface != NULL");   // :915

        typedef PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue Queue;
        typedef CgsSceneManager::SceneManagerIO::PotentialContact                       PotentialContact;

        // ---- (1) racecar vs traffic -----------------------------------------------------------
        {
            const Queue& lrQueue = lpPotentialContactsInterface->GetRaceCarWithTrafficQueue();
            for (s32 liContact = 0; liContact < lrQueue.GetLength(); ++liContact)
            {
                PotentialContact& lrContact = const_cast<PotentialContact&>(lrQueue.GetEvent(liContact));

                const CgsSceneManager::VolumeInstanceId lRaceCarVolInstId = lrContact.muVolumeInstanceIdA;
                CgsSceneManager::VolumeInstanceId       lTrafficVolInstId = lrContact.muVolumeInstanceIdB;

                CGS_ASSERT(GetVolumeInstanceOwner(lRaceCarVolInstId) == 1u &&
                           GetVolumeInstanceOwner(lTrafficVolInstId) == 2u,
                           "lRaceCarVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && "
                           "lTrafficVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");   // :947

                // The Fixup* id-rewrite: the traffic car's GLOBAL entity id -> its LOCAL PHYSICS id.
                const CgsSceneManager::EntityId lPhysicsId =
                    mVehicleManager.GetPhysicsEntityIDFromGlobalEntityID(
                        CgsSceneManager::EntityId(static_cast<u32>(lTrafficVolInstId.muId >> 32)));
                lTrafficVolInstId = SpliceEntityWord(lTrafficVolInstId, lPhysicsId);

                mDeformationManager.FixUpVehicleContactByInterpolation(lrContact, lRaceCarVolInstId,
                                                                       lTrafficVolInstId);
            }
        }

        // ---- (2) racecar vs racecar -----------------------------------------------------------
        {
            const Queue& lrQueue = lpPotentialContactsInterface->GetRaceCarWithRaceCarQueue();
            for (s32 liContact = 0; liContact < lrQueue.GetLength(); ++liContact)
            {
                PotentialContact& lrContact = const_cast<PotentialContact&>(lrQueue.GetEvent(liContact));

                CGS_ASSERT(GetVolumeInstanceOwner(lrContact.muVolumeInstanceIdA) == 1u &&
                           GetVolumeInstanceOwner(lrContact.muVolumeInstanceIdB) == 1u,
                           "lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"
                           " && lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");   // :975

                mDeformationManager.FixUpVehicleContactByInterpolation(lrContact,
                                                                       lrContact.muVolumeInstanceIdA,
                                                                       lrContact.muVolumeInstanceIdB);
            }
        }

        // ---- (3) the scene queue's traffic-vs-traffic pairs (filtered, not asserted) ----------
        {
            const Queue& lrQueue = lpPotentialContactsInterface->GetSceneManagerContactQueue();
            for (s32 liContact = 0; liContact < lrQueue.GetLength(); ++liContact)
            {
                PotentialContact& lrContact = const_cast<PotentialContact&>(lrQueue.GetEvent(liContact));

                CgsSceneManager::VolumeInstanceId lVolInstIdA = lrContact.muVolumeInstanceIdA;
                CgsSceneManager::VolumeInstanceId lVolInstIdB = lrContact.muVolumeInstanceIdB;
                if (GetVolumeInstanceOwner(lVolInstIdA) != 2u || GetVolumeInstanceOwner(lVolInstIdB) != 2u)
                {
                    continue;   // X360 @0x825A6760/@0x825A6774: skip, no assert
                }

                lVolInstIdA = SpliceEntityWord(lVolInstIdA,
                    mVehicleManager.GetPhysicsEntityIDFromGlobalEntityID(
                        CgsSceneManager::EntityId(static_cast<u32>(lVolInstIdA.muId >> 32))));
                lVolInstIdB = SpliceEntityWord(lVolInstIdB,
                    mVehicleManager.GetPhysicsEntityIDFromGlobalEntityID(
                        CgsSceneManager::EntityId(static_cast<u32>(lVolInstIdB.muId >> 32))));

                mDeformationManager.FixUpVehicleContact(lrContact, lVolInstIdA, lVolInstIdB);
            }
        }
    }
}
