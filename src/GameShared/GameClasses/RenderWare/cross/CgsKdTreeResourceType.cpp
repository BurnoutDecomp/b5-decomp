#include "types.hpp"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::KdTreeResourceType::FixDown                       @ 0x828A9020
//   CgsResource::KdTreeResourceType::FixUp                         @ 0x828A9080
//   CgsResource::KdTreeResourceType::GetImportPointer              @ 0x828A7760
//   CgsResource::KdTreeResourceType::GetSerialisedResourceDescriptor @ 0x828A8F68
//   CgsResource::KdTreeResourceType::GetTypeID                     @ 0x828A7758
//
// The resource holds a runtime-data pointer (+64) and the serialised TriangleKDTree (+68).
// FixUp/FixDown rebase the tree pointer and the tree's internal node arrays (+52/+56/+60/+64,
// with +60 also dereferenced), then FixUp reinstalls the runtime vtable and runs RenderWare's
// TriangleKDTreeProcedural::Fixup. GetSerialisedResourceDescriptor queries RenderWare for the
// tree's serialised footprint and emits a five-entry resource descriptor (first entry size =
// reported size + 96). Serialised addresses are 32-bit by format. Foreign helpers are in
// other TUs.

namespace rw
{
namespace collision
{
    struct TriangleKDTreeProcedural
    {
        static void* Fixup(void* pTree, void* pScratch);
        static u32*  GetResourceDescriptor(void* pResult, u32 a, u32 b, u32 c, u32 d, u32 e, u32 f);
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

// Runtime kd-tree vtable installed by FixUp (dword_8327EEF8).
extern const u32 gKdTreeRuntimeVTable;

namespace CgsResource
{
namespace
{
    struct KdTreeResource
    {
        u8  mPad0[64];
        u32 muRuntimeData;   // +64
        u32 muKdTree;        // +68  relocatable pointer to the serialised tree
    };

    struct TriangleKDTree
    {
        u8  mPad0[32];
        u32 muField32;       // +32
        u8  mPad1[4];
        u32 muField40;       // +40
        u8  mPad2[4];
        u32 muField48;       // +48
        u32 muField52;       // +52  relocatable
        u32 muField56;       // +56  relocatable
        u32 muField60;       // +60  relocatable pointer (also dereferenced)
        u32 muField64;       // +64  relocatable
    };
}

class KdTreeResourceType
{
public:
    void* FixDown(void* pResource, const int* pDelta);
    void* FixUp(void* pResource, const int* pDelta);
    void* GetImportPointer();
    void* GetSerialisedResourceDescriptor(void* pOut, void* pUnused, void* pResource);
    int   GetTypeID() { return KI_TYPE_ID; }

private:
    static const int KI_TYPE_ID = 23;
};

void* KdTreeResourceType::FixDown(void* pResource, const int* pDelta)
{
    KdTreeResource* lpRes = static_cast<KdTreeResource*>(pResource);
    const u32 luDelta   = static_cast<u32>(*pDelta);
    const u32 luKdTree  = lpRes->muKdTree;
    const u32 luRuntime = *reinterpret_cast<u32*>(static_cast<uintptr_t>(lpRes->muRuntimeData));

    lpRes->muKdTree      = luKdTree - luDelta;
    lpRes->muRuntimeData = luRuntime;

    TriangleKDTree* lpTree = reinterpret_cast<TriangleKDTree*>(static_cast<uintptr_t>(luKdTree));
    *reinterpret_cast<u32*>(static_cast<uintptr_t>(lpTree->muField60)) -= lpTree->muField60;
    const u32 luField56 = lpTree->muField56 - luKdTree;
    const u32 luField60 = lpTree->muField60 - luKdTree;
    const u32 luField64 = lpTree->muField64 - luKdTree;
    lpTree->muField52 -= luKdTree;
    lpTree->muField56  = luField56;
    lpTree->muField60  = luField60;
    lpTree->muField64  = luField64;
    return pResource;
}

void* KdTreeResourceType::FixUp(void* pResource, const int* pDelta)
{
    KdTreeResource* lpRes = static_cast<KdTreeResource*>(pResource);
    lpRes->muKdTree      += static_cast<u32>(*pDelta);
    lpRes->muRuntimeData  = gKdTreeRuntimeVTable;

    u8 laScratch[352];
    memset(laScratch, 0, 348);
    return rw::collision::TriangleKDTreeProcedural::Fixup(
        reinterpret_cast<void*>(static_cast<uintptr_t>(lpRes->muKdTree)), laScratch);
}

void* KdTreeResourceType::GetImportPointer()
{
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(
        "KdTrees have no import pointers",
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\renderware\\cross/CgsKdTreeResourceType.cpp",
        420);
    return CgsDev::Assert::EndAssert();
}

void* KdTreeResourceType::GetSerialisedResourceDescriptor(void* pOut, void* /*pUnused*/, void* pResource)
{
    KdTreeResource* lpRes  = static_cast<KdTreeResource*>(pResource);
    TriangleKDTree* lpTree = reinterpret_cast<TriangleKDTree*>(static_cast<uintptr_t>(lpRes->muKdTree));

    u8 laQueryScratch[48];
    u32* lpDesc = rw::collision::TriangleKDTreeProcedural::GetResourceDescriptor(
        laQueryScratch,
        lpTree->muField48,
        lpTree->muField40,
        *reinterpret_cast<u32*>(static_cast<uintptr_t>(lpTree->muField60 + 4)),
        lpRes->muKdTree,
        lpTree->muField32,
        80);

    u32 laReport[12];
    for (int liIndex = 0; liIndex < 10; ++liIndex)
        laReport[liIndex] = lpDesc[liIndex];

    u32* lpOut = static_cast<u32*>(pOut);
    lpOut[0] = laReport[0] + 96;
    lpOut[1] = laReport[1];
    lpOut[2] = 0; lpOut[3] = 1;
    lpOut[4] = 0; lpOut[5] = 1;
    lpOut[6] = 0; lpOut[7] = 1;
    lpOut[8] = 0; lpOut[9] = 1;
    return pOut;
}
}
