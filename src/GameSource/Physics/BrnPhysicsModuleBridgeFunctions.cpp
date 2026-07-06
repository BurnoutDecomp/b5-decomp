#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =====================================================================================================
// BrnPhysics::PhysicsModule -- contact-spy bridge slice (X360 ARTIST build).
//
// Home: GameSource/Physics/BrnPhysicsModuleBridgeFunctions.cpp (DWARF-attested TU for the private
// BridgeContactsToSimulation / ProcessContactSpies / ProcessContactSpy / StoreContact /
// ValidateSimulationContacts driver methods of BrnPhysics::PhysicsModule, BrnPhysicsModule.h:274/277/299).
//
// This slice reconstructs three of those methods store-for-store against the X360 asm:
//   * ProcessContactSpies       @0x825B0300  -- iterate the raw contact-spy queue, resolve each spy,
//                                               then sort+build the four typed contact run lists.
//   * ProcessContactSpy         @0x825AB4D8  -- resolve one raw contact spy: deformation fix-ups for
//                                               body-part / wheel vs vehicle pairs, entity-id
//                                               consistency asserts, and StoreContact.
//   * ValidateSimulationContacts@0x825A1368  -- walk the sim contact queue validating each contact's
//                                               entity-type pair, then defer to the vehicle manager.
//
// FACADE MODEL. The PhysicsModule sub-objects touched here (the embedded ContactSpyData at module
// byte offset 0x2ECF0, the DeformationManager at 0x4CBE0, the various IO-buffer queues) do not yet
// have complete reconstructed C++ layouts, and the OutContactSpy / PotentialContactInterface /
// PropRaceCarContactBuffer parameter types are un-homed. So -- exactly as the committed sibling
// GameSource/Physics/BrnPhysicsQueueFacades.cpp does -- these bodies operate on RAW byte pointers,
// reproducing the X360 byte offsets, store order, loop strides and assert bit-tests verbatim. Swap in
// the typed sub-objects/parameters once those layout passes land; the observable behaviour is identical.
//
// The out-of-line callees (queue element accessors, ContactSpyQueue<T,N>::SortAndCreateRunList<K>,
// DeformationManager fix-ups, StoreContact, ValidateSimulationContactTypes, VehicleManager::
// ValidateSimulationContacts) are their own ledger TUs; declared here with asm-faithful raw signatures.
// =====================================================================================================

namespace CgsSceneManager { namespace SceneManagerIO { struct PotentialContact; } }

namespace BrnPhysics
{
    // ---- module sub-object byte offsets (X360-attested from the call-site adds) ----------------------
    // mVehicleManager    @ +0x4AA0  (19104)   -- BrnVehicleManager (see BrnPhysicsModule.h)
    // mContactData       @ +0x2ECF0 (191728)  -- ContactSpyData; its five typed run lists follow.
    // mDeformationManager@ +0x4CBE0 (314272)  -- DeformationManager (FixupWheel/BodyPartVehicleContact).
    static const u32 KU_OFFSET_VEHICLE_MANAGER      = 19104;   // +0x4AA0
    static const u32 KU_OFFSET_CONTACT_DATA         = 191728;  // +0x2ECF0
    static const u32 KU_OFFSET_DEFORMATION_MANAGER  = 314272;  // +0x4CBE0

    // ContactSpyData-relative run-list offsets (cross-validated against BrnContactSpyData.h):
    //   race car  queue @+0x00000, run list @+0x198D0 (ContactSpyRunList<8>)
    //   traffic   queue @+0x070A0, run list @+0x19B70 (ContactSpyRunList<64>)
    //   carpart   queue @+0x106C0, run list @+0x1AF90 (ContactSpyRunList<50>)
    //   prop      queue @+0x167E0, run list @+0x1BF50 (ContactSpyRunList<100>)

    // ---- out-of-line helpers (declared-only; each its own ledger TU) --------------------------------
    // Raw contact-spy queue element accessor (X360 sub_... "CgsPhysics" facade): GetOutContactSpy(i).
    // Each element is 0x38 (56) bytes -- ProcessContactSpies memcpy's exactly 0x38 from element+0x38.
    extern u8* GetOutContactSpy(u8* lpQueue, s32 liIndex);

