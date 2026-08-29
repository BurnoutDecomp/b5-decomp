#include "GameSource/Physics/BrnPhysicsModule.h"

#include <cstdlib>                                                            // getenv (the [spy-owners] BRN_SHOWTIME_WATCH histogram)
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                    // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"      // sim IO buffers + OutContactSpy / InAddPotentialContact
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // CgsSceneManager::SceneManagerIO::PotentialContact (+SwapEntityOrder)
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                            // PhysicsModuleIO::OutputBuffer (GetContactSpyInterface)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"  // PhysicsModuleIO::PotentialContactInterface (GetEvent(ContactId))
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerIO.h"            // Vehicle::VehicleManagerOutputBuffer (the two _PostPhysics/ToOutput bridges, 2026-08-09)
#include "GameSource/Physics/ContactSpies/BrnContactId.h"                     // BrnPhysics::ContactId
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"                // ContactSpyData (typed queues + run lists + AddContact)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"              // BaseContact/RaceCarContact/... event records
#include "GameSource/Physics/ContactSpies/BrnContactSpyQueue.h"               // ContactSpyQueue<T,N>::SortAndCreateRunList<K> (decl)
#include "GameSource/Physics/ContactSpies/BrnContactSpyRunList.h"             // ContactSpyRunList<N>
#include "GameSource/World/BrnEntityTypes.h"                                  // BrnWorld::EEntityTypeID enumerators
#include "rw/math/vpu/vector3_operation.h"                                    // Normalize / Magnitude / IsValid (BridgeContactsToSimulation)

