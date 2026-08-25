#include "GameShared/GameClasses/Sound/Playback/CgsRegistryResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"   // the REAL Playback::Registry (by-name size reads)
#include "rw/rwcore_structs.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RegistryResourceType::GetSerialisedResourceDescriptor @ 0x82667DE8
//     (the shipped body was header-inline: its null assert cites
//      CgsRegistryResourceType.h:91)
//   CgsResource::RegistryResourceType::GetTypeID @ 0x82665908  (return 40960)
//
// (2026-08-25 wave 4: the anchors above were recovered -- the file used to carry a
// literal "@ 0x..." placeholder -- and the fabricated anon-namespace positional
// Registry it read through is retired: the asm's a3[1]/a3[2]/a3[4] word reads are
// mu32EntityCapacity / muDataSize / muStringTableSize on the REAL
// CgsSound::Playback::Registry (CgsRegistry.h word map), read by name. The old
// local names had all three MIS-mapped, and word-indexing is wrong on the host
// anyway -- the pointer members widen.)
//
// GetSerialisedResourceDescriptor returns a five-entry descriptor whose serialised
// size = 4*(capacity + 7) + stringTableSize + dataSize -- the SEVEN-word console
// Registry header (slots begin at word 7, CgsRegistry.h:85-87) + the capacity slot
// array + the entity data blob + the string table. Entries 0/3/4 carry that size
// (entry 0 aligned to 4), the rest are empty.

namespace CgsResource
{
    static const uint32_t KU_REGISTRY_RESOURCE_TYPE_ID = 40960;

    // @ 0x82665908.
    uint32_t RegistryResourceType::GetTypeID() const
    {
        return KU_REGISTRY_RESOURCE_TYPE_ID;
    }

    // @ 0x82667DE8.
    ResourceDescriptor RegistryResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const CgsSound::Playback::Registry* lpRegistry =
            static_cast<const CgsSound::Playback::Registry*>(lpResource);
        // Verbatim assert (the shipped inline cites CgsRegistryResourceType.h:91).
        CGS_ASSERT(lpRegistry != nullptr, "lpRegistry");

        // 4*(capacity + 7): the console 7-word header + the capacity-length slot array
        // (4-byte console slots -- a SERIALISED-form word size, kept as the descriptor
        // maths the asm computes).
        const u32 luSerialisedSize = static_cast<u32>(
            4u * (lpRegistry->GetEntityCapacity() + 7u)
            + lpRegistry->GetStringTableSize()
            + lpRegistry->GetDataSize());

        ResourceDescriptor lDescriptor;
        for (int li = 0; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        // Store order 3 -> 4 -> 0 mirrors the asm (a1[6], a1[8], then the 64-bit
        // {size, align=4} store into a1[0..1]).
        lDescriptor.m_baseResourceDescriptors[3].m_size      = luSerialisedSize;
        lDescriptor.m_baseResourceDescriptors[4].m_size      = luSerialisedSize;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSerialisedSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        return lDescriptor;
    }
}
