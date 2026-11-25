#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "SDKs/EATech/include/rw/math/vpu/vector_intrinsic_operation.h"

// ============================================================================
// CgsGeometric::AxisAlignedBox -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   CgsGeometric::AxisAlignedBox::Set           @ 0x823A6108   (this TU)
//   CgsGeometric::AxisAlignedBox::ContainsPoint @ 0x828AA398   (this TU)
//
// The point predicate preserves the original VMX mask and lane operations.
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
    void AxisAlignedBox::Set(Vector3 lvMin, Vector3 lvMax)
    {
        mMin = lvMin;   // stvx128 v1 -> this+0x00
        mMax = lvMax;   // stvx128 v2 -> this+0x10
    }

    // ARTIST 0x828AA398 uses ordered lower bounds and the complement
    // of the ordered upper-bound compare before checking x, y and z.
    bool AxisAlignedBox::ContainsPoint(Vector3 lvPoint) const
    {
        using rw::math::vpu::VectorIntrinsic;
        using rw::math::vpu::_asmIsEqualV4;
        const VectorIntrinsic lvLower = __vcmpgtfp(lvPoint.mV, mMin.mV);
        const VectorIntrinsic lvZero = __vspltisw(0);
        const VectorIntrinsic lvTooHigh = __vcmpgefp(lvPoint.mV, mMax.mV);
        const VectorIntrinsic lvLowerX = __vspltw(lvLower, 0);
        const VectorIntrinsic lvLowerY = __vspltw(lvLower, 1);
        const VectorIntrinsic lvLowerZ = __vspltw(lvLower, 2);
        const VectorIntrinsic lvUpper = __vnor(lvTooHigh, lvTooHigh);
        const VectorIntrinsic lvUpperX = __vspltw(lvUpper, 0);
        const VectorIntrinsic lvUpperY = __vspltw(lvUpper, 1);
        const VectorIntrinsic lvUpperZ = __vspltw(lvUpper, 2);

        const VectorIntrinsic lvInsideX = __vand(lvLowerX, lvUpperX);
        const VectorIntrinsic lvInsideY = __vand(lvLowerY, lvUpperY);
        const VectorIntrinsic lvInsideZ = __vand(lvLowerZ, lvUpperZ);
        // Assemble each splatted mask with the original x/y/z lane inserts.
        VectorIntrinsic lvInside = __vrlimi(lvZero, lvInsideX, 8, 0);
        lvInside = __vrlimi(lvInside, lvInsideY, 4, 3);
        lvInside = __vrlimi(lvInside, lvInsideZ, 2, 2);
        return !_asmIsEqualV4(__vspltw(lvInside, 0), lvZero) &&
               !_asmIsEqualV4(__vspltw(lvInside, 1), lvZero) &&
               !_asmIsEqualV4(__vspltw(lvInside, 2), lvZero);
    }

}
