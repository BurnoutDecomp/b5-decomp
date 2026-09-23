#include "GameShared/GameClasses/Network/Buddies/DirtySock/X360/CgsBuddyManagerDirtySockX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/CgsNetworkManager.h"   // NetworkManager::GetActiveControllerPort
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"   // GetGameComponent / GetConnAPIRef
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"
#include "GameSource/GameState/BrnCgsPlayerName.h"

#include <cstddef>   // offsetof

// =============================================================================
// CgsNetwork::BuddyManagerX360 -- Xbox 360 buddy-list manager implementation.
//
// Reconstructed from the X360 asm spine. The DirtySock base supplies the HLB
// list/query plumbing; this derived class layers the Xbox Guide UI calls
// (XShowMessagesUI / XShowGamerCardUI / XInviteSend) and the asynchronous
// gamer-picture download pipeline on top.
// =============================================================================

// ---- XDK / Xbox-LIVE entry points -------------------------------------------
// Real prototypes live in the Xbox 360 XDK (<xapi.h>/<xboxmath.h>/<xonline.h>);
// declared here as extern "C" free functions, mirroring the System/X360 glue
// precedent (CgsXOverlappedX360.cpp / CgsGuiKeyboard.cpp). The argument shapes
// are taken from the X360 call sites (register usage), not from the PPC Hex-Rays.
extern "C"
{
    // XGetOverlappedResult(pOverlapped, pdwResult, bWait); 0 == success/complete.
    u32 XGetOverlappedResult(void* lpOverlapped, u32* lpdwResult, u32 bWait);
    u32 XCancelOverlapped(void* lpOverlapped);

    // Guide UI -- all keyed by the signed-in controller/user index.
    u32 XShowMessagesUI(u32 luUserIndex);
    u32 XShowGamerCardUI(u32 luUserIndex, u64 luXuid);

    // Sign-in state: 2 == XUserSigninState_SignedInToLive.
    s32 XUserGetSigninState(u32 luUserIndex);

    // Live invite: (user, xuidCount, pXuids, pszText, pOverlapped). 997 == pending.
    u32 XInviteSend(u32 luUserIndex, u32 luXuidCount, const u64* lpaXuids,
                    const wchar_t* lpwcText, void* lpOverlapped);

    // Profile-settings + gamer-picture async reads (997 == ERROR_IO_PENDING).
    u32 XUserReadProfileSettingsByXuid(u32 luTitleId, u32 luUserIndexRequester,
                                       u32 luNumXuids, const u64* lpaXuids,
                                       u32 luNumSettingIds, const u32* lpaSettingIds,
                                       u32* lpcbResults, void* lpResults, void* lpOverlapped);
    u32 XUserReadGamerPictureByKey(const void* lpPictureKey, s32 lbSmall,
                                   u8* lpTextureBuffer, u32 luPitch, u32 luHeight,
                                   void* lpOverlapped);
}

// ConnApiStatus 'sess' copies the game session text into the buffer: empty until the ConnApi
// session is up.
#include "connapi.h"

// Win32/XDK winerror.h codes consumed by the polling logic.
#ifndef ERROR_IO_INCOMPLETE
#define ERROR_IO_INCOMPLETE 996u
#endif
#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING    997u
#endif

// ConnApiStatus 'sess' selector and the session-text buffer Update hands it.
static const s32 KI_CONNAPI_STATUS_SESSION    = ('s' << 24) | ('e' << 16) | ('s' << 8) | 's';
static const s32 KI_CONNAPI_SESSION_TEXT_SIZE = 128;

// XPROFILE_GAMERCARD_PICTURE_KEY -- the single profile setting id requested per
// buddy (dwNumSettingIds = 1; the value is read from the image's rodata).
static const u32 KU_PROFILE_GAMERCARD_PICTURE_KEY = 0x4064000Fu;

namespace CgsSystem
{
    // The 28-byte XOVERLAPPED wrapper. Construct() / GetResultString() are defined
    // out-of-line in System/X360/CgsXOverlappedX360.cpp; re-declared here (matching
    // that TU's local declaration, exactly as CgsGuiKeyboard.cpp does) only so this
    // TU can name them. The definitions are resolved at link time.
    class CgsXOverlapped
    {
    public:
        void Construct();
        static const char* GetResultString(void* lpOverlapped);
    private:
        u8 maOverlapped[28];
    };
}

