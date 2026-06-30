#pragma once

// ===================================================================================
// BrnNetwork::CameraX360 -- owning header
//   b5-decomp/src/GameSource/Network/Managers/X360/BrnNetworkCameraX360.{h,cpp}
//
// The GAME-side (BrnNetwork) Xbox-360 online spectator / network-camera manager owned by
// BrnNetworkManager. It lets a player request and stream another (remote) player's live
// "vision camera" picture over the network:
//   * it owns an XVID streaming engine (XCamCreateStreamEngine) plus a raw UDP "vision-data
//     protocol" (VDP) socket used to ship the compressed video packets peer-to-peer;
//   * it keeps a fixed table of KI_MAX_REMOTE_PLAYERS remote-player feed slots, each holding
//     the per-feed stream descriptor, the decoded NetworkTexture, the four reliable
//     request/status messages and the per-frame timing latches;
//   * it owns the local player's own camera textures (raw + DXT-compressed) and the
//     compressed-scratch + decoded textures used while receiving a remote feed;
//   * the reliable handshake (CameraRequestMessage / CameraRequestResponseMessage /
//     CameraStatusMessage) is registered per remote player through CgsNetwork::NetworkPlayer
//     and routed back here through the static *Callback trampolines.
//
// Lifecycle is the manager pattern shared with BrnNetwork::GamerPictureManagerX360 /
// BrnNetwork::BuddyManagerX360:
//   Construct(manager, textureCompressor) -- placement-construct the textures, latch deps
//   Prepare(heap) / PrepareTextures(heap)  -- create the stream engine + socket + textures
//   Update()                               -- per-frame: request/stream/receive + status
//   Release() / Destruct()                 -- tear the engine/socket/textures down
//   AddPlayer/RemovePlayer/Disconnected    -- maintain the remote-player feed table
//
// MEMBER LAYOUT (X360-authoritative; absolute byte offsets from `this`, every offset below a
// store/load immediate in the BrnNetworkCameraX360.cpp ARTIST asm). The object is dominated by
// a 256-entry receive-packet ring and an 8-entry remote-player table:
//
//   VideoPacket (stride 1072 == 0x430; ResetVideoPackets walks 256 of them, ReceiveVideoData /
//   SendVideoData stride them by 1072):
//     +0x000  macReceiveBuffer[1040]  char    (the VDP datagram payload; ReceiveFrom recv into +0)
//     +0x410  mOverlapped             CgsXOverlapped (28-byte XOVERLAPPED async I/O; +0x410)
//     +0x42C  mbInUse                 bool    (ReceiveVideoData latches +0x42C == +1068)
//
//   +0x00000  maVideoPackets[256]     (256 * 0x430 == 0x42F00 == 274432)
//   +0x42F08  mpStreamEngine          IXCamStreamEngine*  (XCamCreateStreamEngine handle, +274432)
//
//   RemotePlayerData (stride 360 == 0x168; base S == this + 274440; the table walks 8 of them;
//   the feed-slot loops compare miPlayerID @ S+52 against -1/the wanted id):
//     +0x00  maStreamDescriptor[36]  char  (the per-feed XCAM stream descriptor handed to the
//                                           stream-engine vtable; its leading word is the
//                                           manager back-pointer the asm derefs as **slot)
//     +0x28  mXuid                   u64   (remote player's XUID; AddPlayer stores it)
//     +0x30  miNetworkPlayerIndex    s32   (the server network-player index; -1 == none)
//     +0x34  miPlayerID              s32   (-1 == free slot)
//     +0x38  meReceiveState          s32   (0 idle / 1 first-frame / 2 timed; +0x38 == S+56)
//     +0x3C  meVideoFrameState       s32   (0 idle / 1 first-frame / 2 timed; +0x3C == S+60)
//     +0x40  mLastVideoFrameTime     CgsSystem::Time (8B; S+64)
//     +0x44  mLastReceiveTime        CgsSystem::Time (8B; S+72)
//     +0x50  mDecodedTexture         CgsNetwork::NetworkTexture (28B X360; S+80)
//     +0x6C  mStatusMessageSend      BrnNetwork::CameraStatusMessage (36B; S+108? see note)
//     ...    the four reliable messages + the request/response pair follow (S+60/96/132/176/
//            220/264 in ResetRemotePlayers' Construct calls); the message bodies live in their
//            committed sibling TUs and are embedded BY NAME here.
//     +0x6C  mbFeedActive            bool  (S+108: a remote feed is currently being received)
//     +0x6D  mbFeedRequested         bool  (S+109: our feed request has been accepted)
//
//   After the table (this + 277320 == 0x43B48):
//     +277320 (0x43B48)  mpNetworkManager   BrnNetworkManager*
//     +277324 (0x43B4C)  mVDPSocket         s32   (the VDP UDP socket; -1 == INVALID_SOCKET)
//     +277328 (0x43B50)  mLocalCameraTexture     CgsNetwork::NetworkTexture (the raw local cam)
//     +277356 (0x43B6C)  mReceivedCameraTexture  CgsNetwork::NetworkTexture (decoded remote feed)
//     +277384 (0x43B88)  miNumberActiveFeeds     s32
//     +277388 (0x43B8C)  mRequestTimer           CgsSystem::Time (8B; next-request countdown)
//     +277392 (0x43B90)  -- the timer's fraction word (part of mRequestTimer)
//     +277396 (0x43B94)  miRequestedPlayerID     s32   (-1 == none)
//     +277400 (0x43B98)  mpTextureCompressor     CgsNetwork::NetworkTextureDXTCompress*
//     +277404 (0x43B9C)  miUserSetting           s32   (1 == camera enabled by the user)
//     +277408 (0x43BA0)  mbPrepared              bool
//     +277409 (0x43BA1)  mbGameEnabled           bool
//     +277410 (0x43BA2)  mbFeedActiveAny         bool   (any remote feed currently active)
//
// The X360 byte offsets are 32-bit-pointer/Xenon layout facts; on a 64-bit host the embedded
// NetworkTexture / message sub-objects and the pointers widen, so the absolute offsets are NOT
// static_asserted -- members are accessed BY NAME. _AssertLayout() pins the recovered
// RELATIONSHIPS (the per-entry field order and the table-vs-tail member order), mirroring the
// sibling BrnNetwork::GamerPictureManagerX360::_AssertLayout.
//
// FLAGGED un-homed deps (kept declaration-only, NOT fabricated):
//   * the XVID streaming engine (XCamCreateStreamEngine + the engine's vtable the asm calls by
//     raw slot, e.g. (*(**slot + N))()), CgsSystem::CgsXOverlapped (declared inline in its own
//     platform .cpp, no shared header), the Winsock/XDK socket layer, and
//     CgsNetwork::ServerInterfaceGamesX360::GetPlayerXUIDByID. Methods that pivot on those
//     un-decodable indirect vtable calls are left declaration-only and FLAGGED at their stub.
//
// Original home (from the asm assert strings):
//   ..\..\..\GameSource\Network/X360/BrnNetworkCameraX360.cpp
// ===================================================================================

