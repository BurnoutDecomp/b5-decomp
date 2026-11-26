// ===================================================================================
// CgsNetwork::SignalMessage  -- PrepareAck / PrepareNack bodies
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsNetwork::SignalMessage::PrepareAck  @ 0x8286F658
//       (called by CgsNetwork::PlayerManager::AcceptMessage)
//   CgsNetwork::SignalMessage::PrepareNack @ 0x8286F758
//       (called by CgsNetwork::PlayerManager::ThrowAwayMessage,
//        CgsNetwork::ReliableMessageManager::FakeNackMessage)
//
// A SignalMessage is an ack/nack control message aimed at a sending/receiving player
// pair (the inherited MessageWithPlayerIDs fields at +0x20 / +0x24). Both Prepare*
// take the originating message `lpMessage` whose type + frame they echo, plus the two
// player ids. They:
//   1. assert this message is not already valid (mx8Flags & VALID == 0)
//      ("!IsMessageValid()", CgsSignalMessage.h)
//   2. (Nack only) assert neither id is the K_INVALID_PLAYER_ID sentinel (-1)
//   3. assert lSendingPlayerID >= 0 and lRecvingPlayerID >= 0
//      (CgsMessageWithPlayerIDs.h:123 / :185), storing each into mSendingPlayerID /
//      mRecvingPlayerID (the asm stw's at 0x20 / 0x24 happen between the two id asserts)
//   4. (Ack only) assert lpMessage's frame is not KU16_INVALID_FRAME
//   5. read lpMessage's type (mi8Type @ +0x1A, sign-extended) and frame (mu16Frame @
//      +0x1C), fetch its game-id via lpMessage->GetGameID(), and chain to the base
//      Message::PrepareAck / PrepareNack(type, frame, gameID).
//
// The store-for-store match to the asm:
//   PrepareAck  @0x8286F658: assert VALID==0; assert send>=0; stw send@0x20;
//     assert recv>=0; stw recv@0x24; assert lpMessage->frame != 0xFFFF;
//     type=extsb(lpMessage->mi8Type); frame=lpMessage->mu16Frame;
//     gameID=lpMessage->GetGameID(); Message::PrepareAck(type,frame,gameID).
//   PrepareNack @0x8286F758: assert VALID==0; assert send!=-1; assert recv!=-1;
//     assert send>=0; stw send@0x20; assert recv>=0; stw recv@0x24;
//     type/frame/gameID as above; Message::PrepareNack(type,frame,gameID).
// All members are referenced by name through the committed base layout.
//
// PackOrUnpack (header-carrying field list) is bodied here as well.

#include "GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#define CGS_ARTIST_SIGNAL_MESSAGE "..\\..\\..\\GameShared\\GameClasses\\Network/Packeting/Messages/CgsSignalMessage.h"

namespace CgsNetwork
{

    // ARTIST 0x8286F658.
    void SignalMessage::PrepareAck(Message* lpMessage,
                                   NetworkPlayerID lSendingPlayerID,
                                   NetworkPlayerID lRecvingPlayerID)
    {
        CGS_ASSERT_AT(!IsMessageValid(),
            "!IsMessageValid()", CGS_ARTIST_SIGNAL_MESSAGE, 98);
        SetSendingPlayerID(lSendingPlayerID);
        SetRecvingPlayerID(lRecvingPlayerID);
        CGS_ASSERT_AT(lpMessage->GetU16Frame() != KU16_INVALID_FRAME,
            "lpMessage->GetU16Frame() != KU16_INVALID_FRAME", CGS_ARTIST_SIGNAL_MESSAGE, 103);

        Message::PrepareAck(lpMessage->GetType(), lpMessage->GetU16Frame(), lpMessage->GetGameID());
    }

    // ARTIST 0x8286F758.
    void SignalMessage::PrepareNack(Message* lpMessage,
                                    NetworkPlayerID lSendingPlayerID,
                                    NetworkPlayerID lRecvingPlayerID)
    {
        CGS_ASSERT_AT(!IsMessageValid(),
            "!IsMessageValid()", CGS_ARTIST_SIGNAL_MESSAGE, 126);
        CGS_ASSERT_AT(lSendingPlayerID != KI_INVALID_PLAYER_ID,
            "lSendingPlayerID != K_INVALID_PLAYER_ID", CGS_ARTIST_SIGNAL_MESSAGE, 127);
        CGS_ASSERT_AT(lRecvingPlayerID != KI_INVALID_PLAYER_ID,
            "lRecvingPlayerID != K_INVALID_PLAYER_ID", CGS_ARTIST_SIGNAL_MESSAGE, 128);
        SetSendingPlayerID(lSendingPlayerID);
        SetRecvingPlayerID(lRecvingPlayerID);

        Message::PrepareNack(lpMessage->GetType(), lpMessage->GetU16Frame(), lpMessage->GetGameID());
    }

    // A signal carries the header of the message it acknowledges, so every base scalar
    // travels: the game id and flag byte as full u8s, the type in
    // [0, E_MESSAGE_TYPE_COUNT], the frame in [0, 65534], then both player ids in
    // [-1, 0x7FFFFFFF]. Each field's status is OR-accumulated.
    PackOrUnpackResult SignalMessage::PackOrUnpack()
    {
        PackOrUnpackResult lxResult = CgsNetwork::Message::PackOrUnpack(&mu8GameID, 0, 0xFF);
        lxResult = CgsNetwork::Message::PackOrUnpack(&mx8Flags, 0, 0xFF) | lxResult;
        lxResult = static_cast<PackOrUnpackResult>(
                       Message::PackOrUnpack(&mi8Type, 0, KI_E_MESSAGE_TYPE_COUNT)) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&mu16Frame, 0, 0xFFFE) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&mSendingPlayerID, -1, 0x7FFFFFFF) | lxResult;
        return CgsNetwork::Message::PackOrUnpack(&mRecvingPlayerID, -1, 0x7FFFFFFF) | lxResult;
    }

    // The packed size of a signal: every header field it carries is zeroed first (the
    // game id, flag and type bytes, the frame word and both player ids), then the base
    // packs it into its scratch buffer.
    s32 SignalMessage::GetPackedMessageSize()
    {
        mu8GameID        = 0;
        mx8Flags         = 0;
        mi8Type          = 0;
        mu16Frame        = 0;
        mSendingPlayerID = 0;
        mRecvingPlayerID = 0;
        return Message::GetPackedMessageSize();
    }
} // namespace CgsNetwork

#undef CGS_ARTIST_SIGNAL_MESSAGE
