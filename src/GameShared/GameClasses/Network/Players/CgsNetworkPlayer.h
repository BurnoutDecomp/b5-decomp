#pragma once

// ===================================================================================
// CgsNetwork::NetworkPlayer / PlayerMenuData / NetMessageData -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h
//
// One peer of a network session: owns the per-message-type send/recv tables, drives the
// per-frame send/receive pump, the ping round-trip, and the reliable-message handshake.
//
// SHAPE is taken from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsNetworkPlayer.h), gated against the X360 ARTIST
// binary -- every member offset below is the byte offset the ARTIST asm dereferences:
//
//   +0x000  vptr
//   +0x004  mpNetworkAdapter        (NetworkAdapter*)        Construct: =0
//   +0x008  mPlayerID               (NetworkPlayerID s32)    Construct: =-1
//   +0x00C  mpPlayerManager         (PlayerManager*)         Construct: =ctorParams->mpPlayerManager
//   +0x010  mu16CurrentFrame        (u16)                    Update: stored each frame
//   +0x014  mTimeLastPacketReceived (CgsSystem::Time, 8B: seconds@+0x14, fraction@+0x18)
//   +0x01C  mfPingToReplyTo         (f32)                    Construct: =0.0
//   +0x020  mConnectionData         (ConnectionData, 0x70B)  Construct: zeroed + sentinels
//   +0x090  maSendMessageData[44]   (NetMessageData[44], 0x20 each -> 0x090..0x60F)
//   +0x610  maRecvMessageData[44]   (NetMessageData[44], 0x20 each -> 0x610..0xB8F)
//   +0xB90  miNumberMessagesRegistered (s32)
//   +0xB94  mPacketPacker           (CompressionAndEncryptionUtils, 4B helper)
//   +0xB98  macName[16]             (char[16])               Prepare: strncpy 16
//   +0xBA8  mbHasConnectionFailed   (bool)
//   +0xBA9  mbNetworkPlayerPaused   (bool)
//   +0xBAC  mfPingInMs              (f32)
//   +0xBB0  mPingMessageSend        (PingMessage, 0x24)
//   +0xBD4  mPingMessageRecv        (PingMessage, 0x24)
//   +0xBF8  mPingReplyMessageSend   (PingReplyMessage, 0x24)
//   +0xC1C  mPingReplyMessageRecv   (PingReplyMessage, 0x24)
//   +0xC40  meLocalConsoleFrameRate (CgsSystem::EFrameRate s32)
//   +0xC44  meRemoteConsoleFrameRate(CgsSystem::EFrameRate s32)
//   => sizeof(NetworkPlayer) == 0xC48.
//
// X360-vs-DWARF deltas (rung 1 -- the ARTIST asm -- arbitrates): the DWARF (PS3) spells
//   KI_MAX_MESSAGE_TYPES == 41 and macName/PlayerMenuData::macName as [20]; the ARTIST asm
//   uses 44 message slots (Construct's loop runs 44 times, RegisterMessageType asserts
//   "< KI_MAX_MESSAGE_TYPES" against 44, and the send/recv arrays are 1408 bytes apart =
//   44*0x20) and copies/zeroes only 16 name bytes (strncpy 16; the bool flags sit at
//   +0xBA8/+0xBA9, immediately after a 16-byte name). So the X360 constants are 44 / [16].
//
// The X360 build models the message vtable as Message::mpVTable (no C++ `virtual`), and
// NetworkPlayer itself has a real vtable (Construct/Prepare/Release/Update are virtual in
// the DWARF). The five perfmon-id statics and the timeout/discard-frame tunables are
// file-scope state in the .cpp.
//
// WIDTH MODELLING: the offsets above are the X360 (32-bit-pointer) byte offsets, kept as
// documentation. The reconstruction accesses every field BY NAME (never by raw offset), so
// the C++ struct uses native x64 widths (8-byte pointers) and the compiler lays it out
// naturally -- semantic parity, not byte-exact X360 layout (same rule the rest of the
// project follows; cf. CgsMessage.h). The explicit pad members below are alignment fillers
// the X360 layout implies and are harmless on x64.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsPingMessage.h"
#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h"  // NetworkAdapter, ConnectionData
#include "GameShared/GameClasses/Network/Packeting/CgsCompressionAndEncryptionUtils.h"  // CompressionAndEncryptionUtils

namespace CgsSystem
{
    // Console refresh rate. The X360 Prepare asserts the value is exactly 50 or 60, so the
    // enumerators carry their literal Hz values (the network frame pacing scales off them).
    // (CgsNetworkManager.cpp models a stripped local copy with only E_FRAMERATE_UNKNOWN;
    //  this is the home that names the real values the ARTIST asm compares against.)
    enum EFrameRate
    {
        E_FRAMERATE_50HZ = 50,
        E_FRAMERATE_60HZ = 60,
    };