#include "types.hpp"
#include <cstddef>   // offsetof
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                  // CgsSystem::Time
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"     // CgsNetwork::NetworkTexture
#include "GameSource/Network/Messages/BrnCameraStatusMessage.h"           // CameraStatusMessage
#include "GameSource/Network/Messages/BrnCameraRequestMessage.h"          // CameraRequestMessage
#include "GameSource/Network/Messages/BrnCameraRequestResponseMessage.h"  // CameraRequestResponseMessage
#include "GameShared/Jobs/DXTCompress/CgsNetworkTextureDXTCompress.h"      // NetworkTextureDXTCompress + CompressCallback

namespace CgsMemory { class HeapMalloc; }

namespace CgsNetwork
{
    struct ReliableMessage;
}

namespace BrnNetwork
{
    class BrnNetworkManager;

    class CameraX360
    {
    public:
        // The maximum number of simultaneous remote-player camera feeds (the X360 table walks
        // are all bound by 8, e.g. ResetRemotePlayers' `li r30, 8`).
        static const s32 KI_MAX_REMOTE_PLAYERS = 8;

        // The receive-packet ring size (ResetVideoPackets / ReceiveVideoData / SendVideoData all
        // loop 256 times).
        static const s32 KI_MAX_VIDEO_PACKETS = 256;

