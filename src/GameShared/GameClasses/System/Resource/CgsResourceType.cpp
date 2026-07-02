#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "GameShared/GameClasses/RenderWare/cross/CgsClusteredMeshResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource, rw::ResourceDescriptor, rw::IResourceAllocator

#include <cstring>               // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828A8278
//   (CgsResource::ClusteredMeshResourceType::ReBase)
//
// A pure delegate: the ReBase virtual override forwards to the shared base helper
// CgsResource::Type::ReBaseTechniqueFixDownAndCopy (an inline that fix-downs the
// source, copies the resource bytes, then fix-ups source and dest — per the
// reconstructed CgsResourceType.h shape). The Hex-Rays pseudocode elides the
// forwarded arguments (they pass through in registers); they are restored here
// from the header's declared signature. The class is declared in the shared
// CgsClusteredMeshResourceType.h (its other virtuals live in that TU).

namespace CgsResource
{
    void ClusteredMeshResourceType::ReBase(
        void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest,
        ResourceDescriptor& lrSize, s32 liMemType) const
    {
        ReBaseTechniqueFixDownAndCopy(lpResource, lrSource, lrDest, lrSize, liMemType);
    }

    // ------------------------------------------------------------------------
    // CgsResource::Type TU (X360 ARTIST).
    // ------------------------------------------------------------------------

    // Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828A81E8.
    //
    // The shared "fix-down then copy" rebase technique: fix-down the source resource
    // bytes in place, block-copy one memory type's bytes from the source pool to the
    // destination pool, then fix-up first the source (in its original pool) and finally
    // the destination (in the new pool). The X360 dispatches through the type's own
    // vtable -- FixDown is virtual index 3 (vtable byte offset +12), FixUp is index 4
    // (+16) -- both called twice as below:
    //   (*vtbl[3])(this, lrSource.m_baseResources[0]) = FixDown(srcMem, lrSource)
    //   XMemCpy(lrDest.m_baseResources[liMemType], lrSource.m_baseResources[liMemType],
    //           lrSize.m_baseResourceDescriptors[liMemType].m_size)
    //   (*vtbl[4])(this, srcMem, lrSource)  = FixUp(srcMem, lrSource)
    //   (*vtbl[4])(this, lpResource, lrDest) = FixUp(lpResource, lrDest)
    // where srcMem == lrSource.m_baseResources[0] (the source's main-memory pointer).
    void Type::ReBaseTechniqueFixDownAndCopy(
        void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest,
        ResourceDescriptor& lrSize, s32 liMemType) const
    {
        void* lpSourceMemory = lrSource.m_baseResources[0];

        FixDown(lpSourceMemory, lrSource);

        std::memcpy(
            lrDest.m_baseResources[liMemType],
            lrSource.m_baseResources[liMemType],
            lrSize.m_baseResourceDescriptors[liMemType].m_size);

        FixUp(lpSourceMemory, lrSource);
        FixUp(lpResource, lrDest);
    }

    // Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828EDFC0.
    //
    // Declared (but never defined) in CgsResourceType.h as the protected offset-rebase
    // helper. Computes the pointer delta between the destination and source base-resource
    // pointers for one memory type, stores that single delta into slot [liMemType] of an
    // otherwise-zeroed scratch buffer, and forwards it through the FixUp vtable slot
    // (index 4, byte offset +16) as a synthetic "resource" -- FixUp only reads the slot for
    // the memory type it's being asked to fix up, so the other (still-zero) slots are inert.
    // lrSize (a5) is not referenced by the X360 body.
    void Type::ReBaseTechniqueFixupWithOffset(
        void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest,
        ResourceDescriptor& /*lrSize*/, s32 liMemType) const
    {
        rw::Resource lOffset = {};
        ptrdiff_t liDelta = static_cast<char*>(lrDest.m_baseResources[liMemType])
                           - static_cast<char*>(lrSource.m_baseResources[liMemType]);
        lOffset.m_baseResources[liMemType] = reinterpret_cast<void*>(liDelta);

        FixUp(lpResource, lOffset);
    }

    // Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82666180.
    //
    // Placement-new carving a Type out of a RenderWare resource allocator. The X360
    // builds a five-entry resource descriptor on the stack -- entry 0 = {m_size = luSize,
    // m_alignment = 4}, entries 1..4 = {m_size = 0, m_alignment = 1} -- then dispatches
    // through the allocator's resource-allocation virtual and returns the main-memory base
    // pointer of the carved resource. The PC engine models that virtual as
    // rw::IResourceAllocator::DoAllocate(descriptor, name) returning an rw::Resource whose
    // m_baseResources[0] is the main-memory pointer; the descriptor here is the modelled
    // four-entry rw::ResourceDescriptor (the trailing fifth X360 entry is inert {0,1}).
    void* Type::operator new(size_t luSize, rw::IResourceAllocator* lpAllocator)
    {
        rw::ResourceDescriptor lDescriptor;
        for (u32 li = 0; li < 4; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(luSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;

        rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
        return lResource.m_baseResources[0];
    }
}
