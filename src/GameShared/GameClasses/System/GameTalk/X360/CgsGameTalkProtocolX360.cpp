#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#ifdef SendMessage
#undef SendMessage
#endif
#ifdef DrawText
#undef DrawText
#endif
#include <type_traits>

#include "GameShared/GameClasses/System/GameTalk/X360/CgsGameTalkProtocolX360.h"
#include "GameShared/GameClasses/System/GameTalk/CgsGameTalk.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

#include <cstring>   // strlen
#include <new>       // actual package allocation followed by placement construction

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
// Native Winsock declarations come from the real Windows SDK above.
// FLAG PC-platform leaf: WSADATA, fd_set, sockaddr_in and SOCKET retain the
// host API's real storage/alignment/handle width. Engine state and callbacks
// remain the ARTIST transport. The narrow PC XNet provider owns GameTalk's
// startup; the following status read never acquires another WSA reference.
// -------------------------------------------------------------------------------------
static_assert(std::is_same<CgsGameTalk::SOCKET, ::SOCKET>::value,
              "GameTalk socket storage must match native WinSock SOCKET");
struct XNetStartupParams;
extern "C"
{
    int __cdecl XNetStartup(XNetStartupParams* lpParams);
    int __cdecl CgsPcGameTalkWinsockStartupStatus(unsigned short* lpuVersion);
    // Existing mounted XenonD3D9Shims.cpp provider: exact memcpy implementation.
    void* XMemCpy(void* lpDest, const void* lpSrc, u32 luSize);
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
    const CgsGameTalk::SOCKET KI_INVALID_SOCKET = INVALID_SOCKET;  // socket()/accept() failure (asm -1)

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

    // ARTIST82F3300C is mLog inside the existing LogWindow82F32FD4+0x38.
    // Status text below streams directly through that typed public owner.

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
    // ARTIST82836EE4..6F00 clears all thirteen bytes before the size/flag stores.
    alignas(4) unsigned char laXNetParams[13] = { KU_XNET_CFG_SIZEOFSTRUCT, 1 };
    int liXNetError = XNetStartup(reinterpret_cast<XNetStartupParams*>(laXNetParams));
    if (liXNetError)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lAssertBuffer;
        CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
        lAssertStream << "XNetStartup failed, liError " << static_cast<u32>(liXNetError) << "\n";
        CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 90);
        CgsDev::Assert::EndAssert();
    }

    unsigned short luWinsockVersion = 0;
    int liWsaError = CgsPcGameTalkWinsockStartupStatus(&luWinsockVersion);
    if (liWsaError)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lAssertBuffer;
        CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
        lAssertStream << "WSAStartup failed (error " << static_cast<u32>(liWsaError) << ").\n";
        CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 101);
        CgsDev::Assert::EndAssert();
    }

    if (luWinsockVersion != static_cast<short>(KU_WINSOCK_VERSION))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lAssertBuffer;
        CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
        lAssertStream << "Failed to get proper version of Winsock, got " << static_cast<u32>(luWinsockVersion & 0xFFu) << "." << static_cast<u32>((luWinsockVersion >> 8) & 0xFFu) << "\n";
        CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 107);
        CgsDev::Assert::EndAssert();
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
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lAssertBuffer;
        CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
        const s32 liError = WSAGetLastError();
        lAssertStream << "Failed to close active client socket (error " << static_cast<u32>(liError) << ").\n";
        CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 374);
        CgsDev::Assert::EndAssert();
    }

    sInactivityTimer.Reset();
    sInactivityTimer.SetRunning(false);   // asm 0x82837300: disarm the inactivity timer (mbRunning=0)

    // FLAG PC-platform leaf: the original big-endian halfword port store
    // already produced network order; native little-endian Winsock needs htons.
    const u16 luListenPort = static_cast<u16>(liPort);
    sockaddr_in lAddress = {};
    lAddress.sin_family = KI_AF_INET;
    lAddress.sin_port = htons(luListenPort);
    lAddress.sin_addr.s_addr = INADDR_ANY;

    mListeningSocket = socket(KI_AF_INET, KI_SOCK_STREAM, 0);
    if (mListeningSocket == KI_INVALID_SOCKET)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lAssertBuffer;
        CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
        lAssertStream << "Failed to create listening socket (error " << static_cast<u32>(WSAGetLastError()) << ").\n";
        CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 401);
        CgsDev::Assert::EndAssert();
    }

    // FLAG PC-platform leaf: the same true option is a native BOOL value.
    const BOOL lbReuse = TRUE;
    if (setsockopt(mListeningSocket, KI_SOL_SOCKET, KI_SO_REUSEADDR,
                   reinterpret_cast<const char*>(&lbReuse), sizeof(lbReuse)))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lAssertBuffer;
        CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
        lAssertStream << "Failed to reuse listening socket address (error " << static_cast<u32>(WSAGetLastError()) << ").\n";
        CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 410);
        CgsDev::Assert::EndAssert();
    }

    if (bind(mListeningSocket, reinterpret_cast<const sockaddr*>(&lAddress),
             static_cast<int>(sizeof(lAddress))) == SOCKET_ERROR)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lAssertBuffer;
        CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
        lAssertStream << "Failed to bind listening socket (error " << static_cast<u32>(WSAGetLastError()) << ").\n";
        CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 417);
        CgsDev::Assert::EndAssert();
    }

    if (listen(mListeningSocket, KI_LISTEN_BACKLOG) == SOCKET_ERROR)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lAssertBuffer;
        CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
        lAssertStream << "Listening to client failed.\n";
        CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 423);
        CgsDev::Assert::EndAssert();
    }

    unsigned long luNonBlocking = 1;
    if (ioctlsocket(mListeningSocket, KI_FIONBIO, &luNonBlocking))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lAssertBuffer;
        CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
        lAssertStream << "Failed to set non-blocking listening socket (error " << static_cast<u32>(WSAGetLastError()) << ").\n";
        CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 433);
        CgsDev::Assert::EndAssert();
    }

    meConnectionState = E_CONNECTIONSTATE_OBTAINING_IP; // stw 0,0x4014(this)
    sLogWindow.mLog << "Listening on port " << luListenPort << "\n";
}