namespace CgsNetwork
{
    // =========================================================================
    // Construct @ 0x8287F858
    //
    // Zero the invite count/flag, construct both overlapped wrappers, then chain
    // to the DirtySock base constructor.
    // =========================================================================
    void BuddyManagerX360::Construct(NetworkManager* lpNetworkManager,
                                     ServerInterface* lpServerInterface)
    {
        miNumberOfInvitesToBeSent = 0;
        reinterpret_cast<CgsSystem::CgsXOverlapped*>(mInviteOverlapped)->Construct();
        reinterpret_cast<CgsSystem::CgsXOverlapped*>(mGamerPicOverlapped)->Construct();
        mbSendingInvite = false;

        BuddyManagerBase::Construct(lpNetworkManager, lpServerInterface);
    }

    // =========================================================================
    // Destruct @ 0x8287F948
    //
    // Mirror of Construct's reset (no base Destruct call in the asm).
    // =========================================================================
    void BuddyManagerX360::Destruct()
    {
        miNumberOfInvitesToBeSent = 0;
        reinterpret_cast<CgsSystem::CgsXOverlapped*>(mInviteOverlapped)->Construct();
        reinterpret_cast<CgsSystem::CgsXOverlapped*>(mGamerPicOverlapped)->Construct();
        mbSendingInvite = false;
    }

    // =========================================================================
    // Prepare @ 0x8287F8C0
    //
    // Reset the manager to a clean idle state: clear the invite queue, then the
    // inlined base Prepare (status, buddies-changed callback, buddy API).
    // =========================================================================
    bool BuddyManagerX360::Prepare()
    {
        miNumberOfInvitesToBeSent = 0;
        mbSendingInvite           = false;
        return BuddyManagerBase::Prepare();
    }

    // =========================================================================
    // Release @ 0x8287F8F8
    //
    // Clear the invite state and forward to the base Release (vtable slot +0x04).
    // =========================================================================
    bool BuddyManagerX360::Release()
    {
        miNumberOfInvitesToBeSent = 0;
        mbSendingInvite           = false;
        BuddyManagerBase::Release();
        return true;
    }

    // =========================================================================
    // IsConnectedToNetworkService @ 0x8287FF30 (virtual)
    //
    // Connected == the controller is signed in to LIVE (signin state == 2).
    // =========================================================================
    bool BuddyManagerX360::IsConnectedToNetworkService() const
    {
        const u32 luUserIndex = mpNetworkManager->GetActiveControllerPort();
        return XUserGetSigninState(luUserIndex) == 2;
    }

    // =========================================================================
    // AcceptInvite @ 0x8287FF68 (virtual)
    //
    // On the 360 there is no in-game accept; pop the Xbox Guide messages blade.
    // The PlayerName arg is unused (Hex-Rays dropped it -- the asm reads only the
    // user index out of mpNetworkManager+0x60).
    // =========================================================================
    void BuddyManagerX360::AcceptInvite(const PlayerName* /*lpPlayerName*/)
    {
        const u32 luUserIndex = mpNetworkManager->GetActiveControllerPort();
        XShowMessagesUI(luUserIndex);
    }

    // Declining is handled in the Guide messages blade as well.
    void BuddyManagerX360::DeclineInvite(const PlayerName* /*lpPlayerName*/)
    {
        const u32 luUserIndex = mpNetworkManager->GetActiveControllerPort();
        XShowMessagesUI(luUserIndex);
    }

    // =========================================================================
    // ShowProfile @ 0x82880018
    //
    // Pop the Xbox Guide gamercard for the named buddy.
    // =========================================================================
    u32 BuddyManagerX360::ShowProfile(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(lpBuddyName && lpBuddyName->GetPlayerName()[0] != '\0',
                   "!lpBuddyName->IsEmpty()");

        const u32 luUserIndex = mpNetworkManager->GetActiveControllerPort();

        // The asm takes the high 32 bits of the XUID (>> 32) for XShowGamerCardUI's
        // second argument; the XDK packs the gamertag id in the high dword.
        const u64 luXuid = GetBuddyXUID(lpBuddyName);
        return XShowGamerCardUI(luUserIndex, luXuid >> 32);
    }

    // =========================================================================
    // JoinBuddy @ 0x8288CE30 (virtual)
    //
    // Tail-calls ShowProfile -- joining a buddy on the 360 just shows their card.
    // =========================================================================
    void BuddyManagerX360::JoinBuddy(const PlayerName* lpPlayerName)
    {
        ShowProfile(lpPlayerName);
    }

