#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Network/Buddies/DirtySock/CgsBuddyManagerDirtySock.h"

// =============================================================================
// CgsNetwork::BuddyManagerX360 -- the Xbox 360 platform buddy-list manager.
//
// Derives from CgsNetwork::BuddyManagerBase (the DirtySock/HLB wrapper). On the
// 360 most buddy actions are routed through the Xbox Guide (XShowMessagesUI /
// XShowGamerCardUI / XInviteSend) rather than the HLB API, and the buddy "list"
// is the Xbox friends list. The class additionally drives an asynchronous
// gamer-picture download pipeline (XUserReadProfileSettingsByXuid +
// XUserReadGamerPictureByKey, polled through CgsXOverlapped).
//
// Reconstructed from the X360 asm spine (Construct @0x8287F858 etc.). The big
// member offsets in the asm (0x1C..0x1391C) are pinned below by name; the
// gamer-picture working set and the invite buffer sit in a derived sub-block
// past the base. _AssertLayout() (never called) static_asserts the recovered
// offsets so the gate keeps them honest.
//
// Original home (from the asm assert strings):
//   ..\GameShared\GameClasses\Network\Buddies\DirtySock\X360\CgsBuddyManagerDirtySockX360.cpp
// =============================================================================

namespace CgsNetwork
{
    class NetworkTexture;   // Network/Texture/CgsNetworkTexture.h (held by pointer only)

    // Maximum number of pending Live invites buffered before the async XInviteSend
    // drains them one per Update. Grounded: SendInvite asserts the count stays
    // < this value (cmpwi 5 @0x8287FE94) and the buffer is indexed [0,5).
    const s32 KI_MAX_INVITES_TO_BUFFER = 5;

    // Max profile settings fetched per XUserReadProfileSettingsByXuid batch.
    // Grounded: RequestNextProfilePictures clamps the request span to 15
    // (cmplwi 0xF / li 0xF @0x8288CC60).
    const s32 KI_MAX_PROFILES_PER_REQUEST = 15;

    class BuddyManagerX360 : public BuddyManagerBase
    {
    public:
        // ---- lifecycle (non-virtual, called by BrnNetwork::BuddyManagerBase) ---
        void Construct(NetworkManager* lpNetworkManager, ServerInterface* lpServerInterface);
        void Destruct();
        bool Prepare();
        bool Release();

        // ---- BuddyManagerBase overrides --------------------------------------
        virtual void Update(bool lbCanBlock);
        virtual void JoinBuddy(const PlayerName* lpPlayerName);
        virtual void SendInvite(const PlayerName* lpPlayerName);
        virtual void CancelInvites();
        virtual void AcceptInvite(const PlayerName* lpPlayerName);
        virtual bool AreAnyInvitesOpen();
        virtual bool GetNextUnreadMessage(const PlayerName* lpPlayerName, char* lpcOut, s32 liMaxLength);
        virtual bool GetMessage(const PlayerName* lpPlayerName, s32 liIndex, char* lpcOut, s32 liMaxLength);
        virtual const char* GetTitle(const PlayerName* lpPlayerName);
        virtual bool HaveIInvitedMyBuddy(const PlayerName* lpPlayerName);

        // ---- Xbox-specific buddy actions -------------------------------------
        u64   GetBuddyXUID(const PlayerName* lpBuddyName);
        u32   ShowProfile(const PlayerName* lpBuddyName);

        // ---- async gamer-picture download pipeline ---------------------------
        void StartDownloadingPictures(NetworkTexture* lpaTextures, s32 liNumberOfPictures,
                                      u64* lpaXuids, void* lpaGamerPicKeys);
        void RequestNextProfilePictures();
        void DownloadNextPicture(bool lbContinueCurrent);
        void CancelPictureDownload();

    protected:
        virtual bool IsConnectedToNetworkService() const;

    private:
        // The picture-download working set keys off the same controller/user index
        // the network manager exposes at its +0x60 slot; reached through the base
        // mpNetworkManager accessor (the field has no shared header here).

        // -- gamer-picture download source pointers (set by StartDownloadingPictures) --
        NetworkTexture* mpDownloadTextures;     // +0x1C  destination texture array (stride 28)
        void*           mpGamerPicKeys;         // +0x20  caller a5 picture-key blob (STORED-but-UNUSED here)
        u64*            mpDownloadXuids;         // +0x24  source XUID array (8B/entry)

        // Embedded XUSER_READ_PROFILE_SETTING_RESULTS buffer handed to
        // XUserReadProfileSettingsByXuid as pResults (asm: base + 0x28). The XDK
        // fills its second word (+0x2C) with pSettings -- a pointer to the array of
        // 40-byte XUSER_PROFILE_SETTING records that carry the downloaded gamercard
        // picture keys. The buffer's leading word (+0x28, dwSettingsLen) and the
        // tail past pSettings are opaque; reserved around the named pointer.
        u32             muProfileResultsSettingsLen;                   // +0x28  dwSettingsLen
        void*           mpProfileSettings;                             // +0x2C  pSettings (XDK-filled; 40B/entry; +0x18 = key)
        u8              maProfileReadResults[80040 - 0x30];            // +0x30

        // -- gamer-picture download progress (byte offsets pinned by the asm) --
        s32             miNumberOfPicturesToDownload;                  // +0x138A8 (80040)
        s32             miNextProfileToRequest;                        // +0x138AC (80044)
        s32             miFirstProfileInThisRequest;                   // +0x138B0 (80048)
        s32             miCurrentProfile;                              // +0x138B4 (80052)

        // -- async-I/O overlapped wrappers (28 bytes each) --
        u8              mInviteOverlapped[28];                         // +0x138B8 (80056)
        u8              mGamerPicOverlapped[28];                       // +0x138D4 (80084)

        // -- pending-invite buffer (XUIDs queued for XInviteSend) --
        u64             maInvitesToBeSent[KI_MAX_INVITES_TO_BUFFER];   // +0x138F0 (80112)
        s32             miNumberOfInvitesToBeSent;                     // +0x13918 (80152)
        bool            mbSendingInvite;                               // +0x1391C (80156)

        static void _AssertLayout();
    };
}
