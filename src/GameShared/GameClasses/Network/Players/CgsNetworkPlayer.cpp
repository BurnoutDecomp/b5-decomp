#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"  // CgsSystem::TimerStatus
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h"
#include "GameShared/GameClasses/Network/Packeting/CgsCompressionAndEncryptionUtils.h"
#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstring>   // memset, strncpy, memcpy

// =====================================================================================
// CgsNetwork::NetworkPlayer / PlayerMenuData / NetMessageData -- reconstructed from
//   BURNOUT_X360_ARTIST.XEX (the "Breaker"/Jan-2008 X360 build) with declaration shape
//   from the DecFIGS DWARF (gated on the X360 ledger). Behaviour is store-for-store
//   faithful to the ARTIST pseudocode + asm; members are accessed by name (the offsets
//   in CgsNetworkPlayer.h were read directly off the asm dereferences).
//
// One peer of a network session. It owns the per-message-type send/recv tables, drives
// the per-frame send pump (SendMessages), the ping round-trip (UpdatePing, its own TU),
// and the reliable-message handshake (via the PlayerManager's ReliableMessageManager).
//
// Reconstructed functions (14):
//   CheckForPlayerDisconnectTimeout, Construct, GetRegisteredRecvMessage,
//   GetRegisteredSendMessage, IsMessageTypeRegistered, Prepare, RegisterMessageType,
//   Release, ResetAllMessages, SendMessages, UnRegisterMessageType,
//   UnregisterAllMessages, Update, PlayerMenuData::Clear.
//
// The verbose "NETWORK PERFORMANCE WARNING" / "Failed to find ..." diagnostics the X360
// streamed through its inline message-stream (off_82F335C8 / the assert buffer) are
// de-inlined here into idiomatic `*CgsDev::Log::gpDebugPrint << ...` log lines (the
// project's debug-print front-end) -- same observable side effect, no raw stream vtable.
// =====================================================================================

namespace CgsNetwork
{

// ---- file-scope tunables / perfmon ids ---------------------------------------------
// (DWARF CgsNetworkPlayer.cpp:40-52: the timeout, the discard-frame count, and the six
//  perfmon handles registered once on first Prepare.)
namespace
{
    // K_NETWORK_PLAYER_TIMEOUT -- the no-packet window after which a player counts as
    // paused. A dynamic initialiser builds it as Time(5.0f); its storage is zero in the
    // image until then.
    const CgsSystem::Time K_NETWORK_PLAYER_TIMEOUT(5.0f);

    // First-Prepare guard + the six CPU perfmon handles (CgsNetworkPlayer.cpp:43-49).
    bool s_bRegisteredPerfmons                  = false;
    s32  s_iNetworkPlayerSendMessagesTotalPM    = -1;
    s32  s_iNetworkPlayerSendNewMessagesPM      = -1;
    s32  s_iNetworkPlayerResendReliablePM       = -1;
    s32  s_iNetworkPlayerSendAcksAndNacksPM     = -1;
    s32  s_iNetworkPlayerPackMessagesPM         = -1;
    s32  s_iNetworkPlayerSendToPM               = -1;

    // The per-packet scratch buffer size the packer is handed each pass.
    const s32 KI_PACK_BUFFER_SIZE = 1000;

    // The two built-in ping message slots registered by every player in Prepare.
    const s32 KI_PING_MESSAGE_TYPE       = 6;
    const s32 KI_PING_REPLY_MESSAGE_TYPE = 7;
    const s32 KI_PING_MESSAGE_LENGTH     = 36;   // 0x24

