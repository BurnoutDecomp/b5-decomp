// DirtySDK 5.5.3 X360 VoIP owner and public selectors; ARTIST 0x82B28FE0,
// 0x82B28FF0, 0x82B290F0, 0x82B291F0, 0x82B29378-0x82B294C8,
// and 0x82B29638. The connection state follows its X360 call edges; the two
// XHV processing-mode constructors remain external providers.
// XDK size optimization matches ARTIST at 0x82B29490.

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "voip_x360_internal.h"

// X360 ARTIST callees whose bodies are in the private worker / XHV mode lane.
extern "C" void VoipWorkerThread(void* pVoip);                 // sub_82B28E80
extern "C" void VoipIncomingVoice(VoipUserT*, VoipPacketT*, void*); // sub_82B28A28
extern "C" void VoipRegisterRemoteTalker(VoipUserT*, void*, u32);    // sub_82B28A98
extern "C" void VoipSetRemoteTalkerProcessingModes(VoipRefT*, s32);  // sub_82B28B48
extern "C" s32 VoipBindLocalTalker(VoipRefT*, u32);                  // sub_82B28BC0
extern "C" void VoipUpdatePrivileges(VoipRefT*);                    // sub_82B28858
extern "C" void VoipUpdateMuteList(VoipRefT*);                      // sub_82B28798
extern "C" s32 VoipUpdateVoiceActivity(VoipRefT*);                    // sub_82B28688

namespace
{
VoipRefT* gVoipRef;

PFNMICRAWDATAREADY gVoipMicRawDataReady;

// The ARTIST target at 0x82AD5078 is the actual one-instruction no-op used by
// VoipStartup as its initial event callback.
void VoipDefaultEventCallback(VoipRefT*, s32, s32, void*)
{
}

u64 VoipDecodeXuid(const char* pUserID)
{
    u64 xuid = 0;
    for (s32 i = 0; i < 16; ++i)
    {
        const s32 character = static_cast<s8>(pUserID[i + 1]);
        s32 base = 0x57;
        if (character >= '0' && character < ':')
            base = '0';
        const s64 digit = static_cast<s8>(character - base);
        xuid = (xuid << 4) | static_cast<u64>(digit);
    }
    return xuid;
}
}

extern "C" void VoipIncomingVoice(VoipUserT* pRemoteUser,
                                    VoipPacketT* pPacket,
                                    void* pUserData)
{
    VoipRefT* pVoip = static_cast<VoipRefT*>(pUserData);
    const u8* pPacketBytes = reinterpret_cast<const u8*>(pPacket);
    u64 remoteXuid = 0;
    std::memcpy(&remoteXuid, pRemoteUser->data, sizeof(remoteXuid));
    DWORD dataSize = static_cast<DWORD>(pPacketBytes[16] * 10);
    const HRESULT result = pVoip->pXhvEngine->SubmitIncomingChatData(
        remoteXuid, pPacketBytes + 17, &dataSize);
    if (result)
    {
        NetPrintfCode("voip: SubmitIncomingChatData() failed (err=%s)\n",
                      DirtyErrGetName(static_cast<s32>(result)));
    }
}

extern "C" void VoipRegisterRemoteTalker(VoipUserT* pRemoteUser,
                                           void* pUserData,
                                           u32 bRegister)
{
    VoipRefT* pVoip = static_cast<VoipRefT*>(pUserData);
    VoipConnectionListT& list = pVoip->Connectionlist;
    u64 remoteXuid = 0;
    std::memcpy(&remoteXuid, pRemoteUser->data, sizeof(remoteXuid));
    if (remoteXuid == 0)
        return;

    if (bRegister)
    {
        pVoip->pXhvEngine->RegisterRemoteTalker(remoteXuid, 0, 0, 0);
        pVoip->pXhvEngine->StartRemoteProcessingModes(
            remoteXuid, (PXHV_PROCESSING_MODE)list.pVoiceChatMode, 1);
    }
    else
    {
        pVoip->pXhvEngine->SetPlaybackPriority(
            remoteXuid, static_cast<DWORD>(list.iLocalTalker),
            XHV_PLAYBACK_PRIORITY_NEVER);
        pVoip->pXhvEngine->UnregisterRemoteTalker(remoteXuid);
    }
    list.bMuteListDirty = 1;
}

