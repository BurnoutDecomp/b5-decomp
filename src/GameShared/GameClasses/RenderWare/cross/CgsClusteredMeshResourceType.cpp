#include "types.hpp"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ClusteredMeshResourceType::FixDown          @ 0x828A9148
//   CgsResource::ClusteredMeshResourceType::FixUp            @ 0x828A91A8
//   CgsResource::ClusteredMeshResourceType::GetImportPointer @ 0x828A77A8
//   CgsResource::ClusteredMeshResourceType::GetTypeID        @ 0x828A77A0
//
// The resource wraps a CollisionMeshData whose first field points at the serialised
// clustered-mesh block. FixUp/FixDown chain the base CollisionMeshData relocation with the
// clustered-mesh-specific pointer rebases (the cluster table at +68, and the nested pointers
// at cluster +48/+52). FixUp then reinstalls the runtime vtable and hands the block to
// RenderWare's rw::collision::ClusteredMesh::Fixup with a scratch buffer. The serialised
// addresses are 32-bit by format, so relocatable fields are modelled as u32. Foreign
// helpers (CollisionMeshData, rw clustered mesh, asserts) live in other TUs.

namespace CgsPhysics
{
    struct CollisionMeshData
    {
        static void* FixDown();
        static void* FixUp(void* pResource, int liDelta);
    };
}

namespace rw
{
namespace collision
{
    struct ClusteredMesh
    {
        static void* Fixup(void* pMesh, void* pScratch);
    };
}
}

namespace CgsDev
{
namespace Assert
{
    void  BeginAssert();
    void  FireAssert(const char* pacMessage, const char* pacFile, int liLine);
    void* EndAssert();
}
}

// Runtime clustered-mesh vtable installed by FixUp (dword_8327EEF8).
extern const u32 gClusteredMeshRuntimeVTable;

namespace CgsResource
{
namespace
{
    struct ClusteredMeshData      // *(resource)
    {
        u8  mPad0[64];
        u32 muRuntimeData;        // +64  collapsed to its pointed-to value on FixDown
        u32 muClusterTable;       // +68  relocatable pointer to the cluster table
    };

    struct ClusterTable           // *(meshData.muClusterTable)
    {
        u8  mPad0[48];
        u32 muData;               // +48  relocatable
        u32 muDataEnd;            // +52  relocatable
    };
}

class ClusteredMeshResourceType
{
public:
    void* FixDown(void* pResource, u32* pMeshSlot, const int* pDelta);
    void* FixUp(void* pResource, u32* pMeshSlot, const int* pDelta);
    void* GetImportPointer();
    int   GetTypeID() { return KI_TYPE_ID; }

private:
    static const int KI_TYPE_ID = 36;
};

void* ClusteredMeshResourceType::FixDown(void* /*pResource*/, u32* pMeshSlot, const int* pDelta)
{
    const u32 luDelta = static_cast<u32>(*pDelta);
    ClusteredMeshData* lpMesh = reinterpret_cast<ClusteredMeshData*>(static_cast<uintptr_t>(*pMeshSlot));
    if (lpMesh)
    {
        const u32 luClusters = lpMesh->muClusterTable;
        lpMesh->muRuntimeData  = *reinterpret_cast<u32*>(static_cast<uintptr_t>(lpMesh->muRuntimeData));
        lpMesh->muClusterTable = luClusters - luDelta;

        ClusterTable* lpCluster = reinterpret_cast<ClusterTable*>(static_cast<uintptr_t>(luClusters));
        *reinterpret_cast<u32*>(static_cast<uintptr_t>(lpCluster->muData)) -= lpCluster->muData;
        const u32 luDataEnd = lpCluster->muDataEnd - luClusters;
        lpCluster->muData    -= luClusters;
        lpCluster->muDataEnd  = luDataEnd;
    }
    return CgsPhysics::CollisionMeshData::FixDown();
}

void* ClusteredMeshResourceType::FixUp(void* /*pResource*/, u32* pMeshSlot, const int* pDelta)
{
    const u32 luDelta = static_cast<u32>(*pDelta);
    void* lpResult = CgsPhysics::CollisionMeshData::FixUp(pMeshSlot, static_cast<int>(luDelta));

    ClusteredMeshData* lpMesh = reinterpret_cast<ClusteredMeshData*>(static_cast<uintptr_t>(*pMeshSlot));
    if (lpMesh)
    {
        lpMesh->muClusterTable += luDelta;
        lpMesh->muRuntimeData   = gClusteredMeshRuntimeVTable;

        u8 laScratch[360];
        memset(laScratch, 0, 348);
        return rw::collision::ClusteredMesh::Fixup(
            reinterpret_cast<void*>(static_cast<uintptr_t>(lpMesh->muClusterTable)), laScratch);
    }
    return lpResult;
}

void* ClusteredMeshResourceType::GetImportPointer()
{
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(
        "ClusteredMeshs have no import pointers",
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\renderware\\cross/CgsClusteredMeshResourceType.cpp",
        513);
    return CgsDev::Assert::EndAssert();
}
}