    // The "connected" connection-status value GetConnectionStatus returns when linked.
    const s32 KI_CONNECTION_STATUS_CONNECTED = 3;   // E_CONNECTION_SUCCESS

}

// =====================================================================================
// NetMessageData
// =====================================================================================

// ---- NetMessageData::Construct @ 0x82872... (inlined into NetworkPlayer::Construct) --
// Reset one message slot to the empty/invalid state (sentinels -1, zeroed callbacks).
void NetMessageData::Construct()
{
    meType                  = -1;
    miLength                = -1;
    mpMsg                   = 0;
    mpfMsgArrivedCallback   = 0;
    mpfMsgDeliveredCallback = 0;
    mpCallbackUserData      = 0;
    miValidCountdown        = -1;
    mu16Frame               = 0xFFFF;   // KU16_INVALID_FRAME
}

// ---- NetMessageData::Prepare (inlined into RegisterMessageType) ----------------------
void NetMessageData::Prepare(s32 leType, s32 liLength, Message* lpMsg)
{
    meType                  = leType;
    miLength                = liLength;
    mpMsg                   = lpMsg;
    mpfMsgArrivedCallback   = 0;
    mpfMsgDeliveredCallback = 0;
    mpCallbackUserData      = 0;
    mu16Frame               = 0xFFFF;   // KU16_INVALID_FRAME
}

// =====================================================================================
// NetworkPlayer
// =====================================================================================

// ---- Construct @ 0x828722F0 ----------------------------------------------------------
// Field initialiser. Zeroes the identity/connection state, resets every send & recv
// message slot to invalid, and stamps the embedded ping/ping-reply messages + frame-rate
// fields with their invalid sentinels.
void NetworkPlayer::Construct(CgsNetworkPlayerConstructParams* lpConstructParams)
{
    CGS_ASSERT(lpConstructParams, CgsDev::Assert::MessageAt(
        "lpConstructParams",
        __FILE__, 159 /* ARTIST 0x82872328 */));

    mPlayerID        = KI_INVALID_PLAYER_ID;          // +0x08 = -1
    mpNetworkAdapter = 0;                        // +0x04 = 0

    CGS_ASSERT(lpConstructParams->mpPlayerManager, CgsDev::Assert::MessageAt(
        "mpPlayerManager",
        __FILE__, 164 /* ARTIST 0x82872364 */));
    mpPlayerManager  = lpConstructParams->mpPlayerManager;   // +0x0C

    // mConnectionData: no link, the address sentinels and a zeroed tail.
    mConnectionData.Clear();

    // Reset every send & recv message slot.
    for (s32 liSlot = 0; liSlot < KI_MAX_MESSAGE_TYPES; ++liSlot)
    {
        maSendMessageData[liSlot].Construct();
        maRecvMessageData[liSlot].Construct();
    }

    miNumberMessagesRegistered = 0;    // +0xB90
    macName[0]                 = '\0'; // +0xB98
    mbNetworkPlayerPaused      = false;// +0xBA9

    meLocalConsoleFrameRate    = static_cast<CgsSystem::EFrameRate>(-1);   // +0xC40 = -1
    meRemoteConsoleFrameRate   = static_cast<CgsSystem::EFrameRate>(-1);   // +0xC44 = -1

    // Stamp the four embedded ping messages with the Message invalid sentinels
    // (mu8GameID=0xFF, mx8Flags=0, mi8Type=-1, mu16Frame=0xFFFF).
    mPingMessageSend.Construct();
    mPingMessageRecv.Construct();
    mPingReplyMessageSend.Construct();
    mPingReplyMessageRecv.Construct();

    mfPingToReplyTo = 0.0f;   // +0x1C = 0.0
}

// ---- IsMessageTypeRegistered @ 0x82872AF8 --------------------------------------------
// Has leType been registered? Scans the send table; on a hit, asserts the matching recv
// slot agrees, then returns true.
bool NetworkPlayer::IsMessageTypeRegistered(s32 leType)
{
    const s32 liCount = miNumberMessagesRegistered;
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        if (maSendMessageData[liIndex].meType == leType)
        {
            CGS_ASSERT(maRecvMessageData[liIndex].meType == leType,
                CgsDev::Assert::MessageAt(
                    "maRecvMessageData[i].meType == leType",
                    "..\\..\\..\\GameShared\\GameClasses\\Network\\Players\\CgsNetworkPlayer.cpp", 908 /* ARTIST 0x82872B64 */));
            return true;
        }
    }
    return false;
}

// ---- GetRegisteredSendMessage @ 0x82872C30 -------------------------------------------
// Look up the registered SEND message object for leType. Missing -> assert + nullptr.
Message* NetworkPlayer::GetRegisteredSendMessage(s32 leType)
{
    const s32 liCount = miNumberMessagesRegistered;
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        if (maSendMessageData[liIndex].meType == leType)
            return maSendMessageData[liIndex].mpMsg;
    }

    *CgsDev::Log::gpDebugPrint << "Failed to find registered message type "
                               << CgsDev::E_PRINTMODE_HEXONCE << leType << "\n";
    CGS_ASSERT(false, "Failed to find registered message type");
    return 0;
}

// ---- GetRegisteredRecvMessage @ 0x82872D78 -------------------------------------------
// Look up the registered RECV message object for leType. Missing -> assert + nullptr.
Message* NetworkPlayer::GetRegisteredRecvMessage(s32 leType)
{
    const s32 liCount = miNumberMessagesRegistered;
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        if (maRecvMessageData[liIndex].meType == leType)
            return maRecvMessageData[liIndex].mpMsg;
    }

    *CgsDev::Log::gpDebugPrint << "Failed to find registered message type "
                               << CgsDev::E_PRINTMODE_HEXONCE << leType << "\n";
    CGS_ASSERT(false, "Failed to find registered message type");
    return 0;
}

