#ifndef DIRTYSDK_NETCONN_X360_INTERNAL_H
#define DIRTYSDK_NETCONN_X360_INTERNAL_H

#include "types.hpp"
#include <xtl.h>
#include <xonline.h>
#include <stddef.h>

// NetConnStartup allocates 0x13430 bytes. Unnamed ranges are retained until
// the remaining singleton owner is recovered; the friends fields are ARTIST-attested.
struct NetConnX360User
{
    char strName[16];
    XUID xuid;
    s32 iInfoState;
    GET_USER_INFO_RESPONSE* pInfo;
};
typedef char NetConnX360LanguageOffset[(offsetof(GET_USER_INFO_RESPONSE,wLanguageId)==0x36)?1:-1];
typedef char NetConnX360CountryOffset[(offsetof(GET_USER_INFO_RESPONSE,bCountryId)==0x38)?1:-1];
typedef char NetConnX360UserSize[(sizeof(NetConnX360User)==0x20)?1:-1];

struct NetConnX360AddrMap
{
    u32 uStart;
    u32 uEnd;
    u32 uRemap;
    u8 aOpaque[20];
};

struct NetConnX360State
{
    s32 iMemGroup;                        // +0x0000
    u8 aOpaque004[8];
    s32 iConnectionIndex;                // +0x000c
    u8 aOpaque010[4];
    XONLINE_SERVICE_INFO aServices[4];    // +0x0014
    s32 iServiceCount;                    // +0x0044
    NetConnX360User aUsers[4];            // +0x0048
    u8 aOpaque0C8[4];
    XOVERLAPPED AccountOverlapped;         // +0x00cc
    XOVERLAPPED FriendOverlapped;         // +0x00e8
    HANDLE hNotify;                       // +0x0104
    XNQOS* pQos;                          // +0x0108
    XNADDR LocalAddress;                   // +0x010c
    XNKID LocalKeyId;                     // +0x0130
    XNKEY LocalKey;                       // +0x0138
    s32 iConnectionState;                 // +0x0148
    u32 uAddressStatus;                   // +0x014c
    s32 iConnectionStatus;                // +0x0150
    u8 aOpaque154[4];
    s32 iUpstream;                        // +0x0158
    s32 iDownstream;                      // +0x015c
    s32 iRtt;                             // +0x0160
    s32 iInviteCount;                     // +0x0164
    NetConnX360AddrMap AddressMap;        // +0x0168
    u8 bSyslink;                          // +0x0188
    u8 bKeyRegistered;                    // +0x0189
    u8 bSecure;                           // +0x018a
    u8 bAddressReady;                     // +0x018b
    u8 bOnline;                           // +0x018c
    u8 bGatewayConnected;                 // +0x018d
    u8 aOpaque18E[1];
    u8 bQosRequested;                     // +0x018f
    u8 bAccountBusy;                      // +0x0190
    u8 aFriendReady[4];                   // +0x0191
    u8 aOpaque195[3];
    XONLINE_FRIEND aFriends[4][100];      // +0x0198
    s32 aFriendCount[4];                  // +0x133d8
    s32 iFriendVersion;                   // +0x133e8
    HANDLE hFriendEnumerator;             // +0x133ec
    s32 iFriendEnumeratorUser;            // +0x133f0
    s32 iFriendRefreshPeriod;             // +0x133f4
    u32 uFriendRefreshTick;                // +0x133f8
    DWORD uOwnerThreadId;                   // +0x133fc
    HANDLE hCloseThread;                  // +0x13400
    DWORD uCloseThreadId;                   // +0x13404
    RTL_CRITICAL_SECTION CloseLock;       // +0x13408
    HANDLE hDeferredClose;                // +0x13424
    s32 iCloseThreadStop;                 // +0x13428
    u8 aOpaque1342C[4];
};
typedef char NetConnX360MapOffset[(offsetof(NetConnX360State,AddressMap)==0x168)?1:-1];
typedef char NetConnX360ServicesOffset[(offsetof(NetConnX360State,aServices)==0x14)?1:-1];
typedef char NetConnX360AddressOffset[(offsetof(NetConnX360State,LocalAddress)==0x10c)?1:-1];
typedef char NetConnX360ConnStatusOffset[(offsetof(NetConnX360State,iConnectionStatus)==0x150)?1:-1];
typedef char NetConnX360UsersOffset[(offsetof(NetConnX360State,aUsers)==0x48)?1:-1];
typedef char NetConnX360AccountOffset[(offsetof(NetConnX360State,AccountOverlapped)==0xcc)?1:-1];
typedef char NetConnX360AccountBusyOffset[(offsetof(NetConnX360State,bAccountBusy)==0x190)?1:-1];
typedef char NetConnX360StateOffset[(offsetof(NetConnX360State,iConnectionState)==0x148)?1:-1];
typedef char NetConnX360SyslinkOffset[(offsetof(NetConnX360State,bSyslink)==0x188)?1:-1];
typedef char NetConnX360SecureOffset[(offsetof(NetConnX360State,bSecure)==0x18a)?1:-1];
typedef char NetConnX360FriendSize[(sizeof(XONLINE_FRIEND)==0xc4)?1:-1];
typedef char NetConnX360OverlapOffset[(offsetof(NetConnX360State,FriendOverlapped)==0xe8)?1:-1];
typedef char NetConnX360NotifyOffset[(offsetof(NetConnX360State,hNotify)==0x104)?1:-1];
typedef char NetConnX360ReadyOffset[(offsetof(NetConnX360State,aFriendReady)==0x191)?1:-1];
typedef char NetConnX360FriendsOffset[(offsetof(NetConnX360State,aFriends)==0x198)?1:-1];
typedef char NetConnX360CountOffset[(offsetof(NetConnX360State,aFriendCount)==0x133d8)?1:-1];
typedef char NetConnX360EnumOffset[(offsetof(NetConnX360State,hFriendEnumerator)==0x133ec)?1:-1];
typedef char NetConnX360LockOffset[(offsetof(NetConnX360State,CloseLock)==0x13408)?1:-1];
typedef char NetConnX360DeferredOffset[(offsetof(NetConnX360State,hDeferredClose)==0x13424)?1:-1];
typedef char NetConnX360StateSize[(sizeof(NetConnX360State)==0x13430)?1:-1];

extern NetConnX360State* gNetConnX360;

// Internal fragments of the original singleton. The public owner must initialize
// listener, lock, worker and deferred-handle sentinel and call the refresh from idle.
s32 NetConnX360FriendsStatus(NetConnX360State* pState, s32 iUser, void* pBuffer, s32 iBufSize);
void NetConnX360RefreshFriends(NetConnX360State* pState, u32 uTick);
void NetConnX360CloseWorker(NetConnX360State* pState);
DWORD WINAPI NetConnX360CloseThread(LPVOID pContext);
void NetConnX360Idle(void* pState, u32 uTick);
s32 NetConnX360AddressStatus(NetConnX360State* pState, u32 uAddressStatus);
void NetConnX360RefreshUsers(NetConnX360State* pState);
s32 NetConnX360UpdateUserInfo(NetConnX360State* pState);

#endif
