#include "GameShared/GameClasses/SceneManager/CgsAABBoxBuilder.h"

#include <string.h>

#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"

// ===========================================================================
// CgsSceneManager::AABBoxBuilder -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (dedicated VMX pass wave 2).
//
//   AABBoxBuilder::CreateFromTriangle  @ 0x82B57DA0
//   AABBoxBuilder::CreateFromBox       @ 0x82B57DE0
//   AABBoxBuilder::CreateFromCylinder  @ 0x82B57F20
//
// Register contract (all three): r3 = the output rw::collision::AABBox
// (min row @+0x00, max row @+0x10 -- the committed AABBox.hpp layout),
// r4 = the GPInstance. Hex-Rays's "int result" returns are the untouched-r3
// pass-through artifact; all three are void.
// ===========================================================================

namespace CgsSceneManager
{

namespace
{
    // X360 .data unk_83270F80 -- the three cardinal probe directions handed to
    // GPInstance::mMethods.mGetIntervals, built on first use behind the
    // one-shot flag dword_83270FB0 (bit 0). TU-shared: CreateFromCylinder
    // forwards into CreateFromBox.
    rw::math::vpu::Vector3 gaProbeDirections[3];
    u32                 guProbeDirectionsOnce = 0;   // X360 dword_83270FB0
}

// ===========================================================================
// AABBoxBuilder::CreateFromTriangle @ 0x82B57DA0
//
// Straight 4-lane vminfp/vmaxfp folds of the three vertex rows, loaded from
// r4+0x00 / r4+0x20 / r4+0x30 -- exactly the canonical GPTriangle vertex
// aliases: Vertex0() = mPos (+0x00),
// Vertex1() = mFaceNormals[1] (+0x20), Vertex2() = mFaceNormals[2] (+0x30);
// a GP triangle has mNumFaceNormals == 1 so slots [1]/[2] carry the vertices.
// Both folds run over ALL FOUR lanes (w included). Store/reload order is
// preserved: the min row is stored to r3 FIRST, and only then are the vertex
// rows re-loaded for the max fold (lvx128 @0x82B57DC4/C8/D0 follow the
// stvx128 @0x82B57DC0) -- so the two-pass shape below keeps the exact
// behaviour even if the output box aliased the instance rows. No rodata.
// ===========================================================================
void AABBoxBuilder::CreateFromTriangle(rw::collision::AABBox* lpBBox,
                                       const rw::collision::GPInstance* lpTriangle)
{
    lpBBox->mMin.mV = __vminfp(lpTriangle->mPos.mV,
        __vminfp(lpTriangle->mFaceNormals[1].mV, lpTriangle->mFaceNormals[2].mV));
    lpBBox->mMax.mV = __vmaxfp(lpTriangle->mPos.mV,
        __vmaxfp(lpTriangle->mFaceNormals[1].mV, lpTriangle->mFaceNormals[2].mV));
}

// ===========================================================================
// AABBoxBuilder::CreateFromBox @ 0x82B57DE0
//
// Generic support-projection builder:
//   * Behind a one-shot flag (X360 dword_83270FB0, bit 0) the body builds the
//     three cardinal probe directions into the .data rows unk_83270F80:
//     (1,0,0,0) / (0,1,0,0) / (0,0,1,0). The 1.0f/0.0f immediates are the
//     float-pool loads flt_82001C98 / flt_82001CC0. NOTE the asm sets the
//     flag BEFORE writing the rows (stw @0x82B57E3C precedes the stvx128s)
//     -- the console pattern is not thread-safe and is reproduced in that
//     order.
//   * `lwz r11, 0xAC(r3)` / bctrl with (r3=inst, r4=&dirs, r5=3, r6=&buf) is
//     the batched-interval callback GPInstance.hpp pins at console +0xAC.
//     The stack buffer spans 3 * 0x30 bytes == three Interval records.
//   * Row assembly (both rows identical in shape):
//         vperm  v0, i0.row, i1.row, unk_82CDA350   ; lane0 <- A, lane1 <- B
//         vrlimi128 v0, i2.row, 2, 0                ; lane2 (z) <- i2.row.z
//     giving min = (i0.min, i1.min, i2.min, w) and max likewise. The perm
//     control's SOURCE routing is forced twice over: by AABB semantics
//     (min.x must be the +X-axis projection) and by the same constant's use
//     at 0x827C0360 (perpendicular build from two broadcast operands with
//     the identical vrlimi z-insert).
//
// ARTIST control 82CDA350 is 00010203 14151617 00010203 00010203:
// after inserting interval 2 into z, the four lanes come from A.x, B.y,
// interval 2.z, A.x respectively.
// ===========================================================================
void AABBoxBuilder::CreateFromBox(rw::collision::AABBox* lpBBox,
                                  const rw::collision::GPInstance* lpVolume)
{
    using rw::math::vpu::Vector3;
    if ((guProbeDirectionsOnce & 1u) == 0u)
    {
        guProbeDirectionsOnce |= 1u;
        // Vector3::Set's three-lane fill zeroes each row's w word (ARTIST 0x82B57E58..68).
        Vector3 axisX;
        axisX.Set(1.0f, 0.0f, 0.0f);
        gaProbeDirections[0] = axisX;
        Vector3 axisY;
        axisY.Set(0.0f, 1.0f, 0.0f);
        gaProbeDirections[1] = axisY;
        Vector3 axisZ;
        axisZ.Set(0.0f, 0.0f, 1.0f);
        gaProbeDirections[2] = axisZ;
    }

    rw::collision::Interval laIntervals[3];
    (lpVolume->*(lpVolume->mMethods.mGetIntervals))(gaProbeDirections, 3u, laIntervals);
    lpBBox->mMin = Vector3(laIntervals[0].min, laIntervals[1].min, laIntervals[2].min);
    lpBBox->mMax = Vector3(laIntervals[0].max, laIntervals[1].max, laIntervals[2].max);
}

// ===========================================================================
// AABBoxBuilder::CreateFromCylinder @ 0x82B57F20
//
// The entire X360 body is a single unconditional branch thunk:
//     0x82B57F20  b  CgsSceneManager__AABBoxBuilder__CreateFromBox
// i.e. the cylinder builder IS the generic GetIntervals path (the register
// contract r3=bbox / r4=instance is identical, so the tail branch is exact).
// Reproduced as a plain forwarding call.
// ===========================================================================
void AABBoxBuilder::CreateFromCylinder(rw::collision::AABBox* lpBBox,
                                       const rw::collision::GPInstance* lpVolume)
{
    CreateFromBox(lpBBox, lpVolume);
}

} // namespace CgsSceneManager
