#include "SharedClasses/World/BrnEnvironmentDictionaryResourceType.h"

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "SharedClasses/World/BrnEnvironmentDictionary.h"   // BrnWorld::EnvironmentSettings::Dictionary (owning header)
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::DictionaryResourceType::GetTypeID @ 0x826763A0
//   BrnWorld::EnvironmentSettings::DictionaryResourceType::GetSerialisedResourceDescriptor @ 0x8267D310
//   BrnWorld::EnvironmentSettings::DictionaryResourceType::FixDown   @ 0x8267E250
//   BrnWorld::EnvironmentSettings::DictionaryResourceType::FixUp     @ 0x8267E278
//
// FixUp/FixDown rebase the Dictionary's two array pointers by the rw::Resource's load
// base (the relocation delta). FixUp validates the on-disk version first. Sibling of
// BrnVehicleGraphicsSpec / EnvironmentTimeLine.
//
// 2026-08-16 (env wave, step 9): the former LOCAL `struct Dictionary` of u32 offset
// slots is RETIRED in favour of the owning header
// SharedClasses/World/BrnEnvironmentDictionary.h (DWARF-named members, HOST pointers).
// On x64 muLocationCnt is at +0x10 and mpLocationDatii at +0x18, NOT the console's
// +0x0C/+0x10 -- reading them at the console offsets is exactly the STREETDATA.DAT
// defect. See that header's guest-vs-host table.


namespace BrnWorld
{
namespace EnvironmentSettings
{
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
    //   size = ((((muSeasonCnt << 8) + 47) & ~0xF) + (muLocationCnt << 6) + 15) & ~0xF
    //   entry[0] = { size, 16 }            entry[1..4] = { 0, 1 }
    // (<<8 == sizeof(SeasonData) == 256; <<6 == sizeof(LocationData) == 64; 47 == the
    // 16-aligned start of the season array (0x20) + 0xF; 15/& ~0xF == round up.)
    //
    // ⚠️ THE FORMULA SURVIVES THE x64 RELAYOUT UNCHANGED, and that is not luck: the
    // header grows 20 -> 32 bytes but align16(20) == align16(32) == 32, so the season
    // array still starts at +0x20, and BOTH element records are pointer-free char
    // arrays whose strides (256 / 64) do not move. The Dictionary payload is therefore
    // byte-for-byte the SAME SIZE before and after the relayout (MEASURED: 352 bytes
    // for the retail 1-season/1-location DICTIONARY.BUNDLE.x360). If a future header
    // change pushes sizeof(Dictionary) past 32, this constant and
    // env_transcode.py::_relayout_dictionary must move together --
    // Dictionary::_AssertLayout() is the tripwire.
    CgsResource::ResourceDescriptor
    DictionaryResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const Dictionary* lpDictionary = static_cast<const Dictionary*>(lpResource);

        const u32 luSeasonCount   = lpDictionary->muSeasonCnt;      // *(a3 + 4)
        const u32 luLocationCount = lpDictionary->muLocationCnt;    // *(a3 + 12) console / +0x10 host

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

    // FixUp @ 0x8267E278. Validate version, then rebase the two array pointers. The
    // X360 `result` is the EndAssert() artifact (return value of the assert path);
    // the function is void by contract.
    //
    // x64 delta: GetLoadBase64 -- both slots are HOST pointers in the relaid-out image
    // (env_transcode.py::_relayout_dictionary); the 32-bit form truncates the x64 heap
    // base. Same treatment as BrnStreetDataResourceType / CgsLanguageResourceType.
    void DictionaryResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        Dictionary* lpDictionary = static_cast<Dictionary*>(lpResource);

        // X360: if (*a2 != 2) { Begin/Fire/EndAssert("...","...cpp",212); }
        // The baked file/line 212 is NOT reproduced (CGS_ASSERT injects __FILE__/__LINE__).
        CGS_ASSERT(lpDictionary->muVersion == KU_ENVIRONMENT_DICTIONARY_VERSION,
                   "Incorrect version for Environment Settings Dictionary; get latest code/tools and rebuild data \n");

        const uintptr_t luDelta = CgsResource::GetLoadBase64(lrResource);

        // v8 = a2[4] (read before) ; a2[2] += *a3 ; a2[4] = v8 + *a3
        Dictionary::LocationData* lpLocationDatii = lpDictionary->mpLocationDatii;

        lpDictionary->mpSeasonDatii = reinterpret_cast<Dictionary::SeasonData*>(
            reinterpret_cast<uintptr_t>(lpDictionary->mpSeasonDatii) + luDelta);
        lpDictionary->mpLocationDatii = reinterpret_cast<Dictionary::LocationData*>(
            reinterpret_cast<uintptr_t>(lpLocationDatii) + luDelta);
    }

    // FixDown @ 0x8267E250. The inverse rebase: un-rebase the two pointers (+0x10 then
    // +8 on the console, mpLocationDatii then mpSeasonDatii by name). void by contract.
    void DictionaryResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        Dictionary* lpDictionary = static_cast<Dictionary*>(lpResource);

        const uintptr_t luDelta = CgsResource::GetLoadBase64(lrResource);

        // v2 = *(result + 8) (read before) ; *(result+16) -= *a2 ; *(result+8) = v2 - *a2
        Dictionary::SeasonData* lpSeasonDatii = lpDictionary->mpSeasonDatii;

        lpDictionary->mpLocationDatii = reinterpret_cast<Dictionary::LocationData*>(
            reinterpret_cast<uintptr_t>(lpDictionary->mpLocationDatii) - luDelta);
        lpDictionary->mpSeasonDatii = reinterpret_cast<Dictionary::SeasonData*>(
            reinterpret_cast<uintptr_t>(lpSeasonDatii) - luDelta);
    }
}
}
