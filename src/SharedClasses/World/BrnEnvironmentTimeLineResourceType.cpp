#include "SharedClasses/World/BrnEnvironmentTimeLineResourceType.h"

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "SharedClasses/World/BrnEnvironmentTimeLine.h"   // BrnWorld::EnvironmentSettings::TimeLine (owning header)
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::TimeLineResourceType::GetTypeID @ 0x82676390
//   BrnWorld::EnvironmentSettings::TimeLineResourceType::FixDown   @ 0x8267E0C8
//   BrnWorld::EnvironmentSettings::TimeLineResourceType::FixUp     @ 0x8267E128
//
// FixUp/FixDown rebase the TimeLine's location-array pointer and each location's two
// array pointers by the rw::Resource's load base (the relocation delta). FixUp
// validates the on-disk version first and, after rebasing each location, NULLs that
// location's keyframe pointer array (the import pass then fills it).
//
// ⭐ 2026-08-16 (env wave, step 9). The former LOCAL `struct TimeLine` / `TimeLineEntry`
// models -- u32 offset slots walked with the console's 12-byte stride and cast to host
// pointers by a local PointerFromU32 -- are RETIRED in favour of the owning header
// SharedClasses/World/BrnEnvironmentTimeLine.h (DWARF-named members, HOST pointers).
// The console strides/offsets do not hold on x64: LocationData is 24 bytes, not 12, and
// mppKeyframes elements are 8 bytes, not 4. See that header's guest-vs-host table.


namespace BrnWorld
{
namespace EnvironmentSettings
{
    // Resource registry type id for the environment-settings TimeLine resource
    // (65555 = 0x10013). Recovered verbatim from GetTypeID @ 0x82676390.
    static const uint32_t KU_ENVIRONMENT_TIMELINE_RESOURCE_TYPE_ID = 65555;

    uint32_t TimeLineResourceType::GetTypeID() const
    {
        return KU_ENVIRONMENT_TIMELINE_RESOURCE_TYPE_ID;
    }

    // FixUp @ 0x8267E128. Validate version, rebase the location array, then for each
    // location rebase its two array pointers and NULL its keyframe pointer array.
    // The X360 `result` is the EndAssert() artifact (return value of the assert path);
    // the function is void by contract.
    //
    // x64 delta: GetLoadBase64. Every slot this touches is a HOST pointer in the
    // relaid-out image (env_transcode.py::_relayout_timeline), and the x64 resource
    // load base does not fit in the console's 32-bit int -- truncating it would send
    // mpLocationDatii to a wild address on the very first store. Same treatment as
    // BrnStreetDataResourceType / CgsLanguageResourceType.
    void TimeLineResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        TimeLine* lpTimeLine = static_cast<TimeLine*>(lpResource);

        // X360: if (*a2 != 1) { Begin/Fire/EndAssert("...","...cpp",307); }
        // The baked file/line 307 is NOT reproduced (CGS_ASSERT injects __FILE__/__LINE__).
        CGS_ASSERT(lpTimeLine->muVersion == KU_ENVIRONMENT_TIMELINE_VERSION,
                   "Incorrect version for Environment Settings TimeLine; get latest code/tools and rebuild data \n");

        const uintptr_t luDelta = CgsResource::GetLoadBase64(lrResource);

        const u32 luLocationCnt = lpTimeLine->muLocationCnt;   // v8 = a2[1] (read once, before)

        // a2[2] += *a3 -- BEFORE the loop.
        lpTimeLine->mpLocationDatii = reinterpret_cast<TimeLine::LocationData*>(
            reinterpret_cast<uintptr_t>(lpTimeLine->mpLocationDatii) + luDelta);

        for (u32 luLocation = 0; luLocation < luLocationCnt; ++luLocation)   // do/while v10, v9 stride 12 (host 24)
        {
            TimeLine::LocationData& lrLocation = lpTimeLine->mpLocationDatii[luLocation];   // v11 = a2[2] + v9

            const u32 luKeyframeCnt = lrLocation.muKeyframeCnt;   // v13 = *v11 (read BEFORE the stores)

            // v11[2] += *a3 ; v11[1] = v12 + *a3   (the X360 stores +8 first, then +4)
            lrLocation.mppKeyframes = reinterpret_cast<Keyframe**>(
                reinterpret_cast<uintptr_t>(lrLocation.mppKeyframes) + luDelta);
            lrLocation.mpfKeyframeTimes = reinterpret_cast<f32*>(
                reinterpret_cast<uintptr_t>(lrLocation.mpfKeyframeTimes) + luDelta);

            // The X360 inner loop stores muKeyframeCnt zero WORDS over mppKeyframes
            // (`stwx r27, r8, r9`, r9 += 4) -- i.e. it NULLs one pointer slot per
            // keyframe. On x64 a slot is 8 bytes, so the faithful form is a null per
            // ELEMENT, never `count` 4-byte stores. CgsResource::Pool::
            // ResolveImportsForEntry then fills them from the bundle's import table.
            for (u32 luKeyframe = 0; luKeyframe < luKeyframeCnt; ++luKeyframe)   // if (!v14) do/while --v13
                lrLocation.mppKeyframes[luKeyframe] = 0;
        }
    }

    // FixDown @ 0x8267E0C8. The inverse rebase: while mpLocationDatii is still the
    // runtime pointer, un-rebase each location's two pointers, THEN un-rebase
    // mpLocationDatii after the loop. Does NOT touch the keyframe pointer array (no
    // inverse of the FixUp NULLing). void by contract.
    void TimeLineResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        TimeLine* lpTimeLine = static_cast<TimeLine*>(lpResource);

        const uintptr_t luDelta = CgsResource::GetLoadBase64(lrResource);

        const u32 luLocationCnt = lpTimeLine->muLocationCnt;   // v2 = *(result + 4)

        for (u32 luLocation = 0; luLocation < luLocationCnt; ++luLocation)   // do/while v2, v3 stride 12 (host 24)
        {
            // *(result + 8) (mpLocationDatii) is still the runtime pointer here.
            TimeLine::LocationData& lrLocation = lpTimeLine->mpLocationDatii[luLocation];   // v8 = v7 + *(result + 8)

            lrLocation.mpfKeyframeTimes = reinterpret_cast<f32*>(                 // *(v8 + 4) -= *a2
                reinterpret_cast<uintptr_t>(lrLocation.mpfKeyframeTimes) - luDelta);
            lrLocation.mppKeyframes = reinterpret_cast<Keyframe**>(               // *(v8 + 8) -= *a2
                reinterpret_cast<uintptr_t>(lrLocation.mppKeyframes) - luDelta);
        }

        // *(result + 8) -= *a2 -- AFTER the loop.
        lpTimeLine->mpLocationDatii = reinterpret_cast<TimeLine::LocationData*>(
            reinterpret_cast<uintptr_t>(lpTimeLine->mpLocationDatii) - luDelta);
    }
}
}