extern "C" void VoipSetRemoteTalkerProcessingModes(VoipRefT* pVoip,
                                                    s32 bStart)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    for (s32 index = 0; index < list.iMaxConnections; ++index)
    {
        VoipConnectionT* pConnection = &list.pConnections[index];
        if (pConnection->eState == 2)
            VoipRegisterRemoteTalker(VoipUserForConnection(pConnection), pVoip,
                                     static_cast<u32>(bStart));
    }
}

extern "C" s32 VoipBindLocalTalker(VoipRefT* pVoip, u32 dwUserIndex)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    if (list.iLocalTalker != -1)
    {
        pVoip->pXhvEngine->StopLocalProcessingModes(
            static_cast<DWORD>(list.iLocalTalker),
            (PXHV_PROCESSING_MODE)list.pStartedLocalMode, 1);
        pVoip->pXhvEngine->UnregisterLocalTalker(
            static_cast<DWORD>(list.iLocalTalker));
        list.uLocalStatus &= ~0x50u;
        list.iLocalTalker = -1;
    }

    NetPrintfCode("voip: registering local talker to port %d\n", dwUserIndex);
    if (pVoip->pXhvEngine->RegisterLocalTalker(dwUserIndex) == 0)
    {
        pVoip->pXhvEngine->StartLocalProcessingModes(
            dwUserIndex, (PXHV_PROCESSING_MODE)list.pVoiceChatMode, 1);
        list.iLocalTalker = static_cast<s32>(dwUserIndex);
        list.iLocalUserIndex = static_cast<s32>(dwUserIndex);
        list.pStartedLocalMode = list.pVoiceChatMode;
    }
    return list.iLocalTalker == static_cast<s32>(dwUserIndex);
}

extern "C" void VoipUpdatePrivileges(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    DWORD result = XUserCheckPrivilege(255,
                                       static_cast<XPRIVILEGE_TYPE>(XPRIVILEGE_COMMUNICATIONS),
                                       reinterpret_cast<PBOOL>(&list.bVoiceAllowed));
    if (result)
        NetPrintfCode("voip: error 0x%08x trying to get XPRIVILEGE_COMMUNICATIONS\n",
                      result);
    result = XUserCheckPrivilege(255,
                                 static_cast<XPRIVILEGE_TYPE>(XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY),
                                 reinterpret_cast<PBOOL>(&list.bFriendsOnly));
    if (result)
        NetPrintfCode("voip: error 0x%08x trying to get XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY\n",
                      result);

    const char* status = list.bVoiceAllowed ? "enabled" :
        (list.bFriendsOnly ? "friends only" : "disabled");
    NetPrintfCode("voip: voice is %s\n", status);
}

extern "C" void VoipUpdateMuteList(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    if (!list.bMuteListDirty)
        return;
    for (s32 index = 0; index < list.iMaxConnections; ++index)
    {
        VoipConnectionT* pConnection = &list.pConnections[index];
        if (pConnection->eState != 2)
            continue;
        VoipUserT* pRemoteUser = VoipUserForConnection(pConnection);
        u64 remoteXuid = 0;
        std::memcpy(&remoteXuid, pRemoteUser->data, sizeof(remoteXuid));
        BOOL bMuted = FALSE;
        XUserMuteListQuery(static_cast<DWORD>(list.iLocalTalker), remoteXuid,
                           &bMuted);
        if (pConnection->bMuted != static_cast<u32>(bMuted))
        {
            NetPrintfCode("voip: %d) muting is %s\n", index,
                          bMuted ? "enabled" : "disabled");
            pConnection->bMuted = static_cast<u32>(bMuted);
        }
    }
    list.bMuteListDirty = 0;
}