// =====================================================================================================
// BrnPhysics::PhysicsModule -- contact-spy bridge slice (X360 ARTIST build).
//
// Home: GameSource/Physics/BrnPhysicsModuleBridgeFunctions.cpp (DWARF-attested TU for the private
// BridgeContactsToSimulation / ProcessContactSpies / ProcessContactSpy / StoreContact /
// ValidateSimulationContacts driver methods of BrnPhysics::PhysicsModule; each body below carries
// its DWARF TU line).
//
// DE-FACADED 2026-08-06. This TU's previous revision reconstructed six of these bodies as RAW
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
// TWO FACADE MISREADINGS CORRECTED AGAINST THE ASM (dossier bridge_dossier.txt, re-read):
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
        // (`li r9, 0x23` loop bound + the two `cmplwi ..., 0x23` bound asserts). FLAGGED
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

        // [DIAG] NOT IN THE X360 BINARY. THE OWNER HISTOGRAM -- the witness that tells hop zero
        // apart from a downstream one. GameStateModule::ProcessContacts measured, over 7,801 free-
        // burn frames in which the player provably rammed four traffic cars ([T4-hit] slots 0/3/5,
        // outcomes SLAMMED/CHECKED/CRASHING, crasherOwner=1), props max=22 on 1,723 frames and
        // TRAFFIC max=0 on ZERO frames. StoreContact's `case 2` is the only writer of
        // mTrafficContactQueue and it dispatches on exactly `luEntityTypeA` below -- so either the
        // simulation never hands this function a spy whose A- or B-side owner is
        // E_ENTITYTYPE_TRAFFIC_VEHICLE(2), or it does and the store is lost afterwards. Those are
        // completely different defects in different subsystems and NOTHING measured so far can
        // tell them apart. This counts every (ownerA, ownerB) pair the simulation actually emits.
        // ⚠️ It counts BEFORE every guard and every early-out, for the same reason the
        // [contact-entry] witness accumulates before the mode gates: a contact queue is per-frame
        // and a gated count measures the gate. [[diagnostics-that-lie]]
        // Bounded: the table is 36x36 u16 counters and at most 30 lines are ever printed.
        //
        // ⭐⭐ WHAT IT MEASURED, 2026-08-29, free burn, the 69d654ef rear-end-ram recipe
        // (-Drive -Teleport "3390.2,0.2,-1620.0,182"), first 2,500 spies:
        //     [spy-owners] spies=2500 pairs: 3x0=343 6x0=587 7x0=1566 11x3=4
        // i.e. PROPxWORLD, RACECAR_DEFORMABLE_PARTxWORLD, TRAFFIC_DEFORMABLE_PARTxWORLD and
        // PROP_COLLISION_RACECARxPROP -- and NOTHING ELSE. In the same run [T4-hit] recorded three
        // separate traffic cars struck at 16.5 and 22.6 m/s with crasherOwner=1 == RACECAR.
        // ⛔ SO OWNER 2 (TRAFFIC_VEHICLE) AND OWNER 1 (RACECAR) NEVER REACH THIS FUNCTION AT ALL.
        // Traffic *parts* reach the spy 1,566 times; traffic *vehicles* never do. The loss is
        // therefore UPSTREAM of StoreContact, not inside it -- which is a different subsystem from
        // the one 954a2ac0's hop-zero note pointed at.
        // ⭐ THE STATIC LEAD, so the next wave does not re-derive it: race-car-vs-traffic potential
        // contacts do not go down BridgeContactsToSimulation's path 1 (the merged external queue,
        // the only producer of InAddPotentialContact in this tree) -- the console's own tripwires
        // there assert AGAINST that pair, :100 `!(A == RACECAR && B == TRAFFIC_VEHICLE)`, with
        // :94/:97/:103 covering world-traffic and traffic-traffic. They go down path 7, custom
        // queue [8], into DeformationManager::ReadPotentialContact -- whose lpSimInput parameter is
        // COMMENTED OUT on this build (BrnDeformationManager_ContactBridges.cpp:262). Start there.
        // Evidence page: https://claude.ai/code/artifact/ebe1c741-6e40-4a0b-95ba-9c4fe61d42ca
        // DELETE-WHEN the traffic contact queue is proven to fill.
        {
            static const bool sbWatch = (getenv("BRN_SHOWTIME_WATCH") != 0);
            static u32        suSpies = 0;
            static s32        siLines = 0;
            static u16        sauPairs[36][36] = { { 0 } };

            if (sbWatch)
            {
                const u32 luA = (luEntityTypeA < 36u) ? luEntityTypeA : 35u;
                const u32 luB = (luEntityTypeB < 36u) ? luEntityTypeB : 35u;
                if (sauPairs[luA][luB] < 0xFFFFu) { ++sauPairs[luA][luB]; }
                ++suSpies;

                // ⚠️ 500, not 4000, and the FIRST spy always prints. A 200 s showtime run whose
                // crash landed in an empty street produced 10 prop frames total and never reached
                // a 4000-spy threshold at all -- so the histogram printed NOTHING, which reads
                // identically to "the probe is broken". A witness whose period exceeds the
                // event rate measures the witness. [[diagnostics-that-lie]]
                if (CgsDev::Log::gpDebugPrint != 0 && siLines < 30 &&
                    (suSpies == 1u || (suSpies % 500u) == 0u))
                {
                    ++siLines;
                    *CgsDev::Log::gpDebugPrint << "[spy-owners] spies=" << static_cast<s32>(suSpies) << " pairs:";
                    for (u32 luI = 0; luI < 36u; ++luI)
                    {
                        for (u32 luJ = 0; luJ < 36u; ++luJ)
                        {
                            if (sauPairs[luI][luJ] != 0)
                            {
                                *CgsDev::Log::gpDebugPrint
                                    << " " << static_cast<s32>(luI) << "x" << static_cast<s32>(luJ)
                                    << "=" << static_cast<s32>(sauPairs[luI][luJ]);
                            }
                        }
                    }
                    *CgsDev::Log::gpDebugPrint << "\n";
                }
            }
        }

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

            // Host bounds guard on the increments (the console's rlwinm masks the bucket to
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

    namespace
    {
        // The console's per-lane NaN self-compares (vcmpeqfp v,v): a lane is valid iff it equals
        // itself. IsValid(Vector3) is the vendor vpu tree's own three-lane self-compare.
        inline bool IsValidLane(f32 lfValue) { return lfValue == lfValue; }
        inline bool IsValidVec3Lanes(const Vector3& lrV) { return rw::math::vpu::IsValid(lrV); }

        // The console's renormalisation length check (loops [7]/[13]/[8] of the bridge driver):
        // |mNormal| computed by vmsum3fp128 + two-step Newton-Raphson vrsqrtefp (+ the vsel
        // zero-guard the committed Magnitude reproduces), then | |n| - 1 | > 0.01 fires the
        // "Un-normalised ..." assert. Diagnostics only -- no store back.
        inline bool IsUnitLength(const Vector3& lrV)
        {
            const f32 lfDelta = rw::math::vpu::Magnitude(lrV) - 1.0f;
            return (lfDelta < 0.0f ? -lfDelta : lfDelta) <= 0.0099999998f;
        }
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::BridgeContactsToSimulation  @0x825A99E8   (TU lines :49..:463)
    //
    // The producer bridge of PhysicsModule::Update: turn this frame's potential contacts into
    // simulation add-contact events, then drain the five typed custom queues into the deformation
    // sensors. Reconstructed from the BURNOUT_X360_ARTIST.XEX asm with the PS3 DecFIGS out-of-line
    // build @0x6999C0 as the structural oracle (it keeps every accessor the X360 inlines
    // out-of-line, with the debug parameter names used below).
    //
    //   1) merged external contacts (GetLength(): mpQueue + custom queue [0]): entity-pair
    //      tripwires (:91..:103, :139/:140), InAddPotentialContact build (muTag = event index;
    //      frictions 0.4/0.5/0.5; WORLD ids -> mWorldRigidBodyId, deformation-local owners
    //      {6,7,9,10} keep the full packed id, every other owner keeps only its entity word),
    //      the PROP gate (owner 3 on either side: part/wheel frozen-flag lookup +
    //      PropManager::SetupAndValidatePropContact, false == drop), the per-material friction
    //      overrides ({9,10} -> 0.8/0.8/0.2, {6,7} -> 0.4/0.6/0.05), the IsValid tripwires
    //      (:274..:278), CheckContactQueueSize, then AddEventSafe into the sim queue.
    //   2) custom queue [6] (vehicle-world): normalize the COPY's mNormal in place, then
    //      ReadPotentialVehicleWorldContact with ContactId (i | 0x6000000).
    //   3) custom queue [9] (traffic-world): owner tripwires :334/:335, the
    //      VehicleManager::ValidateTrafficContact gate, then normalize + Read... (i | 0x9000000).
    //   4) the two simple-traffic bridges (world then car).
    //   5) custom queue [7] (racecar-racecar): unit-length tripwire (:384), ReadPotentialContact
    //      (i | 0x7000000).
    //   6) custom queue [13] (traffic-traffic): owner tripwires :410/:411, the
    //      PhysicalTrafficManager::ValidateAndFixUpTrafficTrafficContact gate (global->physical
    //      rewrite inside), unit-length tripwire (:422), ReadPotentialContact (i | 0xD000000).
    //   7) custom queue [8] (racecar-traffic): owner tripwires :447/:448, the traffic B id
    //      rewritten global->physical (GetPhysicsEntityIDFromGlobalEntityID + the high-dword
    //      splice, `sldi 32 ; clrldi 32 ; or` @0x825AB0F0), unit-length tripwire (:463),
    //      ReadPotentialContact (i | 0x8000000).
    //   8) DeformationManager::BridgeBodyPartCarContactsToSimulation +
    //      BridgeDetachedWheelCarContactsToSimulation, then ValidateSimulationContacts.
    //
    // The per-queue ContactId owner byte EQUALS the walked queue's index everywhere ([6]/[7]/
    // [8]/[9]/[13]) -- five independent confirmations of the accessor/index binding.
    // =================================================================================================
    void PhysicsModule::BridgeContactsToSimulation(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
        const PhysicsModuleIO::InputBuffer* lpInputBuffer,
        PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
        Props::PropRaceCarContactBuffer* lpPropRaceCarContactBuffer )
    {
        typedef PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue Queue;

        CGS_ASSERT(lpSimModuleInputBuffer != nullptr, "lpSimModuleInputBuffer != NULL");   // :49
        CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer != NULL");                     // :50

        // Write-locked GetTimeStep (the accessor's own "Not locked for writing" +
        // "mfTimeStep > 0.0f" tripwires, CgsPhysicsSimulationModuleIO.h:848/:849).
        const f32 lfTimeStep = lpSimModuleInputBuffer->GetTimeStep();

        // The vehicle-input tri-cache (GetVehicleInputInterface @0x8259F8A0 + the inlined
        // mTriangleCacheInterface seat -- X360 +128016 into the storage, PS3 buffer+128384).
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface =
            lpInputBuffer->GetVehicleInputInterface()->GetTriangleCacheInterface();

        const s32 liNumPotentialContacts = lpContactInterface->GetLength();

        // The opening `stw 0` at module+179760 == mVehicleManager.miNonPhysicalContactCount.
        mVehicleManager.ResetNonPhysicalContacts();

        // ---- (1) merged external potential contacts -> sim add-contact events -----------------
        for (s32 liEventIndex = 0; liEventIndex < liNumPotentialContacts; ++liEventIndex)
        {
            // 76-byte stack copy (the console's ctr=10 qword copy / PS3 sub_4C1E88 memcpy).
            const PotentialContact lContact = lpContactInterface->GetEvent(liEventIndex);

            // Inlined ContactId construction tripwire (BrnContactId.h:122).
            CGS_ASSERT(liEventIndex >= 0 && liEventIndex < 65535,
                       "liEventIndex >= 0 && liEventIndex < 65535");                        // BrnContactId.h:122

            const u64 luIdA    = lContact.muVolumeInstanceIdA.muId;
            const u64 luIdB    = lContact.muVolumeInstanceIdB.muId;
            const u32 luOwnerA = GetIdOwner(luIdA);
            const u32 luOwnerB = GetIdOwner(luIdB);

            // Pair-legality tripwires (fire-and-continue diagnostics).
            CGS_ASSERT(!(luOwnerA == 1u && luOwnerB == 1u),
                       "!(lVolumeInstanceIDA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && "
                       "lVolumeInstanceIDB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR)");           // :91
            CGS_ASSERT(!(luOwnerA == 0u && luOwnerB == 2u),
                       "!(lVolumeInstanceIDA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_WORLD && "
                       "lVolumeInstanceIDB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)");   // :94
            CGS_ASSERT(!(luOwnerA == 2u && luOwnerB == 0u),
                       "!(lVolumeInstanceIDA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE && "
                       "lVolumeInstanceIDB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_WORLD)");             // :97
            CGS_ASSERT(!(luOwnerA == 1u && luOwnerB == 2u),
                       "!(lVolumeInstanceIDA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && "
                       "lVolumeInstanceIDB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)");   // :100
            CGS_ASSERT(!(luOwnerA == 2u && luOwnerB == 2u),
                       "!(lVolumeInstanceIDA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE && "
                       "lVolumeInstanceIDB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)");   // :103
            CGS_ASSERT(luOwnerA != 8u,
                       "lVolumeInstanceIDA.GetEntityIDOwner() != BrnWorld::E_ENTITYTYPE_RACECAR_WHEEL");      // :139
            CGS_ASSERT(luOwnerB != 8u,
                       "lVolumeInstanceIDB.GetEntityIDOwner() != BrnWorld::E_ENTITYTYPE_RACECAR_WHEEL");      // :140

            InAddPotentialContact lAddContactEvent;
            lAddContactEvent.muTag            = static_cast<u32>(liEventIndex);
            lAddContactEvent.mRestitution     = 0.5f;
            lAddContactEvent.mDynamicFriction = 0.40000001f;
            lAddContactEvent.mStaticFriction  = 0.5f;

            // Rigid-body id resolution: WORLD -> the world body; the deformation-local owners
            // ({6,7,9,10}: parts / detached wheels) keep the full packed id; every other owner
            // keeps only its entity word (`sldi hi,32` @0x825A9DA8/@0x825A9DE4).
            if (luOwnerA == 0u)
            {
                lAddContactEvent.mIDA = mWorldRigidBodyId;
            }
            else if (luOwnerA == 6u || luOwnerA == 7u || luOwnerA == 9u || luOwnerA == 10u)
            {
                lAddContactEvent.mIDA = luIdA;
            }
            else
            {
                lAddContactEvent.mIDA = (luIdA >> 32) << 32;
            }

            if (luOwnerB == 0u)
            {
                lAddContactEvent.mIDB = mWorldRigidBodyId;
            }
            else if (luOwnerB == 6u || luOwnerB == 7u || luOwnerB == 9u || luOwnerB == 10u)
            {
                lAddContactEvent.mIDB = luIdB;
            }
            else
            {
                lAddContactEvent.mIDB = (luIdB >> 32) << 32;
            }

            lAddContactEvent.mNormal   = lContact.mNormal;
            lAddContactEvent.mPointOnA = lContact.mPointOnA;
            lAddContactEvent.mPointOnB = lContact.mPointOnB;

            // ---- the PROP gate (owner 3 on either side) ---------------------------------------
            if (luOwnerA == 3u || luOwnerB == 3u)
            {
                // The deformation-part / detached-wheel frozen flag rides into the prop
                // validation (part+486 / wheel+128 == the mbFrozen bools, read through the
                // pools' own out-of-line accessors @0x825A0758/@0x825A0858/@0x825A0A10/
                // @0x825A0B10).
                bool lbFrozen = false;

                Deformation::DetachedPartManager&  lrPartManager  = mDeformationManager.GetDetachedPartManager();
                Deformation::DetachedWheelManager& lrWheelManager = mDeformationManager.GetDetachedWheelManager();

                if (luOwnerA == 6u)
                {
                    if (lrPartManager.IsPartIndexUsed(static_cast<s32>(luIdA & 0xFFFFu)))
                    {
                        lbFrozen = lrPartManager.GetPartFromIndex(static_cast<u16>(luIdA & 0xFFFFu))->IsFrozen();
                    }
                }
                else if (luOwnerB == 6u)
                {
                    if (lrPartManager.IsPartIndexUsed(static_cast<s32>(luIdB & 0xFFFFu)))
                    {
                        lbFrozen = lrPartManager.GetPartFromIndex(static_cast<u16>(luIdB & 0xFFFFu))->IsFrozen();
                    }
                }

                if (luOwnerA == 9u)
                {
                    if (lrWheelManager.IsSlotUsed(static_cast<u16>(luIdA & 0xFFFFu)))
                    {
                        lbFrozen = lrWheelManager.GetWheel(static_cast<u16>(luIdA & 0xFFFFu))->IsFrozen();
                    }
                }
                else if (luOwnerB == 9u)
                {
                    if (lrWheelManager.IsSlotUsed(static_cast<u16>(luIdB & 0xFFFFu)))
                    {
                        lbFrozen = lrWheelManager.GetWheel(static_cast<u16>(luIdB & 0xFFFFu))->IsFrozen();
                    }
                }

                if (!mPropManager.SetupAndValidatePropContact(&lAddContactEvent,
                                                              &lContact,
                                                              &mVehicleManager,
                                                              lpSimModuleInputBuffer,
                                                              lpPropRaceCarContactBuffer,
                                                              mWorldRigidBodyId,
                                                              lbFrozen,
                                                              lfTimeStep))
                {
                    continue;   // validation dropped the contact
                }
            }

            // ---- per-material friction overrides ----------------------------------------------
            if (luOwnerA == 9u || luOwnerA == 10u || luOwnerB == 9u || luOwnerB == 10u)
            {
                lAddContactEvent.mStaticFriction  = 0.80000001f;
                lAddContactEvent.mDynamicFriction = 0.80000001f;
                lAddContactEvent.mRestitution     = 0.2f;
            }
            if (luOwnerA == 6u || luOwnerA == 7u || luOwnerB == 6u || luOwnerB == 7u)
            {
                lAddContactEvent.mDynamicFriction = 0.40000001f;
                lAddContactEvent.mStaticFriction  = 0.60000002f;
                lAddContactEvent.mRestitution     = 0.050000001f;
            }

            // Validity tripwires (the console's vcmpeqfp self-compares).
            CGS_ASSERT(IsValidLane(lAddContactEvent.mDynamicFriction),
                       "rw::math::IsValid( lAddContactEvent.mDynamicFriction)");           // :274
            CGS_ASSERT(IsValidLane(lAddContactEvent.mStaticFriction),
                       "rw::math::IsValid( lAddContactEvent.mStaticFriction )");           // :275
            CGS_ASSERT(IsValidVec3Lanes(lAddContactEvent.mNormal),
                       "rw::math::IsValid( lAddContactEvent.mNormal)");                    // :276
            CGS_ASSERT(IsValidVec3Lanes(lAddContactEvent.mPointOnA),
                       "rw::math::IsValid( lAddContactEvent.mPointOnA)");                  // :277
            CGS_ASSERT(IsValidVec3Lanes(lAddContactEvent.mPointOnB),
                       "rw::math::IsValid( lAddContactEvent.mPointOnB)");                  // :278

            // Full-queue diagnostics, then the bounds-gated append (the console's separate
            // CheckContactQueueSize call + EventQueue<...,1024>::AddEventSafe @0x8259D308).
            CheckContactQueueSize(lpSimModuleInputBuffer->GetAddContactQueue());
            lpSimModuleInputBuffer->GetAddContactQueue()->AddEventSafe(lAddContactEvent);
        }

        // ---- (2) custom queue [6]: vehicle-world contacts -> the deformation sensors ----------
        {
            const Queue& lrQueue = lpContactInterface->GetRaceCarWithWorldQueueValidated();
            for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
            {
                PotentialContact lContact = lrQueue.GetEvent(liIndex);   // stack copy

                // Normalize the COPY's normal in place (vmsum3fp128 + double Newton-Raphson
                // vrsqrtefp + the vsel zero-guard -- the committed Normalize reproduces all of
                // it); the queue record itself is NOT written back.
                lContact.mNormal = rw::math::vpu::Normalize(lContact.mNormal);

                CGS_ASSERT(liIndex >= 0 && liIndex < 65535,
                           "liEventIndex >= 0 && liEventIndex < 65535");                    // BrnContactId.h:122
                mDeformationManager.ReadPotentialVehicleWorldContact(
                    lContact, ContactId(static_cast<u32>(liIndex) | 0x06000000u), lpSimModuleInputBuffer);
            }
        }

        // ---- (3) custom queue [9]: traffic-world contacts, validated ---------------------------
        {
            const Queue& lrQueue = lpContactInterface->GetTrafficWithWorldQueue();
            for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
            {
                PotentialContact lContact = lrQueue.GetEvent(liIndex);   // stack copy

                CGS_ASSERT(GetIdOwner(lContact.muVolumeInstanceIdA.muId) == 2u,
                           "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE"); // :334
                CGS_ASSERT(GetIdOwner(lContact.muVolumeInstanceIdB.muId) == 0u,
                           "lContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_WORLD");           // :335
                CGS_ASSERT(liIndex >= 0 && liIndex < 65535,
                           "liEventIndex >= 0 && liEventIndex < 65535");                    // BrnContactId.h:122

                if (mVehicleManager.ValidateTrafficContact(&lContact, lpTriangleCacheInterface, lfTimeStep))
                {
                    lContact.mNormal = rw::math::vpu::Normalize(lContact.mNormal);
                    mDeformationManager.ReadPotentialVehicleWorldContact(
                        lContact, ContactId(static_cast<u32>(liIndex) | 0x09000000u), lpSimModuleInputBuffer);
                }
            }
        }

        // ---- (4) the two simple-traffic bridges ------------------------------------------------
        BridgeSimpleTrafficWithWorldContactsToSimulation(lpSimModuleInputBuffer->GetAddContactQueue(),
                                                         lpContactInterface);
        mVehicleManager.BridgeSimpleTrafficWithCarContactsToSimulation(
            lpSimModuleInputBuffer->GetAddContactQueue(), lpContactInterface);

        // ---- (5) custom queue [7]: racecar-racecar ---------------------------------------------
        {
            const Queue& lrQueue = lpContactInterface->GetRaceCarWithRaceCarQueue();
            for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
            {
                const PotentialContact lContact = lrQueue.GetEvent(liIndex);   // stack copy

                CGS_ASSERT(liIndex >= 0 && liIndex < 65535,
                           "liEventIndex >= 0 && liEventIndex < 65535");                    // BrnContactId.h:122
                CGS_ASSERT(IsUnitLength(lContact.mNormal),
                           "Un-normalised race car-race car contact: ");                    // :384
                mDeformationManager.ReadPotentialContact(
                    lContact, ContactId(static_cast<u32>(liIndex) | 0x07000000u), lpSimModuleInputBuffer);
            }
        }

        // ---- (6) custom queue [13]: traffic-traffic, validated + id-rewritten ------------------
        {
            const Queue& lrQueue = lpContactInterface->GetTrafficWithTrafficQueue();
            for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
            {
                PotentialContact lContact = lrQueue.GetEvent(liIndex);   // stack copy

                CGS_ASSERT(GetIdOwner(lContact.muVolumeInstanceIdA.muId) == 2u,
                           "lTrafficTrafficContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE"); // :410
                CGS_ASSERT(GetIdOwner(lContact.muVolumeInstanceIdB.muId) == 2u,
                           "lTrafficTrafficContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE"); // :411

                // The traffic manager's global->physical rewrite of BOTH ids (127 == unmapped
                // drops the contact); the manager mutates the copy in place.
                if (mVehicleManager.GetPhysicalTrafficManager().ValidateAndFixUpTrafficTrafficContact(&lContact))
                {
                    CGS_ASSERT(liIndex >= 0 && liIndex < 65535,
                               "liEventIndex >= 0 && liEventIndex < 65535");                // BrnContactId.h:122
                    CGS_ASSERT(IsUnitLength(lContact.mNormal),
                               "Un-normalised trafficr-traffic car contact: ");             // :422
                    mDeformationManager.ReadPotentialContact(
                        lContact, ContactId(static_cast<u32>(liIndex) | 0x0D000000u), lpSimModuleInputBuffer);
                }
            }
        }

        // ---- (7) custom queue [8]: racecar-traffic, the B id rewritten global->physical --------
        {
            const Queue& lrQueue = lpContactInterface->GetRaceCarWithTrafficQueue();
            for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
            {
                PotentialContact lContact = lrQueue.GetEvent(liIndex);   // stack copy

                CGS_ASSERT(GetIdOwner(lContact.muVolumeInstanceIdA.muId) == 1u,
                           "lRaceCarTrafficContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");         // :447
                CGS_ASSERT(GetIdOwner(lContact.muVolumeInstanceIdB.muId) == 2u,
                           "lRaceCarTrafficContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE"); // :448

                // The traffic B id's entity word becomes its LOCAL PHYSICS id (the X360 inlines
                // GetPhysicsEntityIDFromGlobalEntityID here; the splice is the same
                // `sldi 32 ; clrldi 32 ; or` as the Fixup* rewrites).
                const CgsSceneManager::EntityId lPhysicsIdB =
                    mVehicleManager.GetPhysicsEntityIDFromGlobalEntityID(
                        CgsSceneManager::EntityId(static_cast<u32>(lContact.muVolumeInstanceIdB.muId >> 32)));
                lContact.muVolumeInstanceIdB.muId =
                    (static_cast<u64>(static_cast<u32>(lPhysicsIdB)) << 32)
                    | (lContact.muVolumeInstanceIdB.muId & 0x00000000FFFFFFFFull);

                CGS_ASSERT(liIndex >= 0 && liIndex < 65535,
                           "liEventIndex >= 0 && liEventIndex < 65535");                    // BrnContactId.h:122
                CGS_ASSERT(IsUnitLength(lContact.mNormal),
                           "Un-normalised race car-traffic car contact: ");                 // :463
                mDeformationManager.ReadPotentialContact(
                    lContact, ContactId(static_cast<u32>(liIndex) | 0x08000000u), lpSimModuleInputBuffer);
            }
        }

        // ---- (8) the deformation-part bridges + the validation pass ----------------------------
        mDeformationManager.BridgeBodyPartCarContactsToSimulation(lpSimModuleInputBuffer,
                                                                  lpInputBuffer, lpContactInterface);
        mDeformationManager.BridgeDetachedWheelCarContactsToSimulation(lpSimModuleInputBuffer,
                                                                       lpInputBuffer, lpContactInterface);
        ValidateSimulationContacts(lpSimModuleInputBuffer->GetAddContactQueue());
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::BridgeSimpleTrafficWithWorldContactsToSimulation  @0x825A5618
    //
    // GATE (2026-08-06 big-five #2 wave) -- the REAL body (484 X360 asm lines; PS3 DecFIGS
    // 0x699594, same TU) is NOT reconstructed. Reached every frame from BridgeContactsToSimulation
    // (Update @0x825B0640 is real); the one-shot log below is the gate.
    // SCOPE (wave 4, 2026-08-23): its queue is EMPTY BY CONSTRUCTION. No create path -- console or
    // host -- ever allocates an E_PHYSICAL_TRAFFIC_TYPE_SIMPLE slot (GetFreeTrafficVehicleWithPhysics
    // @0x82637608 has no simple arm), so this gate drops nothing. Not a crash-into-traffic blocker.
    // =================================================================================================
    void PhysicsModule::BridgeSimpleTrafficWithWorldContactsToSimulation(
        CgsPhysics::PhysicsSimulationIO::InputBuffer::InAddContactQueue* /*lpContactQueue*/,
        const PhysicsModuleIO::PotentialContactInterface* /*lpContactInterface*/ )
    {
        // BOOT GATE (conductor wave 2026-08-09): REACHED every frame by
        // BridgeContactsToSimulation now that PhysicsModule::Update is real. Was a
        // CGS_ASSERT(false) trap while the caller chain was dead; a per-frame assert would
        // block the sim, so the deferral is a one-shot log instead -- the simple-traffic
        // world contacts are DROPPED until the real 484-insn body (@0x825A5618, PS3 DecFIGS
        // 0x699594) lands. Reconstruct and DELETE this gate.
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "PhysicsModule::BridgeSimpleTrafficWithWorldContactsToSimulation:"
                                              " inert [FLAG PC boot gate @0x825A5618, 484 insns]\n";
        }
    }

    // =================================================================================================
    // THE THREE REMAINING UPDATE BRIDGES -- landed 2026-08-09 (conductor wave), full bodies.
    // =================================================================================================

    // BrnPhysics::PhysicsModule::BridgeUpdatedVehiclesToSimulation @0x825ADEA8 (45 insns;
    // PS3 DecFIGS 0x691CFC, mangled signature authoritative). Console order kept 1:1:
    // construct a 60-slot InUpdateExternalBody queue on the stack, read (and DROP -- as
    // shipped, the result is dead in r3) the sim time step, push the module input's solver
    // iteration cap into the sim input, harvest the live vehicle bodies, append the queue.
    void PhysicsModule::BridgeUpdatedVehiclesToSimulation(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
        const PhysicsModuleIO::InputBuffer* lpInputBuffer )
    {
        CGS_ASSERT(lpSimModuleInputBuffer != 0, "lpSimModuleInputBuffer != NULL");   // :795
        CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer != NULL");                     // :796

        CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody, 60>
            lUpdatedBodyQueue;
        lUpdatedBodyQueue.Construct();                            // @0x825A8370

        // As shipped: GetTimeStep's result is discarded (the call exists only for its
        // write-lock tripwire).
        (void)lpSimModuleInputBuffer->GetTimeStep();              // @0x8259ECD8

        lpSimModuleInputBuffer->SetMaxIterations(
            static_cast<int>(*lpInputBuffer->GetSolverMaxIterations()) );   // @0x8259EDB0 / @0x8259FD38

        mVehicleManager.GetUpdatedVehicleBodies(&lUpdatedBodyQueue);        // @0x82619340

        lpSimModuleInputBuffer->AppendUpdateExternalBodyQueue<60>(&lUpdatedBodyQueue); // @0x825AC208
    }

    // BrnPhysics::PhysicsModule::BridgeVehicleManagerToSimulation_PostPhysics @0x825ADF60
    // (44 insns). Drain the vehicle manager's three simulation-request queues into the sim
    // input, in the console's order: removes first, then adds, then inertia changes.
    void PhysicsModule::BridgeVehicleManagerToSimulation_PostPhysics(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
        const Vehicle::VehicleManagerOutputBuffer* lpVehManagerOutputBuffer )
    {
        CGS_ASSERT(lpSimModuleInputBuffer != 0, "lpSimModuleInputBuffer != NULL");   // :993
        CGS_ASSERT(lpVehManagerOutputBuffer != 0, "lpVehManagerOutputBuffer != NULL"); // :994

        // FLAG (const seam, deliberate): the console reads the queues through the
        // READ-locked buffer's const request interface (accessor @0x825A0FB0), yet the three
        // sim Append*Queue templates were committed with NON-const source params (their own
        // mangle note). Append only reads the source (BaseEventQueue<T>::Append takes const&),
        // so the const_cast is behaviour-free; reconcile the template params at their own wave.
        Vehicle::VehicleOutputRequestInterface* lpRequests =
            const_cast<Vehicle::VehicleOutputRequestInterface*>(
                lpVehManagerOutputBuffer->GetVehicleOutputRequestInterface());

        lpSimModuleInputBuffer->AppendRemoveRigidBodyQueue<50>(lpRequests->GetRemoveRigidBodyQueue());
        lpSimModuleInputBuffer->AppendAddRigidBodyQueue<50>(lpRequests->GetRequiredRigidBodiesQueue());
        lpSimModuleInputBuffer->AppendChangeRigidBodyInertiaQueue<200>(lpRequests->GetChangeRigidBodyInertiaQueue());
    }

    // =================================================================================================
    // BrnPhysics::PhysicsModule::BridgeVehicleManagerToSimulation_PostScene @0x825AB408 (52 insns)
    // THE SIM FIREWALL -- LANDED 2026-08-11 (prepare-chain wave); its BrnPhysicsConductorGates
    // .cpp boot gate is DELETED in the same commit (LNK2005 is the tripwire if it ever comes back).
    //
    // THE GATE'S PRECONDITION IS SATISFIED, CHECKED RATHER THAN ASSUMED. The gate banner said
    // "DO NOT LAND THIS UNTIL THE TRACTION-LINE CHAIN IS CLOSED -- a body that enters the simulation
    // before the wheels can find the road falls forever." Both halves of that lifetime are now real
    // bodies in BrnVehicleManager_TractionLineTests.cpp: StartVehicleTractionLineTests @0x82629CE0
    // (allocations -> three Add legs -> RunTractionLineTestJobs) and EndVehicleTractionLineTests
    // @0x82633CD8 (WaitOn -> stream End -> ONE result cursor through the three harvests -> release),
    // landed by the 2026-08-11 lifetime wave. Read, not inferred, before writing this body.
    //
    // The console's four appends, in its own order, off the READ-locked request interface. The four
    // source offsets are the ones the gate banner cross-checked against BrnVehicleOutputInterface.h's
    // own queue seats, and each is reached BY NAME here:
    //     AppendRemoveRigidBodyQueue<50>(simIn, vehOut + 9616)   == mRemoveRigidBodyQueue
    //     AppendAddRigidBodyQueue   <50>(simIn, vehOut + 0)      == mRequiredRigidBodiesQueue
    //     AppendAddJointQueue       <10>(simIn, vehOut + 39904)  == mAddJointQueue
    //     AppendRemoveJointQueue    <10>(simIn, vehOut + 41840)  == mRemoveJointQueue
    //
    // FLAG (call-count, behaviour-free): the console re-issues `BrnPhysics::Vehicle::
    // VehicleManagerOut` (@0x825A0FB0 -- the read-lock tripwire that returns `buffer + 16`) ONCE PER
    // APPEND, four times. The already-committed sibling BridgeVehicleManagerToSimulation_PostPhysics
    // @0x825ADF60 has the identical shape (three calls) and hoists it into one local; that precedent
    // is followed here so the two bodies read the same. The accessor has no side effect other than
    // the "Not locked for reading" assert, so the collapse is behaviour-free.
    //
    // FLAG (const seam): identical to the PostPhysics sibling's -- the console reads the queues
    // through the const request interface while two of the four sim Append*Queue templates were
    // committed with NON-const source params. Append only reads the source, so the const_casts are
    // behaviour-free; reconcile the template params at their own wave.
    // =================================================================================================
    void PhysicsModule::BridgeVehicleManagerToSimulation_PostScene(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
        const Vehicle::VehicleManagerOutputBuffer* lpVehManagerOutputBuffer )
    {
        CGS_ASSERT(lpSimModuleInputBuffer != 0, "lpSimModuleInputBuffer != NULL");       // :870
        CGS_ASSERT(lpVehManagerOutputBuffer != 0, "lpVehManagerOutputBuffer != NULL");   // :871

        Vehicle::VehicleOutputRequestInterface* lpRequests =
            const_cast<Vehicle::VehicleOutputRequestInterface*>(
                lpVehManagerOutputBuffer->GetVehicleOutputRequestInterface());

        lpSimModuleInputBuffer->AppendRemoveRigidBodyQueue<50>(lpRequests->GetRemoveRigidBodyQueue());
        lpSimModuleInputBuffer->AppendAddRigidBodyQueue<50>(lpRequests->GetRequiredRigidBodiesQueue());
        lpSimModuleInputBuffer->AppendAddJointQueue<10>(
            const_cast<Vehicle::VehicleOutputRequestInterface::AddArticulatedJointQueue*>(
                lpRequests->GetAddJointQueue()));
        lpSimModuleInputBuffer->AppendRemoveJointQueue<10>(lpRequests->GetRemoveJointQueue());
    }

    // BrnPhysics::PhysicsModule::BridgeVehicleManagerToOutput (PS3 DecFIGS keeps it out of
    // line; the X360 inlines the whole body into Update @0x825B2408..0x825B2510, whose baked
    // asserts cite THIS file's :1025/:1026). Lock both buffers, forward the request
    // interface (VehicleOutputRequestInterface::Append -- five queue appends, the inertia
    // queue deliberately excluded), release in reverse.
    void PhysicsModule::BridgeVehicleManagerToOutput(
        PhysicsModuleIO::OutputBuffer* lpOutputBuffer,
        const Vehicle::VehicleManagerOutputBuffer* lpVehManagerOutputBuffer )
    {
        CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer != NULL");                   // :1025
        CGS_ASSERT(lpVehManagerOutputBuffer != 0, "lpVehManagerOutputBuffer != NULL"); // :1026

        lpOutputBuffer->LockForWrite();
        lpVehManagerOutputBuffer->LockForRead();

        lpOutputBuffer->GetVehicleOutputRequestInterface()->Append(
            lpVehManagerOutputBuffer->GetVehicleOutputRequestInterface());

        lpVehManagerOutputBuffer->UnlockForRead();
        lpOutputBuffer->UnlockForWrite();
    }
}
