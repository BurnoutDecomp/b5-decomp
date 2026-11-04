// Native X360 NetConnStatus, ARTIST 0x82B13248.
#include "netconn_x360_internal.h"
#include "netconn.h"
#include "dirtylib.h"
#include <string.h>

extern "C" u32 NetTick(void);
extern "C" u32 SocketGetLocalAddr(void);
extern "C" u32 SocketNtohl(u32 uAddress);
extern "C" s32 SocketInfo(void* pSocket, s32 iInfo, s32 iData, void* pBuffer, s32 iBufSize);

namespace
{
struct LocalePair { u16 uId; u16 uText; };
static const LocalePair kLanguage[] =
{
    { 1, 0x656e },
    { 2, 0x6a61 },
    { 3, 0x6465 },
    { 4, 0x6672 },
    { 5, 0x6573 },
    { 6, 0x6974 },
    { 7, 0x6b6f },
    { 8, 0x7a68 },
    { 9, 0x7074 },
};

static const LocalePair kCountry[] =
{
    { 1, 0x4145 },
    { 2, 0x414c },
    { 3, 0x414d },
    { 4, 0x4152 },
    { 5, 0x4154 },
    { 6, 0x4155 },
    { 7, 0x415a },
    { 8, 0x4245 },
    { 9, 0x4247 },
    { 10, 0x4248 },
    { 11, 0x424e },
    { 12, 0x424f },
    { 13, 0x4252 },
    { 14, 0x4259 },
    { 15, 0x425a },
    { 16, 0x4341 },
    { 18, 0x4348 },
    { 19, 0x434c },
    { 20, 0x434e },
    { 21, 0x434f },
    { 22, 0x4352 },
    { 23, 0x435a },
    { 24, 0x4445 },
    { 25, 0x444b },
    { 26, 0x444f },
    { 27, 0x445a },
    { 28, 0x4543 },
    { 29, 0x4545 },
    { 30, 0x4547 },
    { 31, 0x4553 },
    { 32, 0x4649 },
    { 33, 0x464f },
    { 34, 0x4652 },
    { 35, 0x4742 },
    { 36, 0x4745 },
    { 37, 0x4752 },
    { 38, 0x4754 },
    { 39, 0x484b },
    { 40, 0x484e },
    { 41, 0x4852 },
    { 42, 0x4855 },
    { 43, 0x4944 },
    { 44, 0x4945 },
    { 45, 0x494c },
    { 46, 0x494e },
    { 47, 0x4951 },
    { 48, 0x4952 },
    { 49, 0x4953 },
    { 50, 0x4954 },
    { 51, 0x4a4d },
    { 52, 0x4a4f },
    { 53, 0x4a50 },
    { 54, 0x4b45 },
    { 55, 0x4b47 },
    { 56, 0x4b52 },
    { 57, 0x4b57 },
    { 58, 0x4b5a },
    { 59, 0x4c42 },
    { 60, 0x4c49 },
    { 61, 0x4c54 },
    { 62, 0x4c55 },
    { 63, 0x4c56 },
    { 64, 0x4c59 },
    { 65, 0x4d41 },
    { 66, 0x4d43 },
    { 67, 0x4d4b },
    { 68, 0x4d4e },
    { 69, 0x4d4f },
    { 70, 0x4d56 },
    { 71, 0x4d58 },
    { 72, 0x4d59 },
    { 73, 0x4e49 },
    { 74, 0x4e4c },
    { 75, 0x4e4f },
    { 76, 0x4e5a },
    { 77, 0x4f4d },
    { 78, 0x5041 },
    { 79, 0x5045 },
    { 80, 0x5048 },
    { 81, 0x504b },
    { 82, 0x504c },
    { 83, 0x5052 },
    { 84, 0x5054 },
    { 85, 0x5059 },
    { 86, 0x5141 },
    { 87, 0x524f },
    { 88, 0x5255 },
    { 89, 0x5341 },
    { 90, 0x5345 },
    { 91, 0x5347 },
    { 92, 0x5349 },
    { 93, 0x534b },
    { 95, 0x5356 },
    { 96, 0x5359 },
    { 97, 0x5448 },
    { 98, 0x544e },
    { 99, 0x5452 },
    { 100, 0x5454 },
    { 101, 0x5457 },
    { 102, 0x5541 },
    { 103, 0x5553 },
    { 104, 0x5559 },
    { 105, 0x555a },
    { 106, 0x5645 },
    { 107, 0x564e },
    { 108, 0x5945 },
    { 109, 0x5a41 },
    { 110, 0x5a57 },
    { 0, 0x0000 },
    { 0, 0x0000 },
};

u32 NetConnX360Locale(const GET_USER_INFO_RESPONSE* pInfo)
{
    u16 uLanguage = 0x7a7a;
    u16 uCountry = 0x5a5a;
    u32 i;
    for (i = 0; i < sizeof(kLanguage) / sizeof(kLanguage[0]); ++i)
    {
        if (kLanguage[i].uId == pInfo->wLanguageId)
        {
            uLanguage = kLanguage[i].uText;
            break;
        }
    }
    for (i = 0; i < sizeof(kCountry) / sizeof(kCountry[0]); ++i)
    {
        if (kCountry[i].uId == pInfo->bCountryId)
        {
            uCountry = kCountry[i].uText;
            break;
        }
    }
    u32 a = uLanguage >> 8, b = uLanguage & 0xff;
    u32 c = uCountry >> 8, d = uCountry & 0xff;
    if ((a >= 'A') && (a <= 'Z')) a |= 0x20;
    if ((b >= 'A') && (b <= 'Z')) b |= 0x20;
    if ((c >= 'a') && (c <= 'z')) c &= ~0x20;
    if ((d >= 'a') && (d <= 'z')) d &= ~0x20;
    return (a << 24) | (b << 16) | (c << 8) | d;
}
}