    class TimerStatus;   // fwd: Update/CheckForPlayerDisconnectTimeout/UpdatePing param
}

namespace CgsNetwork
{
    // Forward declarations -- these are pointer-only members / params here, so an
    // incomplete type suffices and we avoid a transitive header cascade.
    // (NetworkAdapter and ConnectionData come from CgsNetworkAdapterBase.h above.)
    struct PlayerManager;
    struct Message;
    struct ReliableMessage;
    struct SignalMessage;

    // KI_MAX_MESSAGE_TYPES is the per-player message-table capacity. DWARF spells 41; the
    // ARTIST asm uses 44 (the Construct init loop, the RegisterMessageType bound, and the
    // 0x90/0x610 array spacing all agree on 44).
    const s32 KI_MAX_MESSAGE_TYPES = 44;

    // Ping cadence (DWARF CgsNetworkPlayer.h:45-46).
    const s32 KI_FRAME_GAP_BETWEEN_PINGS         = 2;
    const s32 KI_FRAME_GAP_BETWEEN_PINGS_IN_GAME = 5;

    // The shared signed player-index typedef + "no player" sentinel (mirrors
    // MessageWithPlayerIDs::NetworkPlayerID, declared here so the NetworkPlayer API does
    // not have to pull in the whole message hierarchy).
    typedef s32 NetworkPlayerID;
    const NetworkPlayerID KI_INVALID_PLAYER_ID = -1;

    // Reliable-message arrival / delivery callbacks (DWARF CgsNetworkPlayer.h:61/68). The
    // arrived callback fires when a reliable message is received; the delivered callback
    // fires when our reliable send is acked (lbDelivered) or nacked.
    typedef void (*ReliableMessageArrivedCallback)(ReliableMessage* lpMessage,
                                                   NetworkPlayerID liFromPlayerID,
                                                   void* lpUserData);
    typedef void (*ReliableMessageDeliveredCallback)(bool lbDelivered, bool lbWasReliable,
                                                     SignalMessage* lpMessage,
                                                     NetworkPlayerID liToPlayerID,
                                                     void* lpUserData);

    // ------------------------------------------------------------------------------
    // NetMessageData -- one registered message-type slot (send or recv). 0x20 bytes.
    // SHAPE from DWARF CgsNetworkPlayer.h:111; offsets confirmed by the RegisterMessageType
    // / Construct stores.
    struct NetMessageData
    {
        s32                              meType;                 // +0x00 EMessageType
        s32                              miLength;               // +0x04 packed length
        Message*                         mpMsg;                  // +0x08
        ReliableMessageArrivedCallback   mpfMsgArrivedCallback;  // +0x0C
        ReliableMessageDeliveredCallback mpfMsgDeliveredCallback;// +0x10
        void*                            mpCallbackUserData;     // +0x14
        s32                              miValidCountdown;       // +0x18 (DWARF :123)
        u16                              mu16Frame;              // +0x1C (DWARF :124)
        u16                              mu16Pad;                // +0x1E (align to 0x20)

        void Construct();
        void Prepare(s32 leType, s32 liLength, Message* lpMsg);
    };

    // ConnectionData (the embedded "fake network conditions" buffered-message block this TU
    // zero-inits / copies whole) is the CgsNetwork::ConnectionData type homed in
    // CgsNetworkAdapterBase.h (included above), so SendTo receives the same value type.

    // ------------------------------------------------------------------------------
    // Params passed (by pointer) into NetworkPlayer::Construct (DWARF :146).
    struct CgsNetworkPlayerConstructParams
    {
        PlayerManager* mpPlayerManager;   // +0x00
    };

    // ------------------------------------------------------------------------------
    // The per-peer network player.
    struct NetworkPlayer
    {
        virtual void Construct(CgsNetworkPlayerConstructParams* lpConstructParams);
        virtual bool Prepare(NetworkAdapter* lpNetworkAdapter,
                             const CgsSystem::TimerStatus* lpTimerStatus,
                             const char* lpcName,
                             CgsSystem::EFrameRate leLocalConsoleFrameRate,
                             CgsSystem::EFrameRate leRemoteConsoleFrameRate,
                             NetworkPlayerID liPlayerID);
        virtual bool Release();
        virtual void Update(const CgsSystem::TimerStatus* lpTimerStatus,
                            u16 lu16CurrentFrame, bool lbInGame);

        void Destruct();