extern "C" s32 VoipUpdateVoiceActivity(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    s32 status = NetConnStatus(1718775414, 0, 0, 0);
    if (list.iLastNetConnStatus == status)
        return status;

    const u32 maxPeers = static_cast<u32>(list.iMaxConnections);
    list.uFriendsOnlyMask = maxPeers >= 32
        ? 0xffffffffu : (1u << maxPeers) - 1;
    for (s32 iConnID = 0; iConnID < list.iMaxConnections; ++iConnID)
    {
        VoipConnectionT* pConnection = &list.pConnections[iConnID];
        const u32 bit = 1u << (static_cast<u32>(iConnID) & 31);
        for (DWORD userIndex = 0; userIndex < 4; ++userIndex)
        {
            BOOL bFriendsOnly = FALSE;
            if (XUserCheckPrivilege(userIndex,
                                    static_cast<XPRIVILEGE_TYPE>(XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY),
                                    reinterpret_cast<PBOOL>(&bFriendsOnly)) == 0 &&
                bFriendsOnly &&
                NetConnStatus(1769170546, static_cast<s32>(userIndex),
                              VoipUserForConnection(pConnection), 0) == 0)
                list.uFriendsOnlyMask &= ~bit;
        }
    }
    list.iLastNetConnStatus = NetConnStatus(1718775414, 0, 0, 0);
    return list.iLastNetConnStatus;
}

namespace
{
void VoipUpdateRemoteTalkerRouting(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    for (s32 index = 0; index < list.iMaxConnections; ++index)
    {
        VoipConnectionT* pConnection = &list.pConnections[index];
        if (pConnection->eState != 2)
            continue;

        const u32 bit = 1u << (static_cast<u32>(index) & 31);
        const bool bAllowed = !pConnection->bMuted &&
            (list.bVoiceAllowed ||
             (list.bFriendsOnly && (list.uFriendsOnlyMask & bit) != 0));
        const u32 sendMask = bAllowed && (list.uUserSendMask & bit)
            ? list.uSendMask | bit : list.uSendMask & ~bit;
        const u32 recvMask = bAllowed && (list.uUserRecvMask & bit)
            ? list.uRecvMask | bit : list.uRecvMask & ~bit;
        if (sendMask != list.uSendMask)
            VoipConnectionSetSendMask(&list, sendMask);
        if (recvMask != list.uRecvMask)
            VoipConnectionSetRecvMask(&list, recvMask);

        VoipUserT* pRemoteUser = VoipUserForConnection(pConnection);
        u64 remoteXuid = 0;
        std::memcpy(&remoteXuid, pRemoteUser->data, sizeof(remoteXuid));
        const DWORD priority = (recvMask & bit)
            ? XHV_PLAYBACK_PRIORITY_MAX : XHV_PLAYBACK_PRIORITY_NEVER;
        pVoip->pXhvEngine->SetPlaybackPriority(
            remoteXuid, static_cast<DWORD>(list.iLocalTalker), priority);
    }
}
}

