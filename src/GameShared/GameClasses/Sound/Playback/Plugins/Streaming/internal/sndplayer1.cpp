#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/Plugins/Streaming/internal/sndplayer1shared.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8268FE88
//   (rw::audio::core::SndPlayer1_CgsStreamMod::GetPpuTicksEvent)
//
// The X360 `return *(this + 0x50)` is the embedded plugin timer's tick counter:
// TimerHandle sits at console +0x40 and its mCpuTicks at +0x10 -- read BY NAME.
// (2026-08-25, audio-faithfulness wave 2: the former TU-local `u8 mPad[80] + s32`
// rival struct -- which ODR-collided with sndplayer1shared.cpp's rival in the same
// link -- is retired for the single header home.)

namespace rw { namespace audio { namespace core {

    int SndPlayer1_CgsStreamMod::GetPpuTicksEvent() const
    {
        return static_cast<int>(mTimerHandle.mCpuTicks);
    }

}}}
