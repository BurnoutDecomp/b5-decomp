#pragma once

// ===================================================================================
// BrnNetwork::CameraX360 -- owning header
//   b5-decomp/src/GameSource/Network/Managers/X360/BrnNetworkCameraX360.{h,cpp}
//
// The GAME-side (BrnNetwork) Xbox-360 online camera manager owned by BrnNetworkManager. It lets
// a player request and stream another (remote) player's live camera picture over the network:
//   * it owns an XDK camera stream engine (XCamCreateStreamEngine, IXCamStreamEngine in
//     <xcam.h>) plus a VDP UDP socket the encoded video packets travel on peer-to-peer;
//   * it keeps a table of KI_MAX_ALLOWABLE_PLAYERS remote machines, each holding the peer's
//     XNADDR and XUID (the stream engine's per-console key), its decoded picture, the four
//     reliable request/status messages and the per-frame timing latches;
//   * the reliable handshake (CameraRequestMessage / CameraRequestResponseMessage /
//     CameraStatusMessage) is registered per remote player through CgsNetwork::NetworkPlayer
//     and routed back here through the static *Callback trampolines.
//
// The X360 class has no DWARF. Its PS3 sibling BrnNetwork::CameraPS3 (DecFIGS
// GameSource/Network/PS3/BrnNetworkCameraPS3.h) has the same method set, the same Machine
// record and the same feed-request timer; ARTIST's offsets show the X360 Machine is the PS3
// one with the peer's XNADDR and XUID in front of it (PS3 h:182-211: miIPAddress, mPlayerID,
// meLastPictureDecodeState, meLastDataReceivedState, mLastDataReceivedTime,
// mLastPictureDecodeTime, mCameraPicture, mbSendToPlayer, mbReceiveFromPlayer, the six
// messages), so the record and its members carry the PS3 names.
//
// MEMBER LAYOUT (X360-authoritative; every offset is a load/store immediate in ARTIST):
//   VideoPacket (stride 0x430; 256 of them):
//     +0x000  the received VDP datagram (ReceiveFrom reads KI_VDP_PACKET_SIZE bytes here; the
//             u16 at +0x8 is the stream-engine payload length, the payload starts at +0xA)
//     +0x410  mOverlapped  CgsXOverlapped (the SubmitNetworkData completion record)
//     +0x42C  mbInUse
//   +0x00000  maVideoPackets[256]
//   +0x43000  mpStreamEngine           IXCamStreamEngine*
//   +0x43008  maPlayers[8]             Machine (stride 0x168)
//   +0x43B48  mpNetworkManager
//   +0x43B4C  mVDPSocket               (-1 == INVALID_SOCKET)
//   +0x43B50  mLocalCameraPicture      NetworkTexture (the local preview frame)
//   +0x43B6C  mReceivedCameraPicture   NetworkTexture (a remote frame before conversion)
//   +0x43B88  miNumPlayersSendingTo
//   +0x43B8C  mRequestTimer            CgsSystem::Time
//   +0x43B94  miRequestedPlayerID
//   +0x43B98  mpTextureCompressor
//   +0x43B9C  miUserSetting
//   +0x43BA0  mbPrepared / +0x43BA1 mbGameEnabled / +0x43BA2 mbReceivingFromAnyPlayer
//
// Original home (from the asm assert strings):
//   d:\p4\b5_main\burnout\main\code\gamesource\unity\../Network/X360/BrnNetworkCameraX360.cpp
// ===================================================================================

#include "types.hpp"
#include <cstddef>   // offsetof
#include "GameShared/GameClasses/System/CgsXOverlapped.h"                  // CgsSystem::CgsXOverlapped (<xtl.h>: XNADDR, XUID)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                  // CgsSystem::Time
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"     // CgsNetwork::NetworkTexture
#include "GameSource/Network/Messages/BrnCameraStatusMessage.h"           // CameraStatusMessage, ECameraStatus
#include "GameSource/Network/Messages/BrnCameraRequestMessage.h"          // CameraRequestMessage
#include "GameSource/Network/Messages/BrnCameraRequestResponseMessage.h"  // CameraRequestResponseMessage
#include "GameShared/Jobs/DXTCompress/CgsNetworkTextureDXTCompress.h"      // NetworkTextureDXTCompress + CompressionCompleteCallback

struct IXCamStreamEngine;
namespace CgsMemory { class HeapMalloc; }

namespace CgsNetwork
{
    struct ReliableMessage;
    struct SignalMessage;       // pointer-only (delivered-callback arg)
}

namespace BrnNetwork
{
    class BrnNetworkManager;

    class CameraX360
    {
    public:
        // PS3 sibling h:214-218.
        static const s32 KI_MAX_NUM_VIDEO_PACKETS  = 256;
        static const s32 KI_MAX_PLAYERS_TO_SEND_TO = 3;
        static const s32 KI_MAX_REMOTE_CONSOLES    = 4;
        static const s32 KI_MAX_ALLOWABLE_PLAYERS  = 8;