namespace
{
s32 VoipXhvUpdate(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    ULONG_PTR notificationData[2] = {0, 0};
    DWORD notificationId = 0;
    if (XNotifyGetNext(pVoip->hNotifyListener, 17, &notificationId,
                       notificationData))
    {
        NetPrintfCode("voip: mutelist change->0x%02x\n", notificationData[0]);
        list.bMuteListDirty = 1;
    }
    if (XNotifyGetNext(pVoip->hNotifyListener, 10, &notificationId,
                       notificationData))
        VoipUpdatePrivileges(pVoip);
    VoipUpdateMuteList(pVoip);
    VoipUpdateVoiceActivity(pVoip);
    VoipUpdateRemoteTalkerRouting(pVoip);

    u32 headsetMask = 0;
    for (DWORD userIndex = 0; userIndex < 4; ++userIndex)
    {
        const u32 bit = 1u << userIndex;
        if (pVoip->pXhvEngine->IsHeadsetPresent(userIndex))
            headsetMask |= bit;
        if ((list.uHeadsetMask ^ headsetMask) & bit)
            NetPrintfCode("voip: headset in port %d %s\n", userIndex,
                          (headsetMask & bit) ? "inserted" : "removed");
    }
    const u32 localBit = 1u << (static_cast<u32>(list.iLocalTalker) & 31);
    if (headsetMask & localBit)
        list.uLocalStatus |= 0x40;
    else
        list.uLocalStatus &= ~0x40u;
    list.uHeadsetMask = headsetMask;

    const bool bLoopbackMode = list.pStartedLocalMode ==
        (void*)&XHV_LOOPBACK_MODE;
    const DWORD initialSize = bLoopbackMode ? 47 : 10;
    if (list.iLocalTalker >= 0)
    {
        u8 localVoiceData[96];
        DWORD dataSize = initialSize;
        while (pVoip->pXhvEngine->GetLocalChatData(
                   static_cast<DWORD>(list.iLocalTalker), localVoiceData,
                   &dataSize, 0) >= 0)
        {
            const DWORD expectedSize = list.pStartedLocalMode ==
                (void*)&XHV_LOOPBACK_MODE ? 47 : 10;
            if (dataSize != expectedSize)
            {
                NetPrintfCode("voip: invalid voice data size %d (should be %d)\n",
                              dataSize, expectedSize);
                break;
            }
            VoipConnectionSend(&list, localVoiceData,
                               static_cast<s32>(dataSize));
        }
    }
    return VoipConnectionUpdate(&list);
}
}

extern "C" void VoipWorkerThread(void* pData)
{
    VoipRefT* pVoip = static_cast<VoipRefT*>(pData);
    VoipConnectionListT& list = pVoip->Connectionlist;
    while (!list.iThreadActive)
        Sleep(1);
    list.iThreadActive = 2;
    while (list.iThreadActive)
    {
        RtlEnterCriticalSection(&list.ThreadCrit);
        VoipXhvUpdate(pVoip);
        RtlLeaveCriticalSection(&list.ThreadCrit);
        Sleep(20);
    }
    list.hThread = 0;
}

extern "C" VoipRefT* VoipGetRef(void)
{
    return gVoipRef;
}

// ARTIST 0x82B28F18. NetConn passes the current tick as a second argument; the
// console body observes only the ref and the status transitions below.
extern "C" void VoipIdle(void* pData, u32)
{
    VoipRefT* pVoip = static_cast<VoipRefT*>(pData);
    VoipConnectionListT& list = pVoip->Connectionlist;
    if (list.uLastModeState != list.uHeadsetMask)
    {
        list.pCallback(pVoip, 0, static_cast<s32>(list.uHeadsetMask),
                       list.pCallbackUserData);
        list.uLastModeState = list.uHeadsetMask;
    }
    if (list.uLastRecvVoice != list.uRecvVoice)
    {
        list.pCallback(pVoip, 1, static_cast<s32>(list.uRecvVoice),
                       list.pCallbackUserData);
        list.uLastRecvVoice = list.uRecvVoice;
    }
    if (list.uLastLocalStatus != list.uLocalStatus)
    {
        if ((list.uLastLocalStatus ^ list.uLocalStatus) & 0x100)
            list.pCallback(pVoip, 2, static_cast<s32>(list.uLocalStatus & 0x100),
                           list.pCallbackUserData);
        list.uLastLocalStatus = list.uLocalStatus;
    }
}

