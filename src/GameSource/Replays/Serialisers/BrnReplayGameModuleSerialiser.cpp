#include "GameSource/Replays/Serialisers/BrnReplayGameModuleSerialiser.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnReplays::GameModuleSerialiser)
//
//   Construct @ 0x8264C6A0:
//       BrnReplays::BaseSerialiser::Construct(
//                  this, 5, 0, 1024, 1024, "GameModule", 0);
//
// Parameterises the real shared BaseSerialiser with channel id 5, 1 KiB stream
// and static buffers, and the "GameModule" name. No local base/layout fork.

namespace BrnReplays
{
    void GameModuleSerialiser::Construct()
    {
        BaseSerialiser::Construct(static_cast<ESerialiserId>(5), E_CONTEXT_NORMAL,
                                 1024, 1024, "GameModule", false);
    }
}
