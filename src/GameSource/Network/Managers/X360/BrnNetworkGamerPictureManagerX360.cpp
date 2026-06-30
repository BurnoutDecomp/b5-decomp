#include "GameSource/Network/Managers/X360/BrnNetworkGamerPictureManagerX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"             // CgsNetwork::NetworkTexture
#include "GameShared/GameClasses/Network/Utilities/CgsNetworkImageConverter.h"    // ConvertAndResize
#include "GameShared/Jobs/DXTCompress/CgsNetworkTextureDXTCompress.h"             // SetNewTextureToCompress
#include "GameShared/GameClasses/Network/Buddies/DirtySock/X360/CgsBuddyManagerDirtySockX360.h" // StartDownloadingPictures / CancelPictureDownload
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"              // PlayerManager::GetPlayerByID
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"     // GetLocalPlayerInfo
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.h" // ServerInterfacePlayerInfoDataBase
#include "GameSource/Network/BrnNetworkManager.h"  // BrnNetworkManager accessors

// ---- XDK / Xbox-LIVE entry points -------------------------------------------
// Real prototypes live in the Xbox 360 XDK (<xapi.h>/<xonline.h>) / the EA DirtySock library;
// declared here as extern "C" free functions, mirroring the sibling BrnNetworkBuddyManagerX360.cpp
// glue precedent. Argument shapes are taken from the X360 call sites (register usage), not the
// PPC Hex-Rays.
extern "C"
{
    // The XUID signed in to a controller index (0 == success). The X360 Update path resolves the
    // local console XUID before downloading the local player's own gamer picture.
    u32 XUserGetXUID(u32 luUserIndex, u64* lpXuid);

    // DirtySDK / EA network connection status query. The X360 Update polls it with the FOURCC
    // 'onln' (0x6F6E6C6E) selector to learn whether the console is connected to the network
    // service (returns 1 when connected); the trailing three args are 0.
    int NetConnStatus(int luSelector, int liData, void* lpBuf, int liBufSize);
}

namespace BrnNetwork
{
    namespace
    {
        // The 'onln' DirtySDK connection-status selector NetConnStatus is polled with each frame
        // (X360 Update: lis 0x6F6E / ori 0x6C6E -> 0x6F6E6C6E). A return of 1 means connected.
        const int KI_NETCONNSTATUS_ONLINE_SELECTOR = 0x6F6E6C6E;
        const int KI_NETCONNSTATUS_CONNECTED       = 1;

        // The byte offset of the embedded platform buddy manager within BrnNetworkManager. The X360
        // download glue reaches the CgsNetwork::BuddyManagerX360 sub-object as mpNetworkManager +
        // 0x1EFC0 (== 126912; asm: addis r3, r11, 2 / addi r3, r3, -0x1040). Modelled as a named
        // accessor; the storage lives in the full BrnNetworkManager TU.
        const s32 KI_NETWORK_MANAGER_BUDDY_MANAGER_OFFSET = 126912;

        // The decoded mug-shot geometry the picture textures are allocated at (X360
        // PrepareGamerPictureTextures: 160x120, pixel format 0x18280043; the compressed scratch is
        // 64x64, pixel format 0x28280086). The format immediates are renderengine::PixelFormat
        // codes passed straight through to NetworkTexture::Prepare.
        const s32 KI_PICTURE_WIDTH               = 160;
        const s32 KI_PICTURE_HEIGHT              = 120;
        const s32 KI_PICTURE_PIXEL_FORMAT        = 0x18280043;
        const s32 KI_COMPRESSED_WIDTH            = 64;
        const s32 KI_COMPRESSED_HEIGHT           = 64;
        const s32 KI_COMPRESSED_PIXEL_FORMAT     = 0x28280086;

        // The ConvertAndResize column range the downloaded picture is blitted across (X360
        // CopyTexture: r5 = 20, r6 = 140).
        const s32 KI_COPY_START_COLUMN = 20;
        const s32 KI_COPY_END_COLUMN   = 140;

        // The local controller index whose own gamer picture is downloaded on LIVE connect (X360
        // DownloadLocalGamerPicOnLiveConnect reads it whole off the manager as *(manager+0x60); the
        // signed-in local user's controller/pad index, exposed by
        // BrnNetworkManager::GetLocalUserControllerPort).

