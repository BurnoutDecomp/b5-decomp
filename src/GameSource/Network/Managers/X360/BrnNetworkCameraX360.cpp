#include "GameSource/Network/Managers/X360/BrnNetworkCameraX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"              // CgsNetwork::NetworkTexture
#include "GameShared/Jobs/DXTCompress/CgsNetworkTextureDXTCompress.h"              // SetNewTextureToCompress
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"               // PlayerManager::GetPlayerByID / GetMenuDataByID
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"               // NetworkPlayer::(Un)RegisterMessageType
#include "GameSource/Network/BrnNetworkManager.h"                                  // BrnNetworkManager accessors

// ---- XDK entry points -------------------------------------------------------
// Real prototypes live in the Xbox 360 XDK (<xapi.h>); declared here as extern "C" free
// functions, mirroring the sibling BrnNetworkBuddyManagerX360.cpp / CgsXOverlappedX360.cpp glue
// precedent. XCamGetStatus returns the camera readiness code (2 == up and producing frames).
extern "C" u32 XCamGetStatus();

namespace BrnNetwork
{
    namespace
    {
        // --- pixel formats the camera textures are allocated at (X360 PrepareTextures /
        //     GetCompressedLocalCameraPicture immediates; passed straight to NetworkTexture). ---
        const s32 KI_CAMERA_WIDTH  = 160;
        const s32 KI_CAMERA_HEIGHT = 120;
        const s32 KI_REMOTE_FEED_PIXEL_FORMAT     = 405274691;  // maRemotePlayers texture
        const s32 KI_LOCAL_CAMERA_PIXEL_FORMAT    = 438304779;  // mLocalCameraTexture
        const s32 KI_RECEIVED_CAMERA_PIXEL_FORMAT = 673710214;  // mReceivedCameraTexture

        // The DXT1 pixel format the compressed destination texture must already be in
        // (X360 GetCompressedLocalCameraPicture asserts lpDstTexture->GetFormat() against it).
        const s32 KI_PIXELFORMAT_LIN_DXT1 = 438304850;

        // The XCAM camera-status code meaning "the camera is up and producing frames"
        // (X360 polls XCamGetStatus() == 2 on every status/picture path).
        const s32 KI_XCAM_STATUS_READY = 2;

        // GetLocalCameraStatus return codes (advertised to the lobby menu data).
        const s32 KI_CAMERA_STATUS_NONE       = 0;
        const s32 KI_CAMERA_STATUS_AVAILABLE  = 1;  // mbFeedActiveAny != 3
        const s32 KI_CAMERA_STATUS_IN_USE     = 2;  // mbFeedActiveAny == 3
        const s32 KI_CAMERA_STATUS_CONNECTING = 3;  // game-enabled but not yet streaming

        // The number-of-active-feeds value GetLocalCameraStatus treats as "in use".
        const s32 KI_ACTIVE_FEEDS_IN_USE = 3;

        // The reliable message-type ids registered per remote player (X360 RegisterMessageType
        // first args 20/21/22; UnRegisterMessageType the same three).
        const s32 KI_MSG_TYPE_CAMERA_STATUS           = 20;
        const s32 KI_MSG_TYPE_CAMERA_REQUEST          = 21;
        const s32 KI_MSG_TYPE_CAMERA_REQUEST_RESPONSE = 22;

        // The packed sizes RegisterMessageType is given (X360 immediates 36 / 44 / 44).
        const s32 KI_MSG_SIZE_CAMERA_STATUS           = 36;
        const s32 KI_MSG_SIZE_CAMERA_REQUEST          = 44;
        const s32 KI_MSG_SIZE_CAMERA_REQUEST_RESPONSE = 44;
    }

