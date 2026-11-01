#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Vector4
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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
//   x = pos.x; if (x <  mMin.mV.x) return 0; if (x >= mMax.mV.x) return 0;
//   z = pos.z; if (z <  mMin.mV.y) return 0; if (z >= mMax.mV.y) return 0;
//   numX = miNumSubRegionsX;
//   ix = (int)((pos.x - mMin.mV.x) * (1.0f / mfSubRegionSize));
//   iz = (int)((pos.z - mMin.mV.y) * (1.0f / mfSubRegionSize));
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
const SubRegionDescriptor* StaticSoundMap::GetSubRegionDescrip(const rw::math::fpu::Vector3Template<float>& lrPosition) const
{
    const f32 lfX = lrPosition.X();
    if (lfX < mMin.mV.x)
    {
        return 0;
    }
    if (!(lfX < mMax.mV.x))
    {
        return 0;
    }

    const f32 lfZ = lrPosition.Z();
    if (lfZ < mMin.mV.y)
    {
        return 0;
    }
    if (!(lfZ < mMax.mV.y))
    {
        return 0;
    }

    const s32 liNumX = miNumSubRegionsX;

    const f32 lfInvCell = 1.0f / mfSubRegionSize;
    const s32 liX = static_cast<s32>((lfX - mMin.mV.x) * lfInvCell);
    const s32 liZ = static_cast<s32>((lfZ - mMin.mV.y) * lfInvCell);

    if (liX >= liNumX || liZ >= miNumSubRegionsZ)
    {
        return 0;
    }

    return &mpSubRegions[liNumX * liZ + liX];
}

// GetEntity @ 0x82675578
//
//   if (!mpEntities)                 ASSERT("mpEntities");
//   if (liEntityIndex >= miNumEntities) ASSERT("liEntityIndex < miNumEntities");
//   return &mpEntities[liEntityIndex];   // 16-byte stride (asm: 16 * index + mpEntities)
//
// The X360 asm reads mpEntities @ +0x30, miNumEntities @ +0x34, and forms the result
// as `16 * liEntityIndex + mpEntities` (`slwi r11, r29, 4; add`). StaticSoundEntity is
// now a COMPLETE 16-byte type (DWARF Vector3Plus record, size static_asserted at the
// definition), so the walk is a real subscript — the former hardcoded byte stride is
// retired (2026-08-25, audio-faithfulness wave 2).
const StaticSoundEntity& StaticSoundMap::GetEntity(s32 liEntityIndex) const
{
    CGS_ASSERT(mpEntities != 0, "mpEntities");
    CGS_ASSERT(liEntityIndex < miNumEntities, "liEntityIndex < miNumEntities");

    return mpEntities[liEntityIndex];
}

// IsInRange @ 0x82677570
//
// Half-open XZ overlap test between a radius-inflated query point (lrPosition's X = lane 0,
// Z = lane 2) and a packed bounds rectangle (lrPackedMinAndMax = {minX, minZ, maxX, maxZ}
// in lanes x/y/z/w). Recovered store-for-store:
//   if (pos.x <  packed.x - radius) return false;   // minX
//   if (pos.x >= packed.z + radius) return false;   // maxX
//   if (pos.z <  packed.y - radius) return false;   // minZ
//   if (pos.z >= packed.w + radius) return false;   // maxZ
//   return true;
// The X360 body never touches `this`; it works purely on its arguments.
bool StaticSoundMap::IsInRange(const rw::math::fpu::Vector3Template<float>& lrPosition, f32 lfRadius,
                               const rw::math::fpu::Vector4Template<float>& lrPackedMinAndMax)
{
    const f32 lfX = lrPosition.X();
    if (lfX < (lrPackedMinAndMax.X() - lfRadius))
    {
        return false;
    }
    if (!(lfX < (lrPackedMinAndMax.Z() + lfRadius)))
    {
        return false;
    }

    const f32 lfZ = lrPosition.Z();
    if (lfZ < (lrPackedMinAndMax.Y() - lfRadius))
    {
        return false;
    }
    if (!(lfZ < (lrPackedMinAndMax.W() + lfRadius)))
    {
        return false;
    }

    return true;
}

}
}