        // Convenience: reach the platform buddy manager that owns the picture-download pipeline.
        inline CgsNetwork::BuddyManagerX360* GetBuddyManager(BrnNetworkManager* lpNetworkManager)
        {
            return reinterpret_cast<CgsNetwork::BuddyManagerX360*>(
                reinterpret_cast<char*>(lpNetworkManager) + KI_NETWORK_MANAGER_BUDDY_MANAGER_OFFSET);
        }
    }

    // =======================================================================
    // Construct @ 0x825574B8
    // =======================================================================
    void GamerPictureManagerX360::Construct(BrnNetworkManager* lpNetworkManager,
                                            CgsNetwork::NetworkTextureDXTCompress* lpTextureCompressor)
    {
        CGS_ASSERT(lpNetworkManager, "lpNetworkManager");

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_GAMER_PICTURES; ++liPlayerIndex)
        {
            maGamerPictureData[liPlayerIndex].mTexture.Construct();
        }
        mLocalPlayerGamerPictureData.mTexture.Construct();
        mCompressedTexture.Construct();

        mpNetworkManager        = lpNetworkManager;
        mbWasConnectedLastFrame = false;
        mbPictureDownloaded     = false;
        mDownloadingXuid        = 0;

        CGS_ASSERT(lpTextureCompressor, "lpTextureCompressor");
        mpTextureCompressor = lpTextureCompressor;