        // The VDP datagram ReceiveFrom reads into a packet (`li r5, 1040`, ARTIST 0x8258BC7C).
        static const s32 KI_VDP_PACKET_SIZE = 1040;

        // The VDP datagram (KI_VDP_PACKET_SIZE bytes; ReceiveFrom reads one whole, SendVideoData sends
        // its header and payload). Member names are ours: no DWARF/PS3 declaration exists.
        struct VideoDataPacket
        {
            u16  mu16GameDataSize;        // +0x0 the VDP header word (SendVideoData stores 4, 0x8258BA38)
            u8   maReserved[6];           // +0x2 (never written)
            u16  mu16DataSize;            // +0x8 the stream-engine network-data size
            char macData[KI_VDP_PACKET_SIZE - 10]; // +0xA the stream-engine network data
        };

        // A received datagram and its asynchronous submit state (stride 0x430).
        struct VideoPacket
        {
            VideoDataPacket mPacket;              // +0x000
            CgsSystem::CgsXOverlapped mOverlapped; // +0x410
            bool mbInUse;                         // +0x42C
        };

        // One remote machine (stride 0x168); PS3 sibling Machine, h:182.
        struct Machine
        {
            // PS3 h:188.
            enum ETimeState
            {
                E_WAIT_STATE_1   = 0,
                E_WAIT_STATE_2   = 1,
                E_STATE_TIME_SET = 2,
            };

            XNADDR                        mXNAddr;                    // +0x00 the stream engine's console key
            XUID                          mXuid;                      // +0x28 RegisterRemoteConsole's one-entry XUID list
            s32                           miIPAddress;                // +0x30 the VDP peer address (-1 == none)
            s32                           mPlayerID;                  // +0x34 (-1 == free entry)
            ETimeState                    meLastPictureDecodeState;   // +0x38
            ETimeState                    meLastDataReceivedState;    // +0x3C
            CgsSystem::Time               mLastDataReceivedTime;      // +0x40
            CgsSystem::Time               mLastPictureDecodeTime;     // +0x48
            CgsNetwork::NetworkTexture    mCameraPicture;             // +0x50
            bool                          mbSendToPlayer;             // +0x6C
            bool                          mbReceiveFromPlayer;        // +0x6D
            CameraStatusMessage           mStatusMessageSend;         // +0x70
            CameraStatusMessage           mStatusMessageRecv;         // +0x94
            CameraRequestMessage          mRequestMessageSend;        // +0xB8
            CameraRequestMessage          mRequestMessageRecv;        // +0xE4
            CameraRequestResponseMessage  mRequestResponseMessageSend; // +0x110
            CameraRequestResponseMessage  mRequestResponseMessageRecv; // +0x13C
        };

        // X360 0x8258B410 -- latch the manager + compressor, construct the textures, reset the
        // packet ring + the machine table.
        void Construct(BrnNetworkManager* lpNetworkManager,
                       CgsNetwork::NetworkTextureDXTCompress* lpTextureCompressor);

        // X360 0x8258B618 -- tear the textures down, reset the ring + table, drop the deps.
        void Destruct();

        // X360 0x82593B28 -- create the stream engine + VDP socket + textures, mark prepared.
        bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc);

        // X360 0x8258B530 -- release the textures, close the socket + stream engine, reset state.
        bool Release();

        // X360 0x825920A8 -- the per-frame tick.
        void Update();

        // X360 0x82592268 / 0x8258B798 / 0x82592038 / 0x82595188 -- maintain the machine table.
        void AddPlayer(s32 liPlayerID);
        void RemovePlayer(s32 liPlayerID);
        void Disconnected();
        void PlayerFinalised(s32 liPlayerID);

        // X360 0x825872A8 -- on game start, unregister every machine we stream with.
        void OnGameStart();

        // X360 0x82595410 -- the camera picture for a player (the local one for the local player
        // or -1; a remote machine's while its decode and data latches are recent), or null.
        CgsNetwork::NetworkTexture* GetCameraPicture(s32 liPlayerID);

        // X360 0x825977C0 -- GetCameraPicture(miRequestedPlayerID).
        CgsNetwork::NetworkTexture* GetRequestedCameraPicture();

        // X360 0x82586890 -- queue the local camera picture for DXT compression.
        void GetCompressedLocalCameraPicture(
            CgsNetwork::NetworkTexture* lpDstTexture,
            s32 liQualitySetting,
            CgsNetwork::NetworkTextureDXTCompress::CompressionCompleteCallback lCompleteCallback,
            void* lpCompleteData);

        // X360 0x825976A8 -- select whose feed to watch (PS3 h:117).
        void RequestFeed(s32 liPlayerID);

