#include "GameShared/GameClasses/Physics/CgsInstanceCollisionListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsPhysics::InstanceCollisionListResourceType::FixDown   @ 0x828A7380
//   CgsPhysics::InstanceCollisionListResourceType::FixUp     @ 0x828A73A0
//   CgsPhysics::InstanceCollisionListResourceType::GetTypeID @ 0x8289D568
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

    uint32_t InstanceCollisionListResourceType::GetTypeID() const
    {
        return KU_INSTANCE_COLLISION_LIST_RESOURCE_TYPE_ID;
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
