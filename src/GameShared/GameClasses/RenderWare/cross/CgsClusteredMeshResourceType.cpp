#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ClusteredMeshResourceType::FixDown          @ 0x828A9148
//   CgsResource::ClusteredMeshResourceType::FixUp            @ 0x828A91A8
//   CgsResource::ClusteredMeshResourceType::GetImportPointer @ 0x828A77A8
//   CgsResource::ClusteredMeshResourceType::GetTypeID        @ 0x828A77A0
//
// Resource-type handler for a serialised collision ClusteredMesh. FixDown/FixUp
// (un)relocate the embedded rw::collision::ClusteredMesh pointers around the base
// CollisionMeshData relocation, and on fix-up re-run the RenderWare clustered-mesh
// fix-up with a scratch parameter block. Pointer arithmetic operates on the
// serialised blob (file-format offsets), as in the sibling resource types.

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
    // These routines walk RenderWare's serialised rw::collision::ClusteredMesh
    // blob, whose internal field layout is an external/platform format we do not
    // own; field access is therefore by serialised offset (the platform-API
    // exception to member-by-name recovery). The object at offset 0 of the
    // resource is the CollisionMeshData; the clustered mesh pointer is at +64 with
    // a paired offset at +68.
    class ClusteredMeshResourceType
    {
    public:
        void* FixDown(void* pCollisionData, const int* pDelta);
        int   FixUp(void* pCollisionData, const int* pDelta);
        void* GetImportPointer();
        int   GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 36;
    };

    void* ClusteredMeshResourceType::FixDown(void* pCollisionData, const int* pDelta)
    {
        uintptr_t lData = reinterpret_cast<uintptr_t>(pCollisionData);
        u32       luMesh = *reinterpret_cast<u32*>(lData);
        const u32 luDelta = static_cast<u32>(*pDelta);

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
        return CgsPhysics::CollisionMeshData::FixDown(pCollisionData, luDelta);
    }

    int ClusteredMeshResourceType::FixUp(void* pCollisionData, const int* pDelta)
    {
        uintptr_t lData = reinterpret_cast<uintptr_t>(pCollisionData);
        const u32 luDelta = static_cast<u32>(*pDelta);

        void* lResult = CgsPhysics::CollisionMeshData::FixUp(pCollisionData, luDelta);
        u32 luMesh = *reinterpret_cast<u32*>(lData);
        if (luMesh)
        {
            *reinterpret_cast<u32*>(luMesh + 68) += luDelta;
            *reinterpret_cast<u32*>(luMesh + 64) = 0; // patched to the rw clustered-mesh vtable on fix-up
            u8 laParams[348] = {};
            return rw::collision::ClusteredMesh::Fixup(reinterpret_cast<void*>(*reinterpret_cast<u32*>(luMesh + 68)), laParams);
        }
        return reinterpret_cast<int>(lResult);
    }

    void* ClusteredMeshResourceType::GetImportPointer()
    {
        CGS_ASSERT(false, "ClusteredMeshs have no import pointers");
        return nullptr;
    }
}
