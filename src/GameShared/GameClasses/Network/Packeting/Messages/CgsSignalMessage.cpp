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

#include "GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsNetwork
{
    // X360 0x8286F658.
    void SignalMessage::PrepareAck(Message* lpMessage,
                                   NetworkPlayerID liSendingPlayerID,
                                   NetworkPlayerID liRecvingPlayerID)
    {
        CGS_ASSERT((mx8Flags & KX8_FLAGS_VALID) == 0, "!IsMessageValid()");

        CGS_ASSERT(liSendingPlayerID >= 0, "lSendingPlayerID>=0");
        mSendingPlayerID = liSendingPlayerID;

        CGS_ASSERT(liRecvingPlayerID >= 0, "lRecvingPlayerID>=0");
        mRecvingPlayerID = liRecvingPlayerID;

        CGS_ASSERT(lpMessage->mu16Frame != KU16_INVALID_FRAME,
                   "lpMessage->GetU16Frame() != KU16_INVALID_FRAME");

        const s32 liType  = static_cast<s32>(lpMessage->mi8Type);
        const u16 lu16Frame = lpMessage->mu16Frame;
        const u8  lu8GameID = lpMessage->GetGameID();

        Message::PrepareAck(liType, lu16Frame, lu8GameID);
    }

    // X360 0x8286F758.
    void SignalMessage::PrepareNack(Message* lpMessage,
                                    NetworkPlayerID liSendingPlayerID,
                                    NetworkPlayerID liRecvingPlayerID)
    {
        CGS_ASSERT((mx8Flags & KX8_FLAGS_VALID) == 0, "!IsMessageValid()");

        CGS_ASSERT(liSendingPlayerID != KI_INVALID_PLAYER_ID,
                   "lSendingPlayerID != K_INVALID_PLAYER_ID");
        CGS_ASSERT(liRecvingPlayerID != KI_INVALID_PLAYER_ID,
                   "lRecvingPlayerID != K_INVALID_PLAYER_ID");

        CGS_ASSERT(liSendingPlayerID >= 0, "lSendingPlayerID>=0");
        mSendingPlayerID = liSendingPlayerID;

        CGS_ASSERT(liRecvingPlayerID >= 0, "lRecvingPlayerID>=0");
        mRecvingPlayerID = liRecvingPlayerID;

        const s32 liType  = static_cast<s32>(lpMessage->mi8Type);
        const u16 lu16Frame = lpMessage->mu16Frame;
        const u8  lu8GameID = lpMessage->GetGameID();

        Message::PrepareNack(liType, lu16Frame, lu8GameID);
    }
} // namespace CgsNetwork
