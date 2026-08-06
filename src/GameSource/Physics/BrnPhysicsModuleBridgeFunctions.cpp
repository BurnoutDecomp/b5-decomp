#include "GameSource/Physics/BrnPhysicsModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                    // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"      // sim IO buffers + OutContactSpy / InAddPotentialContact
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // CgsSceneManager::SceneManagerIO::PotentialContact (+SwapEntityOrder)
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                            // PhysicsModuleIO::OutputBuffer (GetContactSpyInterface)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"  // PhysicsModuleIO::PotentialContactInterface (GetEvent(ContactId))
#include "GameSource/Physics/ContactSpies/BrnContactId.h"                     // BrnPhysics::ContactId
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"                // ContactSpyData (typed queues + run lists + AddContact)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"              // BaseContact/RaceCarContact/... event records
#include "GameSource/Physics/ContactSpies/BrnContactSpyQueue.h"               // ContactSpyQueue<T,N>::SortAndCreateRunList<K> (decl)
#include "GameSource/Physics/ContactSpies/BrnContactSpyRunList.h"             // ContactSpyRunList<N>
#include "GameSource/World/BrnEntityTypes.h"                                  // BrnWorld::EEntityTypeID enumerators

// =====================================================================================================
// BrnPhysics::PhysicsModule -- contact-spy bridge slice (X360 ARTIST build).
//
// Home: GameSource/Physics/BrnPhysicsModuleBridgeFunctions.cpp (DWARF-attested TU for the private
// BridgeContactsToSimulation / ProcessContactSpies / ProcessContactSpy / StoreContact /
// ValidateSimulationContacts driver methods of BrnPhysics::PhysicsModule; each body below carries
// its DWARF TU line).
//
// ⭐⭐ DE-FACADED 2026-08-06. This TU's previous revision reconstructed six of these bodies as RAW
// FREE FUNCTIONS over u8* ("FACADE MODEL": PhysicsModule_BridgeSimulationToOutput(u8*, ...)) --
// no real caller could ever dispatch them, and a trial mount produced 18 LNK2019 against helper
// facades no TU defines. Every type the slice touches is homed now (OutContactSpy / the
// ContactSpyData aggregate / ContactSpyInterface / the PotentialContactInterface / the sim IO
// buffers), so the bodies below are the REAL private members against the REAL headers, and every
// console byte offset in the old facade is replaced by the named member whose pinned seat it was:
//     module+0x2ECF0 -> mContactData          module+0x4CBA0 -> mDeformationManager
//     module+0x4AA0  -> mVehicleManager       module+0x63630 -> mPropManager
//     module+0x2BE40 -> mVehicleManager.GetDiscardedContacts()
//     contactData+0x00000/0x070A0/0x106C0/0x167E0 + run lists -> the typed queue/run-list members
//
// ⚠️⚠️ TWO FACADE MISREADINGS CORRECTED AGAINST THE ASM (dossier bridge_dossier.txt, re-read):
//   * ProcessContactSpy stores EVERY surviving contact TWICE -- the id-assert paths and the
//     deformation-fixup paths all fall into the same store/swap/store tail (0x825AB87C..0x825AB958,
//     no branch between) -- the facade emitted the second store only for the deformable paths.
//   * The between-stores mutation is SwapEntityOrder() on BOTH records (swap points A/B, swap ids,
//     negate mNormal; the potential contact also swaps its poly tags + primitive indices). The
//     facade negated the wrong rows (image+0x00 / spy+0x40) and "swapped" spy+0x28<->+0x30 -- a
//     straddle of mNormal's tail and mPointOnA's head that corresponds to no field pair.
//
// The out-of-line callees keep their own homes: the four SortAndCreateRunList instantiation TUs,
// BaseContact::Construct (BrnContactSpyEvents.cpp), ContactSpyData::AddContact overloads
// (BrnContactSpyData.cpp), the DeformationManager fixups/creators (BrnDeformationManager_Contacts
// .cpp), PropManager::CreateContactEvent (BrnPropManager.cpp), VehicleManager::
// ValidateSimulationContacts (BrnVehicleManager_ValidateSimulationContacts.cpp), and the
// header-inline BaseEventQueue accessors.
// =====================================================================================================