    // Sim-contact queue element accessor (X360 sub_8259D258): each element spans 0x50 (80) bytes,
    // copied as ten 64-bit block moves by the ValidateSimulationContacts loop.
    extern u8* GetSimulationContact(u8* lpQueue, s32 liIndex);

    // OutContactSpy element accessor keyed by the potential-contact interface + user index
    // (X360 sub_825A06A0), producing the 0x50 (80-byte, ten qword) resolved-contact image used below.
    extern u8* GetOutContactSpyForContact(u8* lpPotentialContactsInterface, u32 luUserIndex);

    // ContactSpyQueue<T,N>::SortAndCreateRunList<K>(RunList*) explicit instantiations (BrnContactSpyData
    // owns these; mangled emissions in ProcessContactSpies). RunList template arg K derived from the
    // mangled symbol: RaceCar<8>, Traffic<64>, PhysicalCarPart<50>, Prop<100>.
    extern void SortAndCreateRunList_RaceCarContact_300_8(u8* lpQueue, u8* lpRunList);
    extern void SortAndCreateRunList_TrafficContact_400_64(u8* lpQueue, u8* lpRunList);
    extern void SortAndCreateRunList_PhysicalCarPartContact_150_50(u8* lpQueue, u8* lpRunList);
    extern void SortAndCreateRunList_PropContact_100_100(u8* lpQueue, u8* lpRunList);

    // DeformationManager fix-ups: return true when the contact survived (should be stored).
    extern bool DeformationManager_FixupWheelVehicleContact(u8* lpDeformationManager, u8* lpContactImage);
    extern bool DeformationManager_FixupBodyPartVehicleContact(u8* lpDeformationManager, u8* lpContactImage);

    // PhysicsModule::StoreContact(module, &outContactSpy, contactImage, propRaceCarBuffer)  @0x82...
    extern void PhysicsModule_StoreContact(u8* lpModule, u8* lpOutContactSpy, u8* lpContactImage,
                                           u8* lpPropRaceCarContactBuffer);

    // PhysicsModule::ValidateSimulationContactTypes(EEntityTypeID, EEntityTypeID)  @0x82...
    extern void PhysicsModule_ValidateSimulationContactTypes(u32 luEntityTypeA, u32 luEntityTypeB);

    // VehicleManager::ValidateSimulationContacts(vehMgr, contactQueue)  @0x82...
    extern void VehicleManager_ValidateSimulationContacts(u8* lpVehicleManager, u8* lpContactQueue);

    // Raw view of a CgsModule::EventQueue<T,N> head (mpEvents@0, miMaxLength@4, miLength@8).
    struct RawContactQueueHead
    {
        u8* mpEvents;     // +0x00
        s32 miMaxLength;  // +0x04
        s32 miLength;     // +0x08
    };

    // Forward declarations (ProcessContactSpies calls ProcessContactSpy).
    void PhysicsModule_ProcessContactSpy(u8* lpModule, u8* lpOutContactSpy, u8* lpContactImage,
                                         u8* lpPotentialContactsInterface, u8* lpPropRaceCarContactBuffer,
                                         const void* lpTimeStep);

