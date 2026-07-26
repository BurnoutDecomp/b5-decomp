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
// FLAG: the producing SetupFailFlagMask @0x82221118 is not reconstructed yet, so
// these carry the pre-setup defaults (flag unset, mask zero) -- exactly the X360
// zero-initialised .bss state before SetupFailFlagMask runs.
bool                         sbFailFlagMaskSet = false;
CgsContainers::BitArray<32u> sFailFlagMask     = {};

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
