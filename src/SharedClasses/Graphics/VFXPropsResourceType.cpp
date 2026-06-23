#include "SharedClasses/Graphics/VFXPropsResourceType.h"
#include "rw/rwcore_structs.h"                        // rw::Resource complete for FixUp
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnParticle::VFXPropCollectionResourceType::GetSerialisedResourceDescriptor @ 0x8267C488
//   BrnParticle::VFXPropCollectionResourceType::DeSerialise      @ 0x826757D8
//   BrnParticle::VFXPropCollectionResourceType::FixUp            @ 0x82678588
//   BrnParticle::VFXPropCollectionResourceType::GetImportPointer @ 0x826759A8
//   BrnParticle::VFXPropCollectionResourceType::GetTypeID        @ 0x82675998
//   BrnParticle::VFXPropCollectionResourceType::Serialise        @ 0x82675958
//
// A serialised VFXPropCollection is a header of six (table-base, count) pairs plus
// a version word; FixUp rebases the table-base pointers to the loaded address (the
// delta is the first word of the rw::Resource arg) and converts each table entry's
// stored file-relative index into an absolute pointer. The collection is accessed
// by raw dword offset (the serialised header layout, not a host struct).

// X360 COMDAT fold / dual-attribution: the Hex-Rays view of DeSerialise shows the
// body merged with CgsSound::Playback::Content::DoOnPostLoad (identical machine
// code at the folded address). Reproduce the fold via a TU-local stub so it is
// faithful without pulling in the sound subsystem (matches the
// ParticleDescriptionResourceType precedent). Given INTERNAL linkage (static) so
// it does not duplicate the byte-identical stub in that precedent at link time
// (the placeholder is superseded once the real Sound::Content::DoOnPostLoad lands).
namespace CgsSound { namespace Playback { namespace Content
{
    static int DoOnPostLoad(void* pContent) { (void)pContent; __debugbreak(); return 0; }
}}}

namespace BrnParticle
{
    static const uint32_t KU_VFX_PROP_COLLECTION_RESOURCE_TYPE_ID = 65563;  // 0x1001B
    static const uint32_t KU_VFX_PROP_COLLECTION_VERSION_CURRENT  = 3;      // E_VERSION_CURRENT

    // Serialised-header dword layout of a VFXPropCollection (a2[N] = base + 4*N):
    //   [ 0] VFXProp[] table base          [ 1] VFXProp count            (stride 16)
    //   [ 2] VFXPropState[] table base      [ 3] VFXPropState count       (stride 16)
    //   [ 4] VFXMaterial[] table base       [ 5] VFXMaterial count        (stride 12)
    //   [ 6] VFXLocator[] table base        [ 7] VFXLocator table size    (stride 80)
    //   [ 8] VFXCoronaType[] table base     [ 9] VFXCoronaType count      (stride 80)
    //   [10] VFXCoronaTypeData[] table base [11] VFXCoronaTypeData count   (stride 32)
    //   [12] muVersion
    enum EVFXPropCollectionDword
    {
        E_VFXPROP_TABLE             = 0,
        E_VFXPROP_COUNT             = 1,
        E_VFXPROPSTATE_TABLE        = 2,
        E_VFXPROPSTATE_COUNT        = 3,
        E_VFXMATERIAL_TABLE         = 4,
        E_VFXMATERIAL_COUNT         = 5,
        E_VFXLOCATOR_TABLE          = 6,
        E_VFXLOCATOR_TABLE_SIZE     = 7,
        E_VFXCORONATYPE_TABLE       = 8,
        E_VFXCORONATYPE_COUNT       = 9,
        E_VFXCORONATYPEDATA_TABLE   = 10,
        E_VFXCORONATYPEDATA_COUNT   = 11,
        E_VERSION                   = 12
    };

    static u32 Align16(u32 luValue) { return (luValue + 15u) & ~static_cast<u32>(15); }