        ResetGamerPictureData();
    }

    // =======================================================================
    // Destruct @ 0x82557600
    // =======================================================================
    void GamerPictureManagerX360::Destruct()
    {
        mpTextureCompressor     = NULL;
        mbWasConnectedLastFrame = false;
        mbPictureDownloaded     = false;
        mDownloadingXuid        = 0;

        ResetGamerPictureData();

        mpNetworkManager = NULL;
    }

    // =======================================================================
    // Prepare @ 0x82557578
    // =======================================================================
    bool GamerPictureManagerX360::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc)
    {
        mbPictureDownloaded = false;
        mDownloadingXuid    = 0;

        ResetGamerPictureData();
        PrepareGamerPictureTextures(lpHeapMalloc);
        return true;
    }

    // =======================================================================
    // PrepareGamerPictureTextures @ 0x8254C4A0
    // =======================================================================
    s32 GamerPictureManagerX360::PrepareGamerPictureTextures(CgsMemory::HeapMalloc* lpHeapMalloc)
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_GAMER_PICTURES; ++liPlayerIndex)
        {
            maGamerPictureData[liPlayerIndex].mTexture.Prepare(
                lpHeapMalloc, KI_PICTURE_WIDTH, KI_PICTURE_HEIGHT,
                static_cast<renderengine::PixelFormat>(KI_PICTURE_PIXEL_FORMAT));
        }
        mLocalPlayerGamerPictureData.mTexture.Prepare(
            lpHeapMalloc, KI_PICTURE_WIDTH, KI_PICTURE_HEIGHT,
            static_cast<renderengine::PixelFormat>(KI_PICTURE_PIXEL_FORMAT));
        return mCompressedTexture.Prepare(
            lpHeapMalloc, KI_COMPRESSED_WIDTH, KI_COMPRESSED_HEIGHT,
            static_cast<renderengine::PixelFormat>(KI_COMPRESSED_PIXEL_FORMAT));
    }

    // =======================================================================
    // Release @ 0x825575D0
    // =======================================================================
    bool GamerPictureManagerX360::Release()
    {
        mbPictureDownloaded = false;
        mDownloadingXuid    = 0;

        ResetGamerPictureData();
        return true;
    }

    // =======================================================================
    // ResetGamerPictureData @ 0x8254C528
    // =======================================================================
    s32 GamerPictureManagerX360::ResetGamerPictureData()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_GAMER_PICTURES; ++liPlayerIndex)
        {
            maGamerPictureData[liPlayerIndex].miPlayerID = KI_INVALID_PLAYER_ID;
            maGamerPictureData[liPlayerIndex].mbReady    = false;
            maGamerPictureData[liPlayerIndex].mXuid      = 0;
            maGamerPictureData[liPlayerIndex].mTexture.Release();
        }

        mLocalPlayerGamerPictureData.miPlayerID = KI_INVALID_PLAYER_ID;
        mLocalPlayerGamerPictureData.mbReady    = false;
        mLocalPlayerGamerPictureData.mXuid      = 0;
        mLocalPlayerGamerPictureData.mTexture.Release();

        return mCompressedTexture.Release();
    }

    // =======================================================================
    // Get @ 0x8254C2A8
    //
    // Find the slot whose miPlayerID matches liPlayerID. The local slot is preferred when it owns
    // the requested id; otherwise the remote table is scanned. Returns null if no slot matches.
    // =======================================================================
    GamerPictureManagerX360::GamerPictureData* GamerPictureManagerX360::Get(s32 liPlayerID)
    {
        if (liPlayerID != KI_INVALID_PLAYER_ID
            && mLocalPlayerGamerPictureData.miPlayerID == liPlayerID)
        {
            return &mLocalPlayerGamerPictureData;
        }

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_GAMER_PICTURES; ++liPlayerIndex)
        {
            if (maGamerPictureData[liPlayerIndex].miPlayerID == liPlayerID)
            {
                return &maGamerPictureData[liPlayerIndex];
            }
        }
        return NULL;
    }

    // =======================================================================
    // GetGamerPictureTexture @ 0x8254C370
    //
    // Return the decoded gamer-picture texture for a player (the local player when
    // liPlayerID == -1), or null if the slot is missing / its picture is not yet ready.
    // =======================================================================
    CgsNetwork::NetworkTexture* GamerPictureManagerX360::GetGamerPictureTexture(s32 liPlayerID)
    {
        GamerPictureData* lpData;
        if (liPlayerID == KI_INVALID_PLAYER_ID)
        {
            lpData = &mLocalPlayerGamerPictureData;
        }
        else
        {
            lpData = Get(liPlayerID);
        }

        if (!lpData)
        {
            return NULL;
        }
        if (!lpData->mbReady)
        {
            return NULL;
        }
        return &lpData->mTexture;
    }

    // =======================================================================
    // CopyTexture @ 0x8254C238
    //
    // Convert + resize the downloaded compressed picture into the owning player's network texture.
    // =======================================================================
    s32 GamerPictureManagerX360::CopyTexture(CgsNetwork::NetworkTexture* lpTexture)
    {
        CGS_ASSERT(lpTexture, "lpTexture");

        CgsNetwork::NetworkImageConverter lConverter;
        lConverter.ConvertAndResize(&mCompressedTexture, lpTexture,
                                    KI_COPY_START_COLUMN, KI_COPY_END_COLUMN);
        return 0;
    }

    // =======================================================================
    // DownloadNextGamerPicture @ 0x8254C590
    //
    // Pick the next XUID to download (the local player's, else the first not-ready remote slot) and
    // -- if one was found -- kick off its download through the platform buddy manager.
    // =======================================================================
    GamerPictureManagerX360* GamerPictureManagerX360::DownloadNextGamerPicture()
    {
        mbPictureDownloaded = false;
        mDownloadingXuid    = 0;

        if (mLocalPlayerGamerPictureData.miPlayerID != KI_INVALID_PLAYER_ID
            && !mLocalPlayerGamerPictureData.mbReady)
        {
            mDownloadingXuid = mLocalPlayerGamerPictureData.mXuid;
        }
        else
        {
            for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_GAMER_PICTURES; ++liPlayerIndex)
            {
                if (maGamerPictureData[liPlayerIndex].miPlayerID != KI_INVALID_PLAYER_ID
                    && !maGamerPictureData[liPlayerIndex].mbReady)
                {
                    mDownloadingXuid = maGamerPictureData[liPlayerIndex].mXuid;
                    break;
                }
            }
        }

        if (mDownloadingXuid != 0)
        {
            GetBuddyManager(mpNetworkManager)->StartDownloadingPictures(
                &mCompressedTexture, 1, &mDownloadingXuid,
                reinterpret_cast<void*>(&mbPictureDownloaded));
        }
        return this;
    }

    // =======================================================================
    // DownloadLocalGamerPicOnLiveConnect @ 0x8254C638
    //
    // On LIVE connect, resolve the local console XUID and -- if not already downloading -- start
    // downloading the local player's own gamer picture.
    // =======================================================================
    u32 GamerPictureManagerX360::DownloadLocalGamerPicOnLiveConnect()
    {
        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");

        u64 lLocalXuid = 0;
        u32 luResult = XUserGetXUID(mpNetworkManager->GetLocalUserControllerPort(), &lLocalXuid);
        if (luResult)
        {
            return luResult;
        }

        mLocalPlayerGamerPictureData.mXuid = lLocalXuid;
        if (mDownloadingXuid != 0)
        {
            return luResult;
        }

        mDownloadingXuid = lLocalXuid;
        GetBuddyManager(mpNetworkManager)->StartDownloadingPictures(
            &mCompressedTexture, 1, &mDownloadingXuid,
            reinterpret_cast<void*>(&mbPictureDownloaded));
        return luResult;
    }

    // =======================================================================
    // GetCompressedGamerPictureTexture @ 0x8254C3C0
    //
    // Queue the player's ready gamer picture for DXT re-compression for transmission.
    // =======================================================================
    s32 GamerPictureManagerX360::GetCompressedGamerPictureTexture(
        s32 liPlayerID,
        CgsNetwork::NetworkTexture* lpCompressedTexture,
        CgsNetwork::CompressCallback lCompleteCallback,
        void* lpCompleteData)
    {
        GamerPictureData* lpData = Get(liPlayerID);
        if (!lpData || !lpData->mbReady)
        {
            return 0;
        }

        CgsNetwork::NetworkTexture* lpSource = &lpData->mTexture;
        CGS_ASSERT(mpTextureCompressor, "mpTextureCompressor");

        const s32 liWidth  = lpSource->GetWidth();
        const s32 liHeight = lpSource->GetHeight();
        const s32 liSourceFormat = static_cast<s32>(lpSource->GetFormat());
        const s32 liSourceSize =
            (lpSource->GetBitsPerPixel() * liWidth * liHeight + 7) / 8;

        const s32 liCmpPitch = lpCompressedTexture->GetStride();
        const s32 liSrcPitch = lpSource->GetStride();

        mpTextureCompressor->SetNewTextureToCompress(
            lpSource->GetTexture(),
            liSourceSize,
            liWidth,
            liHeight,
            liSrcPitch,
            liCmpPitch,
            0,                  // liQuality (X360 `li r10, 0`)
            liSourceFormat,
            0,                  // lbInputIsUncompressedYUYV (X360 `stb 0`)
            lCompleteCallback,
            lpCompleteData);
        return 0;
    }

    // =======================================================================
    // RemovePlayer @ 0x82557770
    //
    // Free a player's slot; if it was the one currently downloading, cancel + advance the queue.
    // =======================================================================
    GamerPictureManagerX360::GamerPictureData* GamerPictureManagerX360::RemovePlayer(s32 liPlayerID)
    {
        GamerPictureData* lpData = Get(liPlayerID);
        if (lpData)
        {
            lpData->miPlayerID = KI_INVALID_PLAYER_ID;

            // If we were downloading this freed slot's picture (its XUID == the in-flight XUID) and
            // it is not the local player's own (still-connected) download, cancel + advance.
            if (mDownloadingXuid == lpData->mXuid
                && (mDownloadingXuid != mLocalPlayerGamerPictureData.mXuid
                    || !mbWasConnectedLastFrame))
            {
                GetBuddyManager(mpNetworkManager)->CancelPictureDownload();
                // X360 returns DownloadNextGamerPicture()'s `this` here; the caller ignores the
                // return, so we advance the queue and return the freed slot (semantic parity,
                // no manager-pointer-as-slot type pun).
                DownloadNextGamerPicture();
            }
        }
        return lpData;
    }

    // =======================================================================
    // Disconnected @ 0x825577F0
    //
    // A network disconnect: cancel any in-flight download whose target slot is about to be freed,
    // then free every remote slot and the local slot (advancing the download whenever the freed
    // slot was the one in flight).
    // =======================================================================
    void GamerPictureManagerX360::Disconnected()
    {
        if (mDownloadingXuid != 0
            && (mDownloadingXuid != mLocalPlayerGamerPictureData.mXuid
                || !mbWasConnectedLastFrame))
        {
            GetBuddyManager(mpNetworkManager)->CancelPictureDownload();
        }

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_GAMER_PICTURES; ++liPlayerIndex)
        {
            const s32 liPlayerID = maGamerPictureData[liPlayerIndex].miPlayerID;
            if (liPlayerID != KI_INVALID_PLAYER_ID)
            {
                // Locate the slot owning this id (the local slot if it owns it, else this slot).
                GamerPictureData* lpData;
                if (mLocalPlayerGamerPictureData.miPlayerID == liPlayerID)
                {
                    lpData = &mLocalPlayerGamerPictureData;
                }
                else
                {
                    lpData = &maGamerPictureData[liPlayerIndex];
                }

                if (lpData)
                {
                    lpData->miPlayerID = KI_INVALID_PLAYER_ID;
                    if (mDownloadingXuid == lpData->mXuid
                        && (mDownloadingXuid != mLocalPlayerGamerPictureData.mXuid
                            || !mbWasConnectedLastFrame))
                    {
                        GetBuddyManager(mpNetworkManager)->CancelPictureDownload();
                        DownloadNextGamerPicture();
                    }
                }

                CGS_ASSERT(maGamerPictureData[liPlayerIndex].miPlayerID == KI_INVALID_PLAYER_ID,
                           "maGamerPictureData[liPlayerIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID");
            }
        }

        // Free the local slot too.
        {
            GamerPictureData* lpData;
            if (mLocalPlayerGamerPictureData.miPlayerID == KI_INVALID_PLAYER_ID)
            {
                lpData = Get(KI_INVALID_PLAYER_ID);
            }
            else
            {
                lpData = &mLocalPlayerGamerPictureData;
            }

            if (lpData)
            {
                lpData->miPlayerID = KI_INVALID_PLAYER_ID;
                if (mDownloadingXuid == lpData->mXuid
                    && (mDownloadingXuid != mLocalPlayerGamerPictureData.mXuid
                        || !mbWasConnectedLastFrame))
                {
                    GetBuddyManager(mpNetworkManager)->CancelPictureDownload();
                    DownloadNextGamerPicture();
                }
            }
        }

        CGS_ASSERT(mLocalPlayerGamerPictureData.miPlayerID == KI_INVALID_PLAYER_ID,
                   "mLocalPlayerGamerPictureData.mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID");
    }

    // =======================================================================
    // Update @ 0x82557650
    //
    // Per-frame tick: poll the LIVE connection, kick off the local picture download on connect,
    // cancel an in-flight download on disconnect, and -- when a download has completed -- copy the
    // result into the owning texture and advance the queue.
    // =======================================================================
    u32 GamerPictureManagerX360::Update()
    {
        u32 luResult = static_cast<u32>(
            NetConnStatus(KI_NETCONNSTATUS_ONLINE_SELECTOR, 0, NULL, 0));
        const bool lbConnected = (luResult == KI_NETCONNSTATUS_CONNECTED);

        if (mDownloadingXuid == 0 && !mLocalPlayerGamerPictureData.mbReady)
        {
            luResult = DownloadLocalGamerPicOnLiveConnect();
        }

        if (!lbConnected)
        {
            if (mbWasConnectedLastFrame)
            {
                // X360 stores CancelPictureDownload's return into the result register; the homed
                // CancelPictureDownload is void, so luResult keeps its prior value (semantic parity).
                GetBuddyManager(mpNetworkManager)->CancelPictureDownload();
            }
        }

        const bool lbPictureDownloaded = mbPictureDownloaded;
        mbWasConnectedLastFrame = lbConnected;

        if (lbPictureDownloaded)
        {
            CgsNetwork::NetworkTexture* lpDestination;
            if (mLocalPlayerGamerPictureData.mXuid == mDownloadingXuid)
            {
                lpDestination = &mLocalPlayerGamerPictureData.mTexture;
                mLocalPlayerGamerPictureData.mbReady = true;
            }
            else
            {
                s32 liPlayerIndex = 0;
                for (; liPlayerIndex < KI_MAX_GAMER_PICTURES; ++liPlayerIndex)
                {
                    if (maGamerPictureData[liPlayerIndex].mXuid == mDownloadingXuid)
                    {
                        break;
                    }
                }
                if (liPlayerIndex >= KI_MAX_GAMER_PICTURES)
                {
                    // No slot owns the downloaded XUID any more: just advance the queue. The X360
                    // returns `this` as a DWORD here; the caller ignores it -- return the pump
                    // result (semantic parity, no pointer truncation on the 64-bit host).
                    DownloadNextGamerPicture();
                    return luResult;
                }
                lpDestination = &maGamerPictureData[liPlayerIndex].mTexture;
                maGamerPictureData[liPlayerIndex].mbReady = true;
            }

            CopyTexture(lpDestination);
            DownloadNextGamerPicture();
            return luResult;
        }
        return luResult;
    }

    // =======================================================================
    // AddPlayer @ 0x825616B8 -- DECLARATION-ONLY (deferred, un-homed deps).
    //
    // The faithful spine (Get gate, GetPlayerByID local/remote split, free-slot scan, id/XUID
    // store, remote-only not-ready clear, DownloadNext kick when mDownloadingXuid==0) is fully
    // recovered, but its XUID-resolution fork dead-ends on TUs that are not yet homed:
    //   * the in-game branch needs class:CgsNetwork::ServerInterfaceGamesX360 (`todo`) for the
    //     in-game predicate (sub_82878620) + GetPlayerXUIDByID, and
    //   * the local branch builds a BrnNetwork::PlayerInfoData on the stack (vtable off_820821EC +
    //     a XUID member past the homed ServerInterfacePlayerInfoDataBase +0xF0 tail) whose derived
    //     layout / Prepare / XUID accessor are not in any homed type.
    // Rather than route the XUID sources through fabricated placeholders, AddPlayer is left
    // declaration-only (header decl present) until ServerInterfaceGamesX360 + the derived
    // PlayerInfoData land. This TU is therefore a 15/16 partial -- BLOCKED, not done.
    // =======================================================================

    // =======================================================================
    // _AssertLayout -- never called; pins the recovered per-entry + tail member RELATIONSHIPS.
    //
    // The X360 absolute offsets (the GamerPictureData stride 0x30 with miPlayerID @ +0x00,
    // mXuid @ +0x08, mTexture @ +0x10, mbReady @ +0x2C; the local entry @ +0x150; the compressed
    // texture @ +0x180; the manager/compressor/latch tail) assume the 32-bit Xenon NetworkTexture
    // (28 bytes) + 4-byte pointers; the PC gate is 64-bit, so the raw byte offsets are NOT
    // reproduced. We pin the ABI-stable relationships, mirroring BuddyManagerX360::_AssertLayout.
    // =======================================================================
    void GamerPictureManagerX360::_AssertLayout()
    {
        // The picture table is a contiguous array of GamerPictureData entries; the per-entry field
        // order is miPlayerID -> mXuid -> mTexture -> mbReady (every one an asm store/load offset).
        static_assert(offsetof(GamerPictureData, mXuid) > offsetof(GamerPictureData, miPlayerID),
                      "miPlayerID precedes mXuid");
        static_assert(offsetof(GamerPictureData, mTexture) > offsetof(GamerPictureData, mXuid),
                      "mXuid precedes mTexture");
        static_assert(offsetof(GamerPictureData, mbReady) > offsetof(GamerPictureData, mTexture),
                      "mTexture precedes mbReady");

        // The local entry follows the 7-entry remote table; the compressed scratch texture follows
        // the local entry; the deps/latch tail follows the compressed texture.
        static_assert(offsetof(GamerPictureManagerX360, mLocalPlayerGamerPictureData)
                      == offsetof(GamerPictureManagerX360, maGamerPictureData)
                         + KI_MAX_GAMER_PICTURES * sizeof(GamerPictureData),
                      "mLocalPlayerGamerPictureData follows the 7-entry remote table");
        static_assert(offsetof(GamerPictureManagerX360, mpNetworkManager)
                      > offsetof(GamerPictureManagerX360, mCompressedTexture),
                      "mCompressedTexture precedes the deps tail");
        static_assert(offsetof(GamerPictureManagerX360, mbWasConnectedLastFrame)
                      > offsetof(GamerPictureManagerX360, mbPictureDownloaded),
                      "mbPictureDownloaded precedes mbWasConnectedLastFrame");
    }
}
