#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
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
    // K_NETWORK_PLAYER_TIMEOUT (CgsNetworkPlayer.cpp:40) -- the disconnect window. The
    // ARTIST CheckForPlayerDisconnectTimeout compares the elapsed time-since-last-packet
    // against a fixed (seconds, fraction) pair loaded from rodata (dword_8307A310 /
    // flt_8307A314). Those two words ARE this Time constant; modelled here by name.
    const CgsSystem::Time K_NETWORK_PLAYER_TIMEOUT(/*seconds*/ 10, /*fraction*/ 0.0f);

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

    // The send pump's "do we have a live connection?" test reads the connection-handle word
    // at the head of the serialised connection blob (the asm's `*(this + 0x20) != 0`). The
    // blob is external/serialised data (its field shape belongs to the FakeNetworkConditions
    // TU), so the leading word is read through a documented raw view here.
    inline bool ConnectionDataIsLive(const ConnectionData& lConnectionData)
    {
        return *reinterpret_cast<const u32*>(&lConnectionData) != 0;   // serialised blob: handle word @ +0
    }
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
    mpMsg                   = nullptr;
    mpfMsgArrivedCallback   = nullptr;
    mpfMsgDeliveredCallback = nullptr;
    mpCallbackUserData      = nullptr;
    miValidCountdown        = -1;
    mu16Frame               = 0xFFFF;   // KU16_INVALID_FRAME
}

// ---- NetMessageData::Prepare (inlined into RegisterMessageType) ----------------------
void NetMessageData::Prepare(s32 leType, s32 liLength, Message* lpMsg)
{
    meType                  = leType;
    miLength                = liLength;
    mpMsg                   = lpMsg;
    mpfMsgArrivedCallback   = nullptr;
    mpfMsgDeliveredCallback = nullptr;
    mpCallbackUserData      = nullptr;
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
    CGS_ASSERT(lpConstructParams, "lpConstructParams");

    mPlayerID        = KI_INVALID_PLAYER_ID;          // +0x08 = -1
    mpNetworkAdapter = nullptr;                        // +0x04 = 0

    CGS_ASSERT(lpConstructParams->mpPlayerManager, "mpPlayerManager");
    mpPlayerManager  = lpConstructParams->mpPlayerManager;   // +0x0C

    // mConnectionData (+0x20 .. +0x8F): sentinels then a zeroed tail (the asm writes the
    // leading words individually, then memsets the remaining 64 bytes to zero).
    std::memset(&mConnectionData, 0, sizeof(mConnectionData));
    {
        s32* lpiConn = reinterpret_cast<s32*>(&mConnectionData);   // serialised blob: documented raw view
        lpiConn[0] = 0;
        lpiConn[1] = -1;
        lpiConn[2] = -1;
        lpiConn[9] = -1;
    }

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
                       "maRecvMessageData[i].meType == leType");
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
    return nullptr;
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
    return nullptr;
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
    CGS_ASSERT(liLength > 0, "liLength > 0");
    CGS_ASSERT(leType >= 0,  "leType >= 0");
    CGS_ASSERT(miNumberMessagesRegistered < KI_MAX_MESSAGE_TYPES,
               "miNumberMessagesRegistered < KI_MAX_MESSAGE_TYPES");

    CGS_ASSERT(!lpSendMsg->IsReliable() || lpfArrived,
               "!lpSendMsg->IsReliable() || lpfMsgArrivedCallback");
    CGS_ASSERT(lpRecvMsg, "lpRecvMsg");

    for (s32 liIndex = 0; liIndex < miNumberMessagesRegistered; ++liIndex)
    {
        CGS_ASSERT(maSendMessageData[liIndex].meType != leType,
                   "maSendMessageData[i].meType != leType");
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
        CGS_ASSERT(lpSendMsg, "maSendMessageData[i].mpMsg");
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
    mpNetworkAdapter = nullptr;
    std::memset(&mConnectionData, 0, sizeof(mConnectionData));
    {
        s32* lpiConn = reinterpret_cast<s32*>(&mConnectionData);   // serialised blob: documented raw view
        lpiConn[0] = 0;
        lpiConn[1] = -1;
        lpiConn[2] = -1;
        lpiConn[9] = -1;
    }

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
                        &mPingMessageSend, &mPingMessageRecv, nullptr, nullptr, nullptr);
    RegisterMessageType(KI_PING_REPLY_MESSAGE_TYPE, KI_PING_MESSAGE_LENGTH,
                        &mPingReplyMessageSend, &mPingReplyMessageRecv, nullptr, nullptr, nullptr);

    // Register the CPU perfmons exactly once (shared across all players).
    if (!s_bRegisteredPerfmons)
    {
        s_iNetworkPlayerSendMessagesTotalPM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - Total",      18, 0, 5.0, 0, 1);
        s_iNetworkPlayerSendNewMessagesPM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - Send New",   18, 0, 5.0, 0, 1);
        s_iNetworkPlayerResendReliablePM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - Resend Rel", 18, 0, 5.0, 0, 1);
        s_iNetworkPlayerSendAcksAndNacksPM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - Acks+Nacks", 18, 0, 5.0, 0, 1);
        s_iNetworkPlayerPackMessagesPM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - Pack",       18, 0, 5.0, 0, 1);
        s_iNetworkPlayerSendToPM = CgsDev::PerfMonCpu::AddMonitor(
            "NetworkPlayer - SendTo",     18, 0, 5.0, 0, 1);
        s_bRegisteredPerfmons = true;
    }

    return true;
}

// ---- Release @ 0x82899B48 ------------------------------------------------------------
// Tear the player down: drop our buffered reliable messages, reset identity/connection
// state, and unregister every message type.
bool NetworkPlayer::Release()
{
    mpPlayerManager->GetReliableMessageManager().ClearPlayersSendReliableMessages(mPlayerID);

    mPlayerID        = KI_INVALID_PLAYER_ID;
    mpNetworkAdapter = nullptr;
    std::memset(&mConnectionData, 0, sizeof(mConnectionData));
    {
        s32* lpiConn = reinterpret_cast<s32*>(&mConnectionData);   // serialised blob: documented raw view
        lpiConn[0] = 0;
        lpiConn[1] = -1;
        lpiConn[2] = -1;
        lpiConn[9] = -1;
    }

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
    CGS_ASSERT(lTimeGap.GetFloatVal() >= 0.0f, "lTimeGap.GetFloatVal() >= 0.0f");

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
        lpPlayerManager->GetConnectionStatus(liPlayerID) != KI_CONNECTION_STATUS_CONNECTED)
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
            if (!mbNetworkPlayerPaused && ConnectionDataIsLive(mConnectionData))
            {
                lpPlayerManager->GetReliableMessageManager().AddBufferedReliableMessage(
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

    ReliableMessageManager& lReliableManager = mpPlayerManager->GetReliableMessageManager();
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
        if (liBytesPacked > 0 && !mbNetworkPlayerPaused && ConnectionDataIsLive(mConnectionData))
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