// ARTIST 0x82B29638; the worker, connection callbacks and mode factories are
// retained as direct providers so this TU preserves the real XHV/transport edges.
extern "C" VoipRefT* VoipStartup(s32 iMaxPeers)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    if (gVoipRef)
    {
        NetPrintfCode("voip: module startup called when not in a shutdown state\n");
        return 0;
    }

    VoipRefT* pVoip = static_cast<VoipRefT*>(
        DirtyMemAlloc(sizeof(VoipRefT), kVoipMemModule, iMemGroup));
    if (!pVoip)
    {
        NetPrintfCode("voip: unable to allocate module state\n");
        return 0;
    }
    std::memset(pVoip, 0, sizeof(*pVoip));
    VoipMemGroupSet(iMemGroup);
    pVoip->Connectionlist.uUserSendMask = 0xffffffffu;
    pVoip->Connectionlist.uUserRecvMask = 0xffffffffu;
    pVoip->Connectionlist.uLocalStatus = 0x20;

    u64 initialLocalXuid = 0;
    if (VoipConnectionStartup(&pVoip->Connectionlist,
                              reinterpret_cast<VoipUserT*>(&initialLocalXuid),
                              iMaxPeers) < 0)
    {
        NetPrintfCode("voip: unable to allocate connectionlist\n");
        DirtyMemFree(pVoip, kVoipMemModule, iMemGroup);
        return 0;
    }

    const XHV_PROCESSING_MODE processingModes[2] = {
        XHV_VOICECHAT_MODE,
        XHV_LOOPBACK_MODE
    };
    XHV_INIT_PARAMS params;
    std::memset(&params, 0, sizeof(params));
    params.dwMaxRemoteTalkers = static_cast<DWORD>(iMaxPeers);
    params.dwMaxLocalTalkers = 1;
    params.localTalkerEnabledModes = processingModes;
    params.dwNumLocalTalkerEnabledModes = 2;
    params.remoteTalkerEnabledModes = processingModes;
    params.dwNumRemoteTalkerEnabledModes = 1;
    if (gVoipMicRawDataReady)
    {
        params.bCustomVADProvided = TRUE;
        params.pfnMicrophoneRawDataReady = gVoipMicRawDataReady;
    }
    HANDLE hXhvWorkerThread = 0;
    IXHVEngine* pXhvEngine = 0;
    const HRESULT hrXhv = XHVCreateEngine(&params, &hXhvWorkerThread,
                                          &pXhvEngine);
    if (hrXhv < 0)
    {
        NetPrintfCode("voip: error 0x%08x creating XVH engine\n",
                      static_cast<u32>(hrXhv));
        DirtyMemFree(pVoip, kVoipMemModule, iMemGroup);
        return 0;
    }

    RtlInitializeCriticalSection(&pVoip->Connectionlist.ThreadCrit);
    pVoip->Connectionlist.hThread = CreateThread(
        0, 0, (LPTHREAD_START_ROUTINE)VoipWorkerThread, pVoip, 0,
        &pVoip->Connectionlist.dwThreadId);
    if (!pVoip->Connectionlist.hThread)
    {
        NetPrintfCode("voip: error creating voip thread\n");
        VoipShutdown(pVoip);
        return 0;
    }
    SetThreadPriority(pVoip->Connectionlist.hThread, 2);
    VoipConnectionSetCallbacks(&pVoip->Connectionlist, VoipIncomingVoice,
                               VoipRegisterRemoteTalker, pVoip);
    pVoip->Connectionlist.pVoiceChatMode = (void*)&XHV_VOICECHAT_MODE;
    pVoip->Connectionlist.pStartedLocalMode = (void*)&XHV_VOICECHAT_MODE;
    pVoip->pXhvEngine = pXhvEngine;
    pVoip->Connectionlist.iLockPort = -1;
    pVoip->Connectionlist.iLocalTalker = -1;
    pVoip->hNotifyListener = XNotifyCreateListener(111);
    if (!pVoip->hNotifyListener)
        NetPrintfCode("netconnxenon: unable to create live event listener\n");
    pVoip->Connectionlist.uLastRecvVoice = pVoip->Connectionlist.uRecvVoice;
    pVoip->Connectionlist.uLastModeState = pVoip->Connectionlist.uHeadsetMask;
    pVoip->Connectionlist.pCallback = VoipDefaultEventCallback;
    NetConnIdleAdd(VoipIdle, pVoip);
    gVoipRef = pVoip;
    pVoip->Connectionlist.iThreadActive = 1;
    return pVoip;
}

