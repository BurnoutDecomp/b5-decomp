#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"

// ============================================================================
// CgsGeometric::AxisAlignedBox -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   CgsGeometric::AxisAlignedBox::Set           @ 0x823A6108   (this TU)
//   CgsGeometric::AxisAlignedBox::ContainsPoint @ 0x828AA398   (this TU)
//
// ContainsPoint's X360 body is hand-vectorised VMX (lvx128 of min/max,
// vcmpgtfp / vcmpgefp per-lane compares, vnot, then a vrlimi128 cross-lane AND
// reduction tested with three vcmpeqfp. self-branches). It carries NO .rdata
// constants -- it is a pure point-in-box predicate, scalarised faithfully here.
// ============================================================================

namespace CgsGeometric
{
    // ------------------------------------------------------------------------
    // Set @ 0x823A6108
    //
    //   li      r11, 0x10
    //   stvx128 v1, r0, r3      ; *(this+0x00) = lvMin   (16-byte vector store)
    //   stvx128 v2, r3, r11     ; *(this+0x10) = lvMax   (16-byte vector store)
    //   blr
    //
    // A trivial two-vector setter: copy the supplied min corner to mMin (+0x00)
    // and the max corner to mMax (+0x10). Each stvx128 writes a full 16 bytes,
    // so all four lanes (xyzw) are copied.
    // ------------------------------------------------------------------------
    void AxisAlignedBox::Set(const Vector4& lvMin, const Vector4& lvMax)
    {
        mMin = lvMin;   // stvx128 v1 -> this+0x00
        mMax = lvMax;   // stvx128 v2 -> this+0x10
    }

    // ------------------------------------------------------------------------
    // ContainsPoint @ 0x828AA398
    //
    //   li        r10, 0x10
    //   lvx128    v0, r0, r3      ; v0  = mMin            (this+0x00)
    //   vcmpgtfp  v0, v1, v0      ; v0  = (point >  mMin) per lane   (strict >)
    //   lvx128    v13, r3, r10    ; v13 = mMax            (this+0x10)
    //   vcmpgefp  v13, v1, v13    ; v13 = (point >= mMax) per lane
    //   vnot      v13, v13        ; v13 = !(point >= mMax) = (point < mMax)
    //   vand ...                  ; per lane: (point > mMin) & (point < mMax)
    //   vrlimi128 ...             ; pack lanes x,y,z; three vcmpeqfp.-vs-0 tests
    //   ...                       ; return 1 iff all three axes are inside
    //
    // Boundary sense read straight off the opcodes: the min edge is a strict
    // vcmpgtfp (> mMin); the max edge is vcmpgefp (>= mMax) NEGATED by vnot,
    // i.e. (< mMax) -- so BOTH edges are strict and the point must lie in the
    // open interval (mMin, mMax) on each of the three axes. (The earlier header
    // note said `<= max`; the asm is strict on the max edge.) NaN on any lane
    // makes both VMX compares false, so a NaN coordinate yields false here too,
    // exactly as the scalar `>` / `<` do. The w lane is unused.
    // ------------------------------------------------------------------------
    bool AxisAlignedBox::ContainsPoint(const Vector4& lvPoint) const
    {
        return lvPoint.x > mMin.x && lvPoint.x < mMax.x    // vcmpgtfp / !vcmpgefp, x lane
            && lvPoint.y > mMin.y && lvPoint.y < mMax.y    // y lane
            && lvPoint.z > mMin.z && lvPoint.z < mMax.z;   // z lane
    }
}
