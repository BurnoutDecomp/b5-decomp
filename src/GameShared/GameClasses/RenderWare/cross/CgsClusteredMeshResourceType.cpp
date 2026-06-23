#include "GameShared/GameClasses/RenderWare/cross/CgsClusteredMeshResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ClusteredMeshResourceType::GetSerialisedResourceDescriptor @ 0x828A90E8
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

    // GetSerialisedResourceDescriptor @ 0x828A90E8 -- the serialised carve-out the resource
    // loader needs to lay this clustered mesh down: a 5-entry descriptor whose first entry is
    // {m_size, m_alignment=16} and whose remaining four entries are {0,1}. The first entry's
    // size is 32 by default, or (when the serialised blob carries an embedded clustered mesh)
    // the mesh's cluster-table byte size (clusters[+0x50]) plus a 0x80 header allowance. Pointer
    // arithmetic walks the serialised blob (an external/platform format), accessed by serialised
    // offset -- the same chain FixDown/FixUp use (resource -> mesh -> clusters). Bodied store-for
    // -store against the X360 asm; we do NOT byte-match.
    ResourceDescriptor ClusteredMeshResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        u32 luSize = 32;   // r9 = 0x20 default

        const u32* lpu = reinterpret_cast<const u32*>(lpResource);
        const u32  luMesh = lpu ? lpu[0] : 0u;   // lwz r11, 0(r5)
        if (luMesh)
        {
            u32 luClusters = *reinterpret_cast<const u32*>(luMesh + 0x44);   // lwz r11, 0x44(r11)
            luSize = *reinterpret_cast<const u32*>(luClusters + 0x50) + 0x80; // lwz r11,0x50 ; addi +0x80
        }

        ResourceDescriptor lDescriptor;
        rw::BaseResourceDescriptor* lpEntries = lDescriptor.m_baseResourceDescriptors;

        lpEntries[0].m_size      = luSize;  // std r9, 0(r3): m_size @ +0, m_alignment @ +4
        lpEntries[0].m_alignment = 16;
        lpEntries[1].m_size      = 0;       // stw r10, 8 ; stw r11, 0xC
        lpEntries[1].m_alignment = 1;
        lpEntries[2].m_size      = 0;       // stw r10, 0x10 ; stw r11, 0x14
        lpEntries[2].m_alignment = 1;
        lpEntries[3].m_size      = 0;       // stw r10, 0x18 ; stw r11, 0x1C
        lpEntries[3].m_alignment = 1;
        lpEntries[4].m_size      = 0;       // stw r10, 0x20 ; stw r11, 0x24
        lpEntries[4].m_alignment = 1;

        return lDescriptor;
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
