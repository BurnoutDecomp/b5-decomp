#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8268FE88
//   (rw::audio::core::SndPlayer1_CgsStreamMod::GetPpuTicksEvent)
//
// Behaviour-faithful to the X360 pseudocode:
//     return *(this + 80);          // 32-bit field at byte offset 0x50

namespace rw { namespace audio { namespace core {

    struct SndPlayer1_CgsStreamMod
    {
        u8   mPad[80];          // [0x00] opaque
        s32  miPpuTicksEvent;   // [0x50] ppu-ticks event handle/value

        int GetPpuTicksEvent() const;
    };

    int SndPlayer1_CgsStreamMod::GetPpuTicksEvent() const
    {
        return miPpuTicksEvent;
    }

}}}
