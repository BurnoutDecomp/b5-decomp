#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3

namespace BrnSound
{
namespace World
{

// GetSubRegionDescrip @ 0x8267AF10
//
// Maps a world-space position to its XZ sub-region cell and returns the cell's
// descriptor, or nullptr when out of range. Recovered store-for-store from the
// X360 asm:
//
//   x = pos.x; if (x <  mMin.x) return 0; if (x >= mMax.x) return 0;
//   z = pos.z; if (z <  mMin.y) return 0; if (z >= mMax.y) return 0;
//   numX = miNumSubRegionsX;
//   ix = (int)((pos.x - mMin.x) * (1.0f / mfSubRegionSize));
//   iz = (int)((pos.z - mMin.y) * (1.0f / mfSubRegionSize));
//   if (ix >= numX || iz >= miNumSubRegionsZ) return 0;
//   return &mpSubRegions[numX * iz + ix];
//
// Notes on the recovery:
//  * The grid is in the world XZ plane. The query vector's lane 0 (x) and lane 2
//    (z) are the two coordinates; mMin/mMax store the X/Z bounds in their .x/.y
//    lanes (the X360 reads bounds at +0x00/+0x04 and +0x10/+0x14).
//  * The reciprocal scale uses the shared rodata constant flt_82001C98, which is
//    the literal 1.0f (`lfs f0, flt_82001C98; fdivs f0, f0, mfSubRegionSize` =
//    1.0f / mfSubRegionSize). The same constant resolves to 1.0f at every other
//    use site, so the value is X360-attested, not assumed.
//  * The float-to-int conversion is the X360 `fctiwz` (truncation toward zero),
//    reproduced with a plain C++ cast to s32.
//  * Both bounds tests are half-open: `< min` and `>= max` both reject, matching
//    the `blt`/`bge` branches; ix/iz are only upper-bounded (`>= count`), so a
//    truncated-negative index cannot arise because the `< min` test already
//    rejected sub-minimum coordinates.
const SubRegionDescriptor* StaticSoundMap::GetSubRegionDescrip(const Vector3& lrPosition) const
{
    const f32 lfX = lrPosition.x;
    if (lfX < mMin.x)
    {
        return 0;
    }
    if (lfX >= mMax.x)
    {
        return 0;
    }

    const f32 lfZ = lrPosition.z;
    if (lfZ < mMin.y)
    {
        return 0;
    }
    if (lfZ >= mMax.y)
    {
        return 0;
    }

    const s32 liNumX = miNumSubRegionsX;

    const f32 lfInvCell = 1.0f / mfSubRegionSize;
    const s32 liX = static_cast<s32>((lfX - mMin.x) * lfInvCell);
    const s32 liZ = static_cast<s32>((lfZ - mMin.y) * lfInvCell);

    if (liX >= liNumX || liZ >= miNumSubRegionsZ)
    {
        return 0;
    }

    return &mpSubRegions[liNumX * liZ + liX];
}

}
}