        // --- message-table management ---
        void     RegisterMessageType(s32 leType, s32 liLength, Message* lpSendMsg,
                                     Message* lpRecvMsg,
                                     ReliableMessageArrivedCallback lpfArrived,
                                     ReliableMessageDeliveredCallback lpfDelivered,
                                     void* lpUserData);
        void     UnRegisterMessageType(s32 leType);
        bool     IsMessageTypeRegistered(s32 leType);
        Message* GetRegisteredSendMessage(s32 leType);
        Message* GetRegisteredRecvMessage(s32 leType);
        void     ResetAllMessages();

        // --- per-frame send pump ---
        void SendMessages();

        // --- connection / identity accessors ---
        NetworkPlayerID GetPlayerID() const;
        void            SetConnectionData(ConnectionData lConnectionData);
        ConnectionData  GetConnectionData() const;
        const char*     GetName() const;
        void            SetLocalConsoleFrameRate(CgsSystem::EFrameRate leFrameRate);
        void            SetRemoteConsoleFrameRate(CgsSystem::EFrameRate leFrameRate);
        CgsSystem::EFrameRate GetLocalConsoleFrameRate();
        CgsSystem::EFrameRate GetRemoteConsoleFrameRate();
        bool            HasConnectionFailed() const;

    protected:
        void CheckForPlayerDisconnectTimeout(const CgsSystem::TimerStatus* lpTimerStatus);

        // Derived players (e.g. BrnNetwork::BrnNetworkPlayer) read the owning session
        // registry directly (X360 dereferences this+0xC); exposed protected so the derived
        // TUs reach it by name rather than by raw offset.
        PlayerManager* GetPlayerManager() const { return mpPlayerManager; }

    private:
        void UnregisterAllMessages();
        void UpdatePing(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame,
                        bool lbInGame);
        void UpdateMessagesReceived();

        // --- layout (frozen against the ARTIST asm offsets) ---
        // (vptr occupies +0x00)
        NetworkAdapter*       mpNetworkAdapter;          // +0x004
        NetworkPlayerID       mPlayerID;                 // +0x008
        PlayerManager*        mpPlayerManager;           // +0x00C
        u16                   mu16CurrentFrame;          // +0x010
        u16                   mu16Pad012;                // +0x012 (align Time to +0x14)
        CgsSystem::Time       mTimeLastPacketReceived;   // +0x014 (8B)
        f32                   mfPingToReplyTo;           // +0x01C
        ConnectionData        mConnectionData;           // +0x020 (0x70B)
        NetMessageData        maSendMessageData[KI_MAX_MESSAGE_TYPES]; // +0x090
        NetMessageData        maRecvMessageData[KI_MAX_MESSAGE_TYPES]; // +0x610
        s32                   miNumberMessagesRegistered;// +0xB90
        // The per-player packet packer. CompressionAndEncryptionUtils is a thin stateless
        // helper (its accumulated-bandwidth ledger is file-scope static), so the X360 reserves
        // just the 4-byte slot here; the explicit pad keeps macName at +0xB98.
        CompressionAndEncryptionUtils mPacketPacker;     // +0xB94
        u8                    mPadB95[3];                // +0xB95 (pad to the 4-byte slot)
        char                  macName[16];               // +0xB98
        bool                  mbHasConnectionFailed;     // +0xBA8
        bool                  mbNetworkPlayerPaused;     // +0xBA9
        u8                    mPadBAA[2];                // +0xBAA (align f32 to +0xBAC)
        f32                   mfPingInMs;                // +0xBAC
        PingMessage           mPingMessageSend;          // +0xBB0 (0x24)
        PingMessage           mPingMessageRecv;          // +0xBD4 (0x24)
        PingReplyMessage      mPingReplyMessageSend;     // +0xBF8 (0x24)
        PingReplyMessage      mPingReplyMessageRecv;     // +0xC1C (0x24)
        CgsSystem::EFrameRate meLocalConsoleFrameRate;   // +0xC40
        CgsSystem::EFrameRate meRemoteConsoleFrameRate;  // +0xC44
    };

    // ------------------------------------------------------------------------------
    // PlayerMenuData -- the lobby/menu view of a player (DWARF :71). The X360 Clear writes
    // the named offsets below (macName is 16 bytes here, matching NetworkPlayer's name).
    struct PlayerMenuData
    {
        virtual void Clear();

        s32  miConnectionIndex;   // +0x04  (Clear: =-1)
        char macName[16];         // +0x08  (Clear: macName[0]=0)
        u32  mNameColour;         // +0x18  RGBA (Clear: =0)
        u8   mu8PlayerType;       // +0x1C  (Clear: =3)
        u8   mu8HeadsetStatus;    // +0x1D  (Clear: =0)
    };
}
