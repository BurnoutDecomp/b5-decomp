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

    // --- record path (via the Serialise dispatch) ---
    lSer.Lock();
    lSer.DriveMode(BaseSerialiser::E_MODE_RECORDING);
    if (!lSer.IsRecording())
        return 1;

    const s32 liPayload = 0x12345678;
    // Serialise must route to Write while recording and report the byte count.
    if (lSer.Serialise(const_cast<s32*>(&liPayload), static_cast<s32>(sizeof(liPayload)))
            != static_cast<s32>(sizeof(liPayload)))
        return 1;
    lSer.Unlock();

    // --- playback path (via the Serialise dispatch) ---
    lSer.Lock();
    lSer.DriveMode(BaseSerialiser::E_MODE_PLAYING);
    if (!lSer.IsPlaying())
        return 1;

    s32 liReadBack = 0;
    // Serialise must route to Read while playing.
    if (lSer.Serialise(&liReadBack, static_cast<s32>(sizeof(liReadBack)))
            != static_cast<s32>(sizeof(liReadBack)))
        return 1;
    lSer.Unlock();

    // --- idle path: Serialise is a no-op outside record/playback ---
    lSer.Lock();
    lSer.DriveMode(BaseSerialiser::E_MODE_IDLE);
    lSer.Unlock();
    s32 liUnused = 0;
    if (lSer.Serialise(&liUnused, static_cast<s32>(sizeof(liUnused))) != 0)
        return 1;

    return (liReadBack == liPayload) ? 0 : 1;
}
