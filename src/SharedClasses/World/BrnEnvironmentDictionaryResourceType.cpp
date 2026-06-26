#include "SharedClasses/World/BrnEnvironmentDictionaryResourceType.h"

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::DictionaryResourceType::GetTypeID @ 0x826763A0
//   BrnWorld::EnvironmentSettings::DictionaryResourceType::FixDown   @ 0x8267E250
//   BrnWorld::EnvironmentSettings::DictionaryResourceType::FixUp     @ 0x8267E278
//
// FixUp/FixDown rebase the Dictionary's two load-relative pointer fields (+8 and
// +16) by the rw::Resource's load base (the relocation delta). FixUp validates the
// on-disk version first. Sibling of BrnVehicleGraphicsSpec / EnvironmentTimeLine.

namespace BrnWorld
{
namespace EnvironmentSettings
{
    // The environment-settings Dictionary relocation slice. Member SEMANTICS are
    // recovered from the PS3 DecFIGS DWARF (same source as the X360): FixUp @
    // PS3 0x81A008 rebases a2[2] via rw::RwPtrAddBasePtr<Dictionary::SeasonData>
    // and a2[4] via rw::RwPtrAddBasePtr<Dictionary::LocationData>; and
    // GetSerialisedResourceDescriptor @ PS3 0x813E00 reads *(+4)<<8 (SeasonData
    // entry stride 256) and *(+12)<<6 (LocationData entry stride 64) for the
    // payload size. So +8 = SeasonData pointer, +16 = LocationData pointer, +4 =
    // season count, +12 = location count.
    //
    // The fields are kept as load-relative u32 offsets (not typed pointers): the
    // X360 target (b5_main FixUp @ 0x8267E278 / FixDown @ 0x8267E250) rebases them
    // with raw += / -= delta arithmetic — RwPtrAddBasePtr was inlined away on the
    // X360 — and this TU faithfully follows the X360 (the rebase TARGET).
    struct Dictionary
    {
        u32 muVersion;       // +0   on-disk version == 2
        u32 muSeasonCount;   // +4   SeasonData entry count (size = count<<8)
        u32 mpSeasonData;    // +8   rebased pointer (a2[2] += delta)
        u32 muLocationCount; // +12  LocationData entry count (size = count<<6)
        u32 mpLocationData;  // +16  rebased pointer (a2[4] = v8 + delta)
    };

    // FLAG: value 2 taken from the X360 `*a2 != 2` version check in FixUp.
    static const u32 KU_ENVIRONMENT_DICTIONARY_VERSION = 2;

    // Resource registry type id for the environment-settings Dictionary resource
    // (65556 = 0x10014). Recovered verbatim from GetTypeID @ 0x826763A0.
    static const uint32_t KU_ENVIRONMENT_DICTIONARY_RESOURCE_TYPE_ID = 65556;

    uint32_t DictionaryResourceType::GetTypeID() const
    {
        return KU_ENVIRONMENT_DICTIONARY_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x8267D310. Builds the five-entry serialised
    // resource descriptor. The first entry's size is computed from two header fields
    // of the on-disk dictionary; the remaining four are {size = 0, alignment = 1}.
    // The descriptor is returned by value (X360 sret in r3).
    //
    // Store-for-store from the X360:
    //   seasonCount   = *(lpResource + 4)
    //   locationCount = *(lpResource + 12)
    //   size = ((((seasonCount << 8) + 47) & ~0xF) + (locationCount << 6) + 15) & ~0xF
    //   entry[0] = { size, 16 }            entry[1..4] = { 0, 1 }
    // (47 == 0x2F; 15 == 0xF; <<8 == * 256; <<6 == * 64; & ~0xF == round up after +.)
    CgsResource::ResourceDescriptor
    DictionaryResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const Dictionary* lpDictionary = static_cast<const Dictionary*>(lpResource);

        const u32 luSeasonCount   = lpDictionary->muSeasonCount;    // *(a3 + 4)
        const u32 luLocationCount = lpDictionary->muLocationCount;  // *(a3 + 12)

        const u32 luSize =
            ((((luSeasonCount << 8) + 0x2F) & ~0xFu) + (luLocationCount << 6) + 0xF) & ~0xFu;

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        for (u32 luIndex = 1; luIndex < 5; ++luIndex)
        {
            lDescriptor.m_baseResourceDescriptors[luIndex].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1;
        }
        return lDescriptor;
    }

    // FixUp @ 0x8267E278. Validate version, then rebase the two load-relative
    // pointers. The X360 `result` is the EndAssert() artifact (return value of the
    // assert path); the function is void by contract.
    void DictionaryResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        Dictionary* lpDictionary = static_cast<Dictionary*>(lpResource);

        // X360: if (*a2 != 2) { Begin/Fire/EndAssert("...","...cpp",<line>); }
        // The baked file/line is NOT reproduced (CGS_ASSERT injects __FILE__/__LINE__).
        CGS_ASSERT(lpDictionary->muVersion == KU_ENVIRONMENT_DICTIONARY_VERSION,
                   "Incorrect version for Environment Settings Dictionary; get latest code/tools and rebuild data \n");

        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        const u32 luLocationData = lpDictionary->mpLocationData;   // v8 = a2[4] (read before)
        lpDictionary->mpSeasonData   += luDelta;                    // a2[2] += *a3
        lpDictionary->mpLocationData  = luLocationData + luDelta;   // a2[4] = v8 + *a3
    }

    // FixDown @ 0x8267E250. The inverse rebase: un-rebase the two pointers (+16
    // then +8, per the asm). void by contract.
    void DictionaryResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        Dictionary* lpDictionary = static_cast<Dictionary*>(lpResource);

        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        const u32 luSeasonData = lpDictionary->mpSeasonData;   // v2 = *(result + 8) (read before)
        lpDictionary->mpLocationData -= luDelta;                // *(result + 16) -= *a2
        lpDictionary->mpSeasonData    = luSeasonData - luDelta; // *(result + 8) = v2 - *a2
    }
}
}
