// ============================================================================
// CgsXboxLivePC.cpp -- PC bodies for the Xbox system-software (X* / XAM) calls the game reaches.
//
// [PC platform leaf] Every function here is a console platform API with no game body to
// reconstruct. The callers declare them extern "C" in local blocks (one C name, so ONE
// definition serves every declaration). Each returns the answer the console gives for:
//   OFFLINE (default)  a profile signed in locally, no network service, no camera, no invites;
//   LAN (BP_LAN=1)     the same profile "signed in to the service", with the shared PC identity
//                      (CgsPcNetIdentity.h) as its gamertag and XUID.
// Win32 error codes used: 0 ERROR_SUCCESS, 18 ERROR_NO_MORE_FILES, 87 ERROR_INVALID_PARAMETER.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/CgsXOverlapped.h"   // XOVERLAPPED
#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"

#include <cstring>

namespace
{
    const u32 KU_ERROR_SUCCESS           = 0u;
    const u32 KU_ERROR_NO_MORE_FILES     = 18u;
    const u32 KU_ERROR_INVALID_PARAMETER = 87u;
    const u32 KU_ERROR_IO_INCOMPLETE     = 996u;
    const u32 KU_ERROR_IO_PENDING        = 997u;
    const u32 KU_ERROR_FUNCTION_FAILED   = 1627u;

    // Sign-in states: 0 not signed in, 1 signed in locally, 2 signed in to the service.
    const u32 KU_SIGNIN_STATE_LOCAL   = 1u;
    const u32 KU_SIGNIN_STATE_SERVICE = 2u;

    // The 40-byte sign-in info block XUserGetSigninInfo fills (callers pass a 40- or 48-byte
    // buffer and read the flags word at +0x08).
    struct SigninInfo
    {
        u64  mu64Xuid;             // +0x00
        u32  muInfoFlags;          // +0x08 -- bit 0 service-enabled, bit 1 guest
        u32  muSigninState;        // +0x0C
        u32  muGuestNumber;        // +0x10
        u32  muSponsorUserIndex;   // +0x14
        char macUserName[16];      // +0x18
    };
    static_assert(sizeof(SigninInfo) == 0x28, "sign-in info block is 40 bytes");

    const u32 KU_SIGNIN_INFO_SERVICE_ENABLED = 0x01u;

    void CopyName(char* lpcOut, u32 luCapacity, const char* lpcName)
    {
        if (lpcOut == 0 || luCapacity == 0u)
            return;
        u32 lu = 0u;
        for (; lu + 1u < luCapacity && lpcName[lu] != '\0'; ++lu)
            lpcOut[lu] = lpcName[lu];
        lpcOut[lu] = '\0';
    }

    // An asynchronous call that finished at once: the completion code lands in the
    // overlapped's status word, where XGetOverlappedResult reads it.
    void CompleteOverlapped(void* lpOverlapped, u32 luResult)
    {
        if (lpOverlapped == 0)
            return;
        XOVERLAPPED* lpXOverlapped = static_cast<XOVERLAPPED*>(lpOverlapped);
        lpXOverlapped->InternalLow     = luResult;
        lpXOverlapped->InternalHigh    = 0u;
        lpXOverlapped->dwExtendedError = 0u;
    }
}

// ---------------------------------------------------------------------------
// Overlapped I/O
// ---------------------------------------------------------------------------

// [PC platform leaf] Polls an asynchronous system call: 996 while it is still pending, else the
// completion code in the overlapped's status word. The PC leaves that start an asynchronous
// call finish it at once (CompleteOverlapped); an overlapped no PC leaf ever used keeps the zero
// its owner's Construct wrote and reads as finished successfully.
extern "C" u32 XGetOverlappedResult(PXOVERLAPPED lpXOverlapped, u32* lpdwResult, u32 /*bWait*/)
{
    if (lpXOverlapped == 0)
        return KU_ERROR_SUCCESS;
    if (lpXOverlapped->InternalLow == KU_ERROR_IO_PENDING)
        return KU_ERROR_IO_INCOMPLETE;
    if (lpdwResult != 0)
        *lpdwResult = lpXOverlapped->InternalHigh;
    return lpXOverlapped->InternalLow;
}

