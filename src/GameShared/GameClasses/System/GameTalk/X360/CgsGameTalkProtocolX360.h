#ifndef GAMESHARED_GAMECLASSES_SYSTEM_GAMETALK_X360_CGSGAMETALKPROTOCOLX360_H
#define GAMESHARED_GAMECLASSES_SYSTEM_GAMETALK_X360_CGSGAMETALKPROTOCOLX360_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTimer.h"
#include "SDKs/EA/GameTalk/GameTalk.h"
#include "SDKs/Packages/GameTalk/1.8.0/include/GameTalk/IGameTalkProtocol.h"

// ============================================================================
// GameShared/GameClasses/System/GameTalk/X360/CgsGameTalkProtocolX360.h
//
// CgsGameTalk::GameTalkProtocol -- the Xbox 360 transport backend for the EA
// GameTalk in-game <-> authoring-tool channel. It derives from
// EA::GameTalk::IGameTalkProtocol and implements the protocol over a raw
// Winsock/XNet TCP socket: it listens for an incoming connection from the
// authoring tool (SetListeningMode + AcceptIncomingConnection), then ferries
// length-prefixed GameTalk messages over the established socket (Send / Poll /
// DigestReceivedData). An inactivity timer (sInactivityTimer) tracks idle time;
// incoming "PingRequest" messages reset it and trigger a "PingResponse" reply
// to the GameExplorer tool.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the X360 source file is
//   d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\system\gametalk\X360\CgsGameTalkProtocolX360.cpp
// Member offsets and constants were read directly off the asm dereferences /
// immediate stores (see CgsGameTalkProtocolX360.cpp). Declaration shape mirrors
// the Feb-2007 PS3 sibling (CgsGameTalkProtocolPS3.h), gated on the X360 build:
// the X360 build replaces the PS3 GetState/cellNetCtl path with the listen/accept
// socket dance and registers OnMessageReceived through the GameTalk manager.
// ============================================================================

namespace CgsGameTalk
{
    // The default TCP port the listening socket binds to when Construct is called
    // without an explicit port (DEFAULT_GAMETALK_PORT in the PS3 sibling).
    // NOT GROUNDED: this TU's caller (CgsGameTalk::GameTalk::Construct) always
    // passes the port explicitly (asm `stw a3,0x4020(this)`), so the default is
    // never materialised in this TU's asm and the literal is unavailable. Flagged
    // placeholder 0 -- do NOT trust this value; fill it from the GameTalk TU asm
    // when that caller is reconstructed.
    static const s32 KI_DEFAULT_GAMETALK_PORT = 0; // FLAGGED-PLACEHOLDER (ungrounded)

    // The receive ring buffer capacity. The asm compares the running message
    // size against 0x4000 (CgsGameTalkProtocolX360.cpp:598 assert) and lays the
    // buffer member at +0x10, ending exactly at the +0x4010 inactivity-timeout
    // member -- so the buffer is 0x4000 bytes.
    static const s32 KI_GAMETALK_BUFFER_SIZE = 0x4000;

    // A raw Winsock socket descriptor.
    typedef s32 SOCKET;

    // CgsGameTalk::GameTalkProtocol -- X360 GameTalk transport. The polymorphic
    // base (IGameTalkProtocol) supplies the vtable the scalar deleting destructor
    // restores (off_820CDC0C @ 0x827DB830).
    class GameTalkProtocol : public EA::GameTalk::IGameTalkProtocol
    {
    public:
        // The connection lifecycle. Enumerator order matches the PS3 sibling; the
        // X360 stores E_CONNECTIONSTATE_LISTENING (=2) into meConnectionState in
        // Construct (asm `li r9,2; stw r9,0x4014(this)`) and again on a reset
        // connection (Poll/Send store 2 after WSAECONNRESET).
        enum EConnectionState
        {
            E_CONNECTIONSTATE_OBTAINING_IP = 0,
            E_CONNECTIONSTATE_OBTAINED_IP  = 1,
            E_CONNECTIONSTATE_LISTENING    = 2,
            E_CONNECTIONSTATE_CONNECTED    = 3,
            E_CONNECTIONSTATE_LOST         = 4,
        };

        // Bring up Winsock/XNet and arm the listening socket for the given port.
        // X360 @0x82836E88 (the only function in this TU that ran in the goal trace).
        void Construct(bool lbInitialiseNetwork, s32 liPort = KI_DEFAULT_GAMETALK_PORT);

        // Arm the inactivity timeout + per-frame update step and register
        // OnMessageReceived with the GameTalk manager. X360 @0x82839238.
        bool Prepare(f32 lrInactivityTimeout, f32 lrUpdateTimeStep,
                     EA::GameTalk::GameTalkManager* lpGameTalk);

        // Static handler the GameTalk manager invokes for each incoming message:
        // a "PingRequest" key resets the inactivity timer and replies with a
        // "PingResponse". X360 @0x82838F50.
        static void OnMessageReceived(EA::GameTalk::GameTalkMessage* lpMsg);

        // IGameTalkProtocol override: connected iff Connect() established the
        // socket. X360 @0x827DB688.
        virtual bool IsConnected();

    private:
        // IGameTalkProtocol override: send liMsgSize bytes over the active socket,
        // pumping select() when the socket would block. X360 @0x82838158.
        virtual bool Send(void* lpaBuffer, s32 liMsgSize);

        // Create/bind/listen a non-blocking listening socket on liPort. X360 @0x82837230.
        void SetListeningMode(s32 liPort);

        // IGameTalkProtocol override: accept a pending connection then drain any
        // received bytes into the digest buffer. X360 @0x82838690.
        virtual void Poll();

        // accept() a pending client and close the listening socket. X360 @0x82837120.
        bool AcceptIncomingConnection();

        // Drive the state machine toward CONNECTED (accept while listening,
        // re-listen on a lost connection). X360 @0x82838090.
        bool Connect();

        // Append received bytes to the buffer and dispatch each complete
        // length-prefixed message to the handler. X360 @0x82838318.
        void DigestReceivedData(const char* lpacReceivedBytes, u32 luNumReceivedBytes);

        // Build a "PingResponse" GameTalk message carrying lpacPingID and send it
        // to the GameExplorer tool endpoint. X360 @0x82838978.
        static void SendPingResponseToGameExplorer(const char* lpacPingID);

        // --- members (offsets read off the X360 asm) ---
        // +0x00 is the IGameTalkProtocol vtable (the base subobject).

        // +0x04: the registered per-message callback DigestReceivedData fans each
        // complete message out to (a no-op if null).
        void (*mpfnMessageHandler)(const char* lpacMessage, u32 luLength); // +0x04

        u32  muBufferMsgStart;                      // +0x08  read cursor into macReceiveBuffer
        u32  muBufferMsgSize;                       // +0x0C  bytes currently buffered
        char macReceiveBuffer[KI_GAMETALK_BUFFER_SIZE]; // +0x10  (0x4000 bytes)
        f32  mrInactivityTimeout;                   // +0x4010 (Construct stores 10.0f)
        EConnectionState meConnectionState;         // +0x4014
        SOCKET mClientSocket;                        // +0x4018
        SOCKET mListeningSocket;                     // +0x401C (Construct stores -1)
        s32  miListenPort;                           // +0x4020

        // Process-wide inactivity timer (the asm's &unk_83035760). File-scope in
        // the X360 build; modelled as a static member.
        static CgsSystem::Timer sInactivityTimer;
    };
}

#endif // GAMESHARED_GAMECLASSES_SYSTEM_GAMETALK_X360_CGSGAMETALKPROTOCOLX360_H
