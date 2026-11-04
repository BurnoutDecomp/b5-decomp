#pragma once
#include "types.hpp"

// DecFIGS CgsSnr.h:52; ARTIST 826C21B0 reads miSnrSize at +4.
namespace CgsSound { namespace Playback {
    struct Snr
    {
        void* mpvSnr;
        s32 miSnrSize;
    };
    CGS_STATIC_ASSERT((offsetof(Snr, miSnrSize) == 4), "ARTIST Snr size offset");
} }