// ---- RegisterMessageType @ 0x828725C0 ------------------------------------------------
// Register a (send, recv) message-object pair for leType, with optional reliable-arrival
// / -delivery callbacks. Appends to the send & recv tables and resets each message's
// internal frame-state sentinels.
void NetworkPlayer::RegisterMessageType(s32 leType, s32 liLength, Message* lpSendMsg,
                                        Message* lpRecvMsg,
                                        ReliableMessageArrivedCallback lpfArrived,
                                        ReliableMessageDeliveredCallback lpfDelivered,
                                        void* lpUserData)
{
    CGS_ASSERT(liLength > 0, CgsDev::Assert::MessageAt(
        "liLength > 0",
        __FILE__, 742 /* ARTIST 0x82872610 */));
    CGS_ASSERT(leType >= 0, CgsDev::Assert::MessageAt(
        "leType >= 0",
        __FILE__, 743 /* ARTIST 0x82872634 */));
    CGS_ASSERT(miNumberMessagesRegistered < KI_MAX_MESSAGE_TYPES, CgsDev::Assert::MessageAt(
        "miNumberMessagesRegistered < KI_MAX_MESSAGE_TYPES",
        __FILE__, 746 /* ARTIST 0x8287265C */));

    CGS_ASSERT(!lpSendMsg->IsReliable() || lpfArrived, CgsDev::Assert::MessageAt(
        "!lpSendMsg->IsReliable() || lpfMsgArrivedCallback",
        __FILE__, 749 /* ARTIST 0x828726A0 */));
    CGS_ASSERT(lpRecvMsg, CgsDev::Assert::MessageAt(
        "lpRecvMsg",
        __FILE__, 752 /* ARTIST 0x828726C4 */));

    for (s32 liIndex = 0; liIndex < miNumberMessagesRegistered; ++liIndex)
    {
        CGS_ASSERT(maSendMessageData[liIndex].meType != leType, CgsDev::Assert::MessageAt(
            "maSendMessageData[i].meType != leType",
            __FILE__, 760 /* ARTIST 0x82872708 */));
    }

    const s32 liSlot = miNumberMessagesRegistered;

    NetMessageData& lSend = maSendMessageData[liSlot];
    lSend.Prepare(leType, liLength, lpSendMsg);

    NetMessageData& lRecv = maRecvMessageData[liSlot];
    lRecv.Prepare(leType, liLength, lpRecvMsg);
    lRecv.mpfMsgArrivedCallback   = lpfArrived;     // recv slot owns the arrived callback
    lRecv.mpfMsgDeliveredCallback = lpfDelivered;
    lRecv.mpCallbackUserData      = lpUserData;

    // Reset the message objects' own frame-state sentinels (mu8GameID=0xFF, flags=0,
    // type=-1, frame=0xFFFF -- the Message::Construct sentinel set).
    lpSendMsg->Construct();
    if (lpRecvMsg)
        lpRecvMsg->Construct();

    ++miNumberMessagesRegistered;
}

// ---- UnRegisterMessageType @ 0x828727F0 ----------------------------------------------
// Remove leType from the tables, releasing its message objects' frame state and
// compacting the (send, recv) arrays so the live entries stay contiguous.
void NetworkPlayer::UnRegisterMessageType(s32 leType)
{
    CGS_ASSERT(leType >= 0, "leType >= 0");

    s32 liIndex = 0;
    while (liIndex < miNumberMessagesRegistered &&
           maSendMessageData[liIndex].meType != leType)
    {
        ++liIndex;
    }

    if (liIndex >= miNumberMessagesRegistered)
    {
        *CgsDev::Log::gpDebugPrint << "Failed to find message of type "
                                   << CgsDev::E_PRINTMODE_HEXONCE << leType << " to unregister";
        CGS_ASSERT(false, "Failed to find message of type to unregister");
        return;
    }

    CGS_ASSERT(maRecvMessageData[liIndex].meType == leType,
               "maRecvMessageData[i].meType == leType");

    // Release the recv message object's frame state.
    if (Message* lpRecvMsg = maRecvMessageData[liIndex].mpMsg)
        lpRecvMsg->Construct();

    const s32 liLast = miNumberMessagesRegistered - 1;
    if (miNumberMessagesRegistered <= 1)
    {
        CGS_ASSERT(liIndex == 0, "i == 0");
        maSendMessageData[liIndex].Construct();
        maRecvMessageData[liIndex].Construct();
    }
    else
    {
        // Move the last live entry into the freed slot (send & recv), then clear the tail.
        maSendMessageData[liIndex] = maSendMessageData[liLast];
        maRecvMessageData[liIndex] = maRecvMessageData[liLast];
        maSendMessageData[liLast].Construct();
        maRecvMessageData[liLast].Construct();
    }

    --miNumberMessagesRegistered;
}