extern "C" s32 NetConnStatus(s32 iKind, s32 iData, void* pBuffer, s32 iBufSize)
{
    NetConnX360State* pState = gNetConnX360;
    if (iKind == 0x6f70656e) return pState != 0;
    if (pState == 0)
    {
        NetPrintfCode("netconnxenon: warning - calling NetConnStatus() while module is not initialized\n");
        return -1;
    }
    if (GetCurrentThreadId() == pState->uOwnerThreadId)
    {
        NetConnX360Idle(pState, NetTick());
    }
    switch (iKind)
    {
    case 0x61646472: return SocketGetLocalAddr();
    case 0x62626e64: return 1;
    case 0x62707364: return pState->iDownstream;
    case 0x62707375: return pState->iUpstream;
    case 0x636f6e6e:
    case 0x73746172: return pState->iConnectionStatus;
    case 0x66726e64: return NetConnX360FriendsStatus(pState, iData, pBuffer, iBufSize);
    case 0x66726e76: return pState->iFriendVersion;
    case 0x67617465: return pState->bGatewayConnected;
    }

    const u32 aLegacy[] = { 0x67746167, 0x696e7674, 0x6c6f636c, 0x73657276, 0x786e666f, 0x78756964 };
    for (u32 i = 0; i < sizeof(aLegacy) / sizeof(aLegacy[0]); ++i)
    {
        u32 uKind = static_cast<u32>(iKind);
        if ((uKind > aLegacy[i]) && (uKind <= aLegacy[i] + 4))
        {
            NetPrintfCode("netconnxenon: warning ---------------------------------------------\n");
            NetPrintfCode("netconnxenon: warning -- old-style selector+index usage detected!\n");
            NetPrintfCode("netconnxenon: warning -- this usage has been deprecated and will be\n");
            NetPrintfCode("netconnxenon: warning -- removed in a future SDK revision.\n");
            iData = uKind - aLegacy[i];
            iKind = aLegacy[i];
        }
    }
    switch (iKind)
    {
    case 0x67746167: // gtag
        if ((pBuffer != 0) && (static_cast<u32>(iBufSize) >= 16))
        {
            memcpy(pBuffer, pState->aUsers[iData].strName, 16);
            return 1;
        }
        return 0;
    case 0x686f7374: // host
        return (pState->bOnline && pState->bSecure) ? SocketNtohl(pState->aServices[0].serviceIP.s_addr) : 0;
    case 0x696e7674: // invt
        if (pState->iInviteCount == 0) return 0;
        {
            union { XINVITE_INFO info; u8 bytes[224]; } invite;
            if (XInviteGetAcceptedInfo(iData, &invite.info) != 0) return -1;
            if (pBuffer != 0) memcpy(pBuffer, invite.bytes, iBufSize);
            --pState->iInviteCount;
            return 1;
        }
    case 0x69736672: // isfr: the original reads pBuffer before validating iData
        {
            XUID xuid = *static_cast<XUID*>(pBuffer);
            if (static_cast<u32>(iData) <= 3)
            {
                for (u32 i = 0; i < static_cast<u32>(pState->aFriendCount[iData]); ++i)
                {
                    XONLINE_FRIEND* pFriend = &pState->aFriends[iData][i];
                    if ((pFriend->xuid == xuid) && ((pFriend->dwFriendState & 0xc0000000u) == 0)) return 1;
                }
            }
            return 0;
        }
    case 0x6c6f636c: // locl
        {
            NetConnX360User* pUser = &pState->aUsers[iData];
            return ((pUser->pInfo != 0) && (pUser->iInfoState == 3)) ? NetConnX360Locale(pUser->pInfo) : 0x7a7a5a5a;
        }
    case 0x6d616378: // macx
        if ((pState->bAddressReady == 1) && (pBuffer != 0) && (static_cast<u32>(iBufSize) >= 6))
        {
            memcpy(pBuffer, pState->LocalAddress.abEnet, 6);
            return 1;
        }
        return 0;
    case 0x6d727474: return pState->iRtt;
    case 0x6f6e6c6e: return ((pState->bOnline == 1) && (pState->iConnectionState == 0)) ? 1 : 0;
    case 0x706c7567: return XNetGetEthernetLinkStatus() & 1;
    case 0x73616472: // sadr
        for (s32 i = 0; i < 4; ++i)
        {
            if (pState->aServices[i].serviceIP.s_addr == static_cast<u32>(iData)) return pState->aServices[i].dwServiceID;
        }
        return 0;
    case 0x73656375: return pState->bSecure;
    case 0x73657276: // serv
        if (static_cast<u32>(iData) > 3)
        {
            return (XOnlineGetServiceInfo(iData, static_cast<XONLINE_SERVICE_INFO*>(pBuffer)) == 0) ? iData : -1;
        }
        if ((pState->bOnline == 0) || (pState->bSecure == 0)) return -1;
        if (pBuffer != 0) memcpy(pBuffer, &pState->aServices[iData], iBufSize);
        return pState->aServices[iData].dwServiceID;
    case 0x736c6e6b: return pState->bSyslink;
    case 0x74737276:
    case 0x786c7370: return 0x45410805;
    case 0x74797065:
        return (pState->bAddressReady == 1) ? ((pState->uAddressStatus & 0x10) ? 20 : 4) : -1;
    case 0x78616464: // xadd
        if ((pBuffer == 0) || (static_cast<u32>(iBufSize) < 0x24)) return -1;
        memcpy(pBuffer, &pState->LocalAddress, 0x24);
        return 0;
    case 0x786e666f: // xnfo
        {
            NetConnX360User* pUser = &pState->aUsers[iData];
            if ((pBuffer == 0) || (pUser->pInfo == 0) || (pUser->iInfoState != 3) || (iBufSize != 4)) return -1;
            memcpy(pBuffer, &pUser->pInfo, 4);
            return 0;
        }
    case 0x786b6579: // xkey
        if (pState->bSyslink != 1) break;
        if ((pBuffer == 0) || (static_cast<u32>(iBufSize) < 16)) return -1;
        memcpy(pBuffer, &pState->LocalKey, 16);
        return 0;
    case 0x786b6964: // xkid
        if (pState->bSyslink != 1) break;
        if ((pBuffer == 0) || (static_cast<u32>(iBufSize) < 8)) return -1;
        memcpy(pBuffer, &pState->LocalKeyId, 8);
        return 0;
    case 0x78756964: // xuid
        if ((pBuffer == 0) || (static_cast<u32>(iBufSize) < 8)) return -1;
        memcpy(pBuffer, &pState->aUsers[iData].xuid, 8);
        return 0;
    }
    return SocketInfo(0, iKind, iData, pBuffer, iBufSize);
}
