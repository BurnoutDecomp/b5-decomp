#pragma once

#include "GameSource/Replays/BrnReplayBaseSerialiser.h"

namespace BrnReplays
{
    struct RaceCarEntitySerialiserStaticLayout;
    struct RaceCarSerialiserFrame;

    // DWARF home: GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.h.
    // Reconstructed 128-byte alignment closes ARTIST's module serialiser+0x18600,
    // invalidation byte+0x186C0 and World embedding+0x280 with one annotation.
    // The exact original C++ attribute spelling is not independently retained.
    class CGS_ALIGN(128) RaceCarEntitySerialiser : public BaseSerialiser
    {
    public:
        // @0x8264C4C0
        void Construct();
        // @0x822A3400 -- returns the static-layout buffer, asserting it is big enough.
        RaceCarEntitySerialiserStaticLayout* GetStaticLayout();
        // @0x82657A48 -- clear both frames once. Its only original caller
        // 0x8230DA90 does not consume the incidental value left in r3.
        void CheckStaticLayoutCleared();
        // @0x8264C518 / @0x8264C618 -- playback / record one frame (top-level dispatch).
        s32 Read();
        s32 Write();

    private:
        // X360: byte at this+0x5C, cleared by Construct, set by CheckStaticLayoutCleared
        // once both frames have been Clear()ed for this record/playback session.
        bool mbStaticLayoutCleared; // @0x5C
    };

}