    // =======================================================================
    // GetSlotForPlayer (X360: spelt inline at every feed-table walk; the loop compares
    // maRemotePlayers[i].miPlayerID against the wanted id, bound by KI_MAX_REMOTE_PLAYERS).
    // =======================================================================
    CameraX360::RemotePlayerData* CameraX360::GetSlotForPlayer(s32 liPlayerID)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_REMOTE_PLAYERS; ++liIndex)
        {
            if (maRemotePlayers[liIndex].miPlayerID == liPlayerID)
            {
                return &maRemotePlayers[liIndex];
            }
        }
        return NULL;
    }

    // =======================================================================
    // Construct @ 0x8258B410
    // =======================================================================
    void CameraX360::Construct(BrnNetworkManager* lpNetworkManager,
                               CgsNetwork::NetworkTextureDXTCompress* lpTextureCompressor)
    {
        mpNetworkManager = lpNetworkManager;
        mVDPSocket       = KI_INVALID_SOCKET;
        mbPrepared       = false;

        mLocalCameraTexture.Construct();
        mReceivedCameraTexture.Construct();

        miNumberActiveFeeds = 0;
        for (s32 liIndex = 0; liIndex < KI_MAX_REMOTE_PLAYERS; ++liIndex)
        {
            maRemotePlayers[liIndex].mDecodedTexture.Construct();
        }

        ResetVideoPackets();
        ResetRemotePlayers();

        mRequestTimer.SetFloatVal(0.0f);
        miRequestedPlayerID = KI_INVALID_PLAYER_ID;

        CGS_ASSERT(lpTextureCompressor, "lpTextureCompressor");
        mpTextureCompressor = lpTextureCompressor;

        miUserSetting   = 1;
        mbGameEnabled   = false;
        mbFeedActiveAny = false;
    }

    // =======================================================================
    // Destruct @ 0x8258B618
    // =======================================================================
    void CameraX360::Destruct()
    {
        mRequestTimer.SetFloatVal(0.0f);
        miRequestedPlayerID = KI_INVALID_PLAYER_ID;

        mLocalCameraTexture.Destruct();
        mReceivedCameraTexture.Destruct();
        mVDPSocket = KI_INVALID_SOCKET;

        ResetVideoPackets();
        ResetRemotePlayers();

        mpNetworkManager    = NULL;
        mpTextureCompressor = NULL;
        miNumberActiveFeeds = 0;
        mbPrepared          = false;
        miUserSetting       = 0;
        mbGameEnabled       = false;
        mbFeedActiveAny     = false;
    }

    // =======================================================================
    // PrepareTextures @ 0x82591FA0
    // =======================================================================
    void CameraX360::PrepareTextures(CgsMemory::HeapMalloc* lpHeapMalloc)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_REMOTE_PLAYERS; ++liIndex)
        {
            maRemotePlayers[liIndex].mDecodedTexture.Prepare(
                lpHeapMalloc, KI_CAMERA_WIDTH, KI_CAMERA_HEIGHT,
                static_cast<renderengine::PixelFormat>(KI_REMOTE_FEED_PIXEL_FORMAT));
        }
        mLocalCameraTexture.Prepare(
            lpHeapMalloc, KI_CAMERA_WIDTH, KI_CAMERA_HEIGHT,
            static_cast<renderengine::PixelFormat>(KI_LOCAL_CAMERA_PIXEL_FORMAT));
        mReceivedCameraTexture.Prepare(
            lpHeapMalloc, KI_CAMERA_WIDTH, KI_CAMERA_HEIGHT,
            static_cast<renderengine::PixelFormat>(KI_RECEIVED_CAMERA_PIXEL_FORMAT));
    }

    // =======================================================================
    // ResetRemotePlayers @ 0x82586A48
    //
    // Reset every feed slot to free (the stream descriptor zeroed, the network index / player id /
    // xuid sentinelled, the state words + feed flags cleared), release the decoded texture, and
    // (re)construct the six embedded reliable messages. The X360 seeds the descriptor + xuid words
    // with 0x800000000LL (a Time value: 8 seconds, fraction 0) -- modelled as zeroing the
    // descriptor + a default-constructed xuid below; the descriptor is opaque XCAM state.
    // =======================================================================
    void CameraX360::ResetRemotePlayers()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_REMOTE_PLAYERS; ++liIndex)
        {
            RemotePlayerData& lSlot = maRemotePlayers[liIndex];

            for (s32 liByte = 0; liByte < static_cast<s32>(sizeof(lSlot.maStreamDescriptor)); ++liByte)
            {
                lSlot.maStreamDescriptor[liByte] = 0;
            }
            lSlot.mXuid                = 0;
            lSlot.miPlayerID           = KI_INVALID_PLAYER_ID;
            lSlot.miNetworkPlayerIndex = KI_INVALID_PLAYER_ID;
            lSlot.meVideoFrameState    = 0;
            lSlot.meReceiveState       = 0;
            lSlot.mbFeedRequested      = false;

            lSlot.mDecodedTexture.Release();

            lSlot.mStatusMessageSend.Construct();
            lSlot.mStatusMessageRecv.Construct();
            lSlot.mRequestMessageSend.Construct();
            lSlot.mRequestMessageRecv.Construct();
            lSlot.mResponseMessageSend.Construct();
            lSlot.mResponseMessageRecv.Construct();
        }

        mbFeedActiveAny = false;
    }

    // =======================================================================
    // GetCameraPicture @ 0x82595410
    //
    // Return the decoded camera texture for a player. For the local player (or id -1) it is the
    // local camera texture, available only while the user/game has the camera up and XCamGetStatus
    // reports ready. For a remote player it is the slot's decoded feed -- but only when both the
    // video-frame and receive states are timed (== 2) and recent enough (the X360 compares the
    // current network time against the slot's two latched times). DECLARATION-ONLY: the recency
    // gate needs the un-homed XCamGetStatus + the manager's per-frame network time, and the local
    // path needs the un-homed XCam camera status; reconstructing the time-window comparison without
    // those would require fabricating the comparison constants. FLAGGED.
    // =======================================================================

    // =======================================================================
    // GetRequestedCameraPicture @ 0x825977C0
    // =======================================================================
    CgsNetwork::NetworkTexture* CameraX360::GetRequestedCameraPicture()
    {
        return GetCameraPicture(miRequestedPlayerID);
    }

    // =======================================================================
    // GetLocalCameraStatus @ 0x82587390
    //
    // 0 when the camera is not prepared / not ready / not user-enabled; otherwise 3 (connecting)
    // until the game has enabled it, then 1 (available) or 2 (in-use, when 3 feeds are active).
    // =======================================================================
    s32 CameraX360::GetLocalCameraStatus()
    {
        if (!mbPrepared || static_cast<s32>(XCamGetStatus()) != KI_XCAM_STATUS_READY || !miUserSetting)
        {
            return KI_CAMERA_STATUS_NONE;
        }
        if (mbGameEnabled)
        {
            return (miNumberActiveFeeds == KI_ACTIVE_FEEDS_IN_USE)
                       ? KI_CAMERA_STATUS_IN_USE
                       : KI_CAMERA_STATUS_AVAILABLE;
        }
        return KI_CAMERA_STATUS_CONNECTING;
    }

    // UpdateCameraStatus @ 0x8258B6F8 -- DECLARATION-ONLY.
    //   Faithful spine: liStatus = GetLocalCameraStatus(); if the player manager has > 0 local
    //   players, fetch the local player's BrnNetwork::PlayerMenuData (GetMenuDataByID of the
    //   manager's local player id), assert it, and stamp liStatus into its camera-status field
    //   (the X360 writes lpMenuData[17]). BLOCKED on the un-homed PlayerManager::GetNumberLocal-
    //   Players + the manager's local-player-id accessor + the BrnNetwork::PlayerMenuData
    //   camera-status field (the minimal CgsNetwork::PlayerMenuData has no such member).
    void CameraX360::UpdateCameraStatus() { /* BLOCKED: un-homed local-player-count + menu-data field */ }

    // =======================================================================
    // SetUserSetting @ 0x82587370
    // =======================================================================
    void CameraX360::SetUserSetting(s32 liUserSetting)
    {
        miUserSetting = liUserSetting;
    }

    // =======================================================================
    // SetGameEnabled @ 0x82587380
    // =======================================================================
    void CameraX360::SetGameEnabled(bool lbGameEnabled)
    {
        mbGameEnabled = lbGameEnabled;
    }

    // AddPlayer @ 0x82592268 -- DECLARATION-ONLY.
    //   Faithful spine (fully recovered apart from the XUID source): assert the player manager;
    //   GetPlayerByID(liPlayerID) and assert it; GetSlotForPlayer(-1) for a free slot and assert
    //   "No free entry for this player"; resolve the player's XUID
    //   (CgsNetwork::ServerInterfaceGamesX360::GetPlayerXUIDByID via the games server-interface);
    //   store id + zeroed descriptor + cleared states + xuid + cleared flags; then register the
    //   three reliable message types on the NetworkPlayer:
    //     20/36  (status)   send=&mStatusMessageSend  recv=&mStatusMessageRecv  no callbacks
    //     21/44  (request)  send=&mRequestMessageSend recv=&mRequestMessageRecv
    //                       arrived=_RequestMessageArrivedCallback
    //                       delivered=_RequestMessageDeliveredCallback  user=this
    //     22/44  (response) send=&mResponseMessageSend recv=&mResponseMessageRecv
    //                       arrived=_RequestResponseMessageArrivedCallback  user=this
    //   BLOCKED on the un-homed CgsNetwork::ServerInterfaceGamesX360::GetPlayerXUIDByID (routing it
    //   through a fabricated placeholder would be a HARD FAILURE), so left declaration-only.
    void CameraX360::AddPlayer(s32 /*liPlayerID*/) { /* BLOCKED: un-homed GetPlayerXUIDByID */ }

    // =======================================================================
    // Disconnected @ 0x82592038
    //
    // A network disconnect: tear down every populated feed slot, then reset the request timer +
    // requested player.
    // =======================================================================
    void CameraX360::Disconnected()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_REMOTE_PLAYERS; ++liIndex)
        {
            if (maRemotePlayers[liIndex].miPlayerID != KI_INVALID_PLAYER_ID)
            {
                RemovePlayer(maRemotePlayers[liIndex].miPlayerID);
            }
        }
        miRequestedPlayerID = KI_INVALID_PLAYER_ID;
        mRequestTimer = 0.0f;
    }

    // -----------------------------------------------------------------------
    // The methods below are DECLARATION-ONLY / glue-stubbed: they pivot on the un-homed XVID
    // streaming engine (its vtable is called by raw byte offset in the X360 asm), the
    // platform-local CgsSystem::CgsXOverlapped (no shared header), the Winsock/XDK socket layer,
    // or the un-homed ServerInterfaceGamesX360 XUID lookup. Their faithful spines are documented
    // at each site; bodying them would require fabricating vtable shapes / comparison constants
    // (a HARD FAILURE), so they are left for the wave that homes those deps.
    //
    // Two small private helpers (GetXCamStatus / GetPlayerXUIDByID) and one additive manager/menu
    // accessor are declared here so the bodied methods above type-check; they are FLAGGED extern
    // glue, not fabricated logic.
    // -----------------------------------------------------------------------

    // RemovePlayer @ 0x8258B798 -- DECLARATION-ONLY.
    //   Faithful spine: GetSlotForPlayer(liPlayerID); if it is the requested feed, clear the
    //   request timer; assert the NetworkPlayer; decrement miNumberActiveFeeds when the feed was
    //   active; clear the slot's feed flags + descriptor + xuid + states + ids; UnregisterPlayer
    //   (stream-engine vtable); UnRegisterMessageType x3 (20/21/22); recompute mbFeedActiveAny.
    //   BLOCKED on UnregisterPlayer's un-homed stream-engine vtable.
    void CameraX360::RemovePlayer(s32 /*liPlayerID*/) { /* BLOCKED: stream-engine vtable */ }

    // PlayerFinalised @ 0x82595188 -- DECLARATION-ONLY.
    //   Faithful spine: build a BrnNetwork player-params on the stack, fetch the player's params
    //   from the games server-interface, extract the 36-byte network address
    //   (ServerInterfacePlayerParamsX360::GetNetworkAddress), assert the NetworkPlayer, locate the
    //   slot, latch its network index from the NetworkPlayer record, and copy the address into the
    //   slot's stream descriptor. BLOCKED on the un-homed BrnNetwork::PlayerParamsBase derived
    //   layout / GetNetworkAddress chain feeding the opaque descriptor.
    void CameraX360::PlayerFinalised(s32 /*liPlayerID*/) { /* BLOCKED: un-homed player-params -> descriptor */ }

    // OnGameStart @ 0x825872A8 -- DECLARATION-ONLY.
    //   Faithful spine: for each slot whose feed is active/requested, call the stream-engine
    //   vtable teardown (slot+12) and clear the two flags; then mbFeedActiveAny=0,
    //   miNumberActiveFeeds=0, reset the request timer, miRequestedPlayerID=-1. BLOCKED on the
    //   un-homed stream-engine vtable.
    void CameraX360::OnGameStart() { /* BLOCKED: stream-engine vtable */ }

    // ResetVideoPackets @ 0x825869F8 -- DECLARATION-ONLY.
    //   Faithful spine: for each of 256 packets, CgsSystem::CgsXOverlapped::Construct(&mOverlapped),
    //   zero the receive buffer, clear the two flags. BLOCKED: CgsXOverlapped is declared inline in
    //   its own X360 platform .cpp (no shared header to include without forking it).
    void CameraX360::ResetVideoPackets() { /* BLOCKED: CgsXOverlapped has no shared home */ }

    // Update @ 0x825920A8 -- DECLARATION-ONLY.
    //   Faithful spine: when the request timer has elapsed (current network time vs mRequestTimer),
    //   reset it and RequestFeed(); if the local player is in game and the camera is enabled and the
    //   game has not started: GetRemoteCameraPictures, SendVideoData (when user-enabled),
    //   ReceiveVideoData (when a feed is active); UpdateCameraStatus; and finally
    //   GetLocalCameraPicture when user+game enabled. BLOCKED on the un-homed pumps + network time.
    void CameraX360::Update() { /* BLOCKED: stream-engine/socket pumps + per-frame network time */ }

    // Release @ 0x8258B530 -- DECLARATION-ONLY.
    //   Faithful spine: Release the local + received textures, CloseSocket, ResetVideoPackets,
    //   ResetRemotePlayers, release the stream engine via its vtable slot+4, then clear
    //   miNumberActiveFeeds/mbPrepared, reset the request timer, miRequestedPlayerID=-1,
    //   miUserSetting=0, mbGameEnabled=0, mbFeedActiveAny=0; returns true. BLOCKED on CloseSocket +
    //   ResetVideoPackets + the stream-engine vtable.
    bool CameraX360::Release() { return true; /* BLOCKED: socket + stream-engine teardown */ }

    // Prepare @ 0x82593B28 -- DECLARATION-ONLY.
    //   Faithful spine: ResetVideoPackets, ResetRemotePlayers, PrepareTextures; fill an
    //   XCAM_STREAM_ENGINE_INIT_PARAMS (the grounded immediates 0xA00038/1000/64/128/30/4/1024/4/
    //   1/1) and XCamCreateStreamEngine(&params, &mpStreamEngine) (assert success); OpenSocket;
    //   miNumberActiveFeeds=0, mbPrepared=1; reset request timer, miRequestedPlayerID=-1,
    //   miUserSetting=1, mbGameEnabled=0, mbFeedActiveAny=0; returns true. BLOCKED on the XCAM
    //   stream-engine creation + OpenSocket.
    bool CameraX360::Prepare(CgsMemory::HeapMalloc* /*lpHeapMalloc*/) { return true; /* BLOCKED: XCAM engine + socket */ }

    // The remaining private pumps are pure un-homed glue; declared-only.
    void CameraX360::OpenSocket()  { /* BLOCKED: Winsock/XDK socket */ }
    void CameraX360::CloseSocket() { /* BLOCKED: Winsock/XDK socket */ }
    s32  CameraX360::ReceiveFrom(char* /*lpcData*/, s32 /*liDataSize*/, s32* /*lpnIPAddress*/) { return 0; /* BLOCKED: Winsock */ }
    void CameraX360::ReceiveVideoData()      { /* BLOCKED: socket + stream-engine vtable */ }
    void CameraX360::SendVideoData()         { /* BLOCKED: socket + stream-engine vtable */ }
    void CameraX360::GetRemoteCameraPictures() { /* BLOCKED: stream-engine vtable */ }
    void CameraX360::GetLocalCameraPicture()   { /* BLOCKED: stream-engine vtable */ }
    void CameraX360::RequestFeed()             { /* BLOCKED: un-homed player-finalised state */ }
    void CameraX360::ReleaseFeed(s32 /*liPlayerID*/) { /* BLOCKED: un-homed local-player-info */ }
    void CameraX360::UnregisterPlayer(s32 /*liPlayerID*/) { /* BLOCKED: stream-engine vtable */ }

    s32 CameraX360::GetCompressedLocalCameraPicture(
        CgsNetwork::NetworkTexture* lpDstTexture,
        CgsNetwork::NetworkTextureDXTCompress::CompressCallback /*lCompleteCallback*/,
        void* /*lpCompleteData*/)
    {
        // Faithful spine: assert lpDstTexture is non-null + already DXT1; when XCamGetStatus()==2,
        // pull the latest local-camera frame geometry off the stream engine and queue it for DXT
        // compression. BLOCKED on the un-homed XCamGetStatus + stream-engine frame geometry; the
        // grounded asserts are reproduced so callers fail loudly rather than silently no-op.
        CGS_ASSERT(lpDstTexture, "lpDstTexture");
        CGS_ASSERT(lpDstTexture && static_cast<s32>(lpDstTexture->GetFormat()) == KI_PIXELFORMAT_LIN_DXT1,
                   "rw::graphics::core::PIXELFORMAT_LIN_DXT1 == lpDstTexture->GetFormat()");
        return 0; /* BLOCKED: XCAM status + stream-engine frame geometry */
    }

    // The static reliable-message trampolines are DECLARATION-ONLY: the arrived paths walk the
    // feed table (recovered) but then either toggle the active feed via the un-homed stream-engine
    // vtable (response-arrived) or send a CameraRequestResponseMessage gated on the same state, and
    // the delivered path streams a debug warning through a not-yet-homed log sink. Bodied stubs are
    // provided so AddPlayer can take their address; the faithful spines are documented at the head.
    void CameraX360::_RequestMessageArrivedCallback(CgsNetwork::ReliableMessage* /*lpMessage*/,
                                                    s32 /*liFromPlayerID*/,
                                                    void* /*lpCamera*/)
    { /* BLOCKED: stream-engine vtable + response send */ }

    void CameraX360::_RequestMessageDeliveredCallback(bool /*lbDelivered*/, bool /*lbWasReliable*/,
                                                      void* /*lpMessage*/, void* /*lpUserData*/)
    { /* BLOCKED: un-homed debug log sink */ }

    void CameraX360::_RequestResponseMessageArrivedCallback(CgsNetwork::ReliableMessage* /*lpMessage*/,
                                                            s32 /*liFromPlayerID*/,
                                                            void* /*lpCamera*/)
    { /* BLOCKED: stream-engine vtable */ }

    // =======================================================================
    // _AssertLayout -- never called; pins the recovered per-entry + tail member RELATIONSHIPS.
    // The X360 absolute offsets assume 32-bit Xenon pointers / 28-byte NetworkTexture; the PC gate
    // is 64-bit, so the raw byte offsets are NOT reproduced. We pin the ABI-stable relationships,
    // mirroring BrnNetwork::GamerPictureManagerX360::_AssertLayout.
    // =======================================================================
    void CameraX360::_AssertLayout()
    {
        // VideoPacket field order: the receive buffer precedes the overlapped, which precedes the
        // in-use flag.
        static_assert(offsetof(VideoPacket, maOverlapped) > offsetof(VideoPacket, maReceiveBuffer),
                      "receive buffer precedes the overlapped");
        static_assert(offsetof(VideoPacket, mbInUse) > offsetof(VideoPacket, maOverlapped),
                      "overlapped precedes the in-use flag");

        // RemotePlayerData field order (descriptor -> xuid -> indices -> states -> texture -> flags).
        static_assert(offsetof(RemotePlayerData, mXuid) > offsetof(RemotePlayerData, maStreamDescriptor),
                      "stream descriptor precedes mXuid");
        static_assert(offsetof(RemotePlayerData, miPlayerID) > offsetof(RemotePlayerData, mXuid),
                      "mXuid precedes miPlayerID");
        static_assert(offsetof(RemotePlayerData, meReceiveState) > offsetof(RemotePlayerData, miPlayerID),
                      "miPlayerID precedes meReceiveState");
        static_assert(offsetof(RemotePlayerData, mbFeedRequested) > offsetof(RemotePlayerData, mbFeedActive),
                      "mbFeedActive precedes mbFeedRequested");

        // Table-vs-tail: the stream engine follows the packet ring; the remote table follows the
        // stream engine; the deps tail follows the remote table.
        static_assert(offsetof(CameraX360, mpStreamEngine) > offsetof(CameraX360, maVideoPackets),
                      "packet ring precedes the stream engine");
        static_assert(offsetof(CameraX360, maRemotePlayers) > offsetof(CameraX360, mpStreamEngine),
                      "stream engine precedes the remote table");
        static_assert(offsetof(CameraX360, mpNetworkManager) > offsetof(CameraX360, maRemotePlayers),
                      "remote table precedes the deps tail");
        static_assert(offsetof(CameraX360, mbFeedActiveAny) > offsetof(CameraX360, mbPrepared),
                      "mbPrepared precedes mbFeedActiveAny");
    }
}