// -------------------------------------------------------------------------------------
// AcceptIncomingConnection @ 0x82837120 -- accept a pending client, drop the listener.
// -------------------------------------------------------------------------------------
bool GameTalkProtocol::AcceptIncomingConnection()
{
    // asm 0x82837130: return false unless meConnectionState == 0 (OBTAINING_IP / listening).
    if (meConnectionState != E_CONNECTIONSTATE_OBTAINING_IP)
        return false;

    sockaddr_in lAddress;
    int liAddressLen = static_cast<int>(sizeof(lAddress));
    mClientSocket = accept(mListeningSocket, reinterpret_cast<sockaddr*>(&lAddress), &liAddressLen);

    if (mClientSocket != KI_INVALID_SOCKET)
    {
        // asm 0x82837174: arm the inactivity timer (mbRunning=1) before resetting it.
        sInactivityTimer.SetRunning(true);
        sInactivityTimer.Reset();

        if (closesocket(mListeningSocket))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lAssertBuffer;
            CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
            const s32 liError = WSAGetLastError();
            lAssertStream << "Failed to close listening socket after establishing connection (error " << static_cast<u32>(liError) << ").\n";
            CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 332);
            CgsDev::Assert::EndAssert();
        }

        meConnectionState = E_CONNECTIONSTATE_OBTAINED_IP; // a1[4101] = 1
        sLogWindow.mLog << "Connected to server\n";
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
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lAssertBuffer;
            CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
            lAssertStream << "Invalid connection state";
            CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 285);
            CgsDev::Assert::EndAssert();
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
        if (liSent == SOCKET_ERROR)
            break;

        liMsgSize  -= liSent;
        lpcBuffer  += liSent;
        if (liMsgSize <= 0)
            return true; // fully sent.

        // Block until the socket is writable again.
        fd_set lSocketDescriptors;
        lSocketDescriptors.fd_count = 1;
        lSocketDescriptors.fd_array[0] = mClientSocket;
        if (!select(0, 0, &lSocketDescriptors, 0, 0))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("select(0, NULL, &lSocketDescriptors, NULL, NULL)",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 524);
            CgsDev::Assert::EndAssert();
        }
    }

    if (WSAGetLastError() == KI_WSAECONNRESET)
    {
        meConnectionState = E_CONNECTIONSTATE_LISTENING;
        sLogWindow.mLog << "Connection reset, returning listening mode\n";
        return false;
    }

    if (WSAGetLastError() == KI_WSAENOTCONN)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lAssertBuffer;
        CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
        lAssertStream << "Attempted to send data when not connected (error " << static_cast<u32>(WSAGetLastError()) << ").\n";
        CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 509);
        CgsDev::Assert::EndAssert();
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
            sLogWindow.mLog << "Connection reset, returning listening mode\n";
        }
        else if (WSAGetLastError() == KI_WSAENOTCONN)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lAssertBuffer;
            CgsDev::StrStream lAssertStream(lAssertBuffer.GetBuffer(), lAssertBuffer.GetSize());
            const s32 liError = WSAGetLastError();
            lAssertStream << "Attempted to receive data when not connected (error " << static_cast<u32>(liError) << ").\n";
            CgsDev::Assert::FireAssert(lAssertBuffer.GetBuffer(),
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 571);
            CgsDev::Assert::EndAssert();
        }
    }
}

