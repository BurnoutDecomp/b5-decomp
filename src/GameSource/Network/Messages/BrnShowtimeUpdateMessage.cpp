#include "types.hpp"

#include "GameSource/Network/Messages/BrnShowtimeUpdateMessage.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::ShowtimeUpdateMessage::Construct       @ 0x8257C340
//   BrnNetwork::ShowtimeUpdateMessage::GetName         @ 0x8257C3A0
//   BrnNetwork::ShowtimeUpdateMessage::PackOrUnpack    @ 0x8257C388
//   BrnNetwork::ShowtimeUpdateMessage::PrepareForSend  @ 0x8257F838
//   BrnNetwork::ShowtimeUpdateMessage::Retrieve        @ 0x8257F8A8
//
// An unreliable per-frame message carrying one player's showtime score. The VALID
// flag (CgsNetwork::KX8_FLAGS_VALID, the inherited Message::mx8Flags bit at +0x19)
// gates send/receive: PrepareForSend only stamps a slot that is not already pending,
// and Retrieve consumes the value and clears the flag.

namespace BrnNetwork
{
    void ShowtimeUpdateMessage::Construct()
    {
        CgsNetwork::Message::Construct();
        miShowtimeScore = 0;
    }

    const char* ShowtimeUpdateMessage::GetName() const
    {
        return "Showtime Update Message";
    }

    // Zero the score, then size through the base (the console body is one folded copy
    // shared with CameraStatusMessage::GetPackedMessageSize).
    s32 ShowtimeUpdateMessage::GetPackedMessageSize()
    {
        miShowtimeScore = 0;
        return CgsNetwork::Message::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult ShowtimeUpdateMessage::PackOrUnpack()
    {
        return CgsNetwork::Message::PackOrUnpack(&miShowtimeScore, 0, 0x7FFFFFFF);
    }

    void ShowtimeUpdateMessage::PrepareForSend(u16 lu16FrameCount, s32 liShowtimeScore)
    {
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
            CgsDev::Assert::MessageAt(
                "lu16FrameCount != KU16_INVALID_FRAME",
                "..\\..\\..\\GameSource\\Network/Messages/BrnShowtimeUpdateMessage.cpp", 124 /* ARTIST 0x8257F864 */));

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            miShowtimeScore = liShowtimeScore;
            CgsNetwork::Message::PrepareForSend(32, lu16FrameCount);
        }
    }

    bool ShowtimeUpdateMessage::Retrieve(s32* lpiShowtimeScore)
    {
        // ARTIST 0x8257F8B8 branches to a trailing `return false` (0x8257F8D8): the copy-out is the body
        // of the valid test.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            *lpiShowtimeScore = miShowtimeScore;
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
            return true;
        }
        return false;
    }
}
