#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h
//
// BrnReplays::PropEntitySerialiser -- the prop-entity replay serialiser (DWARF home:
// GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h).
//
// HOMED 2026-08-12 (prop-spawn wave): this class declaration previously lived INSIDE
// BrnReplayPropEntitySerialiser.cpp, so no other TU could call it. The prop cell manager
// needs it -- the shipped X360 PropCellManager::AddPropToScene / RemovePropFromScene /
// AddPropPartsToScene / RemovePropPartsFromScene all take a PropEntitySerialiser* and
// call SetPropAddedToScene on it when not playing back -- so per the project rule
// ("reconstruct includes; don't fake them; never locally re-declare a type that has a
// real home") the declaration moved here and the .cpp now includes it. No member,
// signature or body changed.
#include "types.hpp"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"

namespace BrnReplays
{
    class PropEntitySerialiser : public BaseSerialiser
    {
    public:
        // @0x8264C6C0
        s32 Construct();
        // @0x822A4090 -- returns the static-layout buffer, asserting it is big enough.
        PropSerialiserStaticLayout* GetStaticLayout();

        // [FLAG PC boot gate] NOT an X360 symbol -- see the long note on the body in
        // BrnReplayPropEntitySerialiser.cpp. True once the replay manager has handed this
        // serialiser its static buffer; on this build nothing ever does, so the four
        // record-side entry points no-op instead of dereferencing a null layout.
        bool HasStaticLayout();
        // @0x8265A8E8 / @0x8265AA40 -- playback / record one frame.
        s32 Read(PropSerialiserStaticLayout* lpStaticLayout);
        s32 Write(PropSerialiserStaticLayout* lpStaticLayout);
        // @0x822CD650 -- lazily clear the previous frame on the first frame after a
        // (re)start, then mark it initialised.
        bool CheckPreviousFrameCleared();
        // @0x822E7738 / @0x822CD5B8 / @0x822CD790 -- zone bookkeeping (record-side only).
        PropLoadedZoneRecord* AddLoadedZone(s32 liZoneId);
        void RemoveLoadedZone(s32 liZoneId);
        void RemoveAllLoadedZones();
        // @0x822CD700 -- forward a prop's scene-membership change to the live frame.
        void SetPropAddedToScene(s32 liArg2, u32 luArg3, s32 liArg4);

    private:
        // True once the previous-frame slot has been cleared for this recording/playback
        // session. X360: byte at this+0x5C, cleared by Construct, set by
        // CheckPreviousFrameCleared. (BaseSerialiser occupies this+0x00..0x5A.)
        bool mbPreviousFrameInitialized; // @0x5C
    };
}