// ---- UnregisterAllMessages @ 0x82872B90 (private) ------------------------------------
// Reset every registered slot (releasing each send message object's frame state) and
// empty the table.
void NetworkPlayer::UnregisterAllMessages()
{
    for (s32 liIndex = 0; liIndex < miNumberMessagesRegistered; ++liIndex)
    {
        // The asm releases the RECV slot's message object here (+1408 from the send base
        // == the recv slot's mpMsg), then resets the send slot.
        if (Message* lpRecvMsg = maRecvMessageData[liIndex].mpMsg)
            lpRecvMsg->Construct();

        maSendMessageData[liIndex].Construct();
        maRecvMessageData[liIndex].Construct();
    }

    miNumberMessagesRegistered = 0;
}

// ---- ResetAllMessages @ 0x82872EC0 ---------------------------------------------------
// Clear the per-message frame state on every registered slot's message objects without
// dropping the registrations (called between rounds by the player-manager send pump).
void NetworkPlayer::ResetAllMessages()
{
    for (s32 liIndex = 0; liIndex < miNumberMessagesRegistered; ++liIndex)
    {
        Message* lpSendMsg = maSendMessageData[liIndex].mpMsg;
        CGS_ASSERT(lpSendMsg, CgsDev::Assert::MessageAt(
            "maSendMessageData[i].mpMsg",
            __FILE__, 1052 /* ARTIST 0x82872F24 */));
        lpSendMsg->Construct();

        if (Message* lpRecvMsg = maRecvMessageData[liIndex].mpMsg)
            lpRecvMsg->Construct();
    }
}

// ---- Prepare @ 0x82883A30 ------------------------------------------------------------
// (Re)initialise the player for a session: log the prepare, validate the frame rates,
// reset identity/connection state, copy the player name, register the two built-in ping
// message types, and (once) register the CPU perfmons.
bool NetworkPlayer::Prepare(NetworkAdapter* lpNetworkAdapter,
                            const CgsSystem::TimerStatus* lpTimerStatus,
                            const char* lpcName,
                            CgsSystem::EFrameRate leLocalConsoleFrameRate,
                            CgsSystem::EFrameRate leRemoteConsoleFrameRate,
                            NetworkPlayerID liPlayerID)
{
    *CgsDev::Log::gpDebugPrint << "Preparing player " << liPlayerID << "\n";

    CGS_ASSERT(leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ||
               leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
               "leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || "
               "leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ");
    CGS_ASSERT(leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ||
               leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
               "leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || "
               "leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ");

    // Reset identity / connection state (same sentinel set Construct writes).
    mPlayerID        = KI_INVALID_PLAYER_ID;
    mpNetworkAdapter = 0;
    mConnectionData.Clear();

    // Bound-check the name length (the X360 string-copy guard: must fit in 16 chars).
    s32 liNameLength = 0;
    while (lpcName[liNameLength] != '\0')
        ++liNameLength;
    if (liNameLength >= 16)
    {
        *CgsDev::Log::gpDebugPrint << "String too long: " << (lpcName ? lpcName : "<NULLSTRING>");
        CGS_ASSERT(false, "String too long");
    }
    std::strncpy(macName, lpcName, 16);

    mpNetworkAdapter        = lpNetworkAdapter;     // +0x04
    mPlayerID               = liPlayerID;           // +0x08
    meLocalConsoleFrameRate = leLocalConsoleFrameRate;   // +0xC40
    meRemoteConsoleFrameRate= leRemoteConsoleFrameRate;  // +0xC44
    mbNetworkPlayerPaused   = false;                // +0xBA9

    mPacketPacker.ResetBandwidthUsed(lpTimerStatus);

    mfPingInMs              = 0.0f;                 // +0xBAC
    mfPingToReplyTo         = 0.0f;                 // +0x1C

    // Stamp the four embedded ping messages' sentinels.
    mPingMessageSend.Construct();
    mPingMessageRecv.Construct();
    mPingReplyMessageSend.Construct();
    mPingReplyMessageRecv.Construct();

    // Register the two built-in ping message types.
    RegisterMessageType(KI_PING_MESSAGE_TYPE, KI_PING_MESSAGE_LENGTH,
                        &mPingMessageSend, &mPingMessageRecv, 0, 0, 0);
    RegisterMessageType(KI_PING_REPLY_MESSAGE_TYPE, KI_PING_MESSAGE_LENGTH,
                        &mPingReplyMessageSend, &mPingReplyMessageRecv, 0, 0, 0);

    // Register the CPU perfmons exactly once (shared across all players).
    if (!s_bRegisteredPerfmons)
    {
        s_iNetworkPlayerSendMessagesTotalPM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - Total",      CgsDev::E_PMP_18, false, 5.0f, true);
        s_iNetworkPlayerSendNewMessagesPM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - Send New",   CgsDev::E_PMP_18, false, 5.0f, true);
        s_iNetworkPlayerResendReliablePM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - Resend Rel", CgsDev::E_PMP_18, false, 5.0f, true);
        s_iNetworkPlayerSendAcksAndNacksPM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - Acks+Nacks", CgsDev::E_PMP_18, false, 5.0f, true);
        s_iNetworkPlayerPackMessagesPM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - Pack",       CgsDev::E_PMP_18, false, 5.0f, true);
        s_iNetworkPlayerSendToPM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - SendTo",     CgsDev::E_PMP_18, false, 5.0f, true);
        s_bRegisteredPerfmons = true;
    }

    return true;
}

