#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"

// ============================================================================
// CgsGeometric::AxisAlignedBox -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   CgsGeometric::AxisAlignedBox::Set @ 0x823A6108   (this TU)
//
// AxisAlignedBox::ContainsPoint @ 0x828AA398 is a separate (heavy-VMX) TU and
// is NOT bodied here -- it is BLOCKED.
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
}
