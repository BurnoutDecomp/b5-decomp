#include "GameShared/GameClasses/Physics/CgsInstanceCollisionListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsPhysics::InstanceCollisionListResourceType::GetTypeID                       @ 0x8289D568
//   CgsPhysics::InstanceCollisionListResourceType::GetSerialisedResourceDescriptor @ 0x828A5FA8
//   CgsPhysics::InstanceCollisionListResourceType::FixDown                         @ 0x828A7380
//   CgsPhysics::InstanceCollisionListResourceType::FixUp                           @ 0x828A73A0
//
// FixUp/FixDown rebase the leading pointer of the resource (when non-null) by the
// delta (the rw::Resource's load base), then (re)construct/destruct the embedded
// BaseCollisionGenerator at dword offset 2. BaseCollisionGenerator is its own TU.

namespace CgsSceneManager
{
    namespace CgsCollision
    {
        struct BaseCollisionGenerator
        {
            BaseCollisionGenerator* Destruct();
        };
    }
}

namespace CgsPhysics
{
    typedef CgsSceneManager::CgsCollision::BaseCollisionGenerator Generator;

    static const uint32_t KU_INSTANCE_COLLISION_LIST_RESOURCE_TYPE_ID = 38;

    // Serialised instance-collision-list header. Only the instance count at +4 is read by
    // GetSerialisedResourceDescriptor (X360 `lwz r9, 4(r5)`); +0 is the rebased pointer the
    // FixUp/FixDown below touch. Modelled by name; the remaining payload (the per-instance
    // records the size formula sizes) is not part of this descriptor TU.
    struct InstanceCollisionList
    {
        u32 mpData;               // +0  rebased leading pointer (FixUp/FixDown)
        u32 muNumberOfInstances;  // +4  number of collision instances
    };

    uint32_t InstanceCollisionListResourceType::GetTypeID() const
    {
        return KU_INSTANCE_COLLISION_LIST_RESOURCE_TYPE_ID;
    }

    // X360 0x828A5FA8 (store-for-store). Builds the five-entry serialised descriptor:
    //   entry0 = { size = (muNumberOfInstances << 6) + 0x20, align = 16 }
    //   entry1..4 = { size = 0, align = 1 }
    // The X360 first writes the four alignments (r11 = 1) at entries 1-4's +4 fields, zeroes
    // the four sizes (r10 = 0) at entries 1-4's +0 fields, then a single 64-bit store of
    // {size = (count<<6)+32, align = 16} fills entry0 (the qword at +0). So per-instance the
    // serialised payload is 64 bytes (the `<< 6`) plus a 32-byte fixed header, 16-byte aligned.
    CgsResource::ResourceDescriptor InstanceCollisionListResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const InstanceCollisionList* lpList =
            static_cast<const InstanceCollisionList*>(lpResource);

        CgsResource::ResourceDescriptor lDescriptor;
        rw::BaseResourceDescriptor* lpEntries = lDescriptor.m_baseResourceDescriptors;

        lpEntries[0].m_size      = (lpList->muNumberOfInstances << 6) + 0x20;
        lpEntries[0].m_alignment = 16;
        lpEntries[1].m_size = 0;  lpEntries[1].m_alignment = 1;
        lpEntries[2].m_size = 0;  lpEntries[2].m_alignment = 1;
        lpEntries[3].m_size = 0;  lpEntries[3].m_alignment = 1;
        lpEntries[4].m_size = 0;  lpEntries[4].m_alignment = 1;

        return lDescriptor;
    }

    void InstanceCollisionListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        u32* lpData  = static_cast<u32*>(lpResource);
        int  liDelta = static_cast<int>(CgsResource::GetLoadBase(lrResource));

        if (*lpData)
            *lpData -= liDelta;
        reinterpret_cast<Generator*>(lpData + 2)->Destruct();
    }

    void InstanceCollisionListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        u32* lpData  = static_cast<u32*>(lpResource);
        int  liDelta = static_cast<int>(CgsResource::GetLoadBase(lrResource));

        if (*lpData)
            *lpData += liDelta;
        reinterpret_cast<Generator*>(lpData + 2)->Destruct();
    }
}