// ---- Release @ 0x82899B48 ------------------------------------------------------------
// Tear the player down: drop our buffered reliable messages, reset identity/connection
// state, and unregister every message type.
bool NetworkPlayer::Release()
{
    mpPlayerManager->mReliableMessageManager.ClearPlayersSendReliableMessages(mPlayerID);

    mPlayerID        = KI_INVALID_PLAYER_ID;
    mpNetworkAdapter = 0;
    mConnectionData.Clear();

    meLocalConsoleFrameRate  = static_cast<CgsSystem::EFrameRate>(-1);
    meRemoteConsoleFrameRate = static_cast<CgsSystem::EFrameRate>(-1);

    UnregisterAllMessages();
    return true;
}

// ---- CheckForPlayerDisconnectTimeout @ 0x82873100 (protected) ------------------------
// If we have ever received a packet, compute the elapsed time since the last one and flag
// the connection failed once that gap exceeds K_NETWORK_PLAYER_TIMEOUT.
void NetworkPlayer::CheckForPlayerDisconnectTimeout(const CgsSystem::TimerStatus* lpTimerStatus)
{
    // mTimeLastPacketReceived.GetFloatVal() != 0 -> we have received at least one packet.
    if (mTimeLastPacketReceived.GetFloatVal() == 0.0f)
        return;

    const CgsSystem::Time lTimeGap = lpTimerStatus->GetTime() - mTimeLastPacketReceived;
    CGS_ASSERT(lTimeGap.GetFloatVal() >= 0.0f, CgsDev::Assert::MessageAt(
        "lTimeGap.GetFloatVal() >= 0.0f",
        "..\\..\\..\\GameShared\\GameClasses\\Network\\Players\\CgsNetworkPlayer.cpp", 1324 /* ARTIST 0x828731B4 */));

    // mbHasConnectionFailed = lTimeGap > K_NETWORK_PLAYER_TIMEOUT. The X360 inlines the
    // CgsSystem::Time::operator> compare (seconds first, then fraction) against the two
    // timeout words; read off the real branch structure, not the Hex-Rays simplification.
    const s32 liGapSeconds   = lTimeGap.GetSeconds();
    const f32 lfGapFraction  = lTimeGap.GetFraction();
    bool lbTimedOut;
    if (liGapSeconds > K_NETWORK_PLAYER_TIMEOUT.GetSeconds())
        lbTimedOut = true;
    else if (liGapSeconds < K_NETWORK_PLAYER_TIMEOUT.GetSeconds())
        lbTimedOut = false;
    else
        lbTimedOut = (lfGapFraction > K_NETWORK_PLAYER_TIMEOUT.GetFraction());

    mbHasConnectionFailed = lbTimedOut;
}

// ---- Update @ 0x82899BF8 -------------------------------------------------------------
// Per-frame tick: record the current frame, advance the packer averages, update the ping,
// check for a disconnect timeout, then process inbound messages.
void NetworkPlayer::Update(const CgsSystem::TimerStatus* lpTimerStatus,
                           u16 lu16CurrentFrame, bool lbInGame)
{
    mu16CurrentFrame = lu16CurrentFrame;   // +0x10

    mPacketPacker.Update(lpTimerStatus);
    UpdatePing(lpTimerStatus, lu16CurrentFrame, lbInGame);
    CheckForPlayerDisconnectTimeout(lpTimerStatus);
    UpdateMessagesReceived();
}

