#include "GameShared/GameClasses/RenderWare/cross/CgsClusteredMeshResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ClusteredMeshResourceType::FixDown          @ 0x828A9148
//   CgsResource::ClusteredMeshResourceType::FixUp            @ 0x828A91A8
//   CgsResource::ClusteredMeshResourceType::GetImportPointer @ 0x828A77A8
//   CgsResource::ClusteredMeshResourceType::GetTypeID        @ 0x828A77A0
//   (ReBase @ 0x828A8278 is defined in CgsResourceType.cpp)
//
// FixDown/FixUp (un)relocate the embedded rw::collision::ClusteredMesh pointers
// around the base CollisionMeshData relocation; the delta is the leading word of
// the rw::Resource (its load base). On fix-up the RenderWare clustered-mesh fix-up
// re-runs with a scratch parameter block. Pointer arithmetic operates on the
// serialised blob (an external/platform format), accessed by serialised offset.

namespace CgsPhysics
{
    class CollisionMeshData
    {
    public:
        static void* FixDown(void* pData, int liDelta);
        static void* FixUp(void* pData, int liDelta);
    };
    void* CollisionMeshData::FixDown(void*, int) { __debugbreak(); return nullptr; }
    void* CollisionMeshData::FixUp(void*, int)   { __debugbreak(); return nullptr; }
}

namespace rw { namespace collision
{
    class ClusteredMesh
    {
    public:
        static int Fixup(void* pMesh, void* pParams);
    };
    int ClusteredMesh::Fixup(void*, void*) { __debugbreak(); return 0; }
}}

namespace CgsResource
{
    static const uint32_t KU_CLUSTERED_MESH_RESOURCE_TYPE_ID = 36;

    uint32_t ClusteredMeshResourceType::GetTypeID() const
    {
        return KU_CLUSTERED_MESH_RESOURCE_TYPE_ID;
    }

    void ClusteredMeshResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lData   = reinterpret_cast<uintptr_t>(lpResource);
        u32       luMesh  = *reinterpret_cast<u32*>(lData);
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));

        if (luMesh)
        {
            u32 luClusters = *reinterpret_cast<u32*>(luMesh + 68);
            *reinterpret_cast<u32*>(luMesh + 64) = *reinterpret_cast<u32*>(*reinterpret_cast<u32*>(luMesh + 64));
            *reinterpret_cast<u32*>(luMesh + 68) = luClusters - luDelta;
            *reinterpret_cast<u32*>(*reinterpret_cast<u32*>(luClusters + 48)) -= *reinterpret_cast<u32*>(luClusters + 48);
            u32 luUnit = *reinterpret_cast<u32*>(luClusters + 52) - luClusters;
            *reinterpret_cast<u32*>(luClusters + 48) -= luClusters;
            *reinterpret_cast<u32*>(luClusters + 52) = luUnit;
        }
        CgsPhysics::CollisionMeshData::FixDown(lpResource, static_cast<int>(luDelta));
    }

    void ClusteredMeshResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lData   = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));

        CgsPhysics::CollisionMeshData::FixUp(lpResource, static_cast<int>(luDelta));
        u32 luMesh = *reinterpret_cast<u32*>(lData);
        if (luMesh)
        {
            *reinterpret_cast<u32*>(luMesh + 68) += luDelta;
            *reinterpret_cast<u32*>(luMesh + 64) = 0; // patched to the rw clustered-mesh vtable on fix-up
            u8 laParams[348] = {};
            rw::collision::ClusteredMesh::Fixup(reinterpret_cast<void*>(*reinterpret_cast<u32*>(luMesh + 68)), laParams);
        }
    }

    void ClusteredMeshResourceType::GetImportPointer(const void*, uint32_t, uint32_t*, const void**) const
    {
        CGS_ASSERT(false, "ClusteredMeshs have no import pointers");
    }
}
