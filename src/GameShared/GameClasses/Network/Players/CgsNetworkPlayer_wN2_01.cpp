#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // round-robin turn, connection status
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"     // arrived-callback argument
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"          // CgsSystem::TimerStatus
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                         // CgsDev::Log::gpDebugPrint

// CgsNetwork::NetworkPlayer -- the per-frame received-message expiry, the ping exchange, the
// receive path and the ack/nack delivery.

namespace CgsNetwork
{

s32 KI_FRAMES_TO_DISCARD_MESSAGE_RECEIVED_DATA = 10800;

namespace
{
    // The acks and nacks one received packet can carry (the receive path's local array).
    const s32 KI_MAX_SIGNAL_MESSAGES_TO_RECEIVE = 20;
}

// ---- UpdateMessagesReceived ----------------------------------------------------------
// Count down each receive slot's validity window; a slot reaching zero is logged and its
// remembered frame forgotten.
void NetworkPlayer::UpdateMessagesReceived()
{
    for (s32 liIndex = 0; liIndex < KI_MAX_MESSAGE_TYPES; ++liIndex)
    {
        NetMessageData& lData = maRecvMessageData[liIndex];
        if (lData.miValidCountdown <= 0)
        {
            continue;
        }
        if (--lData.miValidCountdown > 0)
        {
            continue;
        }

        CGS_ASSERT(lData.miValidCountdown == 0, "maRecvMessageData[liIndex].miValidCountdown == 0");
        *CgsDev::Log::gpDebugPrint << "Removing message received data for message type "
                                   << lData.meType << " with frame "
                                   << static_cast<s32>(lData.mu16Frame) << "\n";
        lData.mu16Frame        = 0xFFFF;
        lData.miValidCountdown = -1;
    }
}

// ---- UpdatePing ----------------------------------------------------------------------
// Once connected: on our round-robin turn send a ping stamped with the current time; answer
// a pending ping (on our turn, else keep it pending); take a received ping's time as the
// packet time and answer or queue it; a received reply sets the round trip in ms.
void NetworkPlayer::UpdatePing(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame,
                               bool lbInGame)
{
    if (mpPlayerManager->mConnectionManager.GetConnectionStatus(mPlayerID) != E_CONNECTION_SUCCESS)
    {
        return;
    }

    if (mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(mPlayerID, lbInGame, 0))
    {
        mPingMessageSend.PrepareForSend(lu16CurrentFrame, lpTimerStatus->GetTime().GetFloatVal());
    }

    f32 lfPingTime;
    if (mfPingToReplyTo > 0.0f)
    {
        if (mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(mPlayerID, lbInGame, 0))
        {
            mPingReplyMessageSend.PrepareForSend(lu16CurrentFrame, mfPingToReplyTo);
            mfPingToReplyTo = 0.0f;
        }
    }
    else if (mPingMessageRecv.Retrieve(&lfPingTime))
    {
        mTimeLastPacketReceived = lpTimerStatus->GetTime();
        if (mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(mPlayerID, lbInGame, 0))
        {
            mPingReplyMessageSend.PrepareForSend(lu16CurrentFrame, lfPingTime);
        }
        else
        {
            mfPingToReplyTo = lfPingTime;
        }
    }

    if (mPingReplyMessageRecv.Retrieve(&lfPingTime))
    {
        mTimeLastPacketReceived = lpTimerStatus->GetTime();
        mfPingInMs = (lpTimerStatus->GetTime().GetFloatVal() - lfPingTime) * 1000.0f;
    }
}

// ---- ReceiveAckOrNack ----------------------------------------------------------------
// Route an ack or nack to the delivery callback registered for its message type (true for
// an ack), with the id of the player it was addressed to.
void NetworkPlayer::ReceiveAckOrNack(SignalMessage* lpMessage, bool lbAck)
{
    CGS_ASSERT((lpMessage->mx8Flags & KX8_FLAGS_ACK) != 0 || (lpMessage->mx8Flags & KX8_FLAGS_NACK) != 0,
               "lpMessage->IsAck() || lpMessage->IsNack()");

    for (s32 liIndex = 0; liIndex < miNumberMessagesRegistered; ++liIndex)
    {
        NetMessageData& lData = maRecvMessageData[liIndex];
        if (lpMessage->GetType() != lData.meType)
        {
            continue;
        }

        if (lData.mpfMsgDeliveredCallback != nullptr)
        {
            void* lpUserData = lData.mpCallbackUserData;
            if ((lpMessage->mx8Flags & KX8_FLAGS_ACK) != 0)
            {
                lData.mpfMsgDeliveredCallback(true, lbAck, lpMessage,
                                              lpMessage->GetRecvingPlayerID(), lpUserData);
            }
            else
            {
                CGS_ASSERT((lpMessage->mx8Flags & KX8_FLAGS_NACK) != 0, "lpMessage->IsNack()");
                lData.mpfMsgDeliveredCallback(false, lbAck, lpMessage,
                                              lpMessage->GetRecvingPlayerID(), lpUserData);
            }
        }
        return;
    }
}

// ---- ReceiveMessage ------------------------------------------------------------------
// Unpack one received packet: every registered receive message is offered to the packer
// (stamped with its registered type, delivered through OnMessageUnpackedCallback), the
// packet's acks and nacks land in a local signal array, and each ack / nack our own
// reliable messages were waiting for is passed to the delivery callback.
void NetworkPlayer::ReceiveMessage(u8* lpacMessageData, s32 liMessageSize,
                                   NetworkPlayerID lSendingPlayerID)
{
    (void)lSendingPlayerID;

    SignalMessage laSignalMessages[KI_MAX_SIGNAL_MESSAGES_TO_RECEIVE];
    SignalMessage* lapSignalMessages[KI_MAX_SIGNAL_MESSAGES_TO_RECEIVE];
    CompressionAndEncryptionUtils::RecvMessageData laRecvMessageData[KI_MAX_MESSAGE_TYPES];
    s32 liNumRecvMessageData = 0;

    CGS_ASSERT(lpacMessageData, "lpacMessageData");

    for (s32 liIndex = 0; liIndex < miNumberMessagesRegistered; ++liIndex)
    {
        Message* lpMsg = maRecvMessageData[liIndex].mpMsg;
        laRecvMessageData[liIndex].mpMessage = lpMsg;
        lpMsg->SetType(maRecvMessageData[liIndex].meType);
        laRecvMessageData[liIndex].mpMsgUnpackedCallback = OnMessageUnpackedCallback;
        laRecvMessageData[liIndex].mpCallbackUserData    = this;
        ++liNumRecvMessageData;
    }

    for (s32 liIndex = 0; liIndex < KI_MAX_SIGNAL_MESSAGES_TO_RECEIVE; ++liIndex)
    {
        laSignalMessages[liIndex].Construct();
        lapSignalMessages[liIndex] = &laSignalMessages[liIndex];
    }

    mPacketPacker.UnPack(mPlayerID, laRecvMessageData, liNumRecvMessageData, lapSignalMessages,
                         KI_MAX_SIGNAL_MESSAGES_TO_RECEIVE, lpacMessageData, liMessageSize);

    for (s32 liIndex = 0; liIndex < KI_MAX_SIGNAL_MESSAGES_TO_RECEIVE; ++liIndex)
    {
        SignalMessage* lpSignalMessage = &laSignalMessages[liIndex];
        if (!lpSignalMessage->IsMessageValid())
        {
            continue;
        }

        if (mpPlayerManager->CheckForAck(lpSignalMessage)
            || mpPlayerManager->CheckForNack(lpSignalMessage))
        {
            ReceiveAckOrNack(lpSignalMessage, false);
        }
    }
}

// ---- OnMessageUnpackedCallback -------------------------------------------------------
// A message from another game is logged and thrown away. Otherwise the registry accepts it;
// a duplicate reliable message is dropped; a reliable message goes to its arrived callback
// (and stays valid); an unreliable one is kept only when it is newer than the last one of
// its type, which restarts the slot's remembered-frame window.
void NetworkPlayer::OnMessageUnpackedCallback(Message* lpMsg, void* lpUserData)
{
    NetworkPlayer* lpNetPlayer = static_cast<NetworkPlayer*>(lpUserData);

    CGS_ASSERT(lpMsg->IsMessageValid(), "lpMsg->IsMessageValid()");
    const u8 lu8GameID = lpMsg->GetGameID();
    CGS_ASSERT(lu8GameID != KU8_INVALID_GAME_ID, "lu8GameID != KU8_INVALID_GAME_ID");

    const u8 lu8MyGameID = lpNetPlayer->mpPlayerManager->GetGameID();
    if (lu8GameID != lu8MyGameID)
    {
        *CgsDev::Log::gpDebugPrint << "My game ID " << static_cast<s32>(lu8MyGameID)
                                   << ", msg game ID " << static_cast<s32>(lu8GameID)
                                   << ", player ID " << lpNetPlayer->mPlayerID << "\n";
        lpNetPlayer->mpPlayerManager->ThrowAwayMessage(lpMsg);
        lpMsg->SetMessageInvalid();
        return;
    }

    lpNetPlayer->mpPlayerManager->AcceptMessage(lpMsg);
    if (lpNetPlayer->mpPlayerManager->mReliableMessageManager.MessageIsDuplicate(lpMsg))
    {
        lpMsg->SetMessageInvalid();
        return;
    }

    s32 i = 0;
    for (; i < lpNetPlayer->miNumberMessagesRegistered; ++i)
    {
        if (lpMsg == lpNetPlayer->maRecvMessageData[i].mpMsg)
        {
            break;
        }
    }
    CGS_ASSERT(i < lpNetPlayer->miNumberMessagesRegistered,
               "i < lpNetPlayer->miNumberMessagesRegistered");

    NetMessageData& lData = lpNetPlayer->maRecvMessageData[i];
    if (lpMsg->IsReliable())
    {
        CGS_ASSERT(lData.mpfMsgArrivedCallback,
                   "lpNetPlayer->maRecvMessageData[i].mpfMsgArrivedCallback");
        void* lpCallbackUserData = lData.mpCallbackUserData;
        ReliableMessage* lpReliableMsg = static_cast<ReliableMessage*>(lpMsg);
        lData.mpfMsgArrivedCallback(lpReliableMsg, lpReliableMsg->GetSendingPlayerID(),
                                    lpCallbackUserData);
        return;
    }

    CGS_ASSERT(!lData.mpfMsgArrivedCallback,
               "!lpNetPlayer->maRecvMessageData[i].mpfMsgArrivedCallback");
    if (lData.mu16Frame == KU16_INVALID_FRAME || UInt16IsLargerWrapped(lpMsg->mu16Frame, lData.mu16Frame))
    {
        lData.mu16Frame        = lpMsg->mu16Frame;
        lData.miValidCountdown = KI_FRAMES_TO_DISCARD_MESSAGE_RECEIVED_DATA;
        return;
    }

    lpMsg->SetMessageInvalid();
}

}
