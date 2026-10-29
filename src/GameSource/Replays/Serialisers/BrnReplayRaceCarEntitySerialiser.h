#pragma once

#include "GameSource/Replays/BrnReplayBaseSerialiser.h"

namespace BrnReplays
{
    struct RaceCarEntitySerialiserStaticLayout;
    struct RaceCarSerialiserFrame;

    // DWARF home: GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.h.
    class RaceCarEntitySerialiser : public BaseSerialiser
    {
    public:
        // @0x8264C4C0
        s32 Construct();
        // @0x822A3400 -- returns the static-layout buffer, asserting it is big enough.
        RaceCarEntitySerialiserStaticLayout* GetStaticLayout();
        // @0x82657A48 -- lazily Clear() both frames on the first frame, mark it done.
        RaceCarSerialiserFrame* CheckStaticLayoutCleared();
        // @0x8264C518 / @0x8264C618 -- playback / record one frame (top-level dispatch).
        s32 Read();
        s32 Write();

    private:
        // X360: byte at this+0x5C, cleared by Construct, set by CheckStaticLayoutCleared
        // once both frames have been Clear()ed for this record/playback session.
        bool mbStaticLayoutCleared; // @0x5C
    };

}