// ---- SendMessages @ 0x82899C50 -------------------------------------------------------
// The per-frame send pump. Gathers the new (valid) messages this frame, the reliable
// messages due for (re)send, and the pending acks/nacks, then hands them to the packer in
// up-to-KI_PACK_BUFFER_SIZE-byte packets and ships each through the network adapter.
void NetworkPlayer::SendMessages()
{
    CgsDev::PerfMonCpu::StartMonitor(s_iNetworkPlayerSendMessagesTotalPM);
    CGS_ASSERT(mu16CurrentFrame != 0xFFFF, "mu16CurrentFrame != KU16_INVALID_FRAME");

    PlayerManager*        lpPlayerManager = mpPlayerManager;
    const NetworkPlayerID liPlayerID      = mPlayerID;

    // We are "in our send window" if it is our round-robin turn OR the connection is up.
    bool lbCanSendToPlayer = false;
    if (lpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(liPlayerID, true, 0) ||
        lpPlayerManager->mConnectionManager.GetConnectionStatus(liPlayerID) != KI_CONNECTION_STATUS_CONNECTED)
    {
        lbCanSendToPlayer = true;
    }

    const NetworkPlayerID liLocalPlayerID = lpPlayerManager->GetLocalPlayerID();
    const u8              lu8GameID        = lpPlayerManager->GetGameID();

    // The send list the packer consumes this frame (new + reliable messages), and the
    // ack/nack signal-message list, plus the per-packet scratch buffer.
    Message*       lapSendMessages[KI_MAX_MESSAGE_TYPES];
    SignalMessage* lapSignalMessages[20];
    u8             lacPackBuffer[1152];        // v107 (the packer fills up to KI_PACK_BUFFER_SIZE)
    s32            liNumSendMessages   = 0;   // new this frame (v98)
    s32            liNumSignalMessages = 0;   // acks + nacks (v103 / v51)

    // ---- pass 1: gather the NEW valid messages this frame ----------------------------
    CgsDev::PerfMonCpu::StartMonitor(s_iNetworkPlayerSendNewMessagesPM);
    for (s32 liIndex = 0; liIndex < miNumberMessagesRegistered; ++liIndex)
    {
        Message* lpMsg = maSendMessageData[liIndex].mpMsg;
        if (!lpMsg->IsMessageValid())
            continue;

        CGS_ASSERT(liLocalPlayerID >= 0, "lLocalPlayerID >= 0");
        CGS_ASSERT(liPlayerID >= 0,      "lPlayerID >= 0");
        CGS_ASSERT(lu8GameID != 255,     "lu8GameID != 255");

        // Reliable messages record who sent/receives them.
        if (lpMsg->IsReliable())
        {
            MessageWithPlayerIDs* lpMsgWithIDs = static_cast<MessageWithPlayerIDs*>(lpMsg);
            CGS_ASSERT(liLocalPlayerID >= 0, "lSendingPlayerID>=0");
            lpMsgWithIDs->SetSendingPlayerID(liLocalPlayerID);
            CGS_ASSERT(liPlayerID >= 0, "lRecvingPlayerID>=0");
            lpMsgWithIDs->SetRecvingPlayerID(liPlayerID);
        }

        CGS_ASSERT(lu8GameID != 255, "lu8GameID != KU8_INVALID_GAME_ID");
        lpMsg->SetGameID(lu8GameID);

        CGS_ASSERT(lpMsg->IsMessageValid(), "maSendMessageData[i].mpMsg->IsMessageValid()");

        if (lpMsg->IsReliable())
        {
            // Buffer it for resend (unless we are paused / have no live connection), then
            // clear its valid flag -- the reliable copy lives in the resend buffer now.
            // The "have a connection" test reads the connection-handle word at the head of
            // the serialised connection blob (mConnectionData[0] != 0).
            if (!mbNetworkPlayerPaused && mConnectionData.IsValid())
            {
                lpPlayerManager->mReliableMessageManager.AddBufferedReliableMessage(
                    mPlayerID, lpMsg, maSendMessageData[liIndex].miLength);
            }
            lpMsg->SetMessageInvalid();
        }
        else
        {
            // Unreliable: ship it new this frame. Warn if we are sending it out of turn.
            if (!lbCanSendToPlayer)
            {
                *CgsDev::Log::gpDebugPrint
                    << "NETWORK PERFORMANCE WARNING: Unreliable message "
                    << static_cast<s32>(lpMsg->GetType())
                    << " sent to player " << liPlayerID << " out of turn\n";
            }
            lapSendMessages[liNumSendMessages++] = lpMsg;
        }
    }
    CgsDev::PerfMonCpu::StopMonitor(s_iNetworkPlayerSendNewMessagesPM);

    // ---- pass 2: gather the reliable messages due for (re)send -----------------------
    CgsDev::PerfMonCpu::StartMonitor(s_iNetworkPlayerResendReliablePM);
    CGS_ASSERT(mpPlayerManager, "mpPlayerManager");

    ReliableMessageManager& lReliableManager = mpPlayerManager->mReliableMessageManager;
    s32 liResendIndex = lReliableManager.GetNextReliableMessageToResend(
        mPlayerID, mu16CurrentFrame, -1);
    while (liResendIndex != -1)
    {
        CGS_ASSERT(liResendIndex >= 0,
                   "liMessageIndex >= 0");
        CGS_ASSERT(liResendIndex < ReliableMessageManager::KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER,
                   "liMessageIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER");
        CGS_ASSERT(liResendIndex >= 0, "liIndex >= 0");
        CGS_ASSERT(liResendIndex < ReliableMessageManager::KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER,
                   "liIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER");

        ReliableMessageManager::BufferedSendMessageData* lpEntry =
            lReliableManager.GetBufferedReliableMessage(liResendIndex);
        CGS_ASSERT(lpEntry, "lpMsgToResend");

        const bool lbFirstSend = (lpEntry->mu16FrameFirstSent == 0xFFFF);
        if (lbFirstSend || lbCanSendToPlayer)
        {
            CGS_ASSERT(lpEntry->mpMsg, "lpMsgToResend->mpMsg");

            if (!lbCanSendToPlayer)
            {
                const char* lpcAction = lbFirstSend ? " sent" : " resent";
                *CgsDev::Log::gpDebugPrint
                    << "NETWORK PERFORMANCE WARNING: Reliable message "
                    << static_cast<s32>(lpEntry->mpMsg->GetType())
                    << lpcAction << " to player " << liPlayerID << " out of turn\n";
            }

            CGS_ASSERT(lpEntry->mpMsg->IsReliable(), "lpMsgToResend->mpMsg->IsReliable()");

            lpEntry->mpMsg->SetMessageValid();
            CGS_ASSERT(liNumSendMessages < KI_MAX_MESSAGE_TYPES,
                       "liMsgArrayIndex < KI_MAX_MESSAGE_TYPES");
            lapSendMessages[liNumSendMessages++] = lpEntry->mpMsg;

            if (lbFirstSend)
                lpEntry->mu16FrameFirstSent = mu16CurrentFrame;
            lpEntry->mu16FrameLastSent = mu16CurrentFrame;
        }

        liResendIndex = lReliableManager.GetNextReliableMessageToResend(
            mPlayerID, mu16CurrentFrame, liResendIndex);
    }
    CgsDev::PerfMonCpu::StopMonitor(s_iNetworkPlayerResendReliablePM);

    // ---- pass 3: gather pending acks + nacks (only when in our send window) ----------
    if (lbCanSendToPlayer)
    {
        CgsDev::PerfMonCpu::StartMonitor(s_iNetworkPlayerSendAcksAndNacksPM);

        for (s32 liAck = 0; liAck < PlayerManager::KI_MAX_ACKS_TO_BUFFER; ++liAck)
        {
            if (!mpPlayerManager->AckNeedsSending(mPlayerID, liAck))
                continue;

            CGS_ASSERT(liAck >= 0, "liIndex >= 0");
            CGS_ASSERT(liAck < PlayerManager::KI_MAX_ACKS_TO_BUFFER, "liIndex < KI_MAX_ACKS_TO_BUFFER");

            SignalMessage* lpAck = mpPlayerManager->GetAck(liAck);
            lapSignalMessages[liNumSignalMessages++] = lpAck;

            *CgsDev::Log::gpDebugPrint
                << " Sending Ack for message type " << static_cast<s32>(lpAck->GetType())
                << " for player " << liPlayerID
                << " on frame " << static_cast<s32>(mu16CurrentFrame) << "\n";
        }

        for (s32 liNack = 0; liNack < PlayerManager::KI_MAX_NACKS_TO_BUFFER; ++liNack)
        {
            if (!mpPlayerManager->NackNeedsSending(mPlayerID, liNack))
                continue;

            CGS_ASSERT(liNack >= 0, "liIndex >= 0");
            CGS_ASSERT(liNack < PlayerManager::KI_MAX_NACKS_TO_BUFFER, "liIndex < KI_MAX_NACKS_TO_BUFFER");

            lapSignalMessages[liNumSignalMessages++] = mpPlayerManager->GetNack(liNack);
        }

        CgsDev::PerfMonCpu::StopMonitor(s_iNetworkPlayerSendAcksAndNacksPM);
    }

    // ---- pass 4: pack + ship, looping until the packer has emitted everything --------
    bool lbPackDone;
    do
    {
        CgsDev::PerfMonCpu::StartMonitor(s_iNetworkPlayerPackMessagesPM);
        s32 liBytesPacked = 0;
        lbPackDone = mPacketPacker.Pack(
            mPlayerID,
            lapSendMessages,   liNumSendMessages,
            lapSignalMessages, liNumSignalMessages,
            lacPackBuffer, KI_PACK_BUFFER_SIZE, &liBytesPacked);
        CgsDev::PerfMonCpu::StopMonitor(s_iNetworkPlayerPackMessagesPM);

        CgsDev::PerfMonCpu::StartMonitor(s_iNetworkPlayerSendToPM);
        if (liBytesPacked > 0 && !mbNetworkPlayerPaused && mConnectionData.IsValid())
        {
            ConnectionData lConnectionData = mConnectionData;   // sent by value
            if (!mpNetworkAdapter->SendTo(lacPackBuffer, liBytesPacked, lConnectionData))
                mbNetworkPlayerPaused = true;   // first send failure pauses the player
        }
        CgsDev::PerfMonCpu::StopMonitor(s_iNetworkPlayerSendToPM);
    }
    while (!lbPackDone);

    CgsDev::PerfMonCpu::StopMonitor(s_iNetworkPlayerSendMessagesTotalPM);
}

