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
    // The environment-settings Dictionary resource layout is not committed; the
    // DecFIGS DWARF gives no member names for this slice. Only the relocation slice
    // is modelled — the two pointer fields the FixUp/FixDown rebase arithmetic
    // touches (a2[2] @+8, a2[4] @+16), stored as load-relative u32 offsets rebased
    // by += / -= delta (like VehicleGraphicsSpec / EnvironmentTimeLine).
    //
    // FLAG: every member name except muVersion is INFERRED from the rebase pattern;
    //       muField4 (+4) and muField12 (+12) are NOT touched by either function
    //       (non-rebased values, modelled only to place mpField8/mpField16 at their
    //       offsets). The true semantics of the two pointers are deferred.
    struct Dictionary
    {
        u32 muVersion;   // +0   on-disk version == 2
        u32 muField4;    // +4   not rebased — FLAG
        u32 mpField8;    // +8   rebased pointer (a2[2] += *a3)
        u32 muField12;   // +12  not rebased — FLAG
        u32 mpField16;   // +16  rebased pointer (a2[4] = v8 + *a3)
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

        const u32 luField16 = lpDictionary->mpField16;   // v8 = a2[4] (read before)
        lpDictionary->mpField8  += luDelta;               // a2[2] += *a3
        lpDictionary->mpField16  = luField16 + luDelta;   // a2[4] = v8 + *a3
    }

    // FixDown @ 0x8267E250. The inverse rebase: un-rebase the two pointers (+16
    // then +8, per the asm). void by contract.
    void DictionaryResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        Dictionary* lpDictionary = static_cast<Dictionary*>(lpResource);

        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        const u32 luField8 = lpDictionary->mpField8;      // v2 = *(result + 8) (read before)
        lpDictionary->mpField16 -= luDelta;               // *(result + 16) -= *a2
        lpDictionary->mpField8   = luField8 - luDelta;    // *(result + 8) = v2 - *a2
    }
}
}