    // Revoking an invite on the 360 also just shows the buddy's gamercard.
    void BuddyManagerX360::RevokeInvite(const PlayerName* lpPlayerName)
    {
        ShowProfile(lpPlayerName);
    }

    // =========================================================================
    // GetBuddyXUID @ 0x8287F9B0
    //
    // Look the buddy up in the HLB list by name and return its platform user id.
    // =========================================================================
    u64 BuddyManagerX360::GetBuddyXUID(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(mpBuddies, "mpBuddies");
        CGS_ASSERT(lpBuddyName, "lpBuddyName");

        DirtySock::HLBBudT* lpBuddy =
            DirtySock::HLBListGetBuddyByName(mpBuddies, lpBuddyName->GetPlayerName());
        CGS_ASSERT(lpBuddy, "lpBuddy");

        return DirtySock::HLBBudGetXenonXUID(lpBuddy);
    }

    // =========================================================================
    // HasBuddyInvitedMe (virtual)
    //
    // The buddy's "has invited me" flag: bit 27 of its game-invite flag word.
    // =========================================================================
    bool BuddyManagerX360::HasBuddyInvitedMe(const PlayerName* lpPlayerName)
    {
        CGS_ASSERT(lpPlayerName, "lpPlayerName");

        DirtySock::HLBBudT* lpBuddy =
            DirtySock::HLBListGetBuddyByName(mpBuddies, lpPlayerName->GetPlayerName());
        CGS_ASSERT(lpBuddy, "lpBuddy");

        return (lpBuddy->uFlags & HLB_BUDFLAG_INVITE_RECV) != 0;
    }

    // =========================================================================
    // HaveIInvitedMyBuddy @ 0x8287FD60 (virtual)
    //
    // Read the "invited" flag out of the buddy record: bit 26 of the dword at
    // buddy+0x18 (asm: (*(lpBuddy+24) >> 26) & 1).
    // =========================================================================
    bool BuddyManagerX360::HaveIInvitedMyBuddy(const PlayerName* lpPlayerName)
    {
        CGS_ASSERT(lpPlayerName, "lpPlayerName");

        DirtySock::HLBBudT* lpBuddy =
            DirtySock::HLBListGetBuddyByName(mpBuddies, lpPlayerName->GetPlayerName());
        CGS_ASSERT(lpBuddy, "lpBuddy");

        // The "I have invited this buddy" flag is bit 26 of the buddy record's flag
        // word (+0x18).
        return (lpBuddy->uFlags & HLB_BUDFLAG_INVITE_SENT) != 0;
    }

    // =========================================================================
    // AreAnyInvitesOpen @ 0x8287FF78 (virtual)
    //
    // Walk the buddy list; return true on the first buddy whose "invited" flag
    // (bit 26 of the status word at +0x18) is set.
    // =========================================================================
    bool BuddyManagerX360::AreAnyInvitesOpen()
    {
        for (s32 liIndex = 0; ; ++liIndex)
        {
            const s32 liBuddyCount = mpBuddies ? DirtySock::HLBListGetBuddyCount(mpBuddies) : 0;
            if (liIndex >= liBuddyCount)
            {
                break;
            }

            DirtySock::HLBBudT* lpBuddy = DirtySock::HLBListGetBuddyByIndex(mpBuddies, liIndex);
            CGS_ASSERT(lpBuddy, "lpBuddy");

            if ((lpBuddy->uFlags & HLB_BUDFLAG_INVITE_SENT) != 0)
            {
                return true;
            }
        }
        return false;
    }

    // =========================================================================
    // CancelInvites @ 0x8287FF18 (virtual)
    //
    // Drop the pending-invite queue.
    // =========================================================================
    void BuddyManagerX360::CancelInvites()
    {
        miNumberOfInvitesToBeSent = 0;
    }