        // The invalid-player / invalid-socket sentinels (`li rN, -1`).
        static const s32 KI_INVALID_PLAYER_ID = -1;
        static const s32 KI_INVALID_SOCKET    = -1;

        // The VDP receive datagram size handed to ReceiveFrom (`li r5, 1040`).
        static const s32 KI_VDP_PACKET_SIZE = 1040;

        // The opaque XVID streaming-engine handle. FLAG: the concrete engine type + its vtable
        // (whose slots the X360 calls by raw byte offset) live in the Xbox 360 XDK / the
        // not-yet-homed CgsNetwork XVID glue; modelled here as an incomplete pointed-to type so
        // the handle member is named without fabricating the vtable shape.
        struct IXCamStreamEngine;

        // -------------------------------------------------------------------------------------
        // One async receive packet (stride 0x430). The XOVERLAPPED async-I/O sub-object is the
        // platform-local CgsSystem::CgsXOverlapped (declared inline in its own X360 .cpp, no
        // shared header), so its 28 bytes are reserved as opaque storage here and FLAGGED.
        struct VideoPacket
        {
            char maReceiveBuffer[1040];   // +0x000  the VDP datagram payload
            u8   maOverlapped[28];        // +0x410  CgsSystem::CgsXOverlapped (opaque; FLAGGED)
            bool mbInUse;                 // +0x42C  (== +1068; an async recv is outstanding)
            u8   maPad42D[3];             // +0x42D  pad to the 0x430 stride
        };

        // One remote-player camera-feed slot (stride 0x168). See the header layout note above
        // for the field map; the embedded NetworkTexture + reliable messages are by name.
        struct RemotePlayerData
        {
            char                          maStreamDescriptor[36]; // +0x00 XCAM per-feed descriptor
            u32                           mu32Pad24;              // +0x24 align mXuid to +0x28
            u64                           mXuid;                  // +0x28
            s32                           miNetworkPlayerIndex;   // +0x30 (-1 == none)
            s32                           miPlayerID;             // +0x34 (-1 == free)
            s32                           meReceiveState;         // +0x38
            s32                           meVideoFrameState;      // +0x3C
            CgsSystem::Time               mLastVideoFrameTime;    // +0x40
            CgsSystem::Time               mLastReceiveTime;       // +0x48
            CgsNetwork::NetworkTexture    mDecodedTexture;        // +0x50
            CameraStatusMessage           mStatusMessageSend;     // (ResetRemotePlayers +60)
            CameraStatusMessage           mStatusMessageRecv;     // (ResetRemotePlayers +96)
            CameraRequestMessage          mRequestMessageSend;    // (ResetRemotePlayers +132)
            CameraRequestMessage          mRequestMessageRecv;    // (ResetRemotePlayers +176)
            CameraRequestResponseMessage  mResponseMessageSend;   // (ResetRemotePlayers +220)
            CameraRequestResponseMessage  mResponseMessageRecv;   // (ResetRemotePlayers +264)
            bool                          mbFeedActive;           // +0x6C (== S+108)
            bool                          mbFeedRequested;        // +0x6D (== S+109)
        };

        // X360 0x8258B410 -- latch the manager + compressor, placement-construct the local /
        // received textures and every remote slot's texture, reset the packet ring + feed table.
        void Construct(BrnNetworkManager* lpNetworkManager,
                       CgsNetwork::NetworkTextureDXTCompress* lpTextureCompressor);

