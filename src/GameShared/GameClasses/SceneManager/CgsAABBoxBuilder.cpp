#include "GameShared/GameClasses/SceneManager/CgsAABBoxBuilder.h"

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
    // vminfp / vmaxfp: per-lane minimum / maximum over all four lanes.
    inline f32 LaneMin(f32 afA, f32 afB) { return (afA < afB) ? afA : afB; }
    inline f32 LaneMax(f32 afA, f32 afB) { return (afA > afB) ? afA : afB; }

    inline void ReadLanes(const rw::collision::Vec4& arRow, f32 (&arfLanes)[4])
    {
        arfLanes[0] = arRow.x;
        arfLanes[1] = arRow.y;
        arfLanes[2] = arRow.z;
        arfLanes[3] = arRow.w;
    }

    // X360 .data unk_83270F80 -- the three cardinal probe directions handed to
    // GPInstance::mMethods.mGetIntervals, built on first use behind the
    // one-shot flag dword_83270FB0 (bit 0). TU-shared: CreateFromCylinder
    // forwards into CreateFromBox.
    rw::collision::Vec4 gaProbeDirections[3];
    u32                 guProbeDirectionsOnce = 0;   // X360 dword_83270FB0
}

// ===========================================================================
// AABBoxBuilder::CreateFromTriangle @ 0x82B57DA0
//
// Straight 4-lane vminfp/vmaxfp folds of the three vertex rows, loaded from
// r4+0x00 / r4+0x20 / r4+0x30 -- exactly the canonical GPTriangle vertex
// aliases (Feb-2007 rwccore.h:1229-1231): Vertex0() = mPos (+0x00),
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
    f32 lafV0[4];
    f32 lafV1[4];
    f32 lafV2[4];

    // --- min row (stored first, @ r3+0x00) ---------------------------------
    // lvx128 vert0/vert2/vert1; vminfp v13, v12(vert1), v13(vert2);
    // vminfp v0, v0(vert0), v13; stvx128 @r3.
    ReadLanes(lpTriangle->mPos,            lafV0);   // r4+0x00  Vertex0
    ReadLanes(lpTriangle->mFaceNormals[1], lafV1);   // r4+0x20  Vertex1
    ReadLanes(lpTriangle->mFaceNormals[2], lafV2);   // r4+0x30  Vertex2
    for (u32 luLane = 0; luLane < 4; ++luLane)
    {
        lpBBox->mMin.mV.mafLane[luLane] =
            LaneMin(lafV0[luLane], LaneMin(lafV1[luLane], lafV2[luLane]));
    }

    // --- max row (vertex rows RE-loaded after the min store, @ r3+0x10) ----
    // lvx128 vert1/vert2; vmaxfp v0, v13(vert1), v0(vert2);
    // lvx128 vert0; vmaxfp v0, v13(vert0), v0; stvx128 @r3+0x10.
    ReadLanes(lpTriangle->mFaceNormals[1], lafV1);
    ReadLanes(lpTriangle->mFaceNormals[2], lafV2);
    ReadLanes(lpTriangle->mPos,            lafV0);
    for (u32 luLane = 0; luLane < 4; ++luLane)
    {
        lpBBox->mMax.mV.mafLane[luLane] =
            LaneMax(lafV0[luLane], LaneMax(lafV1[luLane], lafV2[luLane]));
    }
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
// FLAG -- w lane only: the 16 bytes of the perm control unk_82CDA350 are not
// in the export (un-homed across the whole tree). Interval.min/.max are
// lane-broadcast VecFloats (GPInstance.hpp), so every candidate pick for the
// w lane is a duplicate of either interval[0]'s or interval[1]'s scalar;
// this body takes w from interval[1] (the canonical "X from operand A, YZW
// from operand B" control). x/y/z are exact regardless.
// ===========================================================================
void AABBoxBuilder::CreateFromBox(rw::collision::AABBox* lpBBox,
                                  const rw::collision::GPInstance* lpVolume)
{
    // One-shot build of the probe rows (asm order kept: flag first, rows
    // second -- stw dword_83270FB0 @0x82B57E3C precedes the stvx128 stores).
    if ((guProbeDirectionsOnce & 1u) == 0u)
    {
        guProbeDirectionsOnce |= 1u;

        // flt_82001C98 = 1.0f, flt_82001CC0 = 0.0f (float-pool immediates).
        gaProbeDirections[0].x = 1.0f;   // unk_83270F80 + 0x00: (1,0,0,0)
        gaProbeDirections[0].y = 0.0f;
        gaProbeDirections[0].z = 0.0f;
        gaProbeDirections[0].w = 0.0f;

        gaProbeDirections[1].x = 0.0f;   // unk_83270F80 + 0x10: (0,1,0,0)
        gaProbeDirections[1].y = 1.0f;
        gaProbeDirections[1].z = 0.0f;
        gaProbeDirections[1].w = 0.0f;

        gaProbeDirections[2].x = 0.0f;   // unk_83270F80 + 0x20: (0,0,1,0)
        gaProbeDirections[2].y = 0.0f;
        gaProbeDirections[2].z = 1.0f;
        gaProbeDirections[2].w = 0.0f;
    }

    // `lwz r11, 0xAC(r3)` / bctrl -- the instance's cached batched-interval
    // callback (GPInstance::VolumeMethods::mGetIntervals, console +0xAC):
    // one projection interval per probe direction, Interval stride 0x30.
    rw::collision::Interval laIntervals[3];
    lpVolume->mMethods.mGetIntervals(lpVolume, gaProbeDirections, 3u, laIntervals);

    // min row @ bbox+0x00: vperm(unk_82CDA350)(i0.min, i1.min) then
    // vrlimi128 ...,2,0 inserts z from i2.min. Interval bounds are broadcast
    // VecFloats, so each pick below reads that interval's scalar.
    lpBBox->mMin.mV.mafLane[0] = laIntervals[0].min.x;
    lpBBox->mMin.mV.mafLane[1] = laIntervals[1].min.y;
    lpBBox->mMin.mV.mafLane[2] = laIntervals[2].min.z;
    lpBBox->mMin.mV.mafLane[3] = laIntervals[1].min.w;   // FLAGGED: w-lane source
                                                         // (i1 per the canonical
                                                         // control; i0 the only
                                                         // alternative -- both
                                                         // broadcast duplicates)

    // max row @ bbox+0x10: same shuffle over the interval maxima.
    lpBBox->mMax.mV.mafLane[0] = laIntervals[0].max.x;
    lpBBox->mMax.mV.mafLane[1] = laIntervals[1].max.y;
    lpBBox->mMax.mV.mafLane[2] = laIntervals[2].max.z;
    lpBBox->mMax.mV.mafLane[3] = laIntervals[1].max.w;   // FLAGGED: as above
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