    // =========================================================================
    // SendInvite @ 0x8287FDE8 (virtual)
    //
    // If we have not already received an invite from this buddy (HasBuddyInvitedMe
    // vtable slot +0x58 returns false), look up their XUID and append it to the
    // pending-invite buffer (deduping against entries already queued). The async
    // XInviteSend that actually drains the queue runs in Update.
    // =========================================================================
    void BuddyManagerX360::SendInvite(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(lpBuddyName && lpBuddyName->GetPlayerName()[0] != '\0',
                   "!lpBuddyName->IsEmpty()");

        if (HasBuddyInvitedMe(lpBuddyName))
        {
            return;
        }

        const u64 luXuid = GetBuddyXUID(lpBuddyName);

        CGS_ASSERT(miNumberOfInvitesToBeSent >= 0, "miNumberOfInvitesToBeSent >= 0");
        CGS_ASSERT(miNumberOfInvitesToBeSent < KI_MAX_INVITES_TO_BUFFER,
                   "miNumberOfInvitesToBeSent < KI_MAX_INVITES_TO_BUFFER");

        // Dedupe: skip if the XUID is already queued.
        for (s32 liIndex = 0; liIndex < miNumberOfInvitesToBeSent; ++liIndex)
        {
            if (maInvitesToBeSent[liIndex] == luXuid)
            {
                return;
            }
        }

        if (miNumberOfInvitesToBeSent < KI_MAX_INVITES_TO_BUFFER)
        {
            maInvitesToBeSent[miNumberOfInvitesToBeSent] = luXuid;
            ++miNumberOfInvitesToBeSent;
        }
    }

    // =========================================================================
    // Update @ 0x8288C960 (virtual)
    //
    // Pump the base, run the platform pre-pump (vtable slot +0x7C), then service
    // the pending-invite queue: once the connection is up and no overlapped op is
    // in flight, fire the next buffered XInviteSend asynchronously.
    // =========================================================================
    void BuddyManagerX360::Update(bool lbCanBlock)
    {
        BuddyManagerBase::Update(lbCanBlock);

        UpdatePictureDownload();

        CGS_ASSERT(mpServerInterface, "mpServerInterface");
        CGS_ASSERT(mpServerInterface->GetGameComponent(), "mpServerInterface->GetGameComponent()");

        // Is an overlapped invite op still pending? (997 == pending, 996 == incomplete)
        const u32 luInviteResult = XGetOverlappedResult(mInviteOverlapped, 0, 0);
        const bool lbInviteInFlight =
            (luInviteResult == ERROR_IO_PENDING) || (luInviteResult == ERROR_IO_INCOMPLETE);

        // An invite that was in flight has now finished: validate it and clear the flag.
        // On failure the asm (:161) builds the message "XOverlapped operation failed: "
        // followed by mInviteOverlapped.GetResultString() via a StrStream before firing.
        if (mbSendingInvite && !lbInviteInFlight)
        {
            CGS_ASSERT(XGetOverlappedResult(mInviteOverlapped, 0, 0) == 0,
                       "XOverlapped operation failed: %s "
                       "(mInviteOverlapped.GetResultString())");
            mbSendingInvite = false;
        }

        // Invites go out only while a game session is up: the ConnApi reports a non-empty
        // 'sess' text. The ref is the one the games component's server-interface pointer
        // (+0x86C) leads to, i.e. this manager's own server interface.
        char lacSession[KI_CONNAPI_SESSION_TEXT_SIZE];
        lacSession[0] = 0;
        const bool lbInSession =
            (ConnApiStatus(mpServerInterface->GetConnAPIRef(), KI_CONNAPI_STATUS_SESSION, lacSession,
                           KI_CONNAPI_SESSION_TEXT_SIZE) == 0) &&
            (lacSession[0] != 0);
        if (!lbInSession)
        {
            return;
        }

        // Connection is up: drain one buffered invite if none is currently in flight.
        if (miNumberOfInvitesToBeSent > 0 && !lbInviteInFlight)
        {
            CGS_ASSERT(!mbSendingInvite, "!mbSendingInvite");

            const u32 luUserIndex = mpNetworkManager->GetActiveControllerPort();
            const u32 luResult = XInviteSend(luUserIndex,
                                             static_cast<u32>(miNumberOfInvitesToBeSent),
                                             maInvitesToBeSent, 0, mInviteOverlapped);
            if (luResult == 0)
            {
                // Completed synchronously.
                miNumberOfInvitesToBeSent = 0;
                CGS_ASSERT(XGetOverlappedResult(mInviteOverlapped, 0, 0) == 0,
                           "mInviteOverlapped.DidOperationCompleteSuccessfully()");
            }
            else if (luResult == ERROR_IO_PENDING)
            {
                // Queued asynchronously; track it.
                miNumberOfInvitesToBeSent = 0;
                mbSendingInvite           = true;
            }
            else
            {
                CGS_ASSERT(false, "UNKNOWN ERROR sending invite");
            }
        }
    }

