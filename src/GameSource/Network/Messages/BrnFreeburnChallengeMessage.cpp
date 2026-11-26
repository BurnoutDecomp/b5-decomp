#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Messages/BrnFreeburnChallengeMessage.cpp"
#include "GameSource/Network/Messages/BrnFreeburnChallengeMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (streamed assert)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::FreeburnChallengeMessage::Construct            @ 0x8257C508
//   BrnNetwork::FreeburnChallengeMessage::GetName             @ 0x8257C650
//   BrnNetwork::FreeburnChallengeMessage::GetPackedMessageSize @ 0x8257C560
//   BrnNetwork::FreeburnChallengeMessage::PackOrUnpack         @ 0x8257C580
//   BrnNetwork::FreeburnChallengeMessage::Retrieve             @ 0x8257F920
//
// The enum fields are constructed with a "none" sentinel: meChallengeStatus uses its
// COUNT (7); meEventType is stored as 7, which is one past this enum's COUNT (6) but is
// the value the X360 build writes (it is also the top of the [0,7] wire range the field
// quantises to), so it is reproduced literally as the invalid sentinel.
//
// PackOrUnpack routes each enum through the shared 32-bit int primitive via a scratch
// int (the quantiser works on s32), OR-ing every field's status together exactly as the
// X360 build does.

namespace BrnNetwork
{
    // Value the X360 ctor writes into meEventType as the "no event" sentinel (one past
    // EChallengeEventType's COUNT; top of the field's [0,7] wire range).
    static const BrnNetworkModuleIO::EChallengeEventType KE_CHALLENGE_EVENT_NONE =
        static_cast<BrnNetworkModuleIO::EChallengeEventType>(7);

    void FreeburnChallengeMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();
        mChallengeID      = 0;
        meEventType       = KE_CHALLENGE_EVENT_NONE;
        meChallengeStatus = BrnGameState::E_CHALLENGE_STATUS_COUNT;
        miActionIndex     = -1;
    }

    const char* FreeburnChallengeMessage::GetName() const
    {
        return "Freeburn Challenge Message";
    }

    s32 FreeburnChallengeMessage::GetPackedMessageSize()
    {
        mChallengeID      = 0;
        meEventType       = KE_CHALLENGE_EVENT_NONE;
        meChallengeStatus = BrnGameState::E_CHALLENGE_STATUS_COUNT;
        miActionIndex     = 0;
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult FreeburnChallengeMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();
        lxResult |= CgsNetwork::Message::PackOrUnpack(&mChallengeID);
        lxResult |= CgsNetwork::Message::PackOrUnpack(&miActionIndex, 0, 2);

        // One scratch int carries both enums (ARTIST 0x8257C5E0 / 0x8257C60C and PS3 0x64634C /
        // 0x64637C reuse the same stack slot).
        s32 liEnumValue = meEventType;
        lxResult |= CgsNetwork::Message::PackOrUnpack(&liEnumValue, 0, 7);
        meEventType = static_cast<BrnNetworkModuleIO::EChallengeEventType>(liEnumValue);

        liEnumValue = meChallengeStatus;
        lxResult |= CgsNetwork::Message::PackOrUnpack(&liEnumValue, 0, 7);
        meChallengeStatus = static_cast<BrnGameState::EChallengeStatus>(liEnumValue);
        return lxResult;
    }

    // Stamp a challenge event for sending. An event still waiting to go out is reported (it
    // is overwritten either way), then the reliable base stamps type 34 for the frame.
    void FreeburnChallengeMessage::PrepareForSend(u16 lu16FrameCount, CgsID lChallengeID,
                                                  BrnNetworkModuleIO::EChallengeEventType leEventType,
                                                  BrnGameState::EChallengeStatus leChallengeStatus,
                                                  s32 liActionIndex)
    {
        const s32 KI_FREEBURN_CHALLENGE_MESSAGE_TYPE = 34;

        CGS_ASSERT_AT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
                      "lu16FrameCount != KU16_INVALID_FRAME", CGS_ARTIST_FILE, 150);

        // ARTIST 0x8257C6C4: a streamed assert (the message is built in the shared assert buffer,
        // 0x82F32260), line 152.
        CGS_ASSERT_STREAM_AT(!IsMessageValid(),
                             "Haven't yet sent event type : " << meEventType
                             << " but trying to send new event type : " << leEventType << "\n",
                             CGS_ARTIST_FILE, 152);

        mChallengeID      = lChallengeID;
        meEventType       = leEventType;
        meChallengeStatus = leChallengeStatus;
        miActionIndex     = liActionIndex;
        CgsNetwork::ReliableMessage::PrepareForSend(KI_FREEBURN_CHALLENGE_MESSAGE_TYPE, lu16FrameCount);
    }

    bool FreeburnChallengeMessage::Retrieve(CgsID* lpChallengeID,
                                            BrnNetworkModuleIO::EChallengeEventType* lpeEventType,
                                            BrnGameState::EChallengeStatus* lpeChallengeStatus,
                                            s32* lpiActionIndex)
    {
        CGS_ASSERT_AT(lpChallengeID,
            "lpChallengeID",
            CGS_ARTIST_FILE, 178 /* ARTIST 0x8257F964 */);
        CGS_ASSERT_AT(lpeEventType,
            "lpeEventType",
            CGS_ARTIST_FILE, 179 /* ARTIST 0x8257F988 */);
        CGS_ASSERT_AT(lpeChallengeStatus,
            "lpeChallengeStatus",
            CGS_ARTIST_FILE, 180 /* ARTIST 0x8257F9AC */);

        // ARTIST 0x8257F9C0 branches to a trailing `return false` (0x8257F9FC): the copy-out is the body
        // of the valid test.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            *lpChallengeID       = mChallengeID;
            *lpeEventType        = meEventType;
            *lpeChallengeStatus  = meChallengeStatus;
            *lpiActionIndex      = miActionIndex;
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
            return true;
        }
        return false;
    }
}

#undef CGS_ARTIST_FILE
