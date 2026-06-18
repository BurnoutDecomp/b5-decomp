#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"     // ID (embedded by value)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"   // ResourceDescriptor (rw::BaseResourceDescriptors<5>)

// CgsResource::BundleV2 - the on-disk resource bundle (version 2): a 40-byte header followed by an array
// of ResourceEntry (one per packed resource) and per-resource ImportEntry tables (cross-resource
// references = the fix-ups). Each resource declares its size/alignment + disk offset for each of the
// three memory pools (E_MEMTYPE_NUMTYPES). Reconstructed from the DecFIGS DWARF (CgsResourceBundle2.h)
// + the X360 (GetDiskSize 0x828D62F8 / GetUncompresssedResourceDescriptor 0x828E0B10 / the header
// validation in ProcessBundleHeader 0x828D7A90).
//
// The bundle is big-endian (X360); EndianSwap byte-swaps an entry for opposite-endian reads. The
// platform-specific data handling is out of scope here (the systems are reconstructed faithfully; the
// data side is handled separately).

namespace CgsResource
{
    struct BundleV2
    {
        // CgsResourceBundle2.h:153-157
        static const u32 KU_VERSION                          = 2;
        // Our bundle "platform" tag (muPlatform): the x64 PC reconstruction. Stock Burnout bundles use
        // 1=PC(x86), 2=X360, 3=PS3 -- all with layouts that DON'T match our x64 structs (pointer widths
        // differ). 4 marks a bundle whose resource payloads are this build's native little-endian x64
        // images, so BundleLoader can refuse stock bundles it can't safely fix up. YAP writes this.
        static const u32 KU_PLATFORM                         = 4;
        static const u32 KU_MAINMEM_OPTIMISATION_ALIGNMENT   = 16;
        static const u32 KU_GRAPHICSMEM_OPTIMISATION_ALIGNMENT = 128;
        static const u32 KU_SIZE_MASK                        = 0x0FFFFFFFu;   // low 28 bits = size
        static const u32 KU_ALIGNMENT_MASK                   = 0xF0000000u;   // high 4 bits = log2(alignment)

        // The bundle stores size/alignment/offset per memory pool; the resource mem-type count is 3.
        static const u32 E_MEMTYPE_NUMTYPES = 3;

        enum class Flags : u32
        {
            IsCompressed           = 0x1,
            IsMainMemOptimised     = 0x2,
            IsGraphicsMemOptimised = 0x4,
            ContainsDebugData      = 0x8,
        };

        // CgsResourceBundle2.h:63 - one packed resource. A size/alignment word packs the size in the low
        // 28 bits and log2(alignment) in the high 4 (so alignment = 1 << (word >> 28)).
        struct ResourceEntry
        {
            ID  mResourceId;
            u64 muImportHash;
            u32 mauUncompressedSizeAndAlignment[E_MEMTYPE_NUMTYPES];
            u32 mauSizeAndAlignmentOnDisk[E_MEMTYPE_NUMTYPES];
            u32 mauDiskOffset[E_MEMTYPE_NUMTYPES];
            u32 muImportOffset;
            u32 muResourceTypeId;
            u16 muImportCount;
            u8  muFlags;
            u8  muStreamIndex;

            void EndianSwap();

            u32  GetDiskSize(u32 luMemType) const;                 // 0x828D62F8
            u32  GetDiskAlignment(u32 luMemType) const;
            u32  GetUncompressedSize(u32 luMemType) const;
            u32  GetUncompresssedAlignment(u32 luMemType) const;
            void GetDiskResourceDescriptor(ResourceDescriptor* lpDescriptor) const;
            void GetUncompresssedResourceDescriptor(ResourceDescriptor* lpDescriptor) const;  // 0x828E0B10

            // SetDiskSizeDescriptor / SetUncompressedSizeDescriptor are the bundle-packer (offline) side;
            // the runtime only reads. Reconstructed when a writer path needs them.
        };

        // CgsResourceBundle2.h:106 - a cross-resource reference: the imported resource's id + the byte
        // offset within this resource where its pointer is fixed up.
        struct ImportEntry
        {
            ID  mResourceId;
            u32 muOffset;

            void EndianSwap();
        };

        char macMagicNumber[4];
        u32  muVersion;
        u32  muPlatform;
        u32  muDebugDataOffset;
        u32  muResourceEntriesCount;
        u32  muResourceEntriesOffset;
        u32  mauResourceDataOffset[E_MEMTYPE_NUMTYPES];
        u32  muFlags;

        bool IsFlagSet(u32 luFlag) const;
        bool IsCompressed() const;
        bool IsMainMemOptimised() const;
        bool IsGraphicsMemOptimised() const;
        bool ContainsDebugData() const;
    };
}
