#pragma once

#include "GameSource/Replays/BrnReplayBaseSerialiser.h"

namespace BrnReplays
{
    // DecFIGS BrnReplayGameModuleSerialiser.h:57. This serialiser has no added
    // instance fields: the original game driver uses the real base's stream and
    // data-ready/restored state for the SIM TimerStatus snapshot.
    struct GameModuleSerialiser : public BaseSerialiser
    {
        // ARTIST 8264C6A0; DecFIGS declares void. The tail-call's residual r3
        // is not a constructor result in the public interface.
        void Construct();
    };
}
