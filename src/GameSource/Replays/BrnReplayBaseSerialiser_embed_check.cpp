// Embed check: drive BrnReplays::BaseSerialiser's read/write/seek primitives
// through a full record-then-playback round trip. Compile-only sanity for the
// grown home + bodies (Lock/Unlock/SetMode/Write/Read/IsRecording/IsPlaying).
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/BrnReplayShared.h"

#include <cstring>

using BrnReplays::BaseSerialiser;

namespace
{
    // Test-only subclass: BaseSerialiser keeps its construction path in another TU,
    // so this shim wires up the buffer/id members the primitives read, and exposes
    // SetMode (protected) so the check can move through the mode state machine.
    struct TestSerialiser : public BaseSerialiser
    {
        unsigned char maStorage[64];

        TestSerialiser()
        {
            std::memset(this, 0, sizeof(BaseSerialiser));
            std::memset(maStorage, 0, sizeof(maStorage));

            meMode             = E_MODE_IDLE;
            mbLocked           = false;
            mpBuffer           = maStorage;
            miBufferSize       = static_cast<s32>(sizeof(maStorage));
            miBufferUsed       = 0;
            miBufferRead       = 0;
            meId               = BrnReplays::E_ID_RACECAR_ENTITY;
            meContext          = BrnReplays::E_CONTEXT_NORMAL;
        }

        void DriveMode(EMode leMode) { SetMode(leMode); }
    };
}

int BrnReplayBaseSerialiser_embed_check()
{
    TestSerialiser lSer;

    // --- record path ---
    lSer.Lock();
    lSer.DriveMode(BaseSerialiser::E_MODE_RECORDING);
    if (!lSer.IsRecording())
        return 1;

    const s32 liPayload = 0x12345678;
    if (lSer.Write(&liPayload, static_cast<s32>(sizeof(liPayload)))
            != static_cast<s32>(sizeof(liPayload)))
        return 1;
    lSer.Unlock();

    // --- playback path ---
    lSer.Lock();
    lSer.DriveMode(BaseSerialiser::E_MODE_PLAYING);
    if (!lSer.IsPlaying())
        return 1;

    s32 liReadBack = 0;
    if (lSer.Read(&liReadBack, static_cast<s32>(sizeof(liReadBack)))
            != static_cast<s32>(sizeof(liReadBack)))
        return 1;
    lSer.Unlock();

    return (liReadBack == liPayload) ? 0 : 1;
}