    // =========================================================================
    // StartDownloadingPictures @ 0x828940E8
    //
    // Begin a gamer-picture download run: stash the destination textures, the
    // picture-key blob and the source XUID array, reset the progress counters,
    // and kick off the first profile-settings request.
    // =========================================================================
    void BuddyManagerX360::StartDownloadingPictures(NetworkTexture* lpaTextures,
                                                     s32 liNumberOfPictures,
                                                     u64* lpaXuids,
                                                     void* lpaGamerPicKeys)
    {
        if (liNumberOfPictures == 0)
        {
            return;
        }

        mpDownloadTextures           = lpaTextures;
        mpGamerPicKeys               = lpaGamerPicKeys;
        mpDownloadXuids              = lpaXuids;
        miNumberOfPicturesToDownload = liNumberOfPictures;
        miCurrentProfile             = 0;
        miNextProfileToRequest       = 0;
        miFirstProfileInThisRequest  = 0;

        RequestNextProfilePictures();
    }

    // =========================================================================
    // RequestNextProfilePictures @ 0x8288CC28
    //
    // Fire the next async XUserReadProfileSettingsByXuid batch (up to 15 buddies'
    // gamercard-picture-key settings) and advance the request cursor.
    // =========================================================================
    void BuddyManagerX360::RequestNextProfilePictures()
    {
        u32 luBatchCount = static_cast<u32>(miNumberOfPicturesToDownload - miNextProfileToRequest);
        if (luBatchCount >= static_cast<u32>(KI_MAX_PROFILES_PER_REQUEST))
        {
            luBatchCount = KI_MAX_PROFILES_PER_REQUEST;
        }

        reinterpret_cast<CgsSystem::CgsXOverlapped*>(mGamerPicOverlapped)->Construct();

        const u32 luUserIndex = mpNetworkManager->GetActiveControllerPort();
        const u32 luSettingId = KU_PROFILE_GAMERCARD_PICTURE_KEY;
        u32 luResultsSize = 80000;   // pcbResults seed (asm: v6 = 0x13880 == 80000)

        const u32 luResult = XUserReadProfileSettingsByXuid(
            0,
            luUserIndex,
            luBatchCount,
            mpDownloadXuids + miNextProfileToRequest,
            1,
            &luSettingId,
            &luResultsSize,
            &muProfileResultsSettingsLen,    // pResults (+0x28)
            mGamerPicOverlapped);
        CGS_ASSERT(luResult == ERROR_IO_PENDING, "dwRet == ERROR_IO_PENDING");

        meCurrentStatus             = E_BUDDY_ACTION_DOWNLOADING_PROFILE;
        miFirstProfileInThisRequest = miNextProfileToRequest;
        miNextProfileToRequest      = miNextProfileToRequest + static_cast<s32>(luBatchCount);
    }

    // =========================================================================
    // UpdatePictureDownload (virtual)
    //
    // Once the outstanding read has finished: a finished profile read goes on to
    // fetch the current picture (clearing its downloaded flag when the read
    // failed); a finished picture read sets the current flag on success and
    // advances to the next picture.
    // =========================================================================
    void BuddyManagerX360::UpdatePictureDownload()
    {
        if (meCurrentStatus == E_BUDDY_ACTION_IDLE)
        {
            return;
        }

        if (meCurrentStatus == E_BUDDY_ACTION_DOWNLOADING_PROFILE)
        {
            const u32 luResult = XGetOverlappedResult(mGamerPicOverlapped, 0, 0);
            if (luResult == ERROR_IO_PENDING || luResult == ERROR_IO_INCOMPLETE)
            {
                return;
            }
            if (XGetOverlappedResult(mGamerPicOverlapped, 0, 0) != 0 && mpGamerPicKeys != 0)
            {
                static_cast<u8*>(mpGamerPicKeys)[miCurrentProfile] = 0;
            }
            DownloadNextPicture(true);
            return;
        }

        if (meCurrentStatus == E_BUDDY_ACTION_DOWNLOADING_PIC)
        {
            const u32 luResult = XGetOverlappedResult(mGamerPicOverlapped, 0, 0);
            if (luResult == ERROR_IO_PENDING || luResult == ERROR_IO_INCOMPLETE)
            {
                return;
            }
            if (XGetOverlappedResult(mGamerPicOverlapped, 0, 0) == 0 && mpGamerPicKeys != 0)
            {
                static_cast<u8*>(mpGamerPicKeys)[miCurrentProfile] = 1;
            }
            DownloadNextPicture(false);
        }
    }

