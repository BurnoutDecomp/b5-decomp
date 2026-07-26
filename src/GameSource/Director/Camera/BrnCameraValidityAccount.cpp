#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"

// BrnDirector::Camera::ValidityAccount -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, class:BrnDirector::Camera::ValidityAccount):
//   ValidityAccount::SetFlag @0x82204028

namespace BrnDirector
{
namespace Camera
{

// The fail-flag mask pair CameraState::Clear/Construct consume (DWARF
// BrnCameraValidityAccount.h:169/:172; X360 byte_82FAA5EC / qword_82FAA5D0).
// Zero-initialised .bss state until SetupFailFlagMask (below) runs.
bool                         sbFailFlagMaskSet = false;
CgsContainers::BitArray<32u> sFailFlagMask     = {};

// @ 0x82221118 -- one-time mask setup: sanity-check the flag-name table's
// CONSISTENCY_TEST entry, zero the mask, raise bits [0..E_END_FAILED_FLAG), latch
// the set-up flag. [folded static per convention] the X360 opens with
// `!strcmp(KAAC_FLAG_NAMES[CONSISTENCY_TEST], "CONSISTENCY TEST")` -- a self-check
// of its own static name table (KAAC_FLAG_NAMES is not committed yet; only its
// CONSISTENCY_TEST entry is attested, and the check is a compile-time tautology
// there), so it folds away here. The per-iteration CgsBitArray.h:222 bound assert
// is subsumed by BitArray::SetBit's own guard.
void ValidityAccount::SetupFailFlagMask()
{
    sFailFlagMask.UnSetAll();                       // X360 qword_82FAA5D0 = 0
    for (u32 luFlag = 0; luFlag < static_cast<u32>(E_END_FAILED_FLAG); ++luFlag)
    {
        sFailFlagMask.SetBit(luFlag);               // the 1 << flag OR loop (0..13)
    }
    sbFailFlagMaskSet = true;                       // X360 byte_82FAA5EC = 1
}

// @ 0x82204028 -- range-check the failure reason (h:219; the streamed
// CgsBitArray.h:222 index guard folded static per convention), then raise its bit
// in the u64-backed set (the X360 inlines the BitArray 64-bit-field SetBit).
void ValidityAccount::SetFlag(s32 leFlag)
{
    CGS_ASSERT(leFlag >= E_FIRST_FAILED_FLAG && leFlag < E_END_FAILED_FLAG,
               "leFlag >= E_FIRST_FAILED_FLAG && leFlag < E_END_FAILED_FLAG");   // h:219 (non-gating)
    CGS_ASSERT(static_cast<u32>(leFlag) < 32u,
               "Index < Number of bits");   // CgsBitArray.h:222 (streamed on the X360; folded static)
    mFailedFlags.SetBit(static_cast<u32>(leFlag));
}

}
}
