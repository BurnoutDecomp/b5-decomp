#include "GameSource/Physics/PhysicsUtilities/InterpedParam3.h"

// BrnPhysics::InterpedParam3 -- the two console leaves.
//
// Both are the same shape on X360: three `vperm` lane-inserts into the single 16-byte member,
// driven by control vectors read from a permute-mask table at `unk_8327F140 + lane * 0x40`.
//
// ⚠️ That mask table lives in `.data` and reads ALL ZEROS in the image (it is filled at static
// init), so its bytes cannot be used to name the lanes. The lane mapping below is established
// three independent ways instead, all agreeing:
//
//   1. CgsGeometric::Frustum::SetPlaneByIndex @0x827BAA48 DECODES the table's index arithmetic:
//        _R11 = (luPlaneIndex & 3) << 6      ; lvx128 v7, r11, &unk_8327F140
//      i.e. the 0x40 stride IS the destination lane, and the +0x00/+0x10/+0x20/+0x30 offsets
//      inside one entry select which component of the source vector is taken. So the three
//      controls this file's leaves use -- +0x00, +0x40, +0x80 -- are "insert into lane 0", "lane 1"
//      and "lane 2".
//   2. The DecFIGS DWARF declares the member as `Vector3 mvParams`: three live lanes, `.w` unused.
//   3. The DecFIGS DWARF body hints list exactly THREE
//      `rw::math::vpu::VecFloatRefIndex::operator=` calls in each of Construct and Prepare.
//
// The X360 builds each inserted value the usual way (`stfs` the scalar to a stack slot, `lvlx` +
// `vspltw` to broadcast it, then `vperm` lane 0/1/2 into place); de-SIMD'd that is one scalar
// store per lane.

namespace BrnPhysics
{
    // ---------------------------------------------------------------------------------------
    // Construct  @0x8259CD30 (36 instrs)
    //   asm: lfs f0, flt_82001CC0 (= 0.0f, .rdata); stfs it into all twelve stack floats of the
    //        three scratch vectors; then the same three vperm inserts as Prepare. It is exactly
    //        Prepare(0, 0, 0) with the constant folded in.
    // ---------------------------------------------------------------------------------------
    void InterpedParam3::Construct()
    {
        mvParams.x = 0.0f;   // vperm control unk_8327F140 + 0x00 -> lane 0
        mvParams.y = 0.0f;   // vperm control unk_8327F140 + 0x40 -> lane 1
        mvParams.z = 0.0f;   // vperm control unk_8327F140 + 0x80 -> lane 2
    }

    // ---------------------------------------------------------------------------------------
    // Prepare  @0x8259CDC0 (34 instrs)
    //   asm: stfs f1 -> the four floats of scratch vector A, f2 -> vector B, f3 -> vector C, then
    //        vperm A into lane 0, B into lane 1, C into lane 2.
    //   Parameter names are the Feb-2007 header's (references/Feb-2007/.../InterpedParam3.h:44).
    // ---------------------------------------------------------------------------------------
    void InterpedParam3::Prepare(f32 lParamA, f32 lParamB, f32 lParamC)
    {
        mvParams.x = lParamA;
        mvParams.y = lParamB;
        mvParams.z = lParamC;
    }
}
