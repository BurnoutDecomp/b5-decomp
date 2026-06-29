#include "GameShared/GameClasses/System/GameTalk/X360/CgsGameTalkProtocolX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

#include <cstring>   // strlen

// =====================================================================================
// CgsGameTalk::GameTalkProtocol -- Xbox 360 GameTalk transport backend.
//   Reconstructed from BURNOUT_X360_ARTIST.XEX. Source file (from the asm asserts):
//   d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\system\gametalk\X360\CgsGameTalkProtocolX360.cpp
//
//   Construct                     @ 0x82836E88  (ran in the goal trace)
//   SetListeningMode              @ 0x82837230
//   AcceptIncomingConnection      @ 0x82837120
//   Connect                       @ 0x82838090
//   IsConnected                   @ 0x827DB688
//   Send                          @ 0x82838158
//   Poll                          @ 0x82838690
//   DigestReceivedData            @ 0x82838318
//   OnMessageReceived             @ 0x82838F50
//   SendPingResponseToGameExplorer@ 0x82838978
//   Prepare                       @ 0x82839238
//
//   The protocol listens for a TCP connection from an external authoring tool, then
//   ferries length-prefixed GameTalk messages over the socket. All member offsets and
//   immediate constants were read directly off the asm; members are accessed by name.
// =====================================================================================

// -------------------------------------------------------------------------------------
// Platform socket / XNet surface. These are Xbox-360 Winsock + XNet externals (declared
// here so the per-TU compile resolves them); the asm calls them directly. Signatures /
// constants taken from the asm dereferences + immediate operands.
// -------------------------------------------------------------------------------------
extern "C"
{
    int  socket(int liAddressFamily, int liType, int liProtocol);
    int  bind(CgsGameTalk::SOCKET lSocket, const void* lpAddress, int liAddressLen);
    int  listen(CgsGameTalk::SOCKET lSocket, int liBacklog);
    CgsGameTalk::SOCKET accept(CgsGameTalk::SOCKET lSocket, void* lpAddress, int* lpiAddressLen);
    int  closesocket(CgsGameTalk::SOCKET lSocket);
    int  recv(CgsGameTalk::SOCKET lSocket, void* lpBuffer, int liLength, int liFlags);
    int  send(CgsGameTalk::SOCKET lSocket, const void* lpBuffer, int liLength, int liFlags);
    int  setsockopt(CgsGameTalk::SOCKET lSocket, int liLevel, int liOptName,
                    const void* lpOptVal, int liOptLen);
    int  ioctlsocket(CgsGameTalk::SOCKET lSocket, long liCommand, unsigned long* lpuArg);
    int  select(int liNfds, void* lpReadFds, void* lpWriteFds, void* lpExceptFds,
                const void* lpTimeout);

    int  WSAStartup(unsigned short luVersionRequested, void* lpWSAData);
    int  WSAGetLastError();

    int  XNetStartup(const void* lpParams);
    void XMemCpy(void* lpDest, const void* lpSrc, unsigned int luSize);
}

namespace
{
    // --- socket / Winsock constants (from the asm immediates) ---
    const int    KI_AF_INET            = 2;            // socket(2,1,0) / sockaddr family 2
    const int    KI_SOCK_STREAM        = 1;            // socket(2,1,0)
    const int    KI_SOL_SOCKET         = 0xFFFF;       // setsockopt level (ori r4,r4,0xFFFF)
    const int    KI_SO_REUSEADDR       = 4;            // setsockopt optname
    const long   KI_FIONBIO            = 0x8004667E;   // ioctlsocket cmd (asm -2147195266)
    const int    KI_LISTEN_BACKLOG     = 1;            // listen(s,1)
    const int    KI_SOCKADDR_LEN       = 16;           // accept/bind addr length
    const int    KI_RECV_BUFFER_SIZE   = 4096;         // recv(...,4096,0)
    const CgsGameTalk::SOCKET KI_INVALID_SOCKET = -1;  // socket()/accept() failure (asm -1)

    // WSA error codes the asm tests (cmpwi 0x2746 / 0x2749).
    const int    KI_WSAECONNRESET      = 10054;        // 0x2746
    const int    KI_WSAENOTCONN        = 10057;        // 0x2749