    // =================================================================================================
    // BrnPhysics::PhysicsModule::ProcessContactSpies  @0x825B0300
    //   (const OutContactSpyQueue* lpRawContactSpies, const PotentialContactInterface*,
    //    const PropRaceCarContactBuffer*, VecFloat)
    //
    // For every raw contact spy in the queue, resolve it via ProcessContactSpy, then sort each of the
    // four typed contact queues and build its run list. The VecFloat parameter (v127) is preserved
    // across the loop and forwarded unchanged to each ProcessContactSpy.
    // =================================================================================================
    void PhysicsModule_ProcessContactSpies(u8* lpModule,
                                           u8* lpRawContactSpies,
                                           u8* lpPotentialContactsInterface,
                                           u8* lpPropRaceCarContactBuffer,
                                           /*VecFloat*/ const void* lpTimeStep)
    {
        CGS_ASSERT(lpRawContactSpies != nullptr, "lpRawContactSpies != NULL");
        CGS_ASSERT(lpPotentialContactsInterface != nullptr, "lpPotentialContactsInterface != NULL");

        RawContactQueueHead* lpQueue = reinterpret_cast<RawContactQueueHead*>(lpRawContactSpies);
        for (s32 liIndex = 0; liIndex < lpQueue->miLength; ++liIndex)
        {
            u8* lpSpy = GetOutContactSpy(lpRawContactSpies, liIndex);

            // The resolved-contact image ProcessContactSpy consumes: the 0x38-byte tail of the spy
            // (element+0x38, copied to the stack) plus the leading seven qwords passed by value.
            u8 laContactImage[0x38];
            for (u32 luByte = 0; luByte < sizeof(laContactImage); ++luByte)
            {
                laContactImage[luByte] = lpSpy[0x38 + luByte];
            }

            PhysicsModule_ProcessContactSpy(lpModule,
                                            lpSpy,                       // leading 7 qwords (by value on X360)
                                            laContactImage,
                                            lpPotentialContactsInterface,
                                            lpPropRaceCarContactBuffer,
                                            lpTimeStep);
        }

        u8* lpContactData = lpModule + KU_OFFSET_CONTACT_DATA;
        SortAndCreateRunList_RaceCarContact_300_8         (lpContactData + 0x00000, lpContactData + 0x198D0);
        SortAndCreateRunList_TrafficContact_400_64        (lpContactData + 0x070A0, lpContactData + 0x19B70);
        SortAndCreateRunList_PhysicalCarPartContact_150_50(lpContactData + 0x106C0, lpContactData + 0x1AF90);
        SortAndCreateRunList_PropContact_100_100          (lpContactData + 0x167E0, lpContactData + 0x1BF50);
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::ProcessContactSpy  @0x825AB4D8
    //   (OutContactSpy, const PotentialContactInterface*, const PropRaceCarContactBuffer*, VecFloat)
    //
    // Resolve one raw contact spy. Reads the two contact entity-type-owner bytes (the high byte of each
    // volume-instance id's high dword), rejects illegal traffic<->car / world<->traffic pairs, fetches
    // the matching resolved-contact image, applies the deformation-manager wheel/body-part fix-up for
    // deformable-part-vs-vehicle pairs, asserts entity-id / owner consistency for the remaining pairs,
    // and stores the contact (once, or -- for the wheel/body-part path -- twice: the contact and its
    // A/B-swapped, normal/point-negated reciprocal).
    // =================================================================================================
    void PhysicsModule_ProcessContactSpy(u8* lpModule,
                                         u8* lpOutContactSpy,
                                         u8* lpContactImage,
                                         u8* lpPotentialContactsInterface,
                                         u8* lpPropRaceCarContactBuffer,
                                         /*VecFloat*/ const void* /*lpTimeStep*/)
    {
        CGS_ASSERT(lpPotentialContactsInterface != nullptr, "lpPotentialContactsInterface != NULL");

        // The two volume-instance ids of the potential contact live in the passed-by-value spy image:
        // id A high dword @+0x70, id B high dword @+0x78; entity-type-owner == that high dword >> 24.
        const u32 luEntityOwnerA = *reinterpret_cast<const u32*>(lpOutContactSpy + 0x70) >> 24;
        const u32 luEntityOwnerB = *reinterpret_cast<const u32*>(lpOutContactSpy + 0x78) >> 24;

        // Illegal pair guards (assert messages + line numbers are X360 rodata; negated conditions).
        if (luEntityOwnerA == 2)
        {
            CGS_ASSERT(luEntityOwnerB != 1,
                       "!( leEntityTypeA == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE && leEntityTypeB == BrnWorld::E_ENTITYTYPE_RACECAR )");
        }
        else if (luEntityOwnerA == 0)
        {
            CGS_ASSERT(luEntityOwnerB != 2,
                       "!( leEntityTypeA == BrnWorld::E_ENTITYTYPE_WORLD && leEntityTypeB == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )");
        }

        // Fetch the resolved-contact image for this spy (ten qwords -> 0x50-byte local image).
        u8* lpSrc = GetOutContactSpyForContact(lpPotentialContactsInterface,
                                               *reinterpret_cast<const u32*>(lpOutContactSpy + 0x80 /*user index*/));
        u8 laImage[0x50];
        for (u32 luByte = 0; luByte < sizeof(laImage); ++luByte)
        {
            laImage[luByte] = lpSrc[luByte];
        }

        bool lbStore = true;
        u8* lpDeformationManager = lpModule + KU_OFFSET_DEFORMATION_MANAGER;

        const bool lbBodyPartVsVehicle =
            (luEntityOwnerA == 6 || luEntityOwnerA == 7) && (luEntityOwnerB == 11 || luEntityOwnerB == 12);
        const bool lbWheelVsVehicle =
            (luEntityOwnerA == 9 || luEntityOwnerA == 10) && (luEntityOwnerB == 11 || luEntityOwnerB == 12);

        if (lbBodyPartVsVehicle)
        {
            lbStore = DeformationManager_FixupBodyPartVehicleContact(lpDeformationManager, laImage);
        }
        else if (lbWheelVsVehicle)
        {
            lbStore = DeformationManager_FixupWheelVehicleContact(lpDeformationManager, laImage);
        }
        else
        {
            // Entity-id / owner consistency asserts (debug-only; non-gating). The ids are 8-byte
            // volume-instance words and the entity-id is that word's HIGH dword (srdi ..,32). In the
            // X360 asm the raw-contact ids come from the lower stack image (var_50 / var_48) and the
            // potential-contact ids from the passed spy (arg_70 / arg_78). FLAG: the raw-contact
            // stack-image field homes are not independently attested; modelled here off the resolved
            // image / spy word offsets used elsewhere in this function.
            const u32 luRawIdAHi = static_cast<u32>(*reinterpret_cast<const u64*>(laImage + 0x30) >> 32);
            const u32 luRawIdBHi = static_cast<u32>(*reinterpret_cast<const u64*>(laImage + 0x38) >> 32);
            const u32 luPotIdAHi = static_cast<u32>(*reinterpret_cast<const u64*>(lpOutContactSpy + 0x70) >> 32);
            const u32 luPotIdBHi = static_cast<u32>(*reinterpret_cast<const u64*>(lpOutContactSpy + 0x78) >> 32);

            if ((luEntityOwnerA == 0 && luEntityOwnerB == 1) || (luEntityOwnerA == 1 && luEntityOwnerB == 0))
            {
                // world<->racecar: full-id equality (GetEntityId compares the low 32 bits).
                CGS_ASSERT(luRawIdAHi == luPotIdAHi,
                           "lRawContact.mIDA.GetEntityId() == lPotentialContact.muVolumeInstanceIdA.GetEntityId()");
                CGS_ASSERT(luRawIdBHi == luPotIdBHi,
                           "lRawContact.mIDB.GetEntityId() == lPotentialContact.muVolumeInstanceIdB.GetEntityId()");
            }
            else if (luEntityOwnerA == 1 && luEntityOwnerB == 1)
            {
                CGS_ASSERT(luRawIdAHi == luPotIdAHi,
                           "lRawContact.mIDA.GetEntityId() == lPotentialContact.muVolumeInstanceIdA.GetEntityId()");
                CGS_ASSERT(luRawIdBHi == luPotIdBHi,
                           "lRawContact.mIDB.GetEntityId() == lPotentialContact.muVolumeInstanceIdB.GetEntityId()");
            }
            else if (luEntityOwnerA == 2 && luEntityOwnerB == 0)
            {
                // owner-byte (high byte of high dword) equality.
                CGS_ASSERT((luRawIdAHi >> 24) == (luPotIdAHi >> 24),
                           "lRawContact.mIDA.GetEntityIDOwner() == lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner()");
                CGS_ASSERT((luRawIdBHi >> 24) == (luPotIdBHi >> 24),
                           "lRawContact.mIDB.GetEntityIDOwner() == lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner()");
            }
            else if (luEntityOwnerA == 1 && luEntityOwnerB == 2)
            {
                CGS_ASSERT((luRawIdAHi >> 24) == (luPotIdAHi >> 24),
                           "lRawContact.mIDA.GetEntityIDOwner() == lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner()");
                CGS_ASSERT((luRawIdBHi >> 24) == (luPotIdBHi >> 24),
                           "lRawContact.mIDB.GetEntityIDOwner() == lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner()");
            }
            // (all other pairs fall through to StoreContact with no id check)
        }

        if (!lbStore)
        {
            return;
        }

        // Store the resolved contact.
        PhysicsModule_StoreContact(lpModule, lpOutContactSpy, laImage, lpPropRaceCarContactBuffer);

        // The wheel/body-part deformable path stores a SECOND, reciprocal contact: the spy image is
        // rebuilt with the two volume-instance ids swapped (A<->B), the two 8-byte id/tag fields
        // (@+0x50 / @+0x48 groups) swapped, the two 16-bit trailing fields swapped, and the two
        // stress/geometry vectors negated (vspltisw -1; vslw doubles the sign mask; vxor flips sign).
        if (lbBodyPartVsVehicle || lbWheelVsVehicle)
        {
            // Swap the two potential-contact id qwords (+0x70 <-> +0x78).
            {
                u64* lpA = reinterpret_cast<u64*>(lpOutContactSpy + 0x70);
                u64* lpB = reinterpret_cast<u64*>(lpOutContactSpy + 0x78);
                const u64 luTmp = *lpA; *lpA = *lpB; *lpB = luTmp;
            }
            // Swap the two raw-contact id qwords (+0x50 <-> +0x48).
            {
                u64* lpA = reinterpret_cast<u64*>(lpOutContactSpy + 0x50);
                u64* lpB = reinterpret_cast<u64*>(lpOutContactSpy + 0x48);
                const u64 luTmp = *lpA; *lpA = *lpB; *lpB = luTmp;
            }
            // Swap the two 32-bit fields (+0x40 <-> +0x44) and the two 16-bit fields (+0x38 <-> +0x3A).
            {
                u32* lpA = reinterpret_cast<u32*>(lpOutContactSpy + 0x40);
                u32* lpB = reinterpret_cast<u32*>(lpOutContactSpy + 0x44);
                const u32 luTmp = *lpA; *lpA = *lpB; *lpB = luTmp;
            }
            {
                u16* lpA = reinterpret_cast<u16*>(lpOutContactSpy + 0x38);
                u16* lpB = reinterpret_cast<u16*>(lpOutContactSpy + 0x3A);
                const u16 luTmp = *lpA; *lpA = *lpB; *lpB = luTmp;
            }
            // Negate the two 16-byte stress/geometry vectors (vxor with the all-ones sign mask == sign flip).
            {
                u32* lpVecA = reinterpret_cast<u32*>(laImage + 0x00);
                u32* lpVecB = reinterpret_cast<u32*>(lpOutContactSpy + 0x60);
                for (u32 luLane = 0; luLane < 4; ++luLane)
                {
                    lpVecA[luLane] ^= 0x80000000u;
                    lpVecB[luLane] ^= 0x80000000u;
                }
            }

            PhysicsModule_StoreContact(lpModule, lpOutContactSpy, laImage, lpPropRaceCarContactBuffer);
        }
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::ValidateSimulationContacts  @0x825A1368
    //   (const InputBuffer::InAddContactQueue* lpContactQueue)
    //
    // Validate each queued simulation contact's entity-type pair, then hand the queue to the vehicle
    // manager for its own validation pass and return that result.
    // =================================================================================================
    void PhysicsModule_ValidateSimulationContacts(u8* lpModule, u8* lpContactQueue)
    {
        CGS_ASSERT(lpContactQueue != nullptr, "lpContactQueue != NULL");

        RawContactQueueHead* lpQueue = reinterpret_cast<RawContactQueueHead*>(lpContactQueue);
        for (s32 liIndex = 0; liIndex < lpQueue->miLength; ++liIndex)
        {
            u8* lpSrc = GetSimulationContact(lpContactQueue, liIndex);

            // Copy the ten leading qwords of the contact into a stack image, then read the two
            // entity-type-owner bytes: id A high dword @+0x30, id B high dword @+0x38, each >> 24.
            u8 laImage[0x50];
            for (u32 luByte = 0; luByte < sizeof(laImage); ++luByte)
            {
                laImage[luByte] = lpSrc[luByte];
            }

            const u32 luEntityTypeA = *reinterpret_cast<const u32*>(laImage + 0x30) >> 24;
            const u32 luEntityTypeB = *reinterpret_cast<const u32*>(laImage + 0x38) >> 24;
            PhysicsModule_ValidateSimulationContactTypes(luEntityTypeA, luEntityTypeB);
        }

        VehicleManager_ValidateSimulationContacts(lpModule + KU_OFFSET_VEHICLE_MANAGER, lpContactQueue);
    }
}
