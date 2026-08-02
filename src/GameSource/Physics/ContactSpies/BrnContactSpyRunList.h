#pragma once

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"

// EEntityTypeID's canonical home is GameSource/World/BrnEntityTypes.h (namespace BrnWorld).
// It is now reconstructed in-tree (DWARF-verbatim), so the former opaque-enum forward
// declaration is replaced by the real include.
#include "GameSource/World/BrnEntityTypes.h"

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

            // BrnContactSpyRunList.h:235 static factory (X360 0x8259BA80). Vector args map in
            // vtx order v1->mTotalFrictionStress, v2->mAverageStress, v3->mAverageContactPoint.
            // Called by ContactSpyQueue<T,N>::SortAndCreateRunList.
            static ContactSpyRunData* Construct(ContactSpyRunData* lpResult,
                                                EntityId lEntityId,
                                                const Vector3& lrTotalFrictionStress,
                                                const Vector3& lrAverageStress,
                                                const Vector3& lrAverageContactPoint,
                                                s32 liStartIndex,
                                                s32 liRunLength);
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
            // ContactSpyRunList<MaxLength>::Construct(BrnWorld::EEntityTypeID) -- DWARF
            // BrnContactSpyRunList.h (the tag is an ARGUMENT). Same shape as the queue's
            // Construct: chain the base fixed-capacity construction, then stamp the tag.
            // Inlined at every X360 call site (no out-of-line symbol); ContactSpyData::Construct
            // @0x825AE010 shows `bl EventQueue<ContactSpyRunData,N>::Construct` immediately
            // followed by a store at 0x10 + N*80 -- sizeof(ContactSpyRunData) is 80 because the
            // leading EntityId is padded out to the first 16-byte-aligned Vector3. The four
            // instantiations check out exactly: N=8 -> 0x290, 64 -> 0x1410, 50 -> 0xFB0,
            // 100 -> 0x1F50, which are the four store offsets in that asm.
            void Construct(BrnWorld::EEntityTypeID leEntityType)
            {
                CgsModule::EventQueue<ContactSpyRunData, MaxLength>::Construct();
                meEntityType = leEntityType;
            }

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
