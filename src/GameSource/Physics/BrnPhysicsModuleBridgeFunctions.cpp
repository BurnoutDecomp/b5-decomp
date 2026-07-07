#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

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
// byte offset 0x2ECF0, the DeformationManager at 0x4CBA0, the various IO-buffer queues) do not yet
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
    // mDeformationManager@ +0x4CBA0 (314272)  -- DeformationManager (FixupWheel/BodyPartVehicleContact).
    static const u32 KU_OFFSET_VEHICLE_MANAGER      = 19104;   // +0x4AA0
    static const u32 KU_OFFSET_CONTACT_DATA         = 191728;  // +0x2ECF0
    static const u32 KU_OFFSET_DEFORMATION_MANAGER  = 314272;  // +0x4CBA0

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

    // ---- Bridge-slice out-of-line helpers (declared-only; each its own ledger TU) -------------------
    // BrnPhysics::PhysicsModuleIO::OutputBuffer::GetContactSpyInterface (X360 ...GetConta): returns
    // the address of the output buffer's ContactSpyInterface container (a leading mpData pointer).
    extern u8* OutputBuffer_GetContactSpyInterface(u8* lpOutputBuffer);

    // BrnPhysics::ContactSpy::ContactSpyData::IsEmpty(interface) -> true iff all five contact queues empty.
    extern bool ContactSpyData_IsEmpty(u8* lpContactSpyInterface);

    // SimModuleOutputBuffer -> raw contact-spy queue accessor (X360 sub_"CgsPhysic...").
    extern u8* GetSimModuleContactSpyQueue(u8* lpSimModuleOutputBuffer);

    // Module discarded-contact SOURCE queue element accessor (X360 sub_"BrnPhysics::Cont...") and the
    // ContactSpyData discarded-queue sink (BaseEventQueue<DiscardedContact,20>::AddEventSafe @0x825A3628).
    extern u8*  GetDiscardedContactSourceEvent(u8* lpSourceQueue, s32 liIndex);
    extern bool DiscardedContactQueue_AddEventSafe(u8* lpDiscardedQueue, u8* lpEvent);

    // CgsPhysics::PhysicsSimulationIO::InputBuffer::AppendRemoveJointQueue<10>(inputBuffer, removeJointQueue).
    extern void InputBuffer_AppendRemoveJointQueue_10(u8* lpInputBuffer, u8* lpRemoveJointQueue);

    // BrnWorld::E_ENTITYTYPE_COUNT (entity-type-owner histogram width in CheckContactQueueSize).
    static const s32 KI_ENTITYTYPE_COUNT = 35;   // 0x23

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

        // The two volume-instance ids of the potential contact live in the passed-by-value spy image
        // (a 0x70-byte struct; lpOutContactSpy points at spy+0x00). The X360 homes this by-value copy at
        // stack arg_20 (std r4,arg_20 @0x825AB4EC), so its stack loads arg_70/arg_78/arg_80 are struct
        // offsets +0x50/+0x58/+0x60 (arg_NN - 0x20). id A high dword @+0x50, id B high dword @+0x58;
        // entity-type-owner == that high dword >> 24.
        const u32 luEntityOwnerA = *reinterpret_cast<const u32*>(lpOutContactSpy + 0x50) >> 24;
        const u32 luEntityOwnerB = *reinterpret_cast<const u32*>(lpOutContactSpy + 0x58) >> 24;

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
                                               *reinterpret_cast<const u32*>(lpOutContactSpy + 0x60 /*user index; arg_80*/));
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
            // potential-contact ids from the passed spy (arg_70 / arg_78 == spy +0x50 / +0x58). FLAG:
            // the raw-contact stack-image field homes are not independently attested; modelled here off
            // the resolved image / spy word offsets used elsewhere in this function.
            const u32 luRawIdAHi = static_cast<u32>(*reinterpret_cast<const u64*>(laImage + 0x30) >> 32);
            const u32 luRawIdBHi = static_cast<u32>(*reinterpret_cast<const u64*>(laImage + 0x38) >> 32);
            const u32 luPotIdAHi = static_cast<u32>(*reinterpret_cast<const u64*>(lpOutContactSpy + 0x50) >> 32);
            const u32 luPotIdBHi = static_cast<u32>(*reinterpret_cast<const u64*>(lpOutContactSpy + 0x58) >> 32);

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
        // (@+0x30 / @+0x28 groups) swapped, the two 16-bit trailing fields swapped, and the two
        // stress/geometry vectors negated (vspltisw -1; vslw doubles the sign mask; vxor flips sign).
        // Spy struct offsets = X360 arg_NN - 0x20 (the by-value spy is homed at arg_20).
        if (lbBodyPartVsVehicle || lbWheelVsVehicle)
        {
            // Swap the two potential-contact id qwords (+0x50 <-> +0x58; arg_70/arg_78).
            {
                u64* lpA = reinterpret_cast<u64*>(lpOutContactSpy + 0x50);
                u64* lpB = reinterpret_cast<u64*>(lpOutContactSpy + 0x58);
                const u64 luTmp = *lpA; *lpA = *lpB; *lpB = luTmp;
            }
            // Swap the two raw-contact id qwords (+0x30 <-> +0x28; arg_50/arg_48).
            {
                u64* lpA = reinterpret_cast<u64*>(lpOutContactSpy + 0x30);
                u64* lpB = reinterpret_cast<u64*>(lpOutContactSpy + 0x28);
                const u64 luTmp = *lpA; *lpA = *lpB; *lpB = luTmp;
            }
            // Swap the two 32-bit fields (+0x20 <-> +0x24; arg_40/arg_44) and the two 16-bit fields
            // (+0x18 <-> +0x1A; arg_38/arg_3A).
            {
                u32* lpA = reinterpret_cast<u32*>(lpOutContactSpy + 0x20);
                u32* lpB = reinterpret_cast<u32*>(lpOutContactSpy + 0x24);
                const u32 luTmp = *lpA; *lpA = *lpB; *lpB = luTmp;
            }
            {
                u16* lpA = reinterpret_cast<u16*>(lpOutContactSpy + 0x18);
                u16* lpB = reinterpret_cast<u16*>(lpOutContactSpy + 0x1A);
                const u16 luTmp = *lpA; *lpA = *lpB; *lpB = luTmp;
            }
            // Negate the two 16-byte stress/geometry vectors (vxor with the all-ones sign mask == sign flip).
            {
                u32* lpVecA = reinterpret_cast<u32*>(laImage + 0x00);
                u32* lpVecB = reinterpret_cast<u32*>(lpOutContactSpy + 0x40 /*arg_60*/);
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

    // =================================================================================================
    // BrnPhysics::PhysicsModule::BridgeVehicleManagerRequestsToSimulation  @0x825AB968
    //   (InputBuffer* lpSimModuleInputBuffer, const RequestInterface* lpRequestInterface)
    //
    // Forward the vehicle-manager's per-frame joint requests into the simulation input buffer. Only
    // the remove-joint queue is bridged here (the add-joint queue must already be empty this frame).
    // =================================================================================================
    void PhysicsModule_BridgeVehicleManagerRequestsToSimulation(u8* /*lpModule*/,
                                                                u8* lpSimModuleInputBuffer,
                                                                u8* lpRequestInterface)
    {
        CGS_ASSERT(lpSimModuleInputBuffer != nullptr, "lpSimModuleInputBuffer != NULL");
        CGS_ASSERT(lpRequestInterface != nullptr, "lpRequestInterface != NULL");

        // The add-joint queue (@+0x9BE8) must be drained already -- nothing to add this frame.
        CGS_ASSERT(*reinterpret_cast<const s32*>(lpRequestInterface + 0x9BE8) == 0,
                   "lpRequestInterface->GetAddJointQueue()->GetLength() == 0");

        // Append the request interface's remove-joint queue (@+0xA370) into the sim input buffer.
        InputBuffer_AppendRemoveJointQueue_10(lpSimModuleInputBuffer, lpRequestInterface + 0xA370);
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::CheckContactQueueSize  @0x825A1100
    //   (const InAddContactQueue* lpContactQueue, EEntityTypeID leOwnerId)
    //
    // Diagnostics-only. Fires ONLY when the simulation contact queue is full (miLength == miMaxLength):
    // tally every queued contact's two entity-type owners into a per-owner histogram, dump the
    // histogram to the debug log, then assert "Contact Queue is full". No-op when the queue is not full.
    // =================================================================================================
    void PhysicsModule_CheckContactQueueSize(u8* /*lpModule*/, u8* lpContactQueue, u32 /*luOwnerId*/)
    {
        RawContactQueueHead* lpQueue = reinterpret_cast<RawContactQueueHead*>(lpContactQueue);
        if (lpQueue->miLength != lpQueue->miMaxLength)
        {
            return;   // queue not full -- nothing to report
        }

        // Per-entity-type-owner contact histogram (each contact contributes to two owner buckets).
        s32 laOwnerCounts[KI_ENTITYTYPE_COUNT] = { 0 };

        for (s32 liIndex = 0; liIndex < lpQueue->miLength; ++liIndex)
        {
            u8* lpEvent = GetSimulationContact(lpContactQueue, liIndex);

            // Each id's entity-type owner is the high byte of its high dword: id A @+0x30, id B @+0x38.
            const u32 luOwnerA = *reinterpret_cast<const u32*>(lpEvent + 0x30) >> 24;
            CGS_ASSERT(luOwnerA < static_cast<u32>(KI_ENTITYTYPE_COUNT),
                       "lpContactEvent->mIDA.GetEntityIDOwner() < BrnWorld::E_ENTITYTYPE_COUNT");
            ++laOwnerCounts[luOwnerA];

            const u32 luOwnerB = *reinterpret_cast<const u32*>(lpEvent + 0x38) >> 24;
            CGS_ASSERT(luOwnerB < static_cast<u32>(KI_ENTITYTYPE_COUNT),
                       "lpContactEvent->mIDB.GetEntityIDOwner() < BrnWorld::E_ENTITYTYPE_COUNT");
            ++laOwnerCounts[luOwnerB];
        }

        for (s32 liOwner = 0; liOwner < KI_ENTITYTYPE_COUNT; ++liOwner)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Owner id: " << liOwner
                                           << " Contact count: " << laOwnerCounts[liOwner] << "\n";
            }
        }

        CGS_ASSERT(false, "Contact Queue is full\n");
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::BridgeSimulationToOutput  @0x825B0448
    //   (OutputBuffer* lpOutputBuffer, const PotentialContactInterface*, const PropRaceCarContactBuffer*,
    //    const SimModuleOutputBuffer* lpSimModuleOutputBuffer, VecFloat timeStep)
    //
    // End-of-frame bridge: resolve every contact spy the simulation produced into the module's
    // ContactSpyData (via ProcessContactSpies), drain the module's discarded-contact source queue into
    // that data's discarded-contact queue, then publish the ContactSpyData through the output buffer's
    // contact-spy interface. The VecFloat time step is forwarded unchanged to ProcessContactSpies.
    // =================================================================================================
    void PhysicsModule_BridgeSimulationToOutput(u8* lpModule,
                                                u8* lpOutputBuffer,
                                                u8* lpPotentialContactsInterface,
                                                u8* lpPropRaceCarContactBuffer,
                                                u8* lpSimModuleOutputBuffer,
                                                /*VecFloat*/ const void* lpTimeStep)
    {
        CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer != NULL");
        CGS_ASSERT(lpPotentialContactsInterface != nullptr, "lpPotentialContactsInterface != NULL");
        CGS_ASSERT(lpSimModuleOutputBuffer != nullptr, "lpSimModuleOutputBuffer != NULL");

        // The output buffer's contact-spy interface must be empty (or unbound) before we repopulate it.
        u8* lpContactSpyInterface = *reinterpret_cast<u8**>(OutputBuffer_GetContactSpyInterface(lpOutputBuffer));
        const bool lbEmpty = (lpContactSpyInterface == nullptr) || ContactSpyData_IsEmpty(lpContactSpyInterface);
        CGS_ASSERT(lbEmpty, "lpOutputBuffer->GetContactSpyInterface()->IsEmpty()");

        // Resolve all raw contact spies into the module's typed contact queues.
        PhysicsModule_ProcessContactSpies(lpModule,
                                          GetSimModuleContactSpyQueue(lpSimModuleOutputBuffer),
                                          lpPotentialContactsInterface,
                                          lpPropRaceCarContactBuffer,
                                          lpTimeStep);

        // Drain the module's discarded-contact source queue (@+0x2BE40, length @+0x2BE48) into the
        // ContactSpyData discarded-contact queue (@+0x480B0 == +0x2ECF0 + 0x193C0).
        u8* lpSource       = lpModule + 0x2BE40;
        u8* lpDiscardedDst = lpModule + 0x480B0;
        const s32 liCount  = *reinterpret_cast<const s32*>(lpModule + 0x2BE48);
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            u8* lpEvent = GetDiscardedContactSourceEvent(lpSource, liIndex);
            if (!DiscardedContactQueue_AddEventSafe(lpDiscardedDst, lpEvent)
                && (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "WARNING: Ran out of contacts in DiscardedContactQueue.\n";
            }
        }

        // Publish: point the output buffer's contact-spy interface at the module's ContactSpyData.
        u8* lpInterfaceContainer = OutputBuffer_GetContactSpyInterface(lpOutputBuffer);
        u8* lpData = lpModule + KU_OFFSET_CONTACT_DATA;   // +0x2ECF0
        CGS_ASSERT(lpData != nullptr, "lpData != NULL");
        *reinterpret_cast<u8**>(lpInterfaceContainer) = lpData;
    }
}
