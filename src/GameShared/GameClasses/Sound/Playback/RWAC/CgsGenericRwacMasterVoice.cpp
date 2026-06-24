#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacMasterVoice.h"

// =============================================================================
// CgsSound::Playback::GenericRwacMasterVoice::GetCpuTicks  @ 0x826D82D8
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See CgsGenericRwacMasterVoice.h for the
// PARTIAL/MINIMAL home note. Scalar profiling accessor:
//   0x826D82D8  lwz   r11, 0x34(r3)   ; mpVoice
//   0x826D82DC  cmplwi cr6, r11, 0
//   0x826D82E0  beq   cr6, ret_zero   ; if (!mpVoice) return 0.0f
//   0x826D82E4  lwz   r11, 0x3C(r11)  ; mpVoice->mu32CpuTicks (u32, zero-extended)
//   0x826D82E8  std / lfd / fcfid / frsp f1   ; (f32)(u32 ticks)
//   0x826D82FC  lfs   f1, flt_82001CC0(=0.0f) ; the null path
// flt_82001CC0 == 0.0f (the shared .rdata zero float).
// =============================================================================

namespace CgsSound
{
namespace Playback
{

f32 GenericRwacMasterVoice::GetCpuTicks() const
{
    if (mpVoice == nullptr) // lwz r11,0x34; cmplwi; beq
    {
        return 0.0f; // lfs f1, flt_82001CC0
    }

    // lwz loads the u32 zero-extended; fcfid then converts the (non-negative) integer
    // tick count to single precision.
    return static_cast<f32>(mpVoice->mu32CpuTicks);
}

} // namespace Playback
} // namespace CgsSound
