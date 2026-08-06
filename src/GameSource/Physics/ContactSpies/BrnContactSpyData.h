#pragma once

// Canonical home for BrnPhysics::ContactSpy::ContactSpyData (DWARF home
// GameSource/Physics/ContactSpies/BrnContactSpyData.h:79).
//
// The per-frame contact aggregate produced by the physics simulation: five typed
// resolved-contact queues (race car / traffic / physical car part / hinged part /
// prop) plus a discarded-contact queue, followed by four per-entity contact run
// lists. Owned BY VALUE via the ContactSpyInterface leading `mpData` pointer.
// Populated by PhysicsModule::StoreContact (via AddContact) and drained by
// PhysicsModule::BridgeSimulationToOutput (which calls IsEmpty).
//
// Layout (X360-attested; every offset cross-validated against the IsEmpty asm
// @0x825A50E0 and ContactSpyInterface::GetRaceCarContactRunList @0x82355BF0 which
// returns &mRaceCarContactRunList at byte offset 0x198D0):
//   mRaceCarContactQueue          @0x00000  ContactSpyQueue<RaceCarContact,300>          (len@0x0008)
//   mTrafficContactQueue          @0x070A0  ContactSpyQueue<TrafficContact,400>          (len@0x070A8)
//   mPhysicalCarPartContactQueue  @0x106C0  ContactSpyQueue<PhysicalCarPartContact,150>  (len@0x106C8)
//   mHingedPartContactQueue       @0x151E0  ContactSpyQueue<HingedPartContact,50>        (len@0x151E8)
//   mPropContactQueue             @0x167E0  ContactSpyQueue<PropContact,100>             (len@0x167E8)
//   mDiscardedContactQueue        @0x193C0  EventQueue<DiscardedContact,20>              (len@0x193C8)
//   mRaceCarContactRunList        @0x198D0  ContactSpyRunList<8>                         (len@0x198D8)
//   mTrafficContactRunList        @0x19B70  ContactSpyRunList<64>                        (len@0x19B78)
//   mPhysicalCarPartContactRunList@0x1AF90  ContactSpyRunList<50>                        (len@0x1AF98)
//   mPropContactRunList           @0x1BF50  ContactSpyRunList<100>                       (len@0x1BF58)
//   sizeof(ContactSpyData) == 0x1DEB0.
//
// Member names, typedef names, method shapes and declaration order are DWARF-
// authoritative (references/DecFIGS/dwarfdump/.../BrnContactSpyData.h).
//
// Only the two boot-trace-adjacent methods (AddContact(RaceCarContact) and IsEmpty)
// are DEFINED in-tree (BrnContactSpyData.cpp). The remaining DWARF methods are
// declared for ODR/layout fidelity; each is filled by its own ledger TU.

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"      // RaceCarContact, TrafficContact, PhysicalCarPartContact, HingedPartContact, PropContact, DiscardedContact
#include "GameSource/Physics/ContactSpies/BrnContactSpyQueue.h"       // ContactSpyQueue<T,N>
#include "GameSource/Physics/ContactSpies/BrnContactSpyRunList.h"     // ContactSpyRunList<N>

namespace BrnPhysics
{
    namespace ContactSpy
    {
        // Per-entity-type contact-buffer capacities (DWARF BrnContactSpyData.h:40-45).
        const s32 KI_MAX_RACECAR_CONTACTS     = 300;
        const s32 KI_MAX_TRAFFIC_CONTACTS     = 400;
        const s32 KI_MAX_DEFORMABLE_CONTACTS  = 150;
        const s32 KI_MAX_HINGED_PART_CONTACTS = 50;
        const s32 KI_MAX_PROP_CONTACTS        = 100;
        const s32 KI_MAX_DISCARDED_CONTACTS   = 20;

        // Distinct-colliding-entity run-list capacities (DWARF BrnContactSpyData.h:48-51).
        const s32 KI_MAX_COLLIDING_RACECAR_ENTITIES    = 8;
        const s32 KI_MAX_COLLIDING_TRAFFIC_ENTITIES    = 64;
        const s32 KI_MAX_COLLIDING_DEFORMABLE_ENTITIES = 50;
        const s32 KI_MAX_COLLIDING_PROP_ENTITIES       = 100;

        // DWARF: BrnContactSpyData.h:79 (struct BrnPhysics::ContactSpy::ContactSpyData).
        struct ContactSpyData
        {
            // DWARF typedefs (BrnContactSpyData.h:57-67).
            typedef ContactSpyQueue<RaceCarContact, KI_MAX_RACECAR_CONTACTS>            RaceCarContactQueue;
            typedef ContactSpyQueue<TrafficContact, KI_MAX_TRAFFIC_CONTACTS>            TrafficContactQueue;
            typedef ContactSpyQueue<PhysicalCarPartContact, KI_MAX_DEFORMABLE_CONTACTS> PhysicalCarPartContactQueue;
            typedef ContactSpyQueue<HingedPartContact, KI_MAX_HINGED_PART_CONTACTS>     HingedCarPartContactQueue;
            typedef ContactSpyQueue<PropContact, KI_MAX_PROP_CONTACTS>                  PropContactQueue;
            typedef CgsModule::EventQueue<DiscardedContact, KI_MAX_DISCARDED_CONTACTS>  DiscardedContactQueue;
            typedef ContactSpyRunList<KI_MAX_COLLIDING_RACECAR_ENTITIES>    RaceCarContactRunList;
            typedef ContactSpyRunList<KI_MAX_COLLIDING_TRAFFIC_ENTITIES>    TrafficContactRunList;
            typedef ContactSpyRunList<KI_MAX_COLLIDING_DEFORMABLE_ENTITIES> PhysicalCarPartContactRunList;
            typedef ContactSpyRunList<KI_MAX_COLLIDING_PROP_ENTITIES>       PropContactRunList;

