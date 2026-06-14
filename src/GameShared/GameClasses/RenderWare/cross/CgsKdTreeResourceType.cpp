#include "GameShared/GameClasses/RenderWare/cross/CgsKdTreeResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::KdTreeResourceType::FixDown                       @ 0x828A9020
//   CgsResource::KdTreeResourceType::FixUp                         @ 0x828A9080
//   CgsResource::KdTreeResourceType::GetImportPointer             @ 0x828A7760
//   CgsResource::KdTreeResourceType::GetSerialisedResourceDescriptor @ 0x828A8F68
//   CgsResource::KdTreeResourceType::GetTypeID                     @ 0x828A7758
//
// FixDown/FixUp (un)relocate the embedded pointers (delta = the rw::Resource's load
// base) and re-run the RenderWare KD-tree fix-up. GetSerialisedResourceDescriptor
// copies the rw descriptor and returns it padded to a five-entry block. The KD-tree
// blob is an external/platform (RenderWare) format, accessed by serialised offset.

namespace rw { namespace collision
{
    class TriangleKDTreeProcedural
    {
    public:
        static int   Fixup(void* pTree, void* pParams);
        static void* GetResourceDescriptor(void* pOut, u32 a1, u32 a2, u32 a3, void* pTree, u32 a5, int a6);
    };
    int   TriangleKDTreeProcedural::Fixup(void*, void*) { __debugbreak(); return 0; }
    void* TriangleKDTreeProcedural::GetResourceDescriptor(void*, u32, u32, u32, void*, u32, int) { __debugbreak(); return nullptr; }
}}

namespace CgsResource
{
    static const uint32_t KU_KD_TREE_RESOURCE_TYPE_ID = 23;

    uint32_t KdTreeResourceType::GetTypeID() const
    {
        return KU_KD_TREE_RESOURCE_TYPE_ID;
    }

    void KdTreeResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lRes    = reinterpret_cast<uintptr_t>(lpResource);
        u32       luTree  = *reinterpret_cast<u32*>(lRes + 68);
        u32       luFirst = *reinterpret_cast<u32*>(*reinterpret_cast<u32*>(lRes + 64));
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));

        *reinterpret_cast<u32*>(lRes + 68) = luTree - luDelta;
        *reinterpret_cast<u32*>(lRes + 64) = luFirst;

        *reinterpret_cast<u32*>(*reinterpret_cast<u32*>(luTree + 60)) -= *reinterpret_cast<u32*>(luTree + 60);
        u32 luN1 = *reinterpret_cast<u32*>(luTree + 56) - luTree;
        u32 luN2 = *reinterpret_cast<u32*>(luTree + 60) - luTree;
        u32 luN3 = *reinterpret_cast<u32*>(luTree + 64) - luTree;
        *reinterpret_cast<u32*>(luTree + 52) -= luTree;
        *reinterpret_cast<u32*>(luTree + 56) = luN1;
        *reinterpret_cast<u32*>(luTree + 60) = luN2;
        *reinterpret_cast<u32*>(luTree + 64) = luN3;
    }

    void KdTreeResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        *reinterpret_cast<u32*>(lRes + 68) += static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));
        *reinterpret_cast<u32*>(lRes + 64) = 0; // patched to the rw KD-tree vtable on fix-up
        u8 laParams[348] = {};
        rw::collision::TriangleKDTreeProcedural::Fixup(
            reinterpret_cast<void*>(*reinterpret_cast<u32*>(lRes + 68)), laParams);
    }

    void KdTreeResourceType::GetImportPointer(const void*, uint32_t, uint32_t*, const void**) const
    {
        CGS_ASSERT(false, "KdTrees have no import pointers");
    }

    ResourceDescriptor KdTreeResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        uintptr_t lTree = *reinterpret_cast<const u32*>(reinterpret_cast<uintptr_t>(lpResource) + 68);

        u8  laScratch[48] = {};
        u32 lauDesc[12] = {};
        u32* lpRwDesc = reinterpret_cast<u32*>(rw::collision::TriangleKDTreeProcedural::GetResourceDescriptor(
            laScratch,
            *reinterpret_cast<u32*>(lTree + 48),
            *reinterpret_cast<u32*>(lTree + 40),
            *reinterpret_cast<u32*>(*reinterpret_cast<u32*>(lTree + 60) + 4),
            reinterpret_cast<void*>(lTree),
            *reinterpret_cast<u32*>(lTree + 32),
            80));
        for (int li = 0; li < 10; ++li)
            lauDesc[li] = lpRwDesc[li];

        // First entry: descriptor size + 96 with the descriptor's reported alignment;
        // the remaining four entries are empty.
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = lauDesc[0] + 96;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = lauDesc[1];
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        return lDescriptor;
    }
}
