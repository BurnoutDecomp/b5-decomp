#include "SDKs/XAudio/XAudioReverbEffect.h"

// ===========================================================================
// XAUDIO::CReverbEffect -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioReverbEffect.h for the per-function status: this wave lands the one
// fully-groundable, layout-independent member (GetInfo). The remaining eight
// ledger functions depend on the un-recovered princeton_digital DSP room layout
// and/or the CEffect/reverb-interface vtable order and are blocked there.
// No reference source and no DWARF exist for this TU; the PowerPC asm is
// authoritative.
// ===========================================================================

namespace XAUDIO
{

// @ 0x8295F410:
//   li  r11, 6        ; the +0x00 byte constant
//   li  r10, 0xEA6    ; the +0x02 halfword constant (3750)
//   li  r3, 0         ; return 0
//   stb r11, 0(r4)    ; apInfo->muInfoByte0 = 6
//   sth r10, 2(r4)    ; apInfo->muInfoWord2 = 3750
//   blr
s32 CReverbEffect::GetInfo(SReverbEffectInfo* apInfo)
{
    apInfo->muInfoByte0 = 6;
    apInfo->muInfoWord2 = 3750;
    return 0;
}

} // namespace XAUDIO
