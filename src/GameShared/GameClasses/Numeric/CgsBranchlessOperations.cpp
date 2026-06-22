#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826895D0
//   (CgsSound::Utils::IntClamp)
//
// The boot-trace TU whose home is CgsBranchlessOperations.h: a signed integer
// clamp that the X360 builds by INLINING the branchless CgsNumeric::Max then
// CgsNumeric::Min (both defined in CgsBranchlessOperations.h), each guarded by the
// branchless-vs-branchy CGS_ASSERT firing at CgsBranchlessOperations.h lines 50 and
// 62. The Hex-Rays signature shows trailing uninitialised garbage args (a4..a7);
// the call-site asm sets up only r3/r4/r5, so the real signature is
// IntClamp(value, liLow, liHigh).
//
//   liResult = Min( Max(value, liLow), liHigh );
//
// Bodied here (the using-TU) rather than in the shared CgsSoundUtils.cpp home so
// this group only owns its own files; the inline Max/Min carry the asserts, so the
// composition below reproduces both fire sites in store order (Max first, Min
// second), matching the pseudocode.

namespace CgsSound
{
namespace Utils
{

s32 IntClamp(s32 liValue, s32 liLow, s32 liHigh)
{
    // Max(value, low) -- inlined branchless, asserts at CgsBranchlessOperations.h:50.
    const s32 liClampedLow = CgsNumeric::Max(liValue, liLow);

    // Min(<above>, high) -- inlined branchless, asserts at CgsBranchlessOperations.h:62.
    return CgsNumeric::Min(liClampedLow, liHigh);
}

} // namespace Utils
} // namespace CgsSound