// -------------------------------------------------------------------------------------
// DigestReceivedData @ 0x82838318 -- append received bytes and dispatch each complete
// length-prefixed message (4-byte little-endian length + payload) to the handler.
// -------------------------------------------------------------------------------------
void GameTalkProtocol::DigestReceivedData(const char* lpacReceivedBytes, u32 luNumReceivedBytes)
{
    if ((luNumReceivedBytes + muBufferMsgSize) >= static_cast<u32>(KI_GAMETALK_BUFFER_SIZE))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("(luNumReceivedBytes + muBufferMsgSize) < KI_GAMETALK_BUFFER_SIZE",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 598);
        CgsDev::Assert::EndAssert();
    }

    XMemCpy(&macReceiveBuffer[muBufferMsgSize], lpacReceivedBytes, luNumReceivedBytes);
    muBufferMsgSize += luNumReceivedBytes;

    u32 luCursor = muBufferMsgStart;

    // Peek the first message: a 4-byte little-endian length prefix; the whole message is
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
        u32 luLength = static_cast<u32>(lpucLen[0]) |
                       (static_cast<u32>(lpucLen[1]) << 8) |
                       (static_cast<u32>(lpucLen[2]) << 16) |
                       (static_cast<u32>(lpucLen[3]) << 24);
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
                u32 luLength = static_cast<u32>(lpucLen[0]) |
                               (static_cast<u32>(lpucLen[1]) << 8) |
                               (static_cast<u32>(lpucLen[2]) << 16) |
                               (static_cast<u32>(lpucLen[3]) << 24);
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
    // ARTIST82838F50 reads the real message's count, key and content owners.
    // Existing typed accessors follow their native fields as pointer widths grow.
    for (s32 liEntry = 0; liEntry < lpMsg->GetNumKeys(); ++liEntry)
    {
        // Case-sensitive C-string compare against the "PingRequest" key.
        const char* lpcKey  = lpMsg->GetKey(liEntry);
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
            SendPingResponseToGameExplorer(static_cast<const char*>(lpMsg->GetContent(liEntry)));
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
    if (lpacPingID == nullptr)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpacPingID != NULL",
            "..\\..\\..\\GameShared\\GameClasses\\System/GameTalk/X360/CgsGameTalkProtocolX360.h",
            235);
        CgsDev::Assert::EndAssert();
    }

    // ARTIST828389F4 allocates the complete message in the GameTalk package.
    // Its inlined construction creates one entry table; the canonical constructor
    // performs that same operation using native pointer widths.
    EA::GameTalk::GameTalkMessage* lpMessage =
        static_cast<EA::GameTalk::GameTalkMessage*>(EA::GameTalk::Alloc(
            static_cast<s32>(sizeof(EA::GameTalk::GameTalkMessage))));
    if (lpMessage != nullptr)
    {
        new (lpMessage) EA::GameTalk::GameTalkMessage(KPAC_GAME_EXPLORER_CHANNEL_ID);
    }
    else
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpMessage != NULL",
            "..\\..\\..\\GameShared\\GameClasses\\System/GameTalk/X360/CgsGameTalkProtocolX360.h",
            240);
        CgsDev::Assert::EndAssert();
    }

    const s32 liLength = static_cast<s32>(std::strlen(lpacPingID));
    lpMessage->AddKeyContent(KPAC_PING_RESPONSE_KEY, KI_GAMETALK_TYPE_DEFAULT,
                             lpacPingID, liLength);
    EA::GameTalk::GameTalkManager::SendMessage(KPAC_GAME_EXPLORER_TARGET, lpMessage);
}

// -------------------------------------------------------------------------------------
// Prepare @ 0x82839238 -- arm the inactivity timeout + per-frame step and register the
// message handler with the GameTalk manager.
// -------------------------------------------------------------------------------------
bool GameTalkProtocol::Prepare(f32 lrInactivityTimeout, f32 lrUpdateTimeStep,
                               EA::GameTalk::GameTalkManager* lpGameTalk)
{
    if (lpGameTalk == nullptr)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpGameTalk != NULL",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 137);
        CgsDev::Assert::EndAssert();
    }
    if (!(lrUpdateTimeStep > 0.0f))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lrUpdateTimeStep > 0.0f",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\gametalk\\X360/CgsGameTalkProtocolX360.cpp", 138);
        CgsDev::Assert::EndAssert();
    }

    mrInactivityTimeout = lrInactivityTimeout;
    sInactivityTimer.Prepare(lrUpdateTimeStep);

    EA::GameTalk::GameTalkManager::RegisterMessageHandler(
        lpGameTalk,
        reinterpret_cast<EA::GameTalk::MessageHandler>(&GameTalkProtocol::OnMessageReceived),
        KPAC_GAME_EXPLORER_REGISTER);

    return true;
}

} // namespace CgsGameTalk
