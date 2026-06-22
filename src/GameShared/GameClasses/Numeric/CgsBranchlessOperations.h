#ifndef GAMESHARED_GAMECLASSES_NUMERIC_CGSBRANCHLESSOPERATIONS_H
#define GAMESHARED_GAMECLASSES_NUMERIC_CGSBRANCHLESSOPERATIONS_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

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
// The bodies are reconstructed from the X360 inlining of these helpers inside
// CgsSound::Utils::IntClamp @ 0x826895D0 (the using-TU this dossier covers). In
// that TU the signed Max/Min are inlined as the sign-mask trick AND each result is
// CGS_ASSERTed against the branchy reference rw::core::stdc::Max/Min. The asserts
// fire from CgsBranchlessOperations.h lines 50 (Max) and 62 (Min), so the inline
// helpers carry those asserts:
//
//   Max(li0, li1):  liDiff   = li0 - li1;
//                   liResult = li0 - ((liDiff >> 31) & liDiff);   // arith shift
//                   CGS_ASSERT(liResult == ((li0 > li1) ? li0 : li1), ...);   // L50
//   Min(li0, li1):  liDiff   = li0 - li1;
//                   liResult = li1 + ((liDiff >> 31) & liDiff);
//                   CGS_ASSERT(liResult == ((li0 < li1) ? li0 : li1), ...);   // L62
//
// The branchless trick: (liDiff >> 31) is 0 when liDiff >= 0 and -1 (all ones)
// when liDiff < 0, so (mask & liDiff) is either 0 or liDiff. The reference
// "rw::core::stdc::Max/Min" the assert strings name is the plain ternary; we
// stringize that ternary into the CGS_ASSERT message verbatim.
//
// All three helpers are inline (header-only -- they have no out-of-line address;
// the X360 only ever inlines them into callers like IntClamp). The uint32_t Min
// (DWARF line 79) is declared/defined for shape completeness; its body mirrors the
// signed Min using the same sign-of-difference mask.
// =============================================================================

namespace CgsNumeric
{

// CgsBranchlessOperations.h:43. Branchless signed maximum (asserts against the
// branchy reference, firing at line 50).
inline s32 Max(s32 li0, s32 li1)
{
    const s32 liDiff   = li0 - li1;
    const s32 liResult = li0 - ((liDiff >> 31) & liDiff);

    CGS_ASSERT(liResult == ((li0 > li1) ? li0 : li1),
               "liResult == rw::core::stdc::Max( li0, li1 )");

    return liResult;
}

// CgsBranchlessOperations.h:55. Branchless signed minimum (asserts against the
// branchy reference, firing at line 62).
inline s32 Min(s32 li0, s32 li1)
{
    const s32 liDiff   = li0 - li1;
    const s32 liResult = li1 + ((liDiff >> 31) & liDiff);

    CGS_ASSERT(liResult == ((li0 < li1) ? li0 : li1),
               "liResult == rw::core::stdc::Min( li0, li1 )");

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
