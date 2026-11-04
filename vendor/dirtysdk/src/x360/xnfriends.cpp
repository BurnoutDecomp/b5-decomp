// Native X360 DirtySDK XnFriends implementation, recovered from ARTIST.
#include "types.hpp"
#include "dirtymem.h"
#include "netconn.h"
#include <xtl.h>
#include <xonline.h>
#include "xnfriends_x360.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern "C" u32 NetTick(void);
extern "C" s32 ds_stricmp(const char*, const char*);
extern "C" char* ds_strnzcpy(char*, const char*, s32);

typedef void (*XnFriendsDebugCallback)(void*, const char*);

struct XnFriendsSnapshot
{
    s32 iCount;
    XONLINE_FRIEND aFriends[100];
};
struct XnFriendsChange
{
    char strName[16];
    s32 iAction;
    u32 uFlags;
};
struct XnFriendsRef
{
    s32 iMemGroup;
    s32 iCreateArg;
    void* pCreateArg;
    s32 iInitialised;
    s32 iConnected;
    u32 uConnectTick;
    s32 iCurrentSnapshot;
    XnFriendsSnapshot aSnapshots[2];
    XnFriendsChange aChanges[26];
    void* pChangeUserData;
    XnFriendsChangeCallback pChangeCallback;
    void* pDebugUserData;
    XnFriendsDebugCallback pDebugCallback;
    u8 aOpaque9BC4[8];
    s32 iUserIndex;
    u8 aOpaque9BD0[0x24];
};
typedef char XnFriendSizeC4[(sizeof(XONLINE_FRIEND) == 0xc4) ? 1 : -1];
typedef char XnFriendState18[(offsetof(XONLINE_FRIEND, dwFriendState) == 0x18) ? 1 : -1];
typedef char XnSnapshotSize4C94[(sizeof(XnFriendsSnapshot) == 0x4c94) ? 1 : -1];
typedef char XnChangeSize18[(sizeof(XnFriendsChange) == 0x18) ? 1 : -1];
typedef char XnChangesAt9944[(offsetof(XnFriendsRef, aChanges) == 0x9944) ? 1 : -1];
typedef char XnCallbackAt9BB8[(offsetof(XnFriendsRef, pChangeCallback) == 0x9bb8) ? 1 : -1];
typedef char XnDebugAt9BC0[(offsetof(XnFriendsRef, pDebugCallback) == 0x9bc0) ? 1 : -1];
typedef char XnUserAt9BCC[(offsetof(XnFriendsRef, iUserIndex) == 0x9bcc) ? 1 : -1];
typedef char XnRefSize9BF4[(sizeof(XnFriendsRef) == 0x9bf4) ? 1 : -1];

namespace
{
    const s32 KI_XNFRIENDS_MEMID = 0x786e6672;

    void XnFriendsDebug(XnFriendsRef* pRef, const char* pFormat, ...)
    {
        char strMessage[4096];
        va_list args;
        va_start(args, pFormat);
        vsprintf(strMessage, pFormat, args);
        va_end(args);
        if (pRef->pDebugCallback != 0)
        {
            pRef->pDebugCallback(pRef->pDebugUserData, strMessage);
        }
    }

    XONLINE_FRIEND* FindFriendByName(XnFriendsSnapshot* pSnapshot, const char* pName)
    {
        for (s32 i = 0; i < pSnapshot->iCount; ++i)
        {
            if (ds_stricmp(pSnapshot->aFriends[i].szGamertag, pName) == 0)
            {
                return &pSnapshot->aFriends[i];
            }
        }
        return 0;
    }

    s32 FindFriendByXuid(XnFriendsSnapshot* pSnapshot, XUID xuid)
    {
        for (s32 i = 0; i < pSnapshot->iCount; ++i)
        {
            if (pSnapshot->aFriends[i].xuid == xuid)
            {
                return i;
            }
        }
        return -1;
    }

    void AddChange(XnFriendsChange* pChanges, s32* pCount, s32 iAction,
                   const XONLINE_FRIEND* pFriend, u32 uFlags)
    {
        XnFriendsChange* pChange = &pChanges[*pCount];
        pChange->iAction = iAction;
        pChange->uFlags = uFlags;
        ds_strnzcpy(pChange->strName, pFriend->szGamertag, 16);
        ++*pCount;
    }

    void FlushChanges(XnFriendsRef* pRef, s32 iCount)
    {
        if (pRef->pChangeCallback != 0)
        {
            pRef->pChangeCallback(pRef, iCount, pRef->iInitialised,
                                  pRef->aChanges, pRef->pChangeUserData);
        }
    }