    // =========================================================================
    // DownloadNextPicture @ 0x8288CD08
    //
    // Step the per-buddy gamer-picture download. When advancing (lbContinueCurrent
    // false) bump the cursor and either finish (all done), request the next profile
    // batch (cursor caught up to the requested span), or fall through to fetch the
    // current picture. The fetch reads texture geometry from the destination
    // NetworkTexture and issues XUserReadGamerPictureByKey into its pixel buffer.
    // =========================================================================
    void BuddyManagerX360::DownloadNextPicture(bool lbContinueCurrent)
    {
        if (!lbContinueCurrent)
        {
            const s32 liNext = miCurrentProfile + 1;
            miCurrentProfile = liNext;

            if (liNext >= miNumberOfPicturesToDownload)
            {
                meCurrentStatus = E_BUDDY_ACTION_IDLE;
                return;
            }
            if (liNext >= miNextProfileToRequest)
            {
                RequestNextProfilePictures();
                return;
            }
        }

        meCurrentStatus = E_BUDDY_ACTION_DOWNLOADING_PIC;
        reinterpret_cast<CgsSystem::CgsXOverlapped*>(mGamerPicOverlapped)->Construct();

        // The destination texture for the current buddy (stride == sizeof NetworkTexture).
        NetworkTexture* lpTexture = mpDownloadTextures + miCurrentProfile;
        const u32 luHeight = static_cast<u32>(lpTexture->GetHeight());   // +0x0C
        u8*       lpPixels  = reinterpret_cast<u8*>(lpTexture->GetTexture()); // +0x14
        const u32 luPitch   = static_cast<u32>(lpTexture->GetStride());

        // The picture key for this buddy. The record base is the pSettings pointer
        // the XDK filled into the downloaded results buffer (asm: lwz r10, 0x2C(this)
        // == mpProfileSettings), NOT the caller's per-picture flag array (mpGamerPicKeys
        // @+0x20, which UpdatePictureDownload writes). Records are 40 bytes, with the
        // gamercard picture key at +0x18 (asm: 40*(cur-first) + *(this+0x2C) + 0x18).
        const u8* lpKeyRecord = reinterpret_cast<const u8*>(mpProfileSettings)
                              + 40 * (miCurrentProfile - miFirstProfileInThisRequest);
        const void* lpPictureKey = lpKeyRecord + 0x18;

        const u32 luResult = XUserReadGamerPictureByKey(
            lpPictureKey, 0, lpPixels, luPitch, luHeight, mGamerPicOverlapped);
        CGS_ASSERT(luResult == ERROR_IO_PENDING, "lDwErr == ERROR_IO_PENDING");
    }

    // =========================================================================
    // CancelPictureDownload @ 0x8287FA60
    //
    // If a profile/picture op is in progress, set state idle and cancel the
    // outstanding gamer-picture overlapped op.
    // =========================================================================
    void BuddyManagerX360::CancelPictureDownload()
    {
        if (meCurrentStatus != E_BUDDY_ACTION_IDLE)
        {
            meCurrentStatus = E_BUDDY_ACTION_IDLE;
            const u32 luResult = XCancelOverlapped(mGamerPicOverlapped);
            CGS_ASSERT(luResult == 0, "mGamerPicOverlapped.CancelXOverlapped()");
        }
    }

    // =========================================================================
    // GetNextUnreadMessage @ 0x8287FAC8 (virtual)
    // GetMessage           @ 0x8287FB78 (virtual)
    //
    // Buddy messaging is unsupported on the 360 (the Xbox Guide owns it); these
    // fire a "does nothing on the X360" assert and return false.
    // =========================================================================
    bool BuddyManagerX360::GetNextUnreadMessage(const PlayerName* /*lpPlayerName*/,
                                                char* /*lpcOut*/, s32 /*liMaxLength*/)
    {
        CGS_ASSERT(false,
                   "CgsNetwork::BuddyManagerX360::GetNextUnreadMessage does nothing on the X360 "
                   "you should be using the guide instead of the buddy manager");
        return false;
    }

