// ARTIST NetConnXenon friend snapshot and asynchronous handle-close cohort.
// The public singleton Startup/Idle/Status owner is not yet reconstructed here.
#include "netconn_x360_internal.h"
#include "dirtylib.h"
#include <string.h>

extern "C" const char* DirtyErrGetName(u32 uError);

s32 NetConnX360FriendsStatus(NetConnX360State* pState, s32 iUser, void* pBuffer, s32 iBufSize)
{
    if (static_cast<u32>(iUser) > 3)
    {
        return -1;
    }
    const s32 iBytes = pState->aFriendCount[iUser] * 0xc4;
    if (iBufSize < iBytes)
    {
        return -1;
    }
    memcpy(pBuffer, pState->aFriends[iUser], iBytes);
    return pState->aFriendCount[iUser];
}

// ARTIST 0x82B122F0; called on the NetConn idle thread with the current tick.
void NetConnX360RefreshFriends(NetConnX360State* pState, u32 uTick)
{
    DWORD uEvent;
    ULONG_PTR uUser;
    s32 i;
    if (pState->hFriendEnumerator == 0)
    {
        if (XNotifyGetNext(pState->hNotify, 0x04000002, &uEvent, &uUser))
        {
            pState->aFriendReady[uUser] = 0;
        }
        else if (XNotifyGetNext(pState->hNotify, 0x04000003, &uEvent, &uUser))
        {
            pState->aFriendReady[uUser] = 0;
            pState->aFriendCount[uUser] = 0;
        }
        else if (XNotifyGetNext(pState->hNotify, 0x04000001, &uEvent, &uUser))
        {
            pState->aFriendReady[uUser] = 0;
        }
        else if ((pState->iFriendRefreshPeriod != 0) &&
                 (static_cast<s32>(uTick - pState->uFriendRefreshTick) > pState->iFriendRefreshPeriod))
        {
            memset(pState->aFriendReady, 0, sizeof(pState->aFriendReady));
        }
    }

    for (i = 0; i < 4; ++i)
    {
        if ((pState->hFriendEnumerator != 0) || (pState->aFriendReady[i] != 0) ||
            (XUserGetSigninState(i) != eXUserSigninState_SignedInToLive))
        {
            continue;
        }
        XUSER_SIGNIN_INFO info;
        if ((XUserGetSigninInfo(i, 1, &info) != 0) || ((info.dwInfoFlags & 2) != 0))
        {
            pState->aFriendCount[i] = 0;
            pState->aFriendReady[i] = 1;
            continue;
        }
        pState->iFriendEnumeratorUser = i;
        DWORD uBufferSize;
        DWORD uError = XFriendsCreateEnumerator(i, 0, 100, &uBufferSize, &pState->hFriendEnumerator);
        if (uError != 0)
        {
            NetPrintfCode("netconnxenon: XFriendsCreateEnumerator() failed (err=%s)\n", DirtyErrGetName(uError));
            pState->aFriendCount[i] = 0;
            pState->aFriendReady[i] = 1;
            ++pState->iFriendVersion;
            CloseHandle(pState->hFriendEnumerator);
            pState->hFriendEnumerator = 0;
            pState->uFriendRefreshTick = uTick;
            continue;
        }
        memset(&pState->FriendOverlapped, 0, sizeof(pState->FriendOverlapped));
        uError = XEnumerate(pState->hFriendEnumerator, pState->aFriends[i], 0x4c90, 0,
                            &pState->FriendOverlapped);
        if (uError != ERROR_IO_PENDING)
        {
            NetPrintfCode("netconnxenon: XEnumerate() of friends failed (err=%s)\n", DirtyErrGetName(uError));
            CloseHandle(pState->hFriendEnumerator);
            pState->hFriendEnumerator = 0;
            ++pState->iFriendVersion;
            pState->aFriendReady[i] = 1;
            pState->aFriendCount[i] = 0;
            pState->uFriendRefreshTick = uTick;
        }
    }

    if ((pState->hFriendEnumerator != 0) && XHasOverlappedIoCompleted(&pState->FriendOverlapped))
    {
        DWORD uCount;
        if (XGetOverlappedResult(&pState->FriendOverlapped, &uCount, TRUE) != 0)
        {
            DWORD uError = XGetOverlappedExtendedError(&pState->FriendOverlapped);
            if (uError != 0x80070012u)
            {
                NetPrintfCode("netconnxenon: XGetOverlappedResult() of friends enumeration failed (err=%s)\n", DirtyErrGetName(uError));
            }
            pState->aFriendCount[pState->iFriendEnumeratorUser] = 0;
        }
        else
        {
            pState->aFriendCount[pState->iFriendEnumeratorUser] = uCount;
        }
        pState->aFriendReady[pState->iFriendEnumeratorUser] = 1;
        RtlEnterCriticalSection(&pState->CloseLock);
        if (pState->hDeferredClose == INVALID_HANDLE_VALUE)
        {
            pState->hDeferredClose = pState->hFriendEnumerator;
            pState->hFriendEnumerator = 0;
        }
        RtlLeaveCriticalSection(&pState->CloseLock);
        if (pState->hFriendEnumerator != 0)
        {
            NetPrintfCode("Alex: Warning could not close handle on thread, doing this on UpdateThread.\n");
            CloseHandle(pState->hFriendEnumerator);
            pState->hFriendEnumerator = 0;
        }
        pState->uFriendRefreshTick = uTick;
        ++pState->iFriendVersion;
    }
}

// ARTIST 0x82B12B70; thread return value is not consumed by the singleton.
void NetConnX360CloseWorker(NetConnX360State* pState)
{
    for (;;)
    {
        RtlEnterCriticalSection(&pState->CloseLock);
        HANDLE hClose = pState->hDeferredClose;
        s32 iStop = pState->iCloseThreadStop;
        RtlLeaveCriticalSection(&pState->CloseLock);
        if (hClose != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hClose);
            RtlEnterCriticalSection(&pState->CloseLock);
            pState->hDeferredClose = INVALID_HANDLE_VALUE;
            RtlLeaveCriticalSection(&pState->CloseLock);
        }
        if (iStop != 0)
        {
            break;
        }
        Sleep(16);
    }
    RtlEnterCriticalSection(&pState->CloseLock);
    pState->iCloseThreadStop = 0;
    RtlLeaveCriticalSection(&pState->CloseLock);
}

// The XDK thread entry has a DWORD return while the worker's last native call
// is the VOID RtlLeaveCriticalSection. ARTIST returns its resulting r3 unchanged.
// This X360 compiler emits a backchain frame and saves LR at caller SP-8 for
// this wrapper, with no nonvolatile-register saves. Verify both prologue and
// worker epilogue with every gate. Async unwind across this inline exit remains
// unverified; the compiler also emits an unreachable ordinary epilogue.
DWORD WINAPI NetConnX360CloseThread(LPVOID pContext)
{
    NetConnX360CloseWorker(static_cast<NetConnX360State*>(pContext));
    __asm { lwz r1,0(r1) }
    __asm { lwz r12,-8(r1) }
    __asm { mtlr r12 }
    __asm { blr }
}
