#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44Affine / Vector3

// ============================================================================
// SDKs/Packages/ICE/ICECameraSpaceHandler.cpp
//
// ICE::CameraSpaceHandler::TransformToWorld(Vector3, eICESpace) const
// @0x82533DE8 -- map a point from the given ICE reference space into world
// space. Fetches the space's affine take matrix via GetTransformToWorld(leSpace)
// (a Matrix44Affine returned by value), then applies the AFFINE point transform:
//
//     world = xAxis * p.x + yAxis * p.y + zAxis * p.z + wAxis
//
// i.e. rotate by the 3x3 (rows xAxis/yAxis/zAxis) and add the translation row
// (wAxis). This EXTENDS the CameraSpaceHandler home; the type, the eight take
// matrices, and the GetTransformToWorld declaration live in
// ICECameraSpaceHandler.hpp.
//
// X360 shape (@0x82533DE8): the asm broadcasts the point's x/y/z lanes
// (vspltw128 v,vec,0/1/2) and FMAs (vmaddfp) the four 16-byte matrix rows loaded
// at offsets 0x00/0x10/0x20/0x30 of the returned Matrix44Affine -- exactly the
// four named Vector3 rows xAxis/yAxis/zAxis/wAxis. The DWARF renders the whole
// body as a single rw::math::vpu::TransformPoint(...) call; that vendor free
// function is not present in our reconstructed rwmath type header (which carries
// the layout/vocabulary only -- see vendor/renderware/include/rw/math/vpu/types.h),
// so we reproduce its semantics here by the named-lane affine transform rather
// than forking the vendor header. Per-lane f32 math gives exact semantic parity
// with the vspltw128 + vmaddfp accumulation.
// ============================================================================

namespace ICE
{

Vector3 CameraSpaceHandler::TransformToWorld(Vector3 lvPoint, eICESpace leSpace) const
{
    // The reference-space-to-world affine for the requested space (by value).
    const Matrix44Affine lMatrix = GetTransformToWorld(leSpace);
    // FLAG: GetTransformToWorld is declaration-only in the home; its body lives in
    // a sibling TU. Declaration-only is sufficient for the per-TU `cl /c` gate
    // (no link), and the X360 caller is a plain `bl` into that body.

    // Affine point transform: rotate by the 3x3 (xAxis/yAxis/zAxis rows) and add
    // the translation row (wAxis). Equivalent to the X360
    // vspltw128(x/y/z) + vmaddfp(row0..row3) sequence.
    const f32 lfX = lvPoint.x;
    const f32 lfY = lvPoint.y;
    const f32 lfZ = lvPoint.z;

    Vector3 lvWorld;
    lvWorld.x = lMatrix.xAxis.x * lfX + lMatrix.yAxis.x * lfY + lMatrix.zAxis.x * lfZ + lMatrix.wAxis.x;
    lvWorld.y = lMatrix.xAxis.y * lfX + lMatrix.yAxis.y * lfY + lMatrix.zAxis.y * lfZ + lMatrix.wAxis.y;
    lvWorld.z = lMatrix.xAxis.z * lfX + lMatrix.yAxis.z * lfY + lMatrix.zAxis.z * lfZ + lMatrix.wAxis.z;
    lvWorld.w = lvPoint.w;   // 4th lane carried through (unused for a point)

    return lvWorld;
}

} // namespace ICE