    // Winsock startup version: MAKEWORD(2,2) == 0x0202 (asm li r3,0x202); the asm asserts
    // the negotiated wVersion came back as 0x202.
    const unsigned short KU_WINSOCK_VERSION = 0x0202;

    // XNetStartup parameters block: the asm fills a small struct {byte 0x0D, byte 1, ...}.
    // The leading byte 0x0D is the XNet cfg size/version field the X360 SDK expects.
    const unsigned char KU_XNET_CFG_SIZEOFSTRUCT = 0x0D;

    // The per-frame timer step (asm flt_820DEEE0) and the default inactivity timeout
    // (asm flt_820DEEE4 == 0x41200000 == 10.0f) Construct arms the protocol with.
    const f32 KF_DEFAULT_UPDATE_STEP       = 0.016666668f;  // 1/60
    const f32 KF_DEFAULT_INACTIVITY_TIMEOUT = 10.0f;

    // GameExplorer ping channel/keys/endpoints (the asm's off_82F32FA8 / FB0 / FB4 / FAC /
    // FD0 string pointers).
    const char* KPAC_GAME_EXPLORER_CHANNEL_ID = "AttribSys";
    const char* KPAC_GAME_EXPLORER_REGISTER   = "AttribSys.xenon";
    const char* KPAC_GAME_EXPLORER_TARGET     = "Tool.GameExplorer";
    const char* KPAC_PING_RESPONSE_KEY        = "PingResponse";
    const char* KPAC_PING_REQUEST_KEY         = "PingRequest";

    // The debug log stream the X360 streams its status text into (asm &off_82F3300C). A
    // file-scope StrStream over a shared debug buffer; only its operator<< is exercised
    // here. Modelled with the engine's debug-print buffer.
    char gacGameTalkDebugBuffer[256];
    CgsDev::StrStream gDebugStream(gacGameTalkDebugBuffer, sizeof(gacGameTalkDebugBuffer));

    // GameTalk-message content-type for a NUL-terminated string payload (the asm passes 0
    // as the type word from SendPingResponseToGameExplorer's AddKeyContent call).
    const s32 KI_GAMETALK_TYPE_DEFAULT = 0;
}

// File-scope inactivity timer (the asm's &unk_83035760).
CgsSystem::Timer CgsGameTalk::GameTalkProtocol::sInactivityTimer;