// ARTIST 0x82B28FF0.
extern "C" void VoipShutdown(VoipRefT* pVoip)
{
    if (!gVoipRef)
    {
        NetPrintfCode("voip: module shutdown called when not in a started state\n");
        return;
    }
    VoipConnectionListT& list = pVoip->Connectionlist;
    while (!list.iThreadActive)
    {
    }
    list.iThreadActive = 0;
    if (list.hThread)
    {
        while (list.hThread)
        {
            NetPrintfCode("voip: waiting for shutdown\n");
            Sleep(1);
        }
    }
    NetConnIdleDel(VoipIdle, pVoip);
    VoipConnectionShutdown(&list);
    if (list.iLocalTalker != -1)
        pVoip->pXhvEngine->UnregisterLocalTalker(list.iLocalTalker);
    pVoip->pXhvEngine->Release();
    if (pVoip->hNotifyListener)
        CloseHandle(pVoip->hNotifyListener);
    DirtyMemFree(pVoip, kVoipMemModule, VoipMemGroupQuery());
    gVoipRef = 0;
}

// ARTIST 0x82B290F0; the XUID text is parsed and compared to the four signed-in users.
extern "C" void VoipSetLocalUser(VoipRefT* pVoip, const char* pUserID)
{
    const u64 xuid = VoipDecodeXuid(pUserID);
    s32 localIndex = 0;
    XUID signedInXuid = 0;
    for (; localIndex < 4; ++localIndex)
    {
        XUserGetXUID(static_cast<DWORD>(localIndex), &signedInXuid);
        if (signedInXuid == xuid)
            break;
    }
    if (localIndex == 4)
    {
        NetPrintfCode("voip: could not find user with XUID=%s\n", pUserID);
        return;
    }

    NetPrintfCode("voip: setting local user to '%s' (idx=%d)\n", pUserID, localIndex);
    pVoip->Connectionlist.aLocalXuids[localIndex] = xuid;
    if (pVoip->Connectionlist.iLocalTalker != localIndex)
    {
        VoipSetRemoteTalkerProcessingModes(pVoip, 0);
        VoipBindLocalTalker(pVoip, static_cast<u32>(localIndex));
        VoipSetRemoteTalkerProcessingModes(pVoip, 1);
    }
    VoipUpdatePrivileges(pVoip);
}

// ARTIST 0x82B291F0; connection start/stop stays delegated to the real list providers.
extern "C" s32 VoipConnect4(VoipRefT* pVoip, s32 iConnID, const char* pUserID,
                            u32 uAddress, u32 uManglePort, u32 uGamePort,
                            u32 uClientId, u32 uSessionId)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    if (iConnID == -1)
    {
        iConnID = VoipConnectionAllocate(&list);
        if (iConnID < 0)
            return -3;
    }
    else
    {
        if (iConnID < 0 || iConnID >= list.iMaxConnections)
        {
            NetPrintfCode("voip: connect with iConnID=%d is invalid\n", iConnID);
            return -4;
        }
        if (list.pConnections)
        {
            RtlEnterCriticalSection(&list.ThreadCrit);
            VoipConnectionStop(&list, iConnID);
            RtlLeaveCriticalSection(&list.ThreadCrit);
        }
    }

    u64 xuid = 0;
    VoipUserT* pUser = 0;
    if (pUserID)
    {
        xuid = VoipDecodeXuid(pUserID);
        // The X360 caller supplies a pointer to the parsed 64-bit XUID in the
        // opaque user's first bytes; the connection helper owns the record copy.
        pUser = reinterpret_cast<VoipUserT*>(&xuid);
    }
    RtlEnterCriticalSection(&list.ThreadCrit);
    // ARTIST 82B292E8-EC and 82B2932C-30 pass the game port in both port slots.
    VoipConnectionStart(&list, iConnID, pUser, uAddress, uGamePort,
                        uGamePort, uClientId, uSessionId);
    RtlLeaveCriticalSection(&list.ThreadCrit);
    if (pUserID)
        VoipUpdateMuteList(pVoip);
    return iConnID;
}