        // X360 0x8258B618 -- tear the textures down, reset the ring + table, drop the deps.
        void Destruct();

        // X360 0x82593B28 -- create the XVID stream engine + VDP socket + textures, mark prepared.
        bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc);

        // X360 0x8258B530 -- release the textures, close the socket + stream engine, reset state.
        bool Release();

        // X360 0x825920A8 -- the per-frame tick (request-timer fire -> RequestFeed, then while in
        // game: GetRemoteCameraPictures / SendVideoData / ReceiveVideoData, UpdateCameraStatus,
        // GetLocalCameraPicture). DECLARATION-ONLY (depends on the un-homed stream-engine /
        // socket pump below).
        void Update();

        // X360 0x82592268 -- register a remote player: assert the player manager, resolve a free
        // slot, latch its XUID, and register the three reliable message types on its NetworkPlayer.
        void AddPlayer(s32 liPlayerID);

        // X360 0x8258B798 -- free a player's feed slot, unregister its reliable messages, reset
        // the slot, and clear mbFeedActiveAny when no feed remains.
        void RemovePlayer(s32 liPlayerID);

        // X360 0x82592038 -- a network disconnect: RemovePlayer every populated slot, reset the
        // request timer / requested player.
        void Disconnected();

        // X360 0x82595188 -- a player has finalised: copy its resolved network address into its
        // feed slot (X360 ServerInterfacePlayerParamsX360::GetNetworkAddress).
        void PlayerFinalised(s32 liPlayerID);

        // X360 0x825872A8 -- on game start, tear down any active stream feeds (stream-engine
        // vtable) and reset the request timer. DECLARATION-ONLY (un-homed stream-engine vtable).
        void OnGameStart();

        // X360 0x82593B28's helper -- (re)allocate the local + received + per-slot textures.
        void PrepareTextures(CgsMemory::HeapMalloc* lpHeapMalloc);

        // X360 0x82586A48 -- reset every remote feed slot (sentinels + zeroed state) and
        // (re)construct its embedded reliable messages; release its decoded texture.
        void ResetRemotePlayers();

        // X360 0x825869F8 -- reset the 256-entry receive-packet ring (construct each overlapped,
        // zero the buffer, clear the flags). DECLARATION-ONLY (CgsXOverlapped has no shared home).
        void ResetVideoPackets();

        // X360 0x82595410 -- the decoded camera texture for a player (the local one when the id is
        // the local player / -1; the timed remote feed otherwise), or null when unavailable.
        CgsNetwork::NetworkTexture* GetCameraPicture(s32 liPlayerID);

        // X360 0x825977C0 -- GetCameraPicture(miRequestedPlayerID).
        CgsNetwork::NetworkTexture* GetRequestedCameraPicture();

        // X360 0x82587390 -- the local camera availability/status (0 none / 1 available /
        // 2 in-use / 3 connecting) advertised to the lobby.
        s32 GetLocalCameraStatus();

        // X360 0x8258B6F8 -- stamp the local camera status into the local player's menu data.
        void UpdateCameraStatus();

        // X360 0x82586890 -- queue the local camera picture for DXT compression for transmission.
        // DECLARATION-ONLY (depends on the un-homed XCamGetStatus + stream-engine geometry).
        s32 GetCompressedLocalCameraPicture(
            CgsNetwork::NetworkTexture* lpDstTexture,
            CgsNetwork::NetworkTextureDXTCompress::CompressCallback lCompleteCallback,
            void* lpCompleteData);

        // X360 0x82587370 -- store the user's camera on/off setting.
        void SetUserSetting(s32 liUserSetting);

        // X360 0x82587380 -- store the game-enabled flag.
        void SetGameEnabled(bool lbGameEnabled);

