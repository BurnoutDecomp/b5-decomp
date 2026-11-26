#ifndef GAMESHARED_GAMECLASSES_NUMERIC_CGSBRANCHLESSOPERATIONS_H
#define GAMESHARED_GAMECLASSES_NUMERIC_CGSBRANCHLESSOPERATIONS_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// =============================================================================
// CgsBranchlessOperations.h  (OWNING HEADER for CgsNumeric branchless min/max)
//
// Home for the branchless integer Min/Max helpers. The DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Numeric/CgsBranchlessOperations.h)
// attests:
//   CgsBranchlessOperations.h:43  int32_t  Max(int32_t, int32_t);
//   CgsBranchlessOperations.h:55  int32_t  Min(int32_t, int32_t);
//   CgsBranchlessOperations.h:79  uint32_t Min(uint32_t, uint32_t);
//
// ARTIST's IntClamp (0x826895D0) and SetMixerInputValue (0x82680610) inline
// both signed helpers and their diagnostics. The XDK needs __forceinline to
// retain this inlining in SetMixerInputValue. The original subtracts in 64
// bits, then sign-extends the low 32 bits of the difference and shift result
// before masking. The debug checks compare with the corresponding signed Min
// or Max and use the original source lines 62 and 50.
// =============================================================================

namespace CgsNumeric
{

// CgsBranchlessOperations.h:43. Branchless signed maximum (asserts against the
// branchy reference, firing at line 50).
__forceinline s32 Max(s32 li0, s32 li1)
{
    const s64 liWide0 = li0;
    const s64 liWide1 = li1;
    const s64 liDiff = liWide0 - liWide1;
    const s64 liMask = static_cast<s32>(liDiff >> 31);
    const s64 liLowDiff = static_cast<s32>(liDiff);
    const s32 liResult = static_cast<s32>(liWide0 - (liMask & liLowDiff));

    CGS_ASSERT_AT(liResult == ((li0 > li1) ? li0 : li1),
               "liResult == rw::core::stdc::Max( li0, li1 )",
                   "..\\..\\..\\GameShared\\GameClasses\\Numeric/CgsBranchlessOperations.h", 50);

    return liResult;
}

// CgsBranchlessOperations.h:55. Branchless signed minimum (asserts against the
// branchy reference, firing at line 62).
__forceinline s32 Min(s32 li0, s32 li1)
{
    const s64 liWide0 = li0;
    const s64 liWide1 = li1;
    const s64 liDiff = liWide0 - liWide1;
    const s64 liMask = static_cast<s32>(liDiff >> 31);
    const s64 liLowDiff = static_cast<s32>(liDiff);
    const s32 liResult = static_cast<s32>(liWide1 + (liMask & liLowDiff));

    CGS_ASSERT_AT(liResult == ((li0 < li1) ? li0 : li1),
               "liResult == rw::core::stdc::Min( li0, li1 )",
                   "..\\..\\..\\GameShared\\GameClasses\\Numeric/CgsBranchlessOperations.h", 62);

    return liResult;
}

// CgsBranchlessOperations.h:79. Branchless unsigned minimum. Same sign-of-the-
// difference mask, computed in signed space (matches the signed sibling). Modelled
// for shape completeness from the DWARF declaration; no separate using-TU attests
// its body, so it mirrors the signed Min.
inline u32 Min(u32 lu0, u32 lu1)
{
    const s32 liDiff   = static_cast<s32>(lu0) - static_cast<s32>(lu1);
    const u32 luResult = lu1 + (static_cast<u32>(liDiff >> 31) & static_cast<u32>(liDiff));

    CGS_ASSERT(luResult == ((lu0 < lu1) ? lu0 : lu1),
               "luResult == rw::core::stdc::Min( lu0, lu1 )");

    return luResult;
}

} // namespace CgsNumeric

#endif // GAMESHARED_GAMECLASSES_NUMERIC_CGSBRANCHLESSOPERATIONS_H
