#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacPlayerVoice.h"

// =============================================================================
// CgsSound::Playback::GenericRwacPlayerVoice::GetCpuTicks  @ 0x826C8040
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See CgsGenericRwacPlayerVoice.h for the
// PARTIAL/MINIMAL home note. Scalar profiling accessor:
//   0x826C8040  lwz   r11, 0x30(r3)   ; mpVoice
//   0x826C8044  cmplwi cr6, r11, 0
//   0x826C8048  beq   cr6, ret_zero   ; if (!mpVoice) return 0.0f
//   0x826C804C  lwz   r11, 0x3C(r11)  ; mpVoice->mu32CpuTicks (u32, zero-extended)
//   0x826C8050  std / lfd / fcfid / frsp f1   ; (f32)(u32 ticks)
//   0x826C8064  lfs   f1, flt_82001CC0(=0.0f) ; the null path
// flt_82001CC0 == 0.0f (the shared .rdata zero float).
// =============================================================================

namespace CgsSound
{
namespace Playback
{

f32 GenericRwacPlayerVoice::GetCpuTicks() const
{
    if (mpVoice == nullptr) // lwz r11,0x30; cmplwi; beq
    {
        return 0.0f; // lfs f1, flt_82001CC0
    }

    // lwz loads the u32 zero-extended; fcfid then converts the (non-negative) integer
    // tick count to single precision.
    return static_cast<f32>(mpVoice->mu32CpuTicks);
}

} // namespace Playback
} // namespace CgsSound