// ARTIST 0x82B29378.
extern "C" void VoipDisconnect(VoipRefT* pVoip, s32 iConnID)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    RtlEnterCriticalSection(&list.ThreadCrit);
    VoipConnectionStop(&list, iConnID);
    RtlLeaveCriticalSection(&list.ThreadCrit);
}

// ARTIST 0x82B293B8.
extern "C" void VoipRemove(VoipRefT* pVoip, s32 iConnID)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    RtlEnterCriticalSection(&list.ThreadCrit);
    VoipConnectionRemove(&list, iConnID);
    list.uUserSendMask = VoipConnectionRemoveConnFromMask(list.uUserSendMask, iConnID);
    list.uUserRecvMask = VoipConnectionRemoveConnFromMask(list.uUserRecvMask, iConnID);
    RtlLeaveCriticalSection(&list.ThreadCrit);
}

// ARTIST 0x82B29420.
extern "C" s32 VoipRemote(VoipRefT* pVoip, s32 iConnID)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    if (!list.pConnections)
        return static_cast<s32>(0x80000000u);
    if (iConnID == -2)
    {
        u32 flags = 0;
        for (s32 index = 0; index < list.iMaxConnections; ++index)
            flags |= list.pConnections[index].uRemoteStatus;
        return static_cast<s32>(flags);
    }
    if (iConnID < 0 || iConnID >= list.iMaxConnections)
        return static_cast<s32>(0x80000000u);
    return static_cast<s32>(list.pConnections[iConnID].uRemoteStatus);
}

// ARTIST 0x82B29490.
extern "C" u32 VoipLocal(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    u32 status = list.uLocalStatus;
    if (list.iLockPort >= 0 && list.iLocalTalker != list.iLockPort)
        status &= ~0x40u;
    return status;
}

// ARTIST 0x82B294B8 / 0x82B294C0.
extern "C" void VoipMicrophone(VoipRefT* pVoip, u32 uConnMask)
{
    pVoip->Connectionlist.uUserSendMask = uConnMask;
}

extern "C" void VoipSpeaker(VoipRefT* pVoip, u32 uConnMask)
{
    pVoip->Connectionlist.uUserRecvMask = uConnMask;
}

// ARTIST 0x82B294C8.
extern "C" s32 VoipControl(VoipRefT* pVoip, s32 iControl, s32 iValue)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    switch (iControl)
    {
    case 0x636c6964: // clid
        list.uClientId = static_cast<u32>(iValue);
        return 0;
    case 0x666c7368: // flsh
        return VoipConnectionFlush(&list, iValue);
    case 0x6c6f636b: // lock
        list.iLockPort = iValue;
        return 0;
    case 0x6d726472: // mrd
        gVoipMicRawDataReady = (PFNMICRAWDATAREADY)iValue;
        return 0;
    case 0x706f7274: // port
        if (list.iLocalTalker != iValue)
        {
            VoipSetRemoteTalkerProcessingModes(pVoip, 0);
            const s32 result = VoipBindLocalTalker(pVoip, static_cast<u32>(iValue));
            VoipSetRemoteTalkerProcessingModes(pVoip, 1);
            return result;
        }
        return 1;
    case 0x7175616c: // qual
        NetPrintfCode("voip: SetQualityLevel not supported without XHV_PREVIEW_LIB\n");
        return 1;
    case 0x73657276: // serv
        NetPrintfCode("voip: setting server mode = %d\n", iValue);
        list.bServer = static_cast<u8>(iValue);
        return 0;
    case 0x74696d65: // time
        list.iDataTimeout = iValue;
        return 0;
    default:
        return -1;
    }
}