namespace CgsGameTalk
{

// -------------------------------------------------------------------------------------
// Construct @ 0x82836E88 -- bring up XNet + Winsock and arm the listening state.
// -------------------------------------------------------------------------------------
void GameTalkProtocol::Construct(bool /*lbInitialiseNetwork*/, s32 liPort)
{
    miListenPort       = liPort;                       // stw a3,0x4020(this)
    mListeningSocket   = KI_INVALID_SOCKET;            // stw -1,0x401C(this)
    muBufferMsgSize    = 0;                             // stw 0,0xC(this)
    muBufferMsgStart   = 0;                             // stw 0,8(this)
    meConnectionState  = E_CONNECTIONSTATE_LISTENING;   // stw 2,0x4014(this)

    sInactivityTimer.Prepare(KF_DEFAULT_UPDATE_STEP);
    mrInactivityTimeout = KF_DEFAULT_INACTIVITY_TIMEOUT; // stfs 10.0f,0x4010(this)

    // XNetStartup parameters: { sizeofStruct=0x0D, cfgFlags=1, 0, 0, 0 }.
    unsigned char laXNetParams[2] = { KU_XNET_CFG_SIZEOFSTRUCT, 1 };
    int liXNetError = XNetStartup(laXNetParams);
    if (liXNetError)
    {
        gDebugStream << "XNetStartup failed, liError " << liXNetError << "\n";
        CGS_ASSERT(liXNetError == 0, "XNetStartup failed");
    }

    short lsWinsockData[1];
    int liWsaError = WSAStartup(KU_WINSOCK_VERSION, lsWinsockData);
    if (liWsaError)
    {
        gDebugStream << "WSAStartup failed (error " << liWsaError << ").\n";
        CGS_ASSERT(liWsaError == 0, "WSAStartup failed");
    }

    if (lsWinsockData[0] != static_cast<short>(KU_WINSOCK_VERSION))
    {
        gDebugStream << "Failed to get proper version of Winsock, got "
                     << (lsWinsockData[0] & 0xFF) << "." << ((lsWinsockData[0] >> 8) & 0xFF) << "\n";
        CGS_ASSERT(lsWinsockData[0] == static_cast<short>(KU_WINSOCK_VERSION),
                   "Wrong Winsock version");
    }

    sInactivityTimer.Prepare(KF_DEFAULT_UPDATE_STEP);
    mrInactivityTimeout = KF_DEFAULT_INACTIVITY_TIMEOUT;
}

// -------------------------------------------------------------------------------------
// SetListeningMode @ 0x82837230 -- create/bind/listen a non-blocking listening socket.
// -------------------------------------------------------------------------------------
void GameTalkProtocol::SetListeningMode(s32 liPort)
{
    if (meConnectionState == E_CONNECTIONSTATE_OBTAINING_IP)
        return; // v4 == 0: nothing to do.

    if (meConnectionState == E_CONNECTIONSTATE_OBTAINED_IP && closesocket(mClientSocket))
    {
        gDebugStream << "Failed to close active client socket (error " << WSAGetLastError() << ").\n";
        CGS_ASSERT(false, "Failed to close active client socket");
    }

    sInactivityTimer.Reset();
    sInactivityTimer.SetRunning(false);   // asm 0x82837300: disarm the inactivity timer (mbRunning=0)

    // sockaddr_in laid out on the stack: { family=2, port=liPort, addr=0 }.
    struct
    {
        short          siFamily;
        unsigned short suPort;
        int            liAddress;
        long long      llZero;
    } lAddress;
    lAddress.siFamily  = KI_AF_INET;
    lAddress.suPort    = static_cast<unsigned short>(liPort);
    lAddress.liAddress = 0;
    lAddress.llZero    = 0;

    mListeningSocket = socket(KI_AF_INET, KI_SOCK_STREAM, 0);
    if (mListeningSocket == KI_INVALID_SOCKET)
    {
        gDebugStream << "Failed to create listening socket (error " << WSAGetLastError() << ").\n";
        CGS_ASSERT(false, "Failed to create listening socket");
    }

    int liReuse = 1;
    if (setsockopt(mListeningSocket, KI_SOL_SOCKET, KI_SO_REUSEADDR, &liReuse, 1))
    {
        gDebugStream << "Failed to reuse listening socket address (error " << WSAGetLastError() << ").\n";
        CGS_ASSERT(false, "Failed to reuse listening socket address");
    }

    if (bind(mListeningSocket, &lAddress, KI_SOCKADDR_LEN) == KI_INVALID_SOCKET)
    {
        gDebugStream << "Failed to bind listening socket (error " << WSAGetLastError() << ").\n";
        CGS_ASSERT(false, "Failed to bind listening socket");
    }

    if (listen(mListeningSocket, KI_LISTEN_BACKLOG) == KI_INVALID_SOCKET)
    {
        gDebugStream << "Listening to client failed.\n";
        CGS_ASSERT(false, "Listening to client failed");
    }

    unsigned long luNonBlocking = 1;
    if (ioctlsocket(mListeningSocket, KI_FIONBIO, &luNonBlocking))
    {
        gDebugStream << "Failed to set non-blocking listening socket (error " << WSAGetLastError() << ").\n";
        CGS_ASSERT(false, "Failed to set non-blocking listening socket");
    }

    meConnectionState = E_CONNECTIONSTATE_OBTAINING_IP; // stw 0,0x4014(this)
    gDebugStream << "Listening on port " << lAddress.suPort << "\n";
}

// -------------------------------------------------------------------------------------
// AcceptIncomingConnection @ 0x82837120 -- accept a pending client, drop the listener.
// -------------------------------------------------------------------------------------
bool GameTalkProtocol::AcceptIncomingConnection()
{
    // asm 0x82837130: return false unless meConnectionState == 0 (OBTAINING_IP / listening).
    if (meConnectionState != E_CONNECTIONSTATE_OBTAINING_IP)
        return false;

    char lacAddress[16];
    int liAddressLen = KI_SOCKADDR_LEN;
    mClientSocket = accept(mListeningSocket, lacAddress, &liAddressLen);

    if (mClientSocket != KI_INVALID_SOCKET)
    {
        // asm 0x82837174: arm the inactivity timer (mbRunning=1) before resetting it.
        sInactivityTimer.SetRunning(true);
        sInactivityTimer.Reset();

        if (closesocket(mListeningSocket))
        {
            gDebugStream << "Failed to close listening socket after establishing connection (error "
                         << WSAGetLastError() << ").\n";
            CGS_ASSERT(false, "Failed to close listening socket after connecting");
        }

        meConnectionState = E_CONNECTIONSTATE_OBTAINED_IP; // a1[4101] = 1
        gDebugStream << "Connected to server\n";
    }

    return meConnectionState == E_CONNECTIONSTATE_OBTAINED_IP;
}

// -------------------------------------------------------------------------------------
// Connect @ 0x82838090 -- drive the state machine toward a live connection.
//
//   meConnectionState reads here use the +0x4004 slot (a1[4101]); the asm treats it as a
//   small connection-progress code: 0 -> accept a pending connection; 1 -> connected;
//   2 -> (re)enter listening mode; >=3 -> invalid.
// -------------------------------------------------------------------------------------
bool GameTalkProtocol::Connect()
{
    const u32 luState = static_cast<u32>(meConnectionState);
    if (luState == 0)
    {
        AcceptIncomingConnection();
    }
    else if (luState != 1)
    {
        if (luState < 3)
        {
            SetListeningMode(miListenPort);
        }
        else
        {
            CGS_ASSERT(false, "Invalid connection state");
        }
    }

    return meConnectionState == E_CONNECTIONSTATE_OBTAINED_IP; // a1[4101] == 1
}

// -------------------------------------------------------------------------------------
// IsConnected @ 0x827DB688 -- tailcall to Connect.
// -------------------------------------------------------------------------------------
bool GameTalkProtocol::IsConnected()
{
    return Connect();
}

// -------------------------------------------------------------------------------------
// Send @ 0x82838158 -- push liMsgSize bytes over the active socket.
// -------------------------------------------------------------------------------------
bool GameTalkProtocol::Send(void* lpaBuffer, s32 liMsgSize)
{
    if (!Connect() || liMsgSize <= 0)
        return true; // LABEL_13: nothing to send / not connected -> success.

    char* lpcBuffer = static_cast<char*>(lpaBuffer);
    for (;;)
    {
        int liSent = send(mClientSocket, lpcBuffer, liMsgSize, 0);
        if (liSent == KI_INVALID_SOCKET)
            break;

        liMsgSize  -= liSent;
        lpcBuffer  += liSent;
        if (liMsgSize <= 0)
            return true; // fully sent.

        // Block until the socket is writable again.
        int laSocketDescriptors[2];
        laSocketDescriptors[0] = 1;
        laSocketDescriptors[1] = mClientSocket;
        if (!select(0, 0, laSocketDescriptors, 0, 0))
        {
            CGS_ASSERT(false, "select(0, NULL, &lSocketDescriptors, NULL, NULL)");
        }
    }

    if (WSAGetLastError() == KI_WSAECONNRESET)
    {
        meConnectionState = E_CONNECTIONSTATE_LISTENING;
        gDebugStream << "Connection reset, returning listening mode\n";
        return false;
    }

    if (WSAGetLastError() == KI_WSAENOTCONN)
    {
        gDebugStream << "Attempted to send data when not connected (error " << WSAGetLastError() << ").\n";
        CGS_ASSERT(false, "Attempted to send data when not connected");
    }
    return false;
}

// -------------------------------------------------------------------------------------
// Poll @ 0x82838690 -- accept/connect then drain pending received bytes.
// -------------------------------------------------------------------------------------
void GameTalkProtocol::Poll()
{
    if (!Connect())
        return;

    // The receive scratch buffer (asm &unk_830380C8); 4096 bytes per recv.
    static char sacReceiveScratch[KI_RECV_BUFFER_SIZE];
    int liReceived = recv(mClientSocket, sacReceiveScratch, KI_RECV_BUFFER_SIZE, 0);

    if (liReceived > 0)
    {
        DigestReceivedData(sacReceiveScratch, static_cast<u32>(liReceived));
        return;
    }

    if (liReceived == KI_INVALID_SOCKET)
    {
        if (WSAGetLastError() == KI_WSAECONNRESET)
        {
            meConnectionState = E_CONNECTIONSTATE_LISTENING;
            gDebugStream << "Connection reset, returning listening mode\n";
        }
        else if (WSAGetLastError() == KI_WSAENOTCONN)
        {
            gDebugStream << "Attempted to receive data when not connected (error " << WSAGetLastError() << ").\n";
            CGS_ASSERT(false, "Attempted to receive data when not connected");
        }
    }
}

// -------------------------------------------------------------------------------------
// DigestReceivedData @ 0x82838318 -- append received bytes and dispatch each complete
// length-prefixed message (4-byte big-endian length + payload) to the handler.
// -------------------------------------------------------------------------------------
void GameTalkProtocol::DigestReceivedData(const char* lpacReceivedBytes, u32 luNumReceivedBytes)
{
    CGS_ASSERT((luNumReceivedBytes + muBufferMsgSize) < static_cast<u32>(KI_GAMETALK_BUFFER_SIZE),
               "(luNumReceivedBytes + muBufferMsgSize) < KI_GAMETALK_BUFFER_SIZE");

    XMemCpy(&macReceiveBuffer[muBufferMsgSize], lpacReceivedBytes, luNumReceivedBytes);
    muBufferMsgSize += luNumReceivedBytes;

    u32 luCursor = muBufferMsgStart;

    // Peek the first message: a 4-byte big-endian length prefix; the whole message is
    // (length + 4) bytes. Fewer than 4 buffered bytes means no complete header yet.
    bool lbHasMessage;
    u32  luMsgSize = 0;
    if (muBufferMsgSize - luCursor < 4)
    {
        lbHasMessage = false;
    }
    else
    {
        lbHasMessage = true;
        const unsigned char* lpucLen =
            reinterpret_cast<const unsigned char*>(&macReceiveBuffer[luCursor]);
        u32 luLength = (static_cast<u32>(lpucLen[0]) << 24) |
                       (static_cast<u32>(lpucLen[1]) << 16) |
                       (static_cast<u32>(lpucLen[2]) << 8)  |
                        static_cast<u32>(lpucLen[3]);
        luMsgSize = luLength + 4;
    }

    // Dispatch every complete message currently buffered.
    if (luMsgSize <= muBufferMsgSize - luCursor)
    {
        do
        {
            if (!lbHasMessage)
                break;

            if (mpfnMessageHandler)
                mpfnMessageHandler(&macReceiveBuffer[luCursor], luMsgSize);

            u32 luNextStart = muBufferMsgStart + luMsgSize;
            u32 luRemaining = muBufferMsgSize - luNextStart;
            luCursor          = luNextStart;
            muBufferMsgStart  = luNextStart;

            if (luRemaining < 4)
            {
                lbHasMessage = false;
            }
            else
            {
                lbHasMessage = true;
                const unsigned char* lpucLen =
                    reinterpret_cast<const unsigned char*>(&macReceiveBuffer[luNextStart]);
                u32 luLength = (static_cast<u32>(lpucLen[0]) << 24) |
                               (static_cast<u32>(lpucLen[1]) << 16) |
                               (static_cast<u32>(lpucLen[2]) << 8)  |
                                static_cast<u32>(lpucLen[3]);
                luMsgSize = luLength + 4;
            }
        }
        while (luMsgSize <= muBufferMsgSize - muBufferMsgStart);
    }

    // Compact any partial trailing message down to the front of the buffer.
    char lacPartial[KI_GAMETALK_BUFFER_SIZE];   // asm frame stack-probes ~0x4000 (not 32)
    u32 luLeftover = muBufferMsgSize - muBufferMsgStart;
    XMemCpy(lacPartial, &macReceiveBuffer[muBufferMsgStart], luLeftover);
    XMemCpy(macReceiveBuffer, lacPartial, luLeftover);
    muBufferMsgSize  -= muBufferMsgStart;
    muBufferMsgStart  = 0;
}

// -------------------------------------------------------------------------------------
// OnMessageReceived @ 0x82838F50 -- a "PingRequest" resets the inactivity timer and
// replies with a "PingResponse".
//
//   The X360 walks the message's key array (msg+8) for the msg+12 entries, comparing
//   each entry's key string ([0]) to "PingRequest"; a match resets the timer and sends
//   the entry's value ([8]) back as the ping id.
// -------------------------------------------------------------------------------------
void GameTalkProtocol::OnMessageReceived(EA::GameTalk::GameTalkMessage* lpMsg)
{
    // The message's key entries are read by offset off the GameTalkMessage; this matches
    // the asm's *(msg+12) count and *(msg+8) entry array. The GameTalk SDK message layout
    // is modelled minimally elsewhere; here we mirror the asm's key/value walk.
    struct KeyEntry
    {
        const char* mpcKey;    // [+0]
        s32         miPad;     // [+4]
        const char* mpcValue;  // [+8]
    };
    struct MessageLayout
    {
        char        macHeader[8];  // [+0]
        KeyEntry**  mppEntries;    // [+8]
        s32         miEntryCount;  // [+12]
    };

    MessageLayout* lpLayout = reinterpret_cast<MessageLayout*>(lpMsg);
    for (s32 liEntry = 0; liEntry < lpLayout->miEntryCount; ++liEntry)
    {
        const KeyEntry* lpEntry = lpLayout->mppEntries[liEntry];

        // Case-sensitive C-string compare against the "PingRequest" key.
        const char* lpcKey  = lpEntry->mpcKey;
        const char* lpcWant = KPAC_PING_REQUEST_KEY;
        int liDiff;
        do
        {
            liDiff = static_cast<unsigned char>(*lpcKey) - static_cast<unsigned char>(*lpcWant);
            if (!*lpcKey)
                break;
            ++lpcKey;
            ++lpcWant;
        }
        while (!liDiff);

        if (!liDiff)
        {
            sInactivityTimer.Reset();
            SendPingResponseToGameExplorer(lpEntry->mpcValue);
        }
    }
}

// -------------------------------------------------------------------------------------
// SendPingResponseToGameExplorer @ 0x82838978 -- ship a "PingResponse" to GameExplorer.
//
//   The X360 builds the GameTalkMessage through the AttribSys GameTalk package allocator
//   (placement-construct + AllocateDataBuffer); that is the inlined form of
//   `new EA::GameTalk::GameTalkMessage("AttribSys")`. Reconstructed in that human form.
// -------------------------------------------------------------------------------------
void GameTalkProtocol::SendPingResponseToGameExplorer(const char* lpacPingID)
{
    CGS_ASSERT(lpacPingID != 0, "lpacPingID != NULL");

    EA::GameTalk::GameTalkMessage* lpMessage =
        new EA::GameTalk::GameTalkMessage(KPAC_GAME_EXPLORER_CHANNEL_ID);
    CGS_ASSERT(lpMessage != 0, "lpMessage != NULL");

    lpMessage->AllocateDataBuffer();

    // Length is strlen(lpacPingID): the asm walks to the NUL then subtracts one.
    s32 liLength = static_cast<s32>(std::strlen(lpacPingID));
    lpMessage->AddKeyContent(KPAC_PING_RESPONSE_KEY, KI_GAMETALK_TYPE_DEFAULT, lpacPingID, liLength);

    EA::GameTalk::GameTalkManager::SendMessage(KPAC_GAME_EXPLORER_TARGET, lpMessage);
}

// -------------------------------------------------------------------------------------
// Prepare @ 0x82839238 -- arm the inactivity timeout + per-frame step and register the
// message handler with the GameTalk manager.
// -------------------------------------------------------------------------------------
bool GameTalkProtocol::Prepare(f32 lrInactivityTimeout, f32 lrUpdateTimeStep,
                               EA::GameTalk::GameTalkManager* lpGameTalk)
{
    CGS_ASSERT(lpGameTalk != 0, "lpGameTalk != NULL");
    CGS_ASSERT(lrUpdateTimeStep > 0.0f, "lrUpdateTimeStep > 0.0f");

    mrInactivityTimeout = lrInactivityTimeout;
    sInactivityTimer.Prepare(lrUpdateTimeStep);

    EA::GameTalk::GameTalkManager::RegisterMessageHandler(
        lpGameTalk,
        reinterpret_cast<EA::GameTalk::MessageHandler>(&GameTalkProtocol::OnMessageReceived),
        KPAC_GAME_EXPLORER_REGISTER);

    return true;
}

} // namespace CgsGameTalk