            // ---- Public interface (DWARF BrnContactSpyData.h:84-180) ----
            void Construct();
            void Destruct();
            void Clear();

            // X360 0x825A50E0 -- true iff all five resolved-contact queues are empty
            // (the discarded queue is not consulted); asserts the four run lists are
            // empty when so. DEFINED in BrnContactSpyData.cpp.
            bool IsEmpty() const;

            // INLINE (attested 2026-08-06, bridge de-facade wave): PhysicsModule::
            // ProcessContactSpies @0x825B03E8..0x825B0434 inlines this aggregate pass as four
            // back-to-back SortAndCreateRunList calls on the queue/run-list pairs at their
            // pinned seats (+0x00000/+0x198D0, +0x070A0/+0x19B70, +0x106C0/+0x1AF90,
            // +0x167E0/+0x1BF50) -- the console body was header-inline (no out-of-line
            // emission). The four callees are the explicit instantiation TUs
            // (ContactSpyQueue_*_SortAndCreateRunList_*.cpp).
            void SortQueuesByEntityId()
            {
                mRaceCarContactQueue.SortAndCreateRunList<KI_MAX_COLLIDING_RACECAR_ENTITIES>(&mRaceCarContactRunList);
                mTrafficContactQueue.SortAndCreateRunList<KI_MAX_COLLIDING_TRAFFIC_ENTITIES>(&mTrafficContactRunList);
                mPhysicalCarPartContactQueue.SortAndCreateRunList<KI_MAX_COLLIDING_DEFORMABLE_ENTITIES>(&mPhysicalCarPartContactRunList);
                mPropContactQueue.SortAndCreateRunList<KI_MAX_COLLIDING_PROP_ENTITIES>(&mPropContactRunList);
            }
            void Append(const ContactSpyData* lpOther);
            ContactSpyData& operator=(const ContactSpyData& lrOther);

            const RaceCarContactQueue*         GetRaceCarContacts() const;
            const TrafficContactQueue*         GetTrafficContacts() const;
            const PhysicalCarPartContactQueue* GetPhysicalCarPartContacts() const;
            const HingedCarPartContactQueue*   GetHingedPartContacts() const;
            const PropContactQueue*            GetPropContacts() const;
            const DiscardedContactQueue*       GetDiscardedContacts() const;
            // INLINE (attested): ContactSpyInterface::GetRaceCarContactRunList @0x82355BF0
            // inlines this accessor as bare offset math (mpData + 0x198D0) -- the console body
            // was header-inline. Defined here so the interface's retyped accessor can call it.
            const RaceCarContactRunList*         GetRaceCarContactRunList() const { return &mRaceCarContactRunList; }
            const TrafficContactRunList*         GetTrafficContactRunList() const;
            const PhysicalCarPartContactRunList* GetPhysicalCarPartContactRunList() const;
            const PropContactRunList*            GetPropContactRunList() const;

            // Typed append overloads (DWARF BrnContactSpyData.h:160-180, all void).
            // Only the RaceCarContact overload (X360 0x825A5230) is DEFINED in-tree;
            // the rest are declared-only (their own ledger TUs).
            void AddContact(const RaceCarContact& lrContact);           // @0x825A5230
            void AddContact(const TrafficContact& lrContact);
            void AddContact(const PhysicalCarPartContact& lrContact);
            void AddContact(const PropContact& lrContact);
            void AddContact(const DiscardedContact& lrContact);
            void AddContact(const HingedPartContact& lrContact);

        private:
            // ---- Members (byte offsets X360-attested; DWARF declaration order) ----
            RaceCarContactQueue         mRaceCarContactQueue;          // [0x00000]
            TrafficContactQueue         mTrafficContactQueue;          // [0x070A0]
            PhysicalCarPartContactQueue mPhysicalCarPartContactQueue;  // [0x106C0]
            HingedCarPartContactQueue   mHingedPartContactQueue;       // [0x151E0]
            PropContactQueue            mPropContactQueue;             // [0x167E0]
            DiscardedContactQueue       mDiscardedContactQueue;        // [0x193C0]
            RaceCarContactRunList         mRaceCarContactRunList;         // [0x198D0]
            TrafficContactRunList         mTrafficContactRunList;         // [0x19B70]
            PhysicalCarPartContactRunList mPhysicalCarPartContactRunList; // [0x1AF90]
            PropContactRunList            mPropContactRunList;            // [0x1BF50]
        };
    }
}