        // ---- the reliable-message arrival/delivery trampolines (registered per NetworkPlayer) --
        // X360 0x8258BD60 / 0x82586FD0 / 0x82587060. DECLARATION-ONLY where they pivot on the
        // un-homed stream-engine vtable (the response-arrived path activates the feed via a
        // vtable call); declared here so AddPlayer can take their address.
        static void _RequestMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                    s32 liFromPlayerID,
                                                    void* lpCamera);
        static void _RequestMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                      void* lpMessage, void* lpUserData);
        static void _RequestResponseMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                            s32 liFromPlayerID,
                                                            void* lpCamera);

    private:
        // X360 0x82586AF8 / 0x82586C50 -- open/close the VDP UDP socket. DECLARATION-ONLY
        // (Winsock/XDK socket layer is un-homed glue).
        void OpenSocket();
        void CloseSocket();

        // X360 0x82586D50 -- recv one datagram into lpcData (returns bytes; *lpnIPAddress = peer
        // IP). DECLARATION-ONLY (Winsock layer un-homed).
        s32 ReceiveFrom(char* lpcData, s32 liDataSize, s32* lpnIPAddress);

        // X360 0x8258BB70 / 0x8258BA18 -- the per-frame VDP receive / send pumps. DECLARATION-ONLY
        // (stream-engine vtable + socket layer un-homed).
        void ReceiveVideoData();
        void SendVideoData();

        // X360 0x825866B0 -- decode each active remote feed into mReceivedCameraTexture and
        // convert it into the slot's texture. DECLARATION-ONLY (stream-engine vtable un-homed).
        void GetRemoteCameraPictures();

        // X360 0x825867E8 -- pump the local camera picture out of the stream engine.
        // DECLARATION-ONLY (stream-engine vtable un-homed).
        void GetLocalCameraPicture();

        // X360 0x8258B928 -- send a feed request for the requested player. DECLARATION-ONLY
        // (the requested-player resolution chains through the un-homed player-finalised state).
        void RequestFeed();

        // X360 0x825952C0 -- send a feed-release request + tear the slot down. DECLARATION-ONLY
        // (un-homed ServerInterfacePlayerInfoDataX360 derived local-player-info layout).
        void ReleaseFeed(s32 liPlayerID);

        // X360 0x825871A0 -- tear an active/requested feed slot's stream off the engine.
        // DECLARATION-ONLY (un-homed stream-engine vtable).
        void UnregisterPlayer(s32 liPlayerID);

        // Find the populated slot for liPlayerID, or null. (Spelt inline by the X360 everywhere
        // the feed table is walked; factored out here for the bodied callers.)
        RemotePlayerData* GetSlotForPlayer(s32 liPlayerID);

        // -- the receive-packet ring ----------------------------------------------------------
        VideoPacket             maVideoPackets[KI_MAX_VIDEO_PACKETS]; // +0x00000
        IXCamStreamEngine*      mpStreamEngine;                       // +274432

        // -- the remote-player feed table -----------------------------------------------------
        RemotePlayerData        maRemotePlayers[KI_MAX_REMOTE_PLAYERS]; // +274440

        // -- deps + local textures + state ----------------------------------------------------
        BrnNetworkManager*                     mpNetworkManager;        // +277320
        s32                                    mVDPSocket;              // +277324
        CgsNetwork::NetworkTexture             mLocalCameraTexture;     // +277328
        CgsNetwork::NetworkTexture             mReceivedCameraTexture;  // +277356
        s32                                    miNumberActiveFeeds;     // +277384
        CgsSystem::Time                        mRequestTimer;           // +277388
        s32                                    miRequestedPlayerID;     // +277396
        CgsNetwork::NetworkTextureDXTCompress* mpTextureCompressor;     // +277400
        s32                                    miUserSetting;           // +277404
        bool                                   mbPrepared;              // +277408
        bool                                   mbGameEnabled;           // +277409
        bool                                   mbFeedActiveAny;         // +277410

        static void _AssertLayout();
    };
}
