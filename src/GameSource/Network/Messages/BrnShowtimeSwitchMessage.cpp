#include "types.hpp"

#include "GameSource/Network/Messages/BrnShowtimeSwitchMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::ShowtimeSwitchMessage::Construct            @ 0x8257C3B0
//   BrnNetwork::ShowtimeSwitchMessage::GetName             @ 0x8257C480
//   BrnNetwork::ShowtimeSwitchMessage::GetPackedMessageSize @ 0x8257C3F8
//   BrnNetwork::ShowtimeSwitchMessage::PackOrUnpack         @ 0x8257C408
//   BrnNetwork::ShowtimeSwitchMessage::PrepareForSend       @ 0x8257C490
//   BrnNetwork::ShowtimeSwitchMessage::Retrieve             @ 0x8257F8E0
//
// Reliable showtime enter/leave notification with the final score. An int score plus
// a boolean flag, both serialised after the reliable base's reliable id.

namespace BrnNetwork
{
    void ShowtimeSwitchMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();
        miFinalShowtimeScore = 0;
        mbEnteringShowtime   = false;
    }

    const char* ShowtimeSwitchMessage::GetName() const
    {
        return "Showtime Switch Message";
    }

    s32 ShowtimeSwitchMessage::GetPackedMessageSize()
    {
        miFinalShowtimeScore = 0;
        mbEnteringShowtime   = false;
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult ShowtimeSwitchMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();
        const CgsNetwork::PackOrUnpackResult lxEntering =
            CgsNetwork::PackOrUnpackBool(this, &mbEnteringShowtime);
        return CgsNetwork::PackOrUnpackInt(this, &miFinalShowtimeScore, 0, 0x7FFFFFFF)
             | (lxEntering | lxBase);
    }

    void ShowtimeSwitchMessage::PrepareForSend(u16 lu16FrameCount, s32 liFinalShowtimeScore,
                                               bool lbEnteringShowtime)
    {
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
                   "lu16FrameCount != KU16_INVALID_FRAME");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            miFinalShowtimeScore = liFinalShowtimeScore;
            mbEnteringShowtime   = lbEnteringShowtime;
            CgsNetwork::ReliableMessage::PrepareForSend(33, lu16FrameCount);
        }
    }

    bool ShowtimeSwitchMessage::Retrieve(s32* lpiFinalShowtimeScore, bool* lpbEnteringShowtime)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpiFinalShowtimeScore = miFinalShowtimeScore;
        *lpbEnteringShowtime   = mbEnteringShowtime;
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        return true;
    }
}