    bool BuddyManagerX360::GetMessage(const PlayerName* /*lpPlayerName*/, s32 /*liIndex*/,
                                      char* /*lpcOut*/, s32 /*liMaxLength*/)
    {
        CGS_ASSERT(false,
                   "CgsNetwork::BuddyManagerX360::GetMessage does nothing on the X360 "
                   "you should be using the guide instead of the buddy manager");
        return false;
    }

    // =========================================================================
    // GetTitle @ 0x8287FC28 (virtual)
    //
    // Also unsupported on the 360; fires the same assert and returns the fixed
    // "Information not available" literal.
    // =========================================================================
    const char* BuddyManagerX360::GetTitle(const PlayerName* /*lpPlayerName*/)
    {
        CGS_ASSERT(false,
                   "CgsNetwork::BuddyManagerX360::GetTitle does nothing on the X360 "
                   "you should be using the guide instead of the buddy manager");
        return "Information not available";
    }

    // =========================================================================
    // Layout pin (never called) -- static_asserts the recovered X360 member
    // offsets so the gate fails loudly if the struct drifts.
    // =========================================================================
    void BuddyManagerX360::_AssertLayout()
    {
        // The X360 image is 32-bit (4-byte pointers); the absolute byte offsets the
        // asm encodes (mpDownloadTextures @ +0x1C, the progress ints @ 0x138A8.., the
        // overlapped blocks @ 0x138B8/0x138D4, the invite buffer @ 0x138F0..0x1391C)
        // assume that pointer width. The PC gate is 64-bit, so we pin the ABI-stable
        // *relationships* recovered from the asm rather than the raw 32-bit offsets
        // (which are documented inline above each member):
        //
        //   - the four progress ints are contiguous, in request order;
        //   - the two 28-byte overlapped blocks are adjacent (invite then gamer-pic);
        //   - the invite-XUID buffer is immediately followed by its count then flag.
        // The XDK-filled pSettings pointer is the second word of the embedded
        // results buffer: it follows the leading length word and precedes the
        // opaque tail (asm: pResults == this+0x28, key base read at this+0x2C).
        // The raw 32-bit gap is +4; on the 64-bit gate the pointer is widened, so
        // the ABI-stable relationship pinned here is the field *ordering*.
        static_assert(offsetof(BuddyManagerX360, mpProfileSettings)
                          > offsetof(BuddyManagerX360, muProfileResultsSettingsLen),
                      "pSettings follows the results-buffer length word");
        static_assert(offsetof(BuddyManagerX360, maProfileReadResults)
                          > offsetof(BuddyManagerX360, mpProfileSettings),
                      "the opaque results tail follows pSettings");
        static_assert(offsetof(BuddyManagerX360, miNextProfileToRequest)
                          == offsetof(BuddyManagerX360, miNumberOfPicturesToDownload) + 4,
                      "miNextProfileToRequest immediately follows miNumberOfPicturesToDownload");
        static_assert(offsetof(BuddyManagerX360, miFirstProfileInThisRequest)
                          == offsetof(BuddyManagerX360, miNextProfileToRequest) + 4,
                      "miFirstProfileInThisRequest follows miNextProfileToRequest");
        static_assert(offsetof(BuddyManagerX360, miCurrentProfile)
                          == offsetof(BuddyManagerX360, miFirstProfileInThisRequest) + 4,
                      "miCurrentProfile follows miFirstProfileInThisRequest");
        static_assert(offsetof(BuddyManagerX360, mGamerPicOverlapped)
                          == offsetof(BuddyManagerX360, mInviteOverlapped) + 28,
                      "mGamerPicOverlapped follows the 28-byte mInviteOverlapped");
        static_assert(offsetof(BuddyManagerX360, maInvitesToBeSent)
                          == offsetof(BuddyManagerX360, mGamerPicOverlapped) + 28,
                      "maInvitesToBeSent follows the 28-byte mGamerPicOverlapped");
        static_assert(offsetof(BuddyManagerX360, miNumberOfInvitesToBeSent)
                          == offsetof(BuddyManagerX360, maInvitesToBeSent)
                                 + sizeof(u64) * KI_MAX_INVITES_TO_BUFFER,
                      "miNumberOfInvitesToBeSent follows the 5-entry invite buffer");
        static_assert(offsetof(BuddyManagerX360, mbSendingInvite)
                          == offsetof(BuddyManagerX360, miNumberOfInvitesToBeSent) + 4,
                      "mbSendingInvite follows miNumberOfInvitesToBeSent");
    }
}
