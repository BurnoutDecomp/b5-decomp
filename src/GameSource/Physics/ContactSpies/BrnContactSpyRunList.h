#pragma once

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"

// EEntityTypeID's canonical home is GameSource/World/BrnEntityTypes.h (namespace BrnWorld),
// not yet reconstructed in-tree. Forward-declared with its underlying type so this header can
// self-contain the ContactSpyRunList layout member meEntityType. A bare unscoped
// `enum EEntityTypeID;` is ill-formed C++; this opaque-enum declaration is well-formed and
// matches the DWARF (signed enumerators down to E_ENTITYTYPE_INVALID = -1, so int underlying).
namespace BrnWorld { enum EEntityTypeID : int; }

namespace BrnPhysics
{
    namespace ContactSpy
    {
        // One per-entity run of accumulated contacts. Recovered from
        // BrnContactSpyRunList.h (DecFIGS DWARF, :56). The Vector3 members make it
        // 16-byte aligned, which places the owning queue's inline buffer at +16.
        struct alignas(16) ContactSpyRunData
        {
            EntityId mEntityId;
            Vector3  mTotalFrictionStress;
            Vector3  mAverageStress;
            Vector3  mAverageContactPoint;
            s32      miStartIndex;
            s32      miRunLength;

            // DWARF-real read accessors (BrnContactSpyRunList.h:83/77/80). Trivial inline
            // getters; used by ContactSpyRunList<MaxLength>::GetRunDataWithEntityID and by
            // ContactSpyData::Append/SortQueuesByEntityId.
            EntityId GetEntityId() const   { return mEntityId; }
            s32      GetStartIndex() const { return miStartIndex; }
            s32      GetRunLength() const  { return miRunLength; }
        };

        // Fixed-capacity, per-entity-type list of contact runs (DWARF BrnContactSpyRunList.h:143).
        // Derives the inline maEvents[MaxLength] storage from CgsModule::EventQueue and adds the
        // entity-type tag meEntityType (BrnContactSpyRunList.h:199). Instantiated for 8/64/50/100
        // by ContactSpyData. Only the read path needed by the boot trace (GetRunData const,
        // GetLength, GetRunDataWithEntityID) is reconstructed here; the write path (Construct,
        // AddEvent, the non-const GetRunData, GetMaxIndex) is declared for layout/ODR fidelity and
        // will be filled by its own ledger TUs.
        template <s32 MaxLength>
        class ContactSpyRunList : public CgsModule::EventQueue<ContactSpyRunData, MaxLength>
        {
        public:
            // Forwards to the base live count (CgsBaseEventQueue::miLength). DWARF lists this as a
            // distinct per-specialisation symbol at BrnContactSpyRunList.h:437.
            s32 GetLength() const
            {
                return CgsModule::EventQueue<ContactSpyRunData, MaxLength>::GetLength();
            }

            // Const checked accessor into the inline run buffer (BrnContactSpyRunList.h:387).
            // &GetEvent(i) returns &maEvents[i]; GetEvent carries the bounds asserts.
            const ContactSpyRunData* GetRunData(int liEventIndex) const
            {
                return &this->GetEvent(liEventIndex);
            }

            // ContactSpyRunList<MaxLength>::GetRunDataWithEntityID  (BrnContactSpyRunList.h:412).
            // For <8> this is the out-of-line symbol @ 0x82373C38. Linear scan returning the first
            // run matching lEntityId, else nullptr. No assert (the inlined index-assert is dead
            // under the loop bound). DWARF locals: liRunIndexLoop:414, lpRunData:415.
            const ContactSpyRunData* GetRunDataWithEntityID(EntityId lEntityId) const
            {
                for (int32_t liRunIndexLoop = 0; liRunIndexLoop < this->GetLength(); ++liRunIndexLoop)
                {
                    const ContactSpyRunData* lpRunData = GetRunData(liRunIndexLoop);
                    if (lpRunData->GetEntityId().muValue == lEntityId.muValue)
                    {
                        return lpRunData;
                    }
                }
                return nullptr;
            }

        private:
            // Entity-type tag, laid out after the inherited inline maEvents[MaxLength] buffer
            // (BrnContactSpyRunList.h:199). Not touched by GetRunDataWithEntityID.
            BrnWorld::EEntityTypeID meEntityType;
        };
    }
}