namespace BrnPhysics
{
    using CgsPhysics::PhysicsSimulationIO::OutContactSpy;
    using CgsPhysics::PhysicsSimulationIO::InAddPotentialContact;
    using CgsSceneManager::SceneManagerIO::PotentialContact;

    namespace
    {
        // The entity-type-owner histogram width baked into CheckContactQueueSize @0x825A1100
        // (`li r9, 0x23` loop bound + the two `cmplwi ..., 0x23` bound asserts). ⚠️ FLAGGED
        // DISCREPANCY, carried not smoothed: the assert strings spell it
        // "BrnWorld::E_ENTITYTYPE_COUNT", but the DecFIGS (PS3) DWARF enum pins
        // E_ENTITYTYPE_COUNT == 34 (the committed BrnEntityTypes.h). The X360 image bakes 35 in
        // all three places, so 35 is what this build uses -- the asm literal wins over both the
        // string and the PS3 enum.
        const s32 KI_X360_ENTITYTYPE_HISTOGRAM_WIDTH = 35;   // 0x23

        // Owner byte of an 8-byte packed id/handle word (the high byte of its high dword --
        // `srdi 32 ; srwi 24` everywhere in this TU).
        inline u32 GetIdOwner(u64 lu64Id) { return static_cast<u32>(lu64Id >> 56); }
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::ProcessContactSpies  @0x825B0300   (TU line :1049)
    //
    // For every raw contact spy in the queue, resolve it via ProcessContactSpy (spy passed BY VALUE
    // -- the X360's seven-qword register image + 0x38-byte stack tail ARE that by-value copy), then
    // sort each of the four typed contact queues and build its run list. The VecFloat time step
    // (v127) is preserved across the loop and forwarded unchanged.
    // =================================================================================================
    void PhysicsModule::ProcessContactSpies(
        const CgsPhysics::PhysicsSimulationIO::OutputBuffer::OutContactSpyQueue* lpRawContactSpies,
        const PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface,
        const Props::PropRaceCarContactBuffer* lpPropRaceCarContactBuffer,
        VecFloat lvfTimeStep )
    {
        CGS_ASSERT(lpRawContactSpies != nullptr, "lpRawContactSpies != NULL");                     // :1051
        CGS_ASSERT(lpPotentialContactsInterface != nullptr, "lpPotentialContactsInterface != NULL"); // :1052

        for (s32 liIndex = 0; liIndex < lpRawContactSpies->GetLength(); ++liIndex)
        {
            // BaseEventQueue<OutContactSpy>::GetEvent(s32) const @0x8259D470, then the by-value
            // copy into the callee (the X360 ld r4..r10 + 0x38-byte memcpy).
            ProcessContactSpy(lpRawContactSpies->GetEvent(liIndex),
                              lpPotentialContactsInterface,
                              lpPropRaceCarContactBuffer,
                              lvfTimeStep);
        }

        // Sort + build the four typed run lists (the four explicit SortAndCreateRunList
        // instantiations, called on the queue/run-list pairs at their pinned ContactSpyData
        // seats through the aggregate's own DWARF method -- inlined here on the console).
        mContactData.SortQueuesByEntityId();
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::ProcessContactSpy  @0x825AB4D8   (TU line :1094)
    //
    // Resolve one raw contact spy: reject illegal traffic<->car / world<->traffic pairs, fetch the
    // matching potential-contact record by the spy's muTag (a packed ContactId), apply the
    // deformation-manager wheel/body-part fix-up for deformable-part-vs-vehicle pairs (dropping the
    // contact when the fixup says so), assert entity-id/owner consistency for the world/car/traffic
    // pairs, then store the contact TWICE: as-is, and A/B-swapped with negated normals (the inlined
    // OutContactSpy::SwapEntityOrder + PotentialContact::SwapEntityOrder pair between the two
    // StoreContact calls -- asm 0x825AB87C..0x825AB958, one straight-line tail for ALL paths).
    // =================================================================================================
    void PhysicsModule::ProcessContactSpy(
        OutContactSpy lContactSpy,
        const PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface,
        const Props::PropRaceCarContactBuffer* lpPropRaceCarContactBuffer,
        VecFloat /*lvfTimeStep*/ )
    {
        CGS_ASSERT(lpPotentialContactsInterface != nullptr, "lpPotentialContactsInterface != NULL");   // :1096

        const u32 luEntityTypeA = GetIdOwner(lContactSpy.mIDA);
        const u32 luEntityTypeB = GetIdOwner(lContactSpy.mIDB);

        // Illegal-pair guards (negated conditions; X360 lines 0x454/0x458 == :1108/:1112).
        if (luEntityTypeA == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE))
        {
            CGS_ASSERT(luEntityTypeB != static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
                       "!( leEntityTypeA == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE && leEntityTypeB == BrnWorld::E_ENTITYTYPE_RACECAR )");           // :1108
        }
        else if (luEntityTypeA == static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD))
        {
            CGS_ASSERT(luEntityTypeB != static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE),
                       "!( leEntityTypeA == BrnWorld::E_ENTITYTYPE_WORLD && leEntityTypeB == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )");             // :1112
        }