        // X360 0x825952C0 -- tell a player we no longer want its feed (PS3 h:121).
        void ReleaseFeed(s32 liPlayerID);

        // The player whose feed is being watched (-1 == none).
        s32 GetRequestedPlayerID() const { return miRequestedPlayerID; }

        // The user's camera option (the setting SetUserSetting stores; the image manager reads a
        // friends-only value, 2, as the mugshot privilege).
        s32 GetUserSetting() const { return miUserSetting; }

        // X360 0x82587370 / 0x82587380.
        void SetUserSetting(s32 liUserSetting);
        void SetGameEnabled(bool lbGameEnabled);

        // X360 0x82587390 (PS3 h:135).
        ECameraStatus GetLocalCameraStatus();

    private:
        // Find the machine of lPlayerID, or null (PS3 h:244; ARTIST inlines it at every walk).
        Machine* GetMachineEntry(s32 lPlayerID);

        // X360 0x82591FA0 / 0x82586A48 / 0x825869F8.
        void PrepareTextures(CgsMemory::HeapMalloc* lpHeapMalloc);
        void ResetRemotePlayers();
        void ResetVideoPackets();

        // X360 0x8258B928 -- resend the feed request for the requested player (PS3 h:251).
        void RequestFeed();

        // X360 0x825866B0 / 0x825867E8 -- pull decoded frames out of the stream engine.
        void GetRemoteCameraPictures();
        void GetLocalCameraPicture();

        // X360 0x8258B6F8 -- stamp the local camera status into the local player's menu data.
        void UpdateCameraStatus();

        // X360 0x825871A0 -- (re)register a machine with the stream engine for the directions it
        // still streams, or unregister it when it streams in neither.
        void UnregisterPlayer(s32 lPlayerID);

        // X360 0x82586AF8 / 0x82586C50 -- open/close the VDP UDP socket.
        void OpenSocket();
        void CloseSocket();

        // Send one datagram to liIPAddress on the VDP port. PROVISIONAL NAME: no DWARF/PS3/SDK name
        // found; ARTIST holds no copy and inlines it into SendVideoData (its assert line 1076 sits
        // between CloseSocket's 1051 and ReceiveFrom's 1114, ARTIST 0x8258BB0C).
        void SendTo(const char* lpcData, s32 liDataSize, s32 liIPAddress);

        // X360 0x82586D50 -- receive one datagram (returns its size; *lpnIPAddress = the sender).
        s32 ReceiveFrom(char* lppData, s32 liDataSize, s32* lpnIPAddress);

        // X360 0x8258BB70 / 0x8258BA18 -- the per-frame VDP receive / send pumps.
        void ReceiveVideoData();
        void SendVideoData();

        // ---- the reliable-message trampolines (registered per NetworkPlayer; PS3 h:278-303) ----
        static void _RequestMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                    s32 lSendingPlayerID,
                                                    void* lpUserData);
        static void _RequestMessageDeliveredCallback(bool lbSuccess, bool lbFakeNack,
                                                      CgsNetwork::SignalMessage* lpAck,
                                                      s32 lRecvingPlayerID,
                                                      void* lpUserData);
        static void _RequestResponseMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                            s32 lSendingPlayerID,
                                                            void* lpUserData);
        static void _RequestResponseMessageDeliveredCallback(bool lbSuccess, bool lbFakeNack,
                                                              CgsNetwork::SignalMessage* lpAck,
                                                              s32 lRecvingPlayerID,
                                                              void* lpUserData);

        // -- the receive-packet ring ----------------------------------------------------------
        VideoPacket             maVideoPackets[KI_MAX_NUM_VIDEO_PACKETS]; // +0x00000
        IXCamStreamEngine*      mpStreamEngine;                           // +0x43000

        // -- the remote machines ---------------------------------------------------------------
        Machine                 maPlayers[KI_MAX_ALLOWABLE_PLAYERS];      // +0x43008

        // -- deps + local textures + state ----------------------------------------------------
        BrnNetworkManager*                     mpNetworkManager;          // +0x43B48
        SOCKET                                 mVDPSocket;                // +0x43B4C
        CgsNetwork::NetworkTexture             mLocalCameraPicture;       // +0x43B50
        CgsNetwork::NetworkTexture             mReceivedCameraPicture;    // +0x43B6C
        s32                                    miNumPlayersSendingTo;     // +0x43B88
        CgsSystem::Time                        mRequestTimer;             // +0x43B8C
        s32                                    miRequestedPlayerID;       // +0x43B94
        CgsNetwork::NetworkTextureDXTCompress* mpTextureCompressor;       // +0x43B98
        s32                                    miUserSetting;             // +0x43B9C
        bool                                   mbPrepared;                // +0x43BA0
        bool                                   mbGameEnabled;             // +0x43BA1
        bool                                   mbReceivingFromAnyPlayer;  // +0x43BA2

        static void _AssertLayout();
    };
}