    uint32_t VFXPropCollectionResourceType::GetTypeID() const
    {
        return KU_VFX_PROP_COLLECTION_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x8267C488 (store-for-store). The serialised payload is a
    // VFXPropCollection; entry0's size accumulates one 16-byte-aligned sub-array per table, plus a
    // 64-byte 16-aligned header, then re-aligns the total to 16. The X360 reads the per-table COUNT
    // words from the serialised header and multiplies by each table's record stride:
    //   align16(16 * VFXProp count)             [a3[1],  stride 16]
    // + align16(16 * VFXPropState count)        [a3[3],  stride 16]
    // + align16(12 * VFXMaterial count)         [a3[5],  stride 12]
    // + align16(80 * VFXLocator table size)     [a3[7],  stride 80]
    // + align16(80 * VFXCoronaType count)       [a3[9],  stride 80]
    // + align16(32 * VFXCoronaTypeData count)   [a3[11], stride 32]
    // + 64 (the 16-aligned collection header)
    // then align16 of the running total. entry0 align = 16; entry1..4 = {0,1}. Counts are read by
    // their serialised dword index (the EVFXPropCollectionDword header layout shared with FixUp).
    CgsResource::ResourceDescriptor
    VFXPropCollectionResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32* lpHeader = static_cast<const u32*>(lpResource);

        u32 luSize = Align16(16u * lpHeader[E_VFXPROP_COUNT]);
        luSize    += Align16(16u * lpHeader[E_VFXPROPSTATE_COUNT]);
        luSize    += Align16(12u * lpHeader[E_VFXMATERIAL_COUNT]);
        luSize    += Align16(80u * lpHeader[E_VFXLOCATOR_TABLE_SIZE]);
        luSize    += Align16(80u * lpHeader[E_VFXCORONATYPE_COUNT]);
        luSize    += Align16(32u * lpHeader[E_VFXCORONATYPEDATA_COUNT]);
        luSize    += 64u;                 // the 16-byte-aligned VFXPropCollection header
        luSize     = Align16(luSize);

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;   // entry0 size
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;      // entry0 align
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // X360 passes `this` (the resource type) to Content::DoOnPostLoad in the folded
    // view; lpResource is unused (matches the ParticleDescription precedent).
    bool VFXPropCollectionResourceType::DeSerialise(void* /*lpResource*/) const
    {
        return CgsSound::Playback::Content::DoOnPostLoad(
            const_cast<VFXPropCollectionResourceType*>(this)) != 0;
    }

    void VFXPropCollectionResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        // The X360 `v4 = *a3` reads the first word of the rw::Resource arg, i.e. the
        // load-base delta applied to every file-relative pointer.
        uintptr_t liDelta = reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]);
        u32*      lpHeader = reinterpret_cast<u32*>(lpResource);

        CGS_ASSERT(lpHeader[E_VERSION] == KU_VFX_PROP_COLLECTION_VERSION_CURRENT,
                   "lpVFXPropCollection->muVersion == BrnParticle::VFXPropCollection::E_VERSION_CURRENT");

        // Rebase the six table-base pointers, each only if non-zero. Preserve the
        // X360's traversal order: 0, 4, 2, 6, 8, 10.
        if (lpHeader[E_VFXPROP_TABLE])
            lpHeader[E_VFXPROP_TABLE] += static_cast<u32>(liDelta);
        if (lpHeader[E_VFXMATERIAL_TABLE])
            lpHeader[E_VFXMATERIAL_TABLE] += static_cast<u32>(liDelta);
        if (lpHeader[E_VFXPROPSTATE_TABLE])
            lpHeader[E_VFXPROPSTATE_TABLE] += static_cast<u32>(liDelta);
        if (lpHeader[E_VFXLOCATOR_TABLE])
            lpHeader[E_VFXLOCATOR_TABLE] += static_cast<u32>(liDelta);
        if (lpHeader[E_VFXCORONATYPE_TABLE])
            lpHeader[E_VFXCORONATYPE_TABLE] += static_cast<u32>(liDelta);
        if (lpHeader[E_VFXCORONATYPEDATA_TABLE])
            lpHeader[E_VFXCORONATYPEDATA_TABLE] += static_cast<u32>(liDelta);

        // Loop A — VFXProp table (stride 16): convert the per-prop VFXPropState index
        // at +8 into an absolute pointer. UNCONDITIONAL — no -1 sentinel check here
        // (preserve the X360 exactly).
        for (u32 luIndex = 0; luIndex < lpHeader[E_VFXPROP_COUNT]; ++luIndex)
        {
            uintptr_t liEntry = static_cast<uintptr_t>(lpHeader[E_VFXPROP_TABLE]) + 16 * luIndex;
            u32* lpStateIdx = reinterpret_cast<u32*>(liEntry + 8);
            *lpStateIdx = 16 * (*lpStateIdx) + lpHeader[E_VFXPROPSTATE_TABLE];
        }

        // Loop B — VFXMaterial table (stride 12): the field at +8 is a VFXLocator
        // index (sentinel -1 -> null), range-checked against the locator table size.
        for (u32 luIndex = 0; luIndex < lpHeader[E_VFXMATERIAL_COUNT]; ++luIndex)
        {
            uintptr_t liEntry = static_cast<uintptr_t>(lpHeader[E_VFXMATERIAL_TABLE]) + 12 * luIndex;
            u32* lpLocatorIdx = reinterpret_cast<u32*>(liEntry + 8);
            s32  liIdx        = static_cast<s32>(*lpLocatorIdx);
            if (liIdx == -1)
            {
                *lpLocatorIdx = 0;
            }
            else
            {
                CGS_ASSERT(liIdx >= 0 && liIdx < static_cast<s32>(lpHeader[E_VFXLOCATOR_TABLE_SIZE]),
                           "( lIndex >= 0 ) && ( lIndex < static_cast<int32_t>( lpVFXPropCollection->muLocatorTableSize ) )");
                *lpLocatorIdx = 80 * (*lpLocatorIdx) + lpHeader[E_VFXLOCATOR_TABLE];
            }
        }

        // Loop C — VFXPropState table (stride 16): two indexed fields per entry —
        //   +0: VFXMaterial index (vs muMaterialTableSize), stride 12
        //   +8: VFXCoronaType index (vs muCoronaTableSize),  stride 80
        for (u32 luIndex = 0; luIndex < lpHeader[E_VFXPROPSTATE_COUNT]; ++luIndex)
        {
            uintptr_t liEntry = static_cast<uintptr_t>(lpHeader[E_VFXPROPSTATE_TABLE]) + 16 * luIndex;

            u32* lpMaterialIdx = reinterpret_cast<u32*>(liEntry + 0);
            s32  liMaterialIdx = static_cast<s32>(*lpMaterialIdx);
            if (liMaterialIdx == -1)
            {
                *lpMaterialIdx = 0;
            }
            else
            {
                CGS_ASSERT(liMaterialIdx >= 0 && liMaterialIdx < static_cast<s32>(lpHeader[E_VFXMATERIAL_COUNT]),
                           "( lIndex >= 0 ) && ( lIndex < static_cast<int32_t>( lpVFXPropCollection->muMaterialTableSize ) )");
                *lpMaterialIdx = 12 * (*lpMaterialIdx) + lpHeader[E_VFXMATERIAL_TABLE];
            }

            u32* lpCoronaIdx = reinterpret_cast<u32*>(liEntry + 8);
            s32  liCoronaIdx = static_cast<s32>(*lpCoronaIdx);
            if (liCoronaIdx == -1)
            {
                *lpCoronaIdx = 0;
            }
            else
            {
                CGS_ASSERT(liCoronaIdx >= 0 && liCoronaIdx < static_cast<s32>(lpHeader[E_VFXCORONATYPE_COUNT]),
                           "( lIndex >= 0 ) && ( lIndex < static_cast<int32_t>( lpVFXPropCollection->muCoronaTableSize ) )");
                *lpCoronaIdx = 80 * (*lpCoronaIdx) + lpHeader[E_VFXCORONATYPE_TABLE];
            }
        }

        // Loop D — VFXCoronaType table (stride 80): the field at +64 is a
        // VFXCoronaTypeData index (sentinel -1 -> null), stride 32.
        for (u32 luIndex = 0; luIndex < lpHeader[E_VFXCORONATYPE_COUNT]; ++luIndex)
        {
            uintptr_t liEntry = static_cast<uintptr_t>(lpHeader[E_VFXCORONATYPE_TABLE]) + 80 * luIndex;
            u32* lpDataIdx = reinterpret_cast<u32*>(liEntry + 64);
            s32  liIdx     = static_cast<s32>(*lpDataIdx);
            if (liIdx == -1)
            {
                *lpDataIdx = 0;
            }
            else
            {
                CGS_ASSERT(liIdx >= 0 && liIdx < static_cast<s32>(lpHeader[E_VFXCORONATYPEDATA_COUNT]),
                           "( lIndex >= 0 ) && ( lIndex < static_cast<int32_t>( lpVFXPropCollection->muCoronaDataTableSize ) )");
                *lpDataIdx = 32 * (*lpDataIdx) + lpHeader[E_VFXCORONATYPEDATA_TABLE];
            }
        }
    }

    // No import pointers for this resource type.
    void VFXPropCollectionResourceType::GetImportPointer(const void* /*lpResource*/, uint32_t /*luIndex*/,
                                                         uint32_t* lpuOffset, const void** lppValue) const
    {
        *lppValue = 0;
        *lpuOffset = 0;
    }

    // Non-tool builds never serialise; the X360 fires a single assert and returns null.
    void* VFXPropCollectionResourceType::Serialise(const void* /*lpResource*/, const rw::Resource& /*lrDest*/) const
    {
        CGS_ASSERT(false, "Didn't expect to call this function in non-tool builds.");
        return nullptr;
    }
}
