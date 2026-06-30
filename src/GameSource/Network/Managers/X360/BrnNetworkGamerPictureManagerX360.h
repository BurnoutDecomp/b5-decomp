#pragma once

// ===================================================================================
// BrnNetwork::GamerPictureManagerX360 -- owning header
//   b5-decomp/src/GameSource/Network/Managers/X360/BrnNetworkGamerPictureManagerX360.{h,cpp}
//
// The GAME-side (BrnNetwork) Xbox-360 gamer-picture (avatar mug-shot) manager owned by
// BrnNetworkManager. It keeps a small fixed table of remote-player gamer pictures plus the
// local player's own picture, drives the async download of those pictures through the
// platform buddy manager (CgsNetwork::BuddyManagerX360::StartDownloadingPictures /
// CancelPictureDownload), copies the downloaded mug-shot into the per-player network texture
// (CgsNetwork::NetworkImageConverter::ConvertAndResize), and (on request) re-compresses a
// stored picture to DXT for transmission (CgsNetwork::NetworkTextureDXTCompress).
//
// Lifecycle is the manager pattern shared with BrnNetwork::BuddyManagerX360:
//   Construct(manager, textureCompressor)  -- placement-construct the textures + latch deps
//   Prepare(heap) / PrepareGamerPictureTextures(heap)  -- allocate the texture buffers
//   Update()                               -- per-frame: pump the download state machine
//   Release() / Destruct()                 -- tear the textures down
//   AddPlayer/RemovePlayer/Disconnected    -- maintain the remote-player table
//
// MEMBER LAYOUT (X360-authoritative; absolute byte offsets from `this`, every offset below a
// store/load immediate in the asm). The object is a table of 7 GamerPictureData entries +
// 1 local-player entry, then the compressed scratch texture, then the manager/compressor
// pointers and the download-progress latches:
//
//   GamerPictureData (stride 0x30 = 48 bytes; ResetGamerPictureData walks 7 of them):
//     +0x00  miPlayerID  s32             (CgsNetwork::K_INVALID_PLAYER_ID == -1 when free)
//     +0x08  mXuid       u64             (the player's Xbox LIVE XUID; the download key)
//     +0x10  mTexture    NetworkTexture  (28 bytes on X360; the decoded mug-shot, 160x120)
//     +0x2C  mbReady     bool            (true once the downloaded picture has been copied in)
//
//   +0x000  maGamerPictureData[7]        (7 * 0x30 = 0x150)
//   +0x150  mLocalPlayerGamerPictureData (one more GamerPictureData, 0x30 -> +0x180)
//   +0x180  mCompressedTexture           NetworkTexture (64x64 DXT scratch; +0x180..+0x19C)
//   +0x19C  mpNetworkManager             BrnNetworkManager*
//   +0x1A0  mpTextureCompressor          NetworkTextureDXTCompress*
//   +0x1A8  mDownloadingXuid             u64   (the XUID currently being downloaded; 0 == idle)
//   +0x1B0  mbPictureDownloaded          bool  (set by the download-complete callback; Update
//                                               consumes it to copy + advance the queue)
//   +0x1B1  mbWasConnectedLastFrame      bool  (last frame's NetConnStatus == connected)
//
// The X360 byte offsets are 32-bit-pointer/Xenon layout facts; on a 64-bit host the embedded
// NetworkTexture sub-objects and the pointers widen, so the absolute offsets are NOT
// static_asserted -- members are accessed BY NAME. _AssertLayout() pins the recovered
// RELATIONSHIPS (the per-entry field order/size and the table-vs-tail member order), mirroring
// the sibling BrnNetwork::BuddyManagerX360::_AssertLayout.
//
// Original home (from the asm assert strings):
//   ..\..\..\GameSource\Network/Managers/X360/BrnNetworkGamerPictureManagerX360.cpp
// ===================================================================================

#include "types.hpp"
#include <cstddef>   // offsetof
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"  // CgsNetwork::NetworkTexture

namespace CgsMemory { class HeapMalloc; }

namespace CgsNetwork
{
    class NetworkTextureDXTCompress;
    // The compress-complete callback shape used by NetworkTextureDXTCompress::SetNewTextureToCompress.
    typedef void (*CompressCallback)(char*, void*);
}

namespace BrnNetwork
{
    class BrnNetworkManager;

    class GamerPictureManagerX360
    {
    public:
        // The fixed maximum number of remote players whose pictures are tracked (the X360
        // table-walk loops bound by 7 -- e.g. ResetGamerPictureData's `li r30, 7`).
        static const s32 KI_MAX_GAMER_PICTURES = 7;

        // The invalid-player sentinel stored in a free slot (X360 `li r27, -1` / `stw r11, -1`).
        static const s32 KI_INVALID_PLAYER_ID = -1;

        // One remote-player gamer-picture slot (stride 0x30; see the header layout note).
        struct GamerPictureData
        {
            s32                     miPlayerID;     // +0x00
            u8                      maPad04[4];     // +0x04 (alignment to the u64 below)
            u64                     mXuid;          // +0x08
            CgsNetwork::NetworkTexture mTexture;    // +0x10
            bool                    mbReady;        // +0x2C
        };

