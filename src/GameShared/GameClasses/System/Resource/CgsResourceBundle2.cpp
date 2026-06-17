#include "GameShared/GameClasses/System/Resource/CgsResourceBundle2.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (mem-type bounds)
#include "rw/rwcore_structs.h"                        // rw::BaseResourceDescriptor (descriptor fill)

#include <cstdlib>   // _byteswap_ushort / _byteswap_ulong / _byteswap_uint64

namespace CgsResource
{
    // ---- BundleV2 header flags ----

    bool BundleV2::IsFlagSet(u32 luFlag) const
    {
        return (muFlags & luFlag) != 0;
    }

    bool BundleV2::IsCompressed() const           { return IsFlagSet(static_cast<u32>(Flags::IsCompressed)); }
    bool BundleV2::IsMainMemOptimised() const     { return IsFlagSet(static_cast<u32>(Flags::IsMainMemOptimised)); }
    bool BundleV2::IsGraphicsMemOptimised() const { return IsFlagSet(static_cast<u32>(Flags::IsGraphicsMemOptimised)); }
    bool BundleV2::ContainsDebugData() const      { return IsFlagSet(static_cast<u32>(Flags::ContainsDebugData)); }

    // ---- ResourceEntry size/alignment (a packed word: size in the low 28 bits, log2(align) in the
    // high 4, so size = word & KU_SIZE_MASK and alignment = 1 << (word >> 28)) ----

    u32 BundleV2::ResourceEntry::GetDiskSize(u32 luMemType) const   // 0x828D62F8
    {
        CGS_ASSERT(luMemType < BundleV2::E_MEMTYPE_NUMTYPES, "luMemType < E_MEMTYPE_NUMTYPES");
        return mauSizeAndAlignmentOnDisk[luMemType] & BundleV2::KU_SIZE_MASK;
    }

    u32 BundleV2::ResourceEntry::GetDiskAlignment(u32 luMemType) const
    {
        CGS_ASSERT(luMemType < BundleV2::E_MEMTYPE_NUMTYPES, "luMemType < E_MEMTYPE_NUMTYPES");
        return 1u << (mauSizeAndAlignmentOnDisk[luMemType] >> 28);
    }

    u32 BundleV2::ResourceEntry::GetUncompressedSize(u32 luMemType) const
    {
        CGS_ASSERT(luMemType < BundleV2::E_MEMTYPE_NUMTYPES, "luMemType < E_MEMTYPE_NUMTYPES");
        return mauUncompressedSizeAndAlignment[luMemType] & BundleV2::KU_SIZE_MASK;
    }

    u32 BundleV2::ResourceEntry::GetUncompresssedAlignment(u32 luMemType) const
    {
        CGS_ASSERT(luMemType < BundleV2::E_MEMTYPE_NUMTYPES, "luMemType < E_MEMTYPE_NUMTYPES");
        return 1u << (mauUncompressedSizeAndAlignment[luMemType] >> 28);
    }

    // 0x828E0B10 - fill the rw resource descriptor (size + alignment per memory pool) from the entry.
    void BundleV2::ResourceEntry::GetDiskResourceDescriptor(ResourceDescriptor* lpDescriptor) const
    {
        for (u32 luMemType = 0; luMemType < BundleV2::E_MEMTYPE_NUMTYPES; ++luMemType)
        {
            lpDescriptor->m_baseResourceDescriptors[luMemType].m_size      = GetDiskSize(luMemType);
            lpDescriptor->m_baseResourceDescriptors[luMemType].m_alignment = GetDiskAlignment(luMemType);
        }
    }

    void BundleV2::ResourceEntry::GetUncompresssedResourceDescriptor(ResourceDescriptor* lpDescriptor) const
    {
        for (u32 luMemType = 0; luMemType < BundleV2::E_MEMTYPE_NUMTYPES; ++luMemType)
        {
            lpDescriptor->m_baseResourceDescriptors[luMemType].m_size      = GetUncompressedSize(luMemType);
            lpDescriptor->m_baseResourceDescriptors[luMemType].m_alignment = GetUncompresssedAlignment(luMemType);
        }
    }

    // ---- EndianSwap (the bundle is big-endian); byte-swaps every multi-byte field of the entry ----

    void BundleV2::ResourceEntry::EndianSwap()
    {
        mResourceId.SetHash(_byteswap_uint64(mResourceId.GetHash()));
        muImportHash = _byteswap_uint64(muImportHash);
        for (u32 luMemType = 0; luMemType < BundleV2::E_MEMTYPE_NUMTYPES; ++luMemType)
        {
            mauUncompressedSizeAndAlignment[luMemType] = _byteswap_ulong(mauUncompressedSizeAndAlignment[luMemType]);
            mauSizeAndAlignmentOnDisk[luMemType]       = _byteswap_ulong(mauSizeAndAlignmentOnDisk[luMemType]);
            mauDiskOffset[luMemType]                   = _byteswap_ulong(mauDiskOffset[luMemType]);
        }
        muImportOffset   = _byteswap_ulong(muImportOffset);
        muResourceTypeId = _byteswap_ulong(muResourceTypeId);
        muImportCount    = _byteswap_ushort(muImportCount);
        // muFlags + muStreamIndex are single bytes - no swap.
    }

    void BundleV2::ImportEntry::EndianSwap()
    {
        mResourceId.SetHash(_byteswap_uint64(mResourceId.GetHash()));
        muOffset = _byteswap_ulong(muOffset);
    }
}