    void CompareFriends(XnFriendsRef* pRef, XnFriendsSnapshot* pOld,
                        XnFriendsSnapshot* pNew)
    {
        s32 iChanges = 0;
        s32 i;
        for (i = 0; i < pNew->iCount; ++i)
        {
            XONLINE_FRIEND* pFriend = &pNew->aFriends[i];
            s32 iOld = FindFriendByXuid(pOld, pFriend->xuid);
            if (iOld == -1)
            {
                AddChange(pRef->aChanges, &iChanges, 'A', pFriend, pFriend->dwFriendState);
            }
            else if (pOld->aFriends[iOld].dwFriendState != pFriend->dwFriendState)
            {
                AddChange(pRef->aChanges, &iChanges, 'C', pFriend,
                          pOld->aFriends[iOld].dwFriendState ^ pFriend->dwFriendState);
            }
            if (iChanges >= 25)
            {
                FlushChanges(pRef, iChanges);
                iChanges = 0;
            }
        }
        for (i = 0; i < pOld->iCount; ++i)
        {
            XONLINE_FRIEND* pFriend = &pOld->aFriends[i];
            if (FindFriendByXuid(pNew, pFriend->xuid) == -1)
            {
                AddChange(pRef->aChanges, &iChanges, 'D', pFriend, pFriend->dwFriendState);
            }
            if (iChanges >= 25)
            {
                FlushChanges(pRef, iChanges);
                iChanges = 0;
            }
        }
        if (iChanges != 0)
        {
            FlushChanges(pRef, iChanges);
        }
    }
}

extern "C" XnFriendsRef* XnFriendsCreate(s32 iArg, void* pArg)
{
    s32 iMemGroup = DirtyMemGroupQuery();
    XnFriendsRef* pRef = static_cast<XnFriendsRef*>(DirtyMemAlloc(0x9bf4, KI_XNFRIENDS_MEMID, iMemGroup));
    memset(pRef, 0, 0x9bf4);
    pRef->iMemGroup = iMemGroup;
    pRef->iCreateArg = iArg;
    pRef->pCreateArg = pArg;
    return pRef;
}

extern "C" void XnFriendsDestroy(XnFriendsRef* pRef)
{
    if (pRef != 0)
    {
        DirtyMemFree(pRef, KI_XNFRIENDS_MEMID, pRef->iMemGroup);
    }
}

extern "C" s32 XnFriendsConnect(XnFriendsRef* pRef, s32 /*iArg*/,
                                XnFriendsChangeCallback pCallback, void* pUserData)
{
    if (pRef == 0)
    {
        return -1;
    }
    pRef->pChangeCallback = pCallback;
    pRef->pChangeUserData = pUserData;
    pRef->iInitialised = 0;
    pRef->iConnected = 1;
    pRef->uConnectTick = NetTick();
    pRef->iCurrentSnapshot = 0;
    memset(pRef->aSnapshots, 0, sizeof(pRef->aSnapshots));
    memset(pRef->aChanges, 0, sizeof(pRef->aChanges));
    pRef->iConnected = 1;
    return 0;
}

extern "C" s32 XnFriendsDisconnect(XnFriendsRef* pRef)
{
    pRef->iCurrentSnapshot = 0;
    memset(pRef->aSnapshots, 0, sizeof(pRef->aSnapshots));
    memset(pRef->aChanges, 0, sizeof(pRef->aChanges));
    pRef->iConnected = 0;
    return 0;
}

extern "C" s32 XnFriendsSetUserIndex(XnFriendsRef* pRef, s32 iUserIndex)
{
    XnFriendsDebug(pRef, "Setting new User Index %d\n", iUserIndex);
    pRef->iUserIndex = iUserIndex;
    return 0;
}

extern "C" XONLINE_FRIEND* XnFriendsGetFriend(XnFriendsRef* pRef, s32 iIndex)
{
    if (iIndex < 0)
    {
        return 0;
    }
    XnFriendsSnapshot* pSnapshot = &pRef->aSnapshots[pRef->iCurrentSnapshot];
    if (iIndex >= pSnapshot->iCount)
    {
        return 0;
    }
    return &pSnapshot->aFriends[iIndex];
}

extern "C" XONLINE_FRIEND* XnFriendsGetFriendByName(XnFriendsRef* pRef, const char* pName)
{
    return FindFriendByName(&pRef->aSnapshots[pRef->iCurrentSnapshot], pName);
}

extern "C" void XnFriendsUpdate(XnFriendsRef* pRef)
{
    if ((pRef != 0) && (pRef->iConnected != 0))
    {
        XnFriendsSnapshot* pNew = &pRef->aSnapshots[pRef->iCurrentSnapshot];
        pNew->iCount = NetConnStatus(0x66726e64, pRef->iUserIndex, pNew->aFriends, 0x4c90);
        s32 iOld = (pRef->iCurrentSnapshot + 1) % 2;
        CompareFriends(pRef, &pRef->aSnapshots[iOld], pNew);
        pRef->iCurrentSnapshot = iOld;
        pRef->iInitialised = 1;
    }
}