        // X360 0x825574B8 -- placement-construct all the network textures, latch the manager +
        // texture compressor, clear the download latches, then reset the picture table. Called by
        // BrnNetworkManager::Construct.
        void Construct(BrnNetworkManager* lpNetworkManager,
                       CgsNetwork::NetworkTextureDXTCompress* lpTextureCompressor);

        // X360 0x82557600 -- clear the deps + download latches, reset the picture table, then drop
        // the manager pointer. Called by BrnNetworkManager::Destruct.
        void Destruct();

        // X360 0x82557578 -- clear the download latch, reset the picture table, then allocate all
        // the texture buffers. Returns true. Called by BrnNetworkManager::Prepare.
        bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc);

        // X360 0x825575D0 -- clear the download latch and reset the picture table (releasing the
        // texture buffers). Returns true. Called by BrnNetworkManager::Release.
        bool Release();

        // X360 0x82557650 -- the per-frame tick: poll the LIVE connection, kick off the local
        // picture download on connect, cancel an in-flight download on disconnect, and -- when a
        // download has completed -- copy the result into the owning texture and advance the queue.
        // Called by BrnNetworkManager::ProcessAfterSimulation.
        u32 Update();

        // X360 0x825616B8 -- register a remote player: find/allocate its slot, resolve its XUID
        // (from the games server-interface while in a game, or the local player-info otherwise),
        // store the id + XUID, and start the download if none is in flight. Called by
        // BrnNetwork::StateManager::UpdateLogin / BrnNetworkManager::PlayerManagerEventCallback.
        GamerPictureData* AddPlayer(s32 liPlayerID);

        // X360 0x82557770 -- unregister a player: free its slot and, if it was the one downloading,
        // cancel + advance the queue. Returns the freed slot (or null). Called by
        // BrnNetworkManager::PlayerManagerEventCallback.
        GamerPictureData* RemovePlayer(s32 liPlayerID);

        // X360 0x825577F0 -- a network disconnect: free every remote slot (cancelling + advancing
        // the download whenever the freed slot was the one in flight) and the local slot. Called by
        // BrnNetworkManager::TriggerEventFromServerInterface.
        void Disconnected();

        // X360 0x8254C370 -- return the decoded gamer-picture NetworkTexture for a player (the
        // local player's when liPlayerID == -1), or null if the slot is missing / not yet ready.
        // Called by BrnNetwork::StateManager::OutputPlayerTexture / NetworkImageManager.
        CgsNetwork::NetworkTexture* GetGamerPictureTexture(s32 liPlayerID);

        // X360 0x8254C3C0 -- queue the player's ready gamer picture for DXT re-compression through
        // the texture compressor (for transmission). liPlayerID selects the source slot (passed
        // straight to Get), lpCompressedTexture is the destination texture whose stride drives the
        // compressed pitch. Called by NetworkImageManager::HandleRoundResults.
        s32 GetCompressedGamerPictureTexture(s32 liPlayerID,
                                             CgsNetwork::NetworkTexture* lpCompressedTexture,
                                             CgsNetwork::CompressCallback lCompleteCallback,
                                             void* lpCompleteData);

    private:
        // X360 0x8254C2A8 -- find the GamerPictureData slot whose miPlayerID matches liPlayerID
        // (the local slot when liPlayerID != -1 and it owns that id), or null if no slot matches.
        GamerPictureData* Get(s32 liPlayerID);

        // X360 0x8254C528 -- reset every slot (id = -1, xuid = 0, not-ready) and release its
        // texture; also the local + compressed textures. Returns the last Release() result.
        s32 ResetGamerPictureData();

        // X360 0x8254C4A0 -- (re)allocate the 7 remote + local picture textures (160x120) and the
        // compressed scratch texture (64x64). Returns the last Prepare() result.
        s32 PrepareGamerPictureTextures(CgsMemory::HeapMalloc* lpHeapMalloc);

        // X360 0x8254C590 -- pick the next XUID to download (the local player's, else the first
        // not-ready remote slot) and -- if one was found -- kick off its download. Returns `this`.
        GamerPictureManagerX360* DownloadNextGamerPicture();

        // X360 0x8254C638 -- on LIVE connect, resolve the local console XUID and (if not already
        // downloading) start downloading the local player's own gamer picture.
        u32 DownloadLocalGamerPicOnLiveConnect();

        // X360 0x8254C238 -- convert + resize the downloaded compressed picture into the owning
        // player's network texture (the X360 column range [20, 140)).
        s32 CopyTexture(CgsNetwork::NetworkTexture* lpTexture);

        // -- the picture table -----------------------------------------------------------------
        GamerPictureData            maGamerPictureData[KI_MAX_GAMER_PICTURES]; // +0x000
        GamerPictureData            mLocalPlayerGamerPictureData;              // +0x150
        CgsNetwork::NetworkTexture  mCompressedTexture;                        // +0x180

        // -- deps + download latches -----------------------------------------------------------
        BrnNetworkManager*                     mpNetworkManager;        // +0x19C
        CgsNetwork::NetworkTextureDXTCompress* mpTextureCompressor;     // +0x1A0
        u64                                    mDownloadingXuid;        // +0x1A8
        bool                                   mbPictureDownloaded;     // +0x1B0
        bool                                   mbWasConnectedLastFrame; // +0x1B1

        static void _AssertLayout();
    };
}