// =====================================================================================
// PlayerMenuData
// =====================================================================================

// ---- PlayerMenuData::Clear @ 0x828722C8 ----------------------------------------------
// Reset a menu/lobby player record to the empty default.
void PlayerMenuData::Clear()
{
    macName[0]        = '\0';   // +0x08
    miConnectionIndex = -1;     // +0x04
    mNameColour       = 0;      // +0x18
    mu8PlayerType     = 3;      // +0x1C
    mu8HeadsetStatus  = 0;      // +0x1D
}

}  // namespace CgsNetwork

// FOLDED FROM CgsNetworkPlayer_wN2_01.cpp
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
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

        CGS_ASSERT(lData.miValidCountdown == 0, CgsDev::Assert::MessageAt(
            "maRecvMessageData[liIndex].miValidCountdown == 0",
            __FILE__, 1235 /* ARTIST 0x82873000 */));
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

        if (lData.mpfMsgDeliveredCallback != 0)
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

    CGS_ASSERT(lpMsg->IsMessageValid(), CgsDev::Assert::MessageAt(
        "lpMsg->IsMessageValid()",
        __FILE__, 1142 /* ARTIST 0x82883E58 */));
    const u8 lu8GameID = lpMsg->GetGameID();
    CGS_ASSERT(lu8GameID != KU8_INVALID_GAME_ID, CgsDev::Assert::MessageAt(
        "lu8GameID != KU8_INVALID_GAME_ID",
        __FILE__, 1148 /* ARTIST 0x82883E88 */));

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
    CGS_ASSERT(i < lpNetPlayer->miNumberMessagesRegistered, CgsDev::Assert::MessageAt(
        "i < lpNetPlayer->miNumberMessagesRegistered",
        __FILE__, 1177 /* ARTIST 0x82883FD0 */));

    NetMessageData& lData = lpNetPlayer->maRecvMessageData[i];
    if (lpMsg->IsReliable())
    {
        CGS_ASSERT(lData.mpfMsgArrivedCallback, CgsDev::Assert::MessageAt(
            "lpNetPlayer->maRecvMessageData[i].mpfMsgArrivedCallback",
            __FILE__, 1184 /* ARTIST 0x82884020 */));
        void* lpCallbackUserData = lData.mpCallbackUserData;
        ReliableMessage* lpReliableMsg = static_cast<ReliableMessage*>(lpMsg);
        lData.mpfMsgArrivedCallback(lpReliableMsg, lpReliableMsg->GetSendingPlayerID(),
                                    lpCallbackUserData);
        return;
    }

    CGS_ASSERT(!lData.mpfMsgArrivedCallback, CgsDev::Assert::MessageAt(
        "!lpNetPlayer->maRecvMessageData[i].mpfMsgArrivedCallback",
        __FILE__, 1193 /* ARTIST 0x82884070 */));
    if (lData.mu16Frame == KU16_INVALID_FRAME || UInt16IsLargerWrapped(lpMsg->mu16Frame, lData.mu16Frame))
    {
        lData.mu16Frame        = lpMsg->mu16Frame;
        lData.miValidCountdown = KI_FRAMES_TO_DISCARD_MESSAGE_RECEIVED_DATA;
        return;
    }

    lpMsg->SetMessageInvalid();
}

}
