#pragma once

// Canonical home for BrnPhysics::ContactSpy::ContactSpyQueue<T, N>
// (DWARF home GameSource/Physics/ContactSpies/BrnContactSpyQueue.h:57).
//
// A fixed-capacity contact queue: it IS a CgsModule::EventQueue<T, N> (inline
// maEvents[N] buffer at +0x10) plus one trailing per-queue entity-type tag
// meEntityType (DWARF BrnContactSpyQueue.h:124). Instantiated by ContactSpyData for
//   RaceCarContact,300 / TrafficContact,400 / PhysicalCarPartContact,150 /
//   HingedPartContact,50 / PropContact,100.
//
// Layout (X360-attested via the three DebugGetEntityTypeName getters, and
// cross-validated: ContactSpyData member math places mRaceCarContactRunList at
// exactly 0x198D0, the offset used by ContactSpyInterface::GetRaceCarContactRunList):
//   base EventQueue<T,N>:  mpEvents@0, miMaxLength@4, miLength@8, maEvents[N]@0x10
//   meEntityType @ 0x10 + N*sizeof(T):
//     PhysicalCarPartContact,150 -> 0x10 + 150*128 = 0x4B10 (19216)  [fn @0x8259E428]
//     RaceCarContact,300         -> 0x10 + 300*96  = 0x7090 (28816)  [fn @0x8259E1F0]
//     TrafficContact,400         -> 0x10 + 400*96  = 0x9610 (38416)  [fn @0x8259E308]
//
// The generic DebugGetEntityTypeName() and GetNumUniqueEntities() bodies live
// inline here; each in-scope instantiation is emitted as an explicit
// instantiation line in BrnContactSpyQueue.cpp (mirrors the committed
// EventQueue_*/BaseEventQueue_*/ContactSpyRunList_* pattern in this directory).
//
// DWARF method shapes (BrnContactSpyQueue.h): GetBaseContact(int) const/non-const
// (:155/:142), GetLength() const (:167), DebugGetEntityTypeName() non-const (:105),
// GetNumUniqueEntities() PRIVATE (:176). Construct/SortCompareContacts/
// SortAndCreateRunList/AppendSafe are separate ledger TUs (declared-only elsewhere).

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"

// EEntityTypeID's canonical home is GameSource/World/BrnEntityTypes.h (namespace
// BrnWorld), not yet reconstructed in-tree. Opaque-enum declaration (matches the
// DecFIGS DWARF, signed enumerators -> int underlying), same forward-decl style
// already used by the committed BrnContactSpyRunList.h.
namespace BrnWorld { enum EEntityTypeID : int; }

namespace BrnPhysics
{
    namespace ContactSpy
    {
        // DWARF: BrnContactSpyQueue.h:57.
        template <typename T, s32 N>
        class ContactSpyQueue : public CgsModule::EventQueue<T, N>
        {
        public:
            // Checked element accessor into the inline maEvents buffer, reinterpreted
            // as the contact's BaseContact sub-object (BrnContactSpyQueue.h:142/155).
            // &GetEvent(i) == &maEvents[i]; the base GetEvent carries the bounds asserts
            // (X360 inlines the index bound check into GetNumUniqueEntities).
            BaseContact* GetBaseContact(int liIndex)
            {
                return static_cast<BaseContact*>(&this->GetEvent(liIndex));
            }
            const BaseContact* GetBaseContact(int liIndex) const
            {
                return static_cast<const BaseContact*>(&this->GetEvent(liIndex));
            }

            // Forwards to the base live count (CgsBaseEventQueue::miLength @+8).
            // Distinct per-specialisation symbol at BrnContactSpyQueue.h:167.
            s32 GetLength() const
            {
                return CgsModule::EventQueue<T, N>::GetLength();
            }

            // ContactSpyQueue<T,N>::DebugGetEntityTypeName @ (per-instantiation).
            // Maps the queue's meEntityType tag to a human-readable string. The X360
            // switch tests the raw value as unsigned (cmplwi; cases 4,5 and >7 fall to
            // default). Rodata strings reproduced verbatim; assert message
            // "Invalid entity type" (X360 rodata).
            const char* DebugGetEntityTypeName()
            {
                switch (static_cast<int>(meEntityType))
                {
                    case 0: return "world";
                    case 1: return "race car";
                    case 2: return "traffic vehicle";
                    case 3: return "prop";
                    case 6: return "race car deformable part";
                    case 7: return "traffic deformable part";
                    default:
                        CGS_ASSERT(false, "Invalid entity type");
                        return nullptr;
                }
            }

        private:
            // ContactSpyQueue<T,N>::GetNumUniqueEntities @ (per-instantiation). PRIVATE
            // (DWARF BrnContactSpyQueue.h:176). Counts distinct runs of consecutive
            // contacts by mEntityIdA (the first 32-bit field of BaseContact). Returns 0
            // for an empty queue, else 1 + (number of times the entity id changes as the
            // contacts are scanned in order). The X360 body reads
            // GetBaseContact(0)->mEntityIdA, then walks indices 1..GetLength()-1 comparing
            // each against the running previous id (element stride sizeof(T)).
            int32_t GetNumUniqueEntities()
            {
                if (GetLength() == 0)
                {
                    return 0;
                }

                int32_t liUniqueCount = 1;
                u32 luPrevEntityId = GetBaseContact(0)->mEntityIdA.muValue;

                for (int32_t liIndex = 1; liIndex < GetLength(); ++liIndex)
                {
                    u32 luEntityId = GetBaseContact(liIndex)->mEntityIdA.muValue;
                    if (luEntityId != luPrevEntityId)
                    {
                        ++liUniqueCount;
                        luPrevEntityId = luEntityId;
                    }
                }

                return liUniqueCount;
            }

            // Entity-type tag, laid out AFTER the inherited inline maEvents[N] buffer
            // (DWARF BrnContactSpyQueue.h:124).
            BrnWorld::EEntityTypeID meEntityType;
        };
    }
}
