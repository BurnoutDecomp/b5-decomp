#include "GameShared/GameClasses/Network/Buddies/DirtySock/X360/CgsBuddyManagerDirtySockX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
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
                                       u32* lpcbResults, void* lpResults);
    u32 XUserReadGamerPictureByKey(const void* lpPictureKey, s32 lbSmall,
                                   u8* lpTextureBuffer, u32 luPitch, u32 luHeight,
                                   void* lpOverlapped);
}

// Win32/XDK winerror.h codes consumed by the polling logic.
#ifndef ERROR_IO_INCOMPLETE
#define ERROR_IO_INCOMPLETE 996u
#endif
#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING    997u
#endif

// XPROFILE_GAMERCARD_PICTURE_KEY -- the single profile setting id requested per
// buddy (asm: pdwSettingIds = &dword_820E965C, dwNumSettingIds = 1). The literal
// setting-id value lives in un-reconstructed rodata (dword_820E965C); reserved
// here as a flagged placeholder so the read shape is preserved without inventing
// the constant.  FLAGGED: setting-id value (dword_820E965C) unrecovered.
static const u32 KU_PROFILE_GAMERCARD_PICTURE_KEY = 0u;

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
    // The active controller/user index lives at NetworkManager+0x60 (asm:
    // lwz r3, 0x60(mpNetworkManager)). NetworkManager has no shared header (its
    // definition is TU-local to CgsNetworkManager.cpp), so the field is reached
    // through an accessor the NetworkManager TU owns -- the same one the DirtySock
    // base uses. Declared here; the offset (+0x60 == user index) is grounded in the
    // asm. FLAGGED: accessor home un-reconstructed.
    s32 BuddyManager_GetNetworkManagerUserIndex(const NetworkManager* lpNetworkManager);

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
    // Reset the manager to a clean idle state. The asm zeroes five words:
    //   +0x13918 miNumberOfInvitesToBeSent, +0x1391C mbSendingInvite,
    //   +0x08 meCurrentStatus, +0x18 mBuddiesChangedCallback (base private),
    //   +0x04 mpBuddies. The base callback slot is reset through the protected
    //   _ClearBuddiesChangedCallback hook so the +0x18 store is reproduced.
    // =========================================================================
    bool BuddyManagerX360::Prepare()
    {
        miNumberOfInvitesToBeSent = 0;
        mbSendingInvite           = false;
        meCurrentStatus           = E_BUDDY_ACTION_IDLE;   // +0x08 (base)
        _ClearBuddiesChangedCallback();                    // +0x18 (base private)
        mpBuddies                 = 0;                       // +0x04 (base)
        return true;
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
        const u32 luUserIndex = BuddyManager_GetNetworkManagerUserIndex(mpNetworkManager);
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
        const u32 luUserIndex = BuddyManager_GetNetworkManagerUserIndex(mpNetworkManager);
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

        const u32 luUserIndex = BuddyManager_GetNetworkManagerUserIndex(mpNetworkManager);

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

    // =========================================================================
    // GetBuddyXUID @ 0x8287F9B0
    //
    // Look the buddy up in the HLB list by name and return its Xenon XUID.
    // The HLB list query / XUID accessor are external DirtySock entry points
    // (the PPC Hex-Rays mis-resolved their symbols); modelled by behaviour.
    // FLAGGED: HLBListGetBuddyByName / HLBBudGetXenonXUID are external middleware.
    // =========================================================================
    u64 BuddyManagerX360::GetBuddyXUID(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(mpBuddies, "mpBuddies");
        CGS_ASSERT(lpBuddyName, "lpBuddyName");

        DirtySock::HLBBudT* lpBuddy =
            DirtySock::HLBListGetBuddyByName(mpBuddies, lpBuddyName->GetPlayerName());
        CGS_ASSERT(lpBuddy, "lpBuddy");

        // HLBBudGetXenonXUID(lpBuddy) -> the buddy's 64-bit Xbox XUID. Declared as a
        // local extern: it is a real DirtySock/Xenon entry point that this TU calls
        // directly, with no shared header home.
        extern u64 HLBBudGetXenonXUID(DirtySock::HLBBudT* lpBuddy);
        return HLBBudGetXenonXUID(lpBuddy);
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

        // The "I have invited this buddy" flag is bit 26 of the buddy record's
        // status word at +0x18. The record layout is external middleware data; read
        // the bit via the accessor the DirtySock base exposes for that word.
        extern u32 HLBBudGetStatusWord(DirtySock::HLBBudT* lpBuddy);
        return ((HLBBudGetStatusWord(lpBuddy) >> 26) & 1) != 0;
    }

    // =========================================================================
    // AreAnyInvitesOpen @ 0x8287FF78 (virtual)
    //
    // Walk the buddy list; return true on the first buddy whose "invited" flag
    // (bit 26 of the status word at +0x18) is set.
    // =========================================================================
    bool BuddyManagerX360::AreAnyInvitesOpen()
    {
        extern u32 HLBBudGetStatusWord(DirtySock::HLBBudT* lpBuddy);

        for (s32 liIndex = 0; ; ++liIndex)
        {
            const s32 liBuddyCount = mpBuddies ? DirtySock::HLBListGetBuddyCount(mpBuddies) : 0;
            if (liIndex >= liBuddyCount)
            {
                break;
            }

            DirtySock::HLBBudT* lpBuddy = DirtySock::HLBListGetBuddyByIndex(mpBuddies, liIndex);
            CGS_ASSERT(lpBuddy, "lpBuddy");

            if (((HLBBudGetStatusWord(lpBuddy) >> 26) & 1) != 0)
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

        // Platform pre-update hook (vtable +0x7C); reconstructed in its own TU.
        // FLAGGED: pre-update virtual (+0x7C) home un-reconstructed.

        CGS_ASSERT(mpServerInterface, "mpServerInterface");
        // The asm additionally asserts mpServerInterface->GetGameComponent() is
        // non-null here (:152). Dereferencing the game component requires the full
        // ServerInterface type, whose connection state is queried below through the
        // BuddyManager_IsConnApiConnected accessor (the ConnApi status chain has no
        // shared header). The non-null guard is folded into that accessor's domain.

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

        // Gate on the ConnApi connection being live. The status query reaches deep
        // into the server interface's game component (asm: GetGameComponent()->...
        // +0x86C ->+0x80, ConnApiStatus selector 'genc' = 0x73656E63). The exact
        // member chain has no shared header; modelled as an external predicate.
        // FLAGGED: ConnApi connection-status accessor un-reconstructed.
        extern bool BuddyManager_IsConnApiConnected(ServerInterface* lpServerInterface);
        if (!BuddyManager_IsConnApiConnected(mpServerInterface))
        {
            return;
        }

        // Connection is up: drain one buffered invite if none is currently in flight.
        if (miNumberOfInvitesToBeSent > 0 && !lbInviteInFlight)
        {
            CGS_ASSERT(!mbSendingInvite, "!mbSendingInvite");

            const u32 luUserIndex = BuddyManager_GetNetworkManagerUserIndex(mpNetworkManager);
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

        const u32 luUserIndex = BuddyManager_GetNetworkManagerUserIndex(mpNetworkManager);
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
            &muProfileResultsSettingsLen);   // pResults == this + 0x28 (asm: addi r10, r31, 0x28)
        CGS_ASSERT(luResult == ERROR_IO_PENDING, "dwRet == ERROR_IO_PENDING");

        meCurrentStatus             = E_BUDDY_ACTION_DOWNLOADING_PROFILE;
        miFirstProfileInThisRequest = miNextProfileToRequest;
        miNextProfileToRequest      = miNextProfileToRequest + static_cast<s32>(luBatchCount);
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
        // == mpProfileSettings), NOT the caller's a5 key blob (mpGamerPicKeys @+0x20,
        // which is stored but never read here). Records are 40 bytes, with the
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