// [PC platform leaf] Extended error of a finished asynchronous call (the overlapped's
// extended-error word).
extern "C" u32 XGetOverlappedExtendedError(PXOVERLAPPED lpXOverlapped)
{
    if (lpXOverlapped == 0)
        return KU_ERROR_SUCCESS;
    return lpXOverlapped->dwExtendedError;
}

// [PC platform leaf] Cancels an asynchronous call; 0, nothing is in flight.
extern "C" u32 XCancelOverlapped(void* /*lpOverlapped*/)
{
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Reads the next batch from a system enumerator; no enumerator ever exists
// on PC, so the answer is "no more items" with nothing returned.
extern "C" u32 XEnumerate(void* /*lhEnum*/, void* /*lpvBuffer*/, u32 /*lcbBuffer*/,
                          u32* lpcItemsReturned, void* /*lpOverlapped*/)
{
    if (lpcItemsReturned != 0)
        *lpcItemsReturned = 0u;
    return KU_ERROR_NO_MORE_FILES;
}

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------

// [PC platform leaf] The console's block fill; memset on the host.
extern "C" void* XMemSet(void* lpDest, s32 liValue, u32 luCount)
{
    return std::memset(lpDest, liValue, luCount);
}

// ---------------------------------------------------------------------------
// Notifications
// ---------------------------------------------------------------------------

extern "C" __declspec(dllimport) void* __stdcall CreateEventW(void* lpEventAttributes, int bManualReset,
                                                           int bInitialState, const wchar_t* lpName);

// [PC platform leaf] Creates a system-notification listener. The console always hands back a
// handle (the buddy manager asserts on it and CloseHandle()s it on Destruct), so the PC returns a
// real kernel event that is never signalled: the handle is valid and closable, and XNotifyGetNext
// below never has a notification to dequeue from it.
extern "C" void* XNotifyCreateListener(unsigned long long /*qwAreas*/)
{
    return CreateEventW(0, 1, 0, 0);
}

// [PC platform leaf] Dequeues one system notification; 0 == nothing pending.
extern "C" int XNotifyGetNext(void* /*hListener*/, unsigned long /*dwMsgFilter*/,
                              unsigned long* /*pdwId*/, unsigned long* /*pParam*/)
{
    return 0;
}

// ---------------------------------------------------------------------------
// User / profile
// ---------------------------------------------------------------------------

// [PC platform leaf] Sign-in state of a controller's profile. OFFLINE: 1, signed in locally.
// The PC player always has a local profile, so 0 (no user) would be wrong: it made the pause
// menu's SAVE/LOAD row inert (BrnCrashNavSettings gates it on != 0). Callers read the state
// two ways only: != 0 "is there a user" (save/load, SystemUserProfile) and == 2 "is that user
// on the service" (training, buddies, network adapter, GUI cache, login); 1 opens the first
// and keeps every service path closed. LAN: 2, signed in to the service, which opens the
// online flow (login substate 0 gates on it).
extern "C" u32 XUserGetSigninState(u32 /*luUserIndex*/)
{
    return CgsPcNetLanEnabled() ? KU_SIGNIN_STATE_SERVICE : KU_SIGNIN_STATE_LOCAL;
}

// [PC platform leaf] Privilege query (multiplayer, communications, ...). OFFLINE: 87, the
// caller keeps its running answer. LAN: success with the privilege granted.
extern "C" s32 XUserCheckPrivilege(u32 /*luUserIndex*/, u32 /*luPrivilegeType*/, u32* lpbResult)
{
    if (!CgsPcNetLanEnabled())
        return static_cast<s32>(KU_ERROR_INVALID_PARAMETER);
    if (lpbResult != 0)
        *lpbResult = 1u;
    return static_cast<s32>(KU_ERROR_SUCCESS);
}

// [PC platform leaf] Fills the sign-in info block of a controller's profile. OFFLINE: 87, the
// caller treats the query as failed. LAN: the shared identity, service-enabled, not a guest.
extern "C" s32 XUserGetSigninInfo(u32 /*luUserIndex*/, u32 /*luFlags*/, void* lpSigninInfo)
{
    if (!CgsPcNetLanEnabled() || lpSigninInfo == 0)
        return static_cast<s32>(KU_ERROR_INVALID_PARAMETER);

    SigninInfo lInfo;
    std::memset(&lInfo, 0, sizeof(lInfo));
    lInfo.mu64Xuid      = CgsPcNetIdentityXuid();
    lInfo.muInfoFlags   = KU_SIGNIN_INFO_SERVICE_ENABLED;
    lInfo.muSigninState = KU_SIGNIN_STATE_SERVICE;
    CopyName(lInfo.macUserName, sizeof(lInfo.macUserName), CgsPcNetIdentityName());
    std::memcpy(lpSigninInfo, &lInfo, sizeof(lInfo));
    return static_cast<s32>(KU_ERROR_SUCCESS);
}

// [PC platform leaf] Gamertag of a controller's profile. OFFLINE: success with an empty name.
// LAN: the shared persona.
extern "C" u32 XUserGetName(u32 /*luUserIndex*/, char* lpszUserName, u32 luCchUserName)
{
    CopyName(lpszUserName, luCchUserName, CgsPcNetLanEnabled() ? CgsPcNetIdentityName() : "");
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] XUID of a controller's profile. OFFLINE: 87 with a zero XUID (no online
// identity). LAN: the shared stand-in XUID.
extern "C" u32 XUserGetXUID(u32 /*luUserIndex*/, u64* lpXuid)
{
    if (!CgsPcNetLanEnabled())
    {
        if (lpXuid != 0)
            *lpXuid = 0u;
        return KU_ERROR_INVALID_PARAMETER;
    }
    if (lpXuid != 0)
        *lpXuid = CgsPcNetIdentityXuid();
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Reads the local profile's settings; 87, the caller treats the read as
// failed and keeps its defaults.
extern "C" u32 XUserReadProfileSettings(u32 /*luTitleId*/, u32 /*luUserIndex*/,
                                        u32 /*luNumSettingIds*/, unsigned long* /*lpaSettingIds*/,
                                        unsigned long* /*lpcbResults*/, void* /*lpResults*/,
                                        void* /*lpOverlapped*/)
{
    return KU_ERROR_INVALID_PARAMETER;
}

// [PC platform leaf] Reads other users' profile settings (the gamer-picture keys). The call is
// asynchronous: 997 (started), and the overlapped finishes with 1627, there are no remote
// profiles on PC. The results block is left as the caller cleared it.
extern "C" u32 XUserReadProfileSettingsByXuid(u32 /*luTitleId*/, u32 /*luUserIndexRequester*/,
                                              u32 /*luNumXuids*/, const u64* /*lpaXuids*/,
                                              u32 /*luNumSettingIds*/, const u32* /*lpaSettingIds*/,
                                              u32* /*lpcbResults*/, void* /*lpResults*/,
                                              void* lpOverlapped)
{
    CompleteOverlapped(lpOverlapped, KU_ERROR_FUNCTION_FAILED);
    return KU_ERROR_IO_PENDING;
}

// [PC platform leaf] Downloads a gamer picture into a texture buffer. Asynchronous: 997
// (started), and the overlapped finishes with 1627, there are no gamer pictures on PC.
extern "C" u32 XUserReadGamerPictureByKey(const void* /*lpPictureKey*/, s32 /*lbSmall*/,
                                          u8* /*lpTextureBuffer*/, u32 /*luPitch*/,
                                          u32 /*luHeight*/, void* lpOverlapped)
{
    CompleteOverlapped(lpOverlapped, KU_ERROR_FUNCTION_FAILED);
    return KU_ERROR_IO_PENDING;
}

// [PC platform leaf] Asks whether a remote talker is on the local user's mute list; 0 with
// "not muted", there is no system mute list on PC.
extern "C" u32 XUserMuteListQuery(u32 /*luUserIndex*/, u64 /*luXuidRemoteTalker*/,
                                  s32* lpbOnMuteList)
{
    if (lpbOnMuteList != 0)
        *lpbOnMuteList = 0;
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Sets a rich-presence / matchmaking context value; no-op, there is no
// presence service on PC.
extern "C" void XUserSetContext(u32 /*uUserIndex*/, u32 /*uContextId*/, u32 /*uContextValue*/)
{
}

// ---------------------------------------------------------------------------
// Presence, invites, launch data
// ---------------------------------------------------------------------------

// [PC platform leaf] Creates a presence enumerator for a set of XUIDs; 87, no presence service.
extern "C" u32 XPresenceCreateEnumerator(u32 /*luUserIndex*/, u32 /*luPeers*/,
                                         const u64* /*lpaPeers*/, u32 /*luStartingIndex*/,
                                         u32 /*luPeersToReturn*/, u32* /*lpcbBuffer*/,
                                         void** /*lphEnum*/)
{
    return KU_ERROR_INVALID_PARAMETER;
}

// [PC platform leaf] Reads the invite the user accepted from the system guide; 87, no invites
// on PC (unreached: XNotifyGetNext never dequeues the invite-accepted notification).
extern "C" u32 XInviteGetAcceptedInfo(u32 /*luUserIndex*/, void* /*lpInviteInfo*/)
{
    return KU_ERROR_INVALID_PARAMETER;
}

// [PC platform leaf] Sends game invites to a list of XUIDs; 87, no invite service.
extern "C" u32 XInviteSend(u32 /*luUserIndex*/, u32 /*luXuidCount*/, const u64* /*lpaXuids*/,
                           const wchar_t* /*lpwcText*/, void* /*lpOverlapped*/)
{
    return KU_ERROR_INVALID_PARAMETER;
}

// [PC platform leaf] Stores data handed to the title on its next launch; no-op success.
extern "C" u32 XSetLaunchData(const void* /*lpLaunchData*/, u32 /*luLaunchDataSize*/)
{
    return KU_ERROR_SUCCESS;
}

// ---------------------------------------------------------------------------
// System guide UI
// ---------------------------------------------------------------------------

// [PC platform leaf] Shows the "disc unreadable" system screen; 0, nothing shown (the disk-error
// thread that calls it never runs on PC).
extern "C" unsigned long XShowDirtyDiscErrorUI(unsigned long /*dwUserIndex*/)
{
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Opens the guide's sign-in panes; 0, nothing shown.
extern "C" u32 XShowSigninUI(u32 /*luPanes*/, u32 /*luFlags*/)
{
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Opens a player's gamer card in the guide; 0, nothing shown.
extern "C" u32 XShowGamerCardUI(u32 /*luUserIndex*/, u64 /*luXuid*/)
{
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Opens the "review player" guide page; 0, nothing shown.
extern "C" u32 XShowPlayerReviewUI(u32 /*luUserIndex*/, u64 /*luXuid*/)
{
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Opens the guide's message inbox; 0, nothing shown.
extern "C" u32 XShowMessagesUI(u32 /*luUserIndex*/)
{
    return KU_ERROR_SUCCESS;
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

// [PC platform leaf] Camera readiness; 0, no camera (callers only proceed on 2, ready).
extern "C" u32 XCamGetStatus()
{
    return 0u;
}

// [PC platform leaf] Tears the camera session down; nothing to tear down.
extern "C" void XCamShutdown()
{
}

// ---------------------------------------------------------------------------
// Network address
// ---------------------------------------------------------------------------

// [PC platform leaf] NAT type of the connection; 1, open (there is no NAT between LAN peers,
// and offline nothing is ever reached). The player-parameters Prepare asserts on anything but
// 1 open, 2 moderate, 3 strict.
extern "C" u32 XOnlineGetNatType()
{
    return 1u;
}

// [PC platform leaf] The title's 36-byte network address (address, online address, online
// port, 6-byte adapter address at +0x0A, 20-byte online id). OFFLINE: all zero, 1 (no address
// acquired). LAN: the adapter bytes are the low 48 bits of the PC XUID (the same bytes
// NetConnMAC prints), 2 (address from the ethernet adapter).
extern "C" u32 XNetGetTitleXnAddr(void* lpXnAddr)
{
    const u32 KU_XNADDR_SIZE          = 36u;
    const u32 KU_XNADDR_ENET_OFFSET   = 0x0Au;
    const u32 KU_XNET_ADDR_NONE       = 0x01u;
    const u32 KU_XNET_ADDR_ETHERNET   = 0x02u;

    u8* lpAddr = static_cast<u8*>(lpXnAddr);
    memset(lpAddr, 0, KU_XNADDR_SIZE);
    if (!CgsPcNetLanEnabled())
    {
        return KU_XNET_ADDR_NONE;
    }
    const u64 lu64Xuid = CgsPcNetIdentityXuid();
    for (u32 luByte = 0u; luByte < 6u; ++luByte)
    {
        lpAddr[KU_XNADDR_ENET_OFFSET + luByte] = static_cast<u8>(lu64Xuid >> (8u * (5u - luByte)));
    }
    return KU_XNET_ADDR_ETHERNET;
}
