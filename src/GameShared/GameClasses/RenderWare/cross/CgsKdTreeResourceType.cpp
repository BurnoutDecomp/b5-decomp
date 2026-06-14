#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::KdTreeResourceType::FixDown                       @ 0x828A9020
//   CgsResource::KdTreeResourceType::FixUp                         @ 0x828A9080
//   CgsResource::KdTreeResourceType::GetImportPointer             @ 0x828A7760
//   CgsResource::KdTreeResourceType::GetSerialisedResourceDescriptor @ 0x828A8F68
//   CgsResource::KdTreeResourceType::GetTypeID                     @ 0x828A7758
//
// Resource-type handler for a serialised rw::collision::TriangleKDTreeProcedural.
// FixDown/FixUp (un)relocate the embedded pointers and re-run the RenderWare
// KD-tree fix-up; GetSerialisedResourceDescriptor copies the rw descriptor and
// pads it to a five-entry block. The KD-tree blob layout is an external/platform
// (RenderWare) format, so its fields are accessed by serialised offset.

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
    class KdTreeResourceType
    {
    public:
        void* FixDown(void* pResource, const int* pDelta);
        int   FixUp(void* pResource, const int* pDelta);
        void* GetImportPointer();
        void* GetSerialisedResourceDescriptor(void* pOut, const void* pResource);
        int   GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 23;
    };

    void* KdTreeResourceType::FixDown(void* pResource, const int* pDelta)
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(pResource);
        u32       luTree  = *reinterpret_cast<u32*>(lRes + 68);
        u32       luFirst = *reinterpret_cast<u32*>(*reinterpret_cast<u32*>(lRes + 64));

        *reinterpret_cast<u32*>(lRes + 68) = luTree - static_cast<u32>(*pDelta);
        *reinterpret_cast<u32*>(lRes + 64) = luFirst;

        *reinterpret_cast<u32*>(*reinterpret_cast<u32*>(luTree + 60)) -= *reinterpret_cast<u32*>(luTree + 60);
        u32 luN1 = *reinterpret_cast<u32*>(luTree + 56) - luTree;
        u32 luN2 = *reinterpret_cast<u32*>(luTree + 60) - luTree;
        u32 luN3 = *reinterpret_cast<u32*>(luTree + 64) - luTree;
        *reinterpret_cast<u32*>(luTree + 52) -= luTree;
        *reinterpret_cast<u32*>(luTree + 56) = luN1;
        *reinterpret_cast<u32*>(luTree + 60) = luN2;
        *reinterpret_cast<u32*>(luTree + 64) = luN3;
        return pResource;
    }

    int KdTreeResourceType::FixUp(void* pResource, const int* pDelta)
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(pResource);
        *reinterpret_cast<u32*>(lRes + 68) += static_cast<u32>(*pDelta);
        *reinterpret_cast<u32*>(lRes + 64) = 0; // patched to the rw KD-tree vtable on fix-up
        u8 laParams[348] = {};
        return rw::collision::TriangleKDTreeProcedural::Fixup(
            reinterpret_cast<void*>(*reinterpret_cast<u32*>(lRes + 68)), laParams);
    }

    void* KdTreeResourceType::GetImportPointer()
    {
        CGS_ASSERT(false, "KdTrees have no import pointers");
        return nullptr;
    }

    void* KdTreeResourceType::GetSerialisedResourceDescriptor(void* pOut, const void* pResource)
    {
        u32*      lpOut = reinterpret_cast<u32*>(pOut);
        uintptr_t lTree = *reinterpret_cast<const u32*>(reinterpret_cast<uintptr_t>(pResource) + 68);

        u8  laScratch[48] = {};
        u32 lauDesc[12] = {};
        u32* lpDesc = reinterpret_cast<u32*>(rw::collision::TriangleKDTreeProcedural::GetResourceDescriptor(
            laScratch,
            *reinterpret_cast<u32*>(lTree + 48),
            *reinterpret_cast<u32*>(lTree + 40),
            *reinterpret_cast<u32*>(*reinterpret_cast<u32*>(lTree + 60) + 4),
            reinterpret_cast<void*>(lTree),
            *reinterpret_cast<u32*>(lTree + 32),
            80));
        for (int li = 0; li < 10; ++li)
            lauDesc[li] = lpDesc[li];

        lpOut[2] = 0; lpOut[3] = 1;
        lpOut[4] = 0; lpOut[5] = 1;
        lpOut[6] = 0; lpOut[7] = 1;
        lpOut[8] = 0; lpOut[9] = 1;
        // First entry is a 64-bit (size, alignment) pair: the X360 stores the
        // descriptor size + 96 in the high word, which on its big-endian layout is
        // the first word, followed by the descriptor's reported alignment.
        lpOut[0] = lauDesc[0] + 96;
        lpOut[1] = lauDesc[1];
        return lpOut;
    }
}
