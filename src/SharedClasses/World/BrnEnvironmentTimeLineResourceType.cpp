#include "SharedClasses/World/BrnEnvironmentTimeLineResourceType.h"

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::TimeLineResourceType::GetTypeID @ 0x82676390
//   BrnWorld::EnvironmentSettings::TimeLineResourceType::FixDown   @ 0x8267E0C8
//   BrnWorld::EnvironmentSettings::TimeLineResourceType::FixUp     @ 0x8267E128
//
// FixUp/FixDown rebase the TimeLine's entry-array pointer and each entry's two
// load-relative pointer fields by the rw::Resource's load base (the relocation
// delta). FixUp validates the on-disk version first and, after rebasing each
// entry, zeroes that entry's sub-array of element ids.

namespace BrnWorld
{
namespace EnvironmentSettings
{
    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

    // The environment-settings TimeLine resource layout is not committed; the
    // X360 ARTIST DWARF gives no member names for this slice. Only the relocation
    // slice is modelled here — the fields the FixUp/FixDown rebase arithmetic
    // touches, with pointers stored as load-relative u32 offsets (rebased by
    // += / -= delta like VehicleList::mpEntries / VehicleGraphicsSpec).
    //
    // The entry is the guest's BrnWorld::EnvironmentSettings::TimeLine::LocationData
    // (12-byte stride); the field TYPES below are recovered from the DecFIGS PS3
    // FixUp/FixDown, which carries the real templated relocation helpers:
    //   rw::RwPtrAddBasePtr<float>(*(v12 + 4), ...)            -> +4 is float*
    //   rw::RwPtrAddBasePtr<TimeLine::Keyframe*>(*(v12 + 8), ) -> +8 is Keyframe**
    //   rw::RwPtrAddBasePtr<TimeLine::LocationData>(*(v3 + 8)) -> +8 of TimeLine is the entry array
    // (PS3 0x819E34 FixUp / 0x80E7B8 FixDown). The pointers are kept as load-relative
    // u32 offsets here because the rebase is plain pointer arithmetic on PC.
    //
    // FLAG: member NAMES are inferred (the DWARF names the relocation templates, not
    //       the struct fields); the TYPES are the PS3-recovered ones above. The
    //       Keyframe element struct and the keyframe-times array contents are not
    //       otherwise modelled (only the rebase slice matters for FixUp/FixDown).
    struct TimeLineEntry   // guest TimeLine::LocationData
    {
        s32 miElementCount;   // +0   element count (read before rebase; v16 = *v12)
        u32 mpKeyframeTimes;  // +4   rebased float* array (RwPtrAddBasePtr<float>)
        u32 mpElements;       // +8   rebased Keyframe** array (RwPtrAddBasePtr<Keyframe*>),
                              //      then the FixUp zero-loop clears miElementCount u32s here
    };

    struct TimeLine
    {
        u32 muVersion;        // +0   on-disk version == 1
        s32 miCount;          // +4   number of TimeLineEntry in mpEntries
        u32 mpEntries;        // +8   rebased TimeLine::LocationData* -> TimeLineEntry[miCount]
    };

    // FLAG: value 1 taken from the X360 `*a2 != 1` version check in FixUp.
    static const u32 KU_ENVIRONMENT_TIMELINE_VERSION = 1;

    // Resource registry type id for the environment-settings TimeLine resource
    // (65555 = 0x10013). Recovered verbatim from GetTypeID @ 0x82676390.
    static const uint32_t KU_ENVIRONMENT_TIMELINE_RESOURCE_TYPE_ID = 65555;

    uint32_t TimeLineResourceType::GetTypeID() const
    {
        return KU_ENVIRONMENT_TIMELINE_RESOURCE_TYPE_ID;
    }

    // FixUp @ 0x8267E128. Validate version, rebase the entry array, then for each
    // entry rebase its two pointers and zero its element sub-array. The X360
    // `result` is the EndAssert() artifact (return value of the assert path); the
    // function is void by contract.
    void TimeLineResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        TimeLine* lpTimeLine = static_cast<TimeLine*>(lpResource);

        // X360: if (*a2 != 1) { Begin/Fire/EndAssert("...","...cpp",307); }
        // The baked file/line 307 is NOT reproduced (CGS_ASSERT injects __FILE__/__LINE__).
        CGS_ASSERT(lpTimeLine->muVersion == KU_ENVIRONMENT_TIMELINE_VERSION,
                   "Incorrect version for Environment Settings TimeLine; get latest code/tools and rebuild data \n");

        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        lpTimeLine->mpEntries += luDelta;   // a2[2] += *a3 (BEFORE the loop)

        if (lpTimeLine->miCount)            // if (v8 = a2[1])
        {
            TimeLineEntry* lpEntries = PointerFromU32<TimeLineEntry>(lpTimeLine->mpEntries);
            for (s32 li = 0; li < lpTimeLine->miCount; ++li)   // do/while v10=count, v9 stride 12
            {
                TimeLineEntry* lpEntry = &lpEntries[li];        // v11 = a2[2] + v9

                const s32 liElementCount = lpEntry->miElementCount;     // v13 = *v11   (read before)
                const u32 luKeyframeTimes = lpEntry->mpKeyframeTimes;   // v12 = v11[1] (read before)

                lpEntry->mpElements     += luDelta;                 // v11[2] += *a3
                lpEntry->mpKeyframeTimes = luKeyframeTimes + luDelta; // v11[1] = v12 + *a3

                if (liElementCount != 0)                        // if (!v14), v14 = (*v11 == 0)
                {
                    u32* lpElements = PointerFromU32<u32>(lpEntry->mpElements);
                    for (s32 lj = 0; lj < liElementCount; ++lj) // inner do/while --v13, v15 += 4
                        lpElements[lj] = 0;                     // *(v11[2] + v15) = 0
                }
            }
        }
    }

    // FixDown @ 0x8267E0C8. The inverse rebase: while mpEntries is still the
    // runtime pointer, un-rebase each entry's two pointers, THEN un-rebase
    // mpEntries after the loop. Does NOT touch/zero the element sub-array (no
    // inverse of the FixUp zeroing). void by contract.
    void TimeLineResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        TimeLine* lpTimeLine = static_cast<TimeLine*>(lpResource);

        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        if (lpTimeLine->miCount)            // if (v2 = *(result + 4))
        {
            // *(result + 8) (mpEntries) is still the runtime pointer here.
            TimeLineEntry* lpEntries = PointerFromU32<TimeLineEntry>(lpTimeLine->mpEntries);
            for (s32 li = 0; li < lpTimeLine->miCount; ++li)   // do/while v2, v3 stride 12
            {
                TimeLineEntry* lpEntry = &lpEntries[li];        // v8 = v7 + *(result + 8)
                lpEntry->mpKeyframeTimes -= luDelta;            // *(v8 + 4) -= *a2 (RwPtrSubtractBasePtr<float>)
                lpEntry->mpElements      -= luDelta;            // *(v8 + 8) -= *a2 (RwPtrSubtractBasePtr<Keyframe*>)
            }
        }

        lpTimeLine->mpEntries -= luDelta;   // *(result + 8) -= *a2 (AFTER the loop)
    }
}
}