        // Fetch the potential-contact record this spy resolved from (the spy's muTag is a packed
        // ContactId; GetEvent(ContactId) @0x825A06A0), and copy it (the X360's ten-qword local
        // image at var_80).
        PotentialContact lPotentialContact =
            lpPotentialContactsInterface->GetEvent(ContactId(lContactSpy.muTag));

        bool lbStore = true;

        const bool lbBodyPartVsVehicle =
            (luEntityTypeA == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART) ||
             luEntityTypeA == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART)) &&
            (luEntityTypeB == static_cast<u32>(BrnWorld::E_ENTITYTYPE_PROP_COLLISION_RACECAR) ||
             luEntityTypeB == static_cast<u32>(BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC));
        const bool lbWheelVsVehicle =
            (luEntityTypeA == static_cast<u32>(BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL) ||
             luEntityTypeA == static_cast<u32>(BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL)) &&
            (luEntityTypeB == static_cast<u32>(BrnWorld::E_ENTITYTYPE_PROP_COLLISION_RACECAR) ||
             luEntityTypeB == static_cast<u32>(BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC));

        if (lbBodyPartVsVehicle)
        {
            lbStore = mDeformationManager.FixupBodyPartVehicleContact(&lPotentialContact);
        }
        else if (lbWheelVsVehicle)
        {
            lbStore = mDeformationManager.FixupWheelVehicleContact(&lPotentialContact);
        }
        else
        {
            // Entity-id / owner consistency asserts (debug-only, fire-and-continue). "lRawContact"
            // is the spy record; "lPotentialContact" the fetched image -- the asm compares the
            // spy's id words (arg_70/arg_78) against the image's (var_50/var_48).
            const u32 luRawIdAHi = static_cast<u32>(lContactSpy.mIDA >> 32);
            const u32 luRawIdBHi = static_cast<u32>(lContactSpy.mIDB >> 32);
            const u32 luPotIdAHi = static_cast<u32>(lPotentialContact.muVolumeInstanceIdA.muId >> 32);
            const u32 luPotIdBHi = static_cast<u32>(lPotentialContact.muVolumeInstanceIdB.muId >> 32);

            if ((luEntityTypeA == 0u && luEntityTypeB == 1u) || (luEntityTypeB == 0u && luEntityTypeA == 1u))
            {
                // world<->racecar (either order): full entity-word equality. Lines :1151/:1152.
                CGS_ASSERT(luRawIdAHi == luPotIdAHi,
                           "lRawContact.mIDA.GetEntityId() == lPotentialContact.muVolumeInstanceIdA.GetEntityId()");
                CGS_ASSERT(luRawIdBHi == luPotIdBHi,
                           "lRawContact.mIDB.GetEntityId() == lPotentialContact.muVolumeInstanceIdB.GetEntityId()");
            }
            else if (luEntityTypeA == 1u && luEntityTypeB == 1u)
            {
                // racecar<->racecar: full entity-word equality. Lines :1156/:1157.
                CGS_ASSERT(luRawIdAHi == luPotIdAHi,
                           "lRawContact.mIDA.GetEntityId() == lPotentialContact.muVolumeInstanceIdA.GetEntityId()");
                CGS_ASSERT(luRawIdBHi == luPotIdBHi,
                           "lRawContact.mIDB.GetEntityId() == lPotentialContact.muVolumeInstanceIdB.GetEntityId()");
            }
            else if (luEntityTypeA == 2u && luEntityTypeB == 0u)
            {
                // traffic<->world: owner-byte equality. Lines :1161/:1162.
                CGS_ASSERT((luRawIdAHi >> 24) == (luPotIdAHi >> 24),
                           "lRawContact.mIDA.GetEntityIDOwner() == lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner()");
                CGS_ASSERT((luRawIdBHi >> 24) == (luPotIdBHi >> 24),
                           "lRawContact.mIDB.GetEntityIDOwner() == lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner()");
            }
            else if (luEntityTypeA == 1u && luEntityTypeB == 2u)
            {
                // racecar<->traffic: owner-byte equality. Lines :1166/:1167.
                CGS_ASSERT((luRawIdAHi >> 24) == (luPotIdAHi >> 24),
                           "lRawContact.mIDA.GetEntityIDOwner() == lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner()");
                CGS_ASSERT((luRawIdBHi >> 24) == (luPotIdBHi >> 24),
                           "lRawContact.mIDB.GetEntityIDOwner() == lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner()");
            }
            // (all other pairs fall through with no id check)
        }

        if (!lbStore)
        {
            return;   // the fixup dropped the contact (dead slot)
        }

        // Store the contact, then its A/B-swapped reciprocal -- ONE straight-line tail for every
        // path that reaches here (asm 0x825AB87C..0x825AB958; the swap pair is the inlined
        // SwapEntityOrder of each record).
        StoreContact(&lContactSpy, &lPotentialContact, lpPropRaceCarContactBuffer);

        lContactSpy.SwapEntityOrder();
        lPotentialContact.SwapEntityOrder();

        StoreContact(&lContactSpy, &lPotentialContact, lpPropRaceCarContactBuffer);
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::StoreContact  @0x825A5DB0   (TU line :1206)
    //
    // Store one resolved contact into mContactData, dispatched on the raw contact's A-side entity
    // owner (an 11-case jump table on owner-1):
    //   RACECAR(1) / PROP_COLLISION_RACECAR(11)  -> BaseContact::Construct + AddContact(RaceCarContact)
    //   TRAFFIC_VEHICLE(2)                       -> BaseContact::Construct + AddContact(TrafficContact)
    //   PROP(3)                                  -> prefill + PropManager::CreateContactEvent +
    //                                               the prop/racecar-vs-traffic B-id patch +
    //                                               AddContact(PropContact)
    //   RACECAR/TRAFFIC_DEFORMABLE_PART(6,7)     -> Construct + CreateDetachedPartContactEvent +
    //                                               AddContact(PhysicalCarPartContact)
    //   DETACHED_*_WHEEL(9,10)                   -> Construct + CreateDetachedWheelContactEvent +
    //                                               AddContact(PhysicalCarPartContact)
    //   everything else (0,4,5,8,12+)            -> dropped (jump-table default; the console's own
    //                                               choice, not a reconstruction gap)
    // =================================================================================================
    void PhysicsModule::StoreContact(
        const OutContactSpy* lpRawContact,
        const PotentialContact* lpPotentialContact,
        const Props::PropRaceCarContactBuffer* /*lpPropRaceCarContactBuffer*/ )
    {
        CGS_ASSERT(lpRawContact != nullptr, "lpRawContact != NULL");               // :1208
        CGS_ASSERT(lpPotentialContact != nullptr, "lpPotentialContact != NULL");   // :1209

        switch (GetIdOwner(lpRawContact->mIDA))
        {
            case 1u:    // E_ENTITYTYPE_RACECAR
            case 11u:   // E_ENTITYTYPE_PROP_COLLISION_RACECAR
            {
                ContactSpy::RaceCarContact lContact;
                ContactSpy::BaseContact::Construct(&lContact,
                                                   &lpRawContact->mFrictionStress,
                                                   lpPotentialContact);
                mContactData.AddContact(lContact);
                break;
            }

            case 2u:    // E_ENTITYTYPE_TRAFFIC_VEHICLE
            {
                ContactSpy::TrafficContact lContact;
                ContactSpy::BaseContact::Construct(&lContact,
                                                   &lpRawContact->mFrictionStress,
                                                   lpPotentialContact);
                mContactData.AddContact(lContact);
                break;
            }

            case 3u:    // E_ENTITYTYPE_PROP
            {
                // The X360 prefills the local before CreateContactEvent: zero entity words, the
                // 0xFFFF8000 collision-tag sentinel, and five zeroed vectors (vspltisw 0 splats).
                ContactSpy::PropContact lContact;
                lContact.mEntityIdA.muValue     = 0;
                lContact.mEntityIdB.muValue     = 0;
                lContact.mCollisionTagB.muValue = 0xFFFF8000u;
                lContact.mFrictionStress.SetZero();   // the five vspltisw-0 stvx splats
                lContact.mNormalStress.SetZero();
                lContact.mNormal.SetZero();
                lContact.mPointOnA.SetZero();
                lContact.mPointOnB.SetZero();

                mPropManager.CreateContactEvent(&lContact, lpRawContact, lpPotentialContact);

                // The prop-vs-vehicle B-id patch: when the raw B id says PROP_COLLISION_RACECAR
                // (11) and the potential contact's B owner is RACECAR (1) -- or 12/TRAFFIC (2) --
                // the event's B entity word becomes the potential contact's real B entity word.
                const u32 luRawOwnerB = GetIdOwner(lpRawContact->mIDB);
                const u32 luPotIdBHi  = static_cast<u32>(lpPotentialContact->muVolumeInstanceIdB.muId >> 32);
                const u32 luPotOwnerB = luPotIdBHi >> 24;
                if ((luRawOwnerB == 11u && luPotOwnerB == 1u) ||
                    (luRawOwnerB == 12u && luPotOwnerB == 2u))
                {
                    lContact.mEntityIdB.muValue = luPotIdBHi;
                }

                mContactData.AddContact(lContact);
                break;
            }

            case 6u:    // E_ENTITYTYPE_RACECAR_DEFORMABLE_PART
            case 7u:    // E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART
            {
                ContactSpy::PhysicalCarPartContact lContact;
                ContactSpy::BaseContact::Construct(&lContact,
                                                   &lpRawContact->mFrictionStress,
                                                   lpPotentialContact);
                mDeformationManager.CreateDetachedPartContactEvent(&lContact, lpRawContact, lpPotentialContact);
                mContactData.AddContact(lContact);
                break;
            }

            case 9u:    // E_ENTITYTYPE_DETACHED_RACECAR_WHEEL
            case 10u:   // E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL
            {
                ContactSpy::PhysicalCarPartContact lContact;
                ContactSpy::BaseContact::Construct(&lContact,
                                                   &lpRawContact->mFrictionStress,
                                                   lpPotentialContact);
                mDeformationManager.CreateDetachedWheelContactEvent(&lContact, lpRawContact, lpPotentialContact);
                mContactData.AddContact(lContact);
                break;
            }

            default:
                // Owners 0 (world), 4 (trigger), 5 (world graphics), 8 (racecar wheel) and >= 12:
                // the console's jump-table default -- no contact is stored.
                break;
        }
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::ValidateSimulationContacts  @0x825A1368   (TU line :604)
    //
    // Validate each queued simulation contact's entity-type pair, then hand the queue to the
    // vehicle manager for its own validation pass.
    // =================================================================================================
    void PhysicsModule::ValidateSimulationContacts(
        const CgsPhysics::PhysicsSimulationIO::InputBuffer::InAddContactQueue* lpContactQueue )
    {
        CGS_ASSERT(lpContactQueue != nullptr, "lpContactQueue != NULL");   // :608

        for (s32 liIndex = 0; liIndex < lpContactQueue->GetLength(); ++liIndex)
        {
            // The X360 copies the 80-byte event to the stack (ctr = 10) before reading the ids.
            const InAddPotentialContact lContact = lpContactQueue->GetEvent(liIndex);

            ValidateSimulationContactTypes(
                static_cast<BrnWorld::EEntityTypeID>(GetIdOwner(lContact.mIDA)),
                static_cast<BrnWorld::EEntityTypeID>(GetIdOwner(lContact.mIDB)));
        }

        mVehicleManager.ValidateSimulationContacts(lpContactQueue);
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::ValidateSimulationContactTypes  @0x8259C3F8   (TU line :649)
    //
    // The per-pair entity-type legality matrix: a 13-case switch on type A, each case asserting
    // type B against its legal set. The console builds every message dynamically ("... with entity
    // B of type %d") through the gpcMessageBuffer / StrStream machinery; lowered to CGS_ASSERT with
    // the static prefix per the standing project rule. Fire-and-continue diagnostics throughout.
    // Legal sets are branch-for-branch from the asm (each case's line number cited).
    // =================================================================================================
    void PhysicsModule::ValidateSimulationContactTypes( BrnWorld::EEntityTypeID leEntityTypeA,
                                                        BrnWorld::EEntityTypeID leEntityTypeB )
    {
        const u32 luB = static_cast<u32>(leEntityTypeB);

        switch (static_cast<u32>(leEntityTypeA))
        {
            case 0u:   // WORLD: B in {2,3,6,7,8,9,10}                                    :662
                CGS_ASSERT(luB == 3u || luB == 2u || luB == 6u || luB == 7u || luB == 8u || luB == 9u || luB == 10u,
                           "Bridging invalid WORLD contact to simulation with entity B of type ");
                break;

            case 1u:   // RACECAR: B in {2,3}                                             :670
                CGS_ASSERT(luB == 3u || luB == 2u,
                           "Bridging invalid RACECAR contact to simulation with entity B of type ");
                break;

            case 2u:   // TRAFFIC_VEHICLE: B in {0,1,2}                                   :679
                CGS_ASSERT(luB <= 2u,
                           "Bridging invalid TRAFFIC_VEHICLE contact to simulation with entity B of type ");
                break;

            case 3u:   // PROP: B in {0,1,3,6,7,8,9,10,11,12}                             :695
                CGS_ASSERT(luB == 0u || luB == 3u || luB == 6u || luB == 7u || luB == 8u || luB == 9u ||
                           luB == 10u || luB == 11u || luB == 1u || luB == 12u,
                           "Bridging invalid PROP contact to simulation with entity B of type ");
                break;

            case 4u:   // TRIGGER / WORLD_GRAPHICS: never legal                           :702
            case 5u:
                CGS_ASSERT(false,
                           "Bridging trigger or world graphics contact to simulation with entity B of type ");
                break;

            case 6u:   // RACECAR_DEFORMABLE_PART: B in {0,3,11,12}                       :712
                CGS_ASSERT(luB == 0u || luB == 11u || luB == 12u || luB == 3u,
                           "Bridging invalid RACECAR_DEFORMABLE_PART contact to simulation with entity B of type ");
                break;

            case 7u:   // TRAFFIC_DEFORMABLE_PART: B in {0,3,11,12}                       :722
                CGS_ASSERT(luB == 0u || luB == 11u || luB == 12u || luB == 3u,
                           "Bridging invalid TRAFFIC_DEFORMABLE_PART contact to simulation with entity B of type ");
                break;

            case 8u:   // RACECAR_WHEEL: B in {0,3,11,12}                                 :732
                CGS_ASSERT(luB == 0u || luB == 11u || luB == 12u || luB == 3u,
                           "Bridging invalid RACECAR_WHEEL contact to simulation with entity B of type ");
                break;

            case 9u:   // DETACHED_*_WHEEL: B in {0,3,11,12}                              :743
            case 10u:
                CGS_ASSERT(luB == 0u || luB == 11u || luB == 12u || luB == 3u,
                           "Bridging invalid DETACHED_WHEEL contact to simulation with entity B of type ");
                break;

            case 11u:  // PROP_COLLISION_RACECAR: B in {3,6,7,8,9,10}                     :755
                CGS_ASSERT(luB == 3u || luB == 6u || luB == 7u || luB == 8u || luB == 9u || luB == 10u,
                           "Bridging invalid PROP_COLLISION_RACECAR contact to simulation with entity B of type ");
                break;

            case 12u:  // PROP_COLLISION_TRAFFIC: B in {3,6,7,8,9,10}                     :767
                CGS_ASSERT(luB == 3u || luB == 6u || luB == 7u || luB == 8u || luB == 9u || luB == 10u,
                           "Bridging invalid PROP_COLLISION_TRAFFIC contact to simulation with entity B of type ");
                break;

            default:   // out-of-range A                                                  :773
                CGS_ASSERT(false, "Invalid entity type: ");
                break;
        }
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::BridgeVehicleManagerRequestsToSimulation  @0x825AB968  (TU line :1313)
    //
    // Forward the vehicle manager's per-frame joint requests into the simulation input buffer.
    // Only the remove-joint queue is bridged here; the add-joint queue must already be empty.
    // =================================================================================================
    void PhysicsModule::BridgeVehicleManagerRequestsToSimulation(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
        const Vehicle::VehicleOutputRequestInterface* lpRequestInterface )
    {
        CGS_ASSERT(lpSimModuleInputBuffer != nullptr, "lpSimModuleInputBuffer != NULL");   // :1315
        CGS_ASSERT(lpRequestInterface != nullptr, "lpRequestInterface != NULL");           // :1316

        CGS_ASSERT(lpRequestInterface->GetAddJointQueue()->GetLength() == 0,
                   "lpRequestInterface->GetAddJointQueue()->GetLength() == 0");            // :1322

        // AppendRemoveJointQueue<10> @0x825A8678 (the header template instantiation).
        lpSimModuleInputBuffer->AppendRemoveJointQueue<10>(lpRequestInterface->GetRemoveJointQueue());
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::CheckContactQueueSize  @0x825A1100   (TU line :552)
    //
    // Diagnostics-only. Fires ONLY when the simulation contact queue is full (miLength ==
    // miMaxLength): tally every queued contact's two entity-type owners into a per-owner
    // histogram, dump the histogram to the debug log, then assert "Contact Queue is full".
    // No-op when the queue is not full. (One parameter per the DWARF; the old facade's third
    // `luOwnerId` argument was an IDA register artifact.)
    // =================================================================================================
    void PhysicsModule::CheckContactQueueSize(
        const CgsPhysics::PhysicsSimulationIO::InputBuffer::InAddContactQueue* lpContactQueue )
    {
        if (lpContactQueue->GetLength() != lpContactQueue->GetMaxLength())
        {
            return;   // queue not full -- nothing to report
        }

        // Per-entity-type-owner contact histogram (each contact feeds two owner buckets).
        s32 laOwnerCounts[KI_X360_ENTITYTYPE_HISTOGRAM_WIDTH] = { 0 };

        for (s32 liIndex = 0; liIndex < lpContactQueue->GetLength(); ++liIndex)
        {
            const InAddPotentialContact& lrContactEvent = lpContactQueue->GetEvent(liIndex);

            // ⚠️ Host bounds guard on the increments (the console's rlwinm masks the bucket to
            // (owner & 0xFF)*4 and writes whatever stack byte that hits when the tripwire above
            // has already fired; the host skips the out-of-bounds write instead -- diagnostics
            // only, nothing live changes).
            const u32 luOwnerA = GetIdOwner(lrContactEvent.mIDA);
            CGS_ASSERT(luOwnerA < static_cast<u32>(KI_X360_ENTITYTYPE_HISTOGRAM_WIDTH),
                       "lpContactEvent->mIDA.GetEntityIDOwner() < BrnWorld::E_ENTITYTYPE_COUNT");   // :570
            if (luOwnerA < static_cast<u32>(KI_X360_ENTITYTYPE_HISTOGRAM_WIDTH))
            {
                ++laOwnerCounts[luOwnerA];
            }

            const u32 luOwnerB = GetIdOwner(lrContactEvent.mIDB);
            CGS_ASSERT(luOwnerB < static_cast<u32>(KI_X360_ENTITYTYPE_HISTOGRAM_WIDTH),
                       "lpContactEvent->mIDB.GetEntityIDOwner() < BrnWorld::E_ENTITYTYPE_COUNT");   // :573
            if (luOwnerB < static_cast<u32>(KI_X360_ENTITYTYPE_HISTOGRAM_WIDTH))
            {
                ++laOwnerCounts[luOwnerB];
            }
        }

        for (s32 liOwner = 0; liOwner < KI_X360_ENTITYTYPE_HISTOGRAM_WIDTH; ++liOwner)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Owner id: " << liOwner
                                           << " Contact count: " << laOwnerCounts[liOwner] << "\n";
            }
        }

        CGS_ASSERT(false, "Contact Queue is full\n");   // :583
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::BridgeSimulationToOutput  @0x825B0448   (TU line :832)
    //
    // End-of-frame bridge: resolve every contact spy the simulation produced into mContactData
    // (via ProcessContactSpies), drain the vehicle manager's discarded-contact queue into that
    // data's discarded-contact queue, then publish mContactData through the output buffer's
    // contact-spy interface. The VecFloat time step rides through unchanged (v127).
    // =================================================================================================
    void PhysicsModule::BridgeSimulationToOutput(
        PhysicsModuleIO::OutputBuffer* lpOutputBuffer,
        const PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface,
        const Props::PropRaceCarContactBuffer* lpPropRaceCarContactBuffer,
        const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimModuleOutputBuffer,
        VecFloat lvfTimeStep )
    {
        CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer != NULL");                             // :834
        CGS_ASSERT(lpPotentialContactsInterface != nullptr, "lpPotentialContactsInterface != NULL"); // :835
        CGS_ASSERT(lpSimModuleOutputBuffer != nullptr, "lpSimModuleOutputBuffer != NULL");           // :836

        // The output buffer's contact-spy interface must be empty (or unbound) before we
        // repopulate it (ContactSpyInterface::IsEmpty inlined at 0x825B04EC..0x825B0514).
        CGS_ASSERT(lpOutputBuffer->GetContactSpyInterface()->IsEmpty(),
                   "lpOutputBuffer->GetContactSpyInterface()->IsEmpty()");                           // :838

        // Resolve all raw contact spies into the typed contact queues (const spy-queue accessor
        // @0x8259F078).
        ProcessContactSpies(lpSimModuleOutputBuffer->GetContactSpyQueue(),
                            lpPotentialContactsInterface,
                            lpPropRaceCarContactBuffer,
                            lvfTimeStep);

        // Drain the vehicle manager's discarded-contact queue (mVehicleManager.mDiscardedContacts,
        // the module's +0x2BE40 seat) into mContactData's discarded queue (+0x193C0 seat). The
        // AddContact overload carries the full-queue warning (inlined on the console).
        const ContactSpy::ContactSpyData::DiscardedContactQueue* lpSource = mVehicleManager.GetDiscardedContacts();
        for (s32 liIndex = 0; liIndex < lpSource->GetLength(); ++liIndex)
        {
            mContactData.AddContact(lpSource->GetEvent(liIndex));
        }

        // Publish: bind the output buffer's contact-spy interface to mContactData
        // (SetData carries the "lpData != NULL" tripwire, BrnContactSpyInterface.h:264).
        lpOutputBuffer->GetContactSpyInterface()->SetData(&mContactData);
    }
}
