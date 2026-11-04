#ifndef DIRTYSDK_X360_LOBBYAPI_INTERNAL_H
#define DIRTYSDK_X360_LOBBYAPI_INTERNAL_H

#include "lobbyapi.h"
#include "lobbydisplist.h"

#include <stddef.h>

struct ProtoAriesRefT;
struct PingManagerRefT;
struct LobbyApiXenonRefT;
struct LobbyListRefT;
struct HasherRef;

struct LobbyApiRoomCriteriaT
{
    u32 uFlags;
    u32 uMask;
    s32 iNumVals;
    u32 uVals[8];
};

struct LobbyGlobalCBRefT
{
    s32                 eType;
    LobbyApiCallbackT*  pCallback;
    void*               pUserData;
};

struct LobbyCBQueueRefT
{
    s32                 iId;
    s32                 iKind;
    s32                 iTick;
    s32                 iSeqn;
    LobbyApiCallbackT*  pCallback;
    void*               pUserData;
};

struct LobbyApiServerStatT
{
    s32 iEnabled;
    s32 iUpdate;
    u32 uUsersInLobby;
    u32 uUsersInRooms;
    u32 uUsersInGames;
    u32 uGamesInProgress;
    u32 uGamesCreated;
    u32 uGamesCompleted;
};

struct LobbyDispListT
{
    HasherRef*          pHash;
    DispListRef*        pDisp;
    LobbyApiCallbackT*  pCallback;
    void*               pUserData;
    u32                  uLast;
};

struct LobbyListRefT
{
    s32             iMemGroup;
    LobbyDispListT  Lists[11];
};

// The lobby room record begins with the DecFIGS fields below. ARTIST reads
// popul at +0x10 in LobbyApiStatus (0x82B1ABAC) and name at +0x24 in
// LobbyApiListFindByName (0x82B22E7C).
struct LobbyApiRoomRecordPrefixT
{
    s32 iIdent;
    s32 iCheck;
    s32 iFlags;
    s32 iLimit;
    s32 iPopulation;
    s32 iGamePopulation;
    s32 iGameCount;
    char strPopulation[4];
    char strPublic[4];
    char strName[36];
};

// Private X360 SDK layout. The ARTIST constructor allocates 8,388 bytes and
// stores LobbyApiXenonCreate at +0x10; the status/callback/list code pins the
// remaining offsets below. DecFIGS supplies member order and names.
struct LobbyApiRefT
{
    ProtoAriesRefT*      net;                // +0x000
    PingManagerRefT*     pPingManager;       // +0x004
    s32                  iMemGroup;          // +0x008
    s32                  iRefMemGroup;       // +0x00c
    LobbyApiXenonRefT*   pXenon;             // +0x010
    s32                  state;             // +0x014
    u32                  bSuspended;        // +0x018
    u32                  bEnableDispElemCb; // +0x01c
    u32                  uTimeout;          // +0x020
    u32                  uUpdated;          // +0x024
    u32                  nextServerPingTime;// +0x028
    u32                  serverPingLatency; // +0x02c
    s32                  nFailedServerPings;// +0x030
    u32                  uNextUserPing;     // +0x034
    u32                  uConnTimeout;      // +0x038
    u32                  uIdleTimeout;      // +0x03c
    u32                  uTrnsTimeout;      // +0x040
    char                 skey[16];          // +0x044
    char                 strVersion[256];   // +0x054
    char                 strAuthResult[512];// +0x154
    char                 strPersResult[1024];// +0x354
    char                 strPartition[64];  // +0x754
    char                 strLKey[64];       // +0x794
    char                 strConfig[4100];   // +0x7d4
    char                 acctname[20];      // +0x17d8
    LobbyApiUserT        user;              // +0x17ec
    s32                  roomidx;           // +0x1a20
    s32                  roomflags;         // +0x1a24
    char                 roomname[36];      // +0x1a28
    LobbyApiRoomCriteriaT roomcrit;          // +0x1a4c
    LobbyListRefT*       pListRef;          // +0x1a78
    s32                  lobbyport;         // +0x1a7c
    u32                  lobbyaddr;         // +0x1a80
    s32                  rdirport;          // +0x1a84
    u32                  sess;              // +0x1a88
    char                 mask[64];          // +0x1a8c
    u32                  extaddr;           // +0x1acc
    u32                  ouraddr;           // +0x1ad0
    s32                  rankmore;          // +0x1ad4
    s32                  snapchan;          // +0x1ad8
    s32                  snapopen;          // +0x1adc
    s32                  ranknumb;          // +0x1ae0
    LobbyGlobalCBRefT    callbacks[32];      // +0x1ae4
    LobbyCBQueueRefT     CallbackQueue[32];  // +0x1c64
    s32                  iQueueTail;         // +0x1f64
    s32                  numpingreq;        // +0x1f68
    void*                debugref;          // +0x1f6c
    void (*debugptr)(void*, const char*);   // +0x1f70
    u32 (*debugflt)(void*);              // +0x1f74 (ARTIST calls with debugref only)
    s32                  iCallbackID;       // +0x1f78
    s32                  iCallbackSeqn;     // +0x1f7c
    s32                  iAutoRoomID;       // +0x1f80
    u32                  uServTime;         // +0x1f84
    u32                  uServLastTick;     // +0x1f88
    u32                  uServTickSum;      // +0x1f8c
    LobbyApiUserSetT     UserSet;           // +0x1f90
    LobbyApiServerStatT  serverStats;       // +0x2064
    char                 strServerName[64]; // +0x2084
};

CGS_STATIC_ASSERT(sizeof(LobbyApiRoomCriteriaT) == 0x2c, "LobbyApiRoomCriteriaT");
CGS_STATIC_ASSERT(sizeof(LobbyGlobalCBRefT) == 0x0c, "LobbyGlobalCBRefT");
CGS_STATIC_ASSERT(sizeof(LobbyCBQueueRefT) == 0x18, "LobbyCBQueueRefT");
CGS_STATIC_ASSERT(sizeof(LobbyApiServerStatT) == 0x20, "LobbyApiServerStatT");
CGS_STATIC_ASSERT(sizeof(LobbyDispListT) == 0x14, "LobbyDispListT");
CGS_STATIC_ASSERT(sizeof(LobbyListRefT) == 0xe0, "LobbyListRefT");
CGS_STATIC_ASSERT(offsetof(LobbyApiRoomRecordPrefixT, iPopulation) == 0x10,
                  "LobbyApiRoomRecordPrefixT::iPopulation");
CGS_STATIC_ASSERT(offsetof(LobbyApiRoomRecordPrefixT, strName) == 0x24,
                  "LobbyApiRoomRecordPrefixT::strName");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, pXenon) == 0x10, "LobbyApiRefT::pXenon");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, strVersion) == 0x54, "LobbyApiRefT::strVersion");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, strPartition) == 0x754, "LobbyApiRefT::strPartition");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, pListRef) == 0x1a78, "LobbyApiRefT::pListRef");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, acctname) == 0x17d8, "LobbyApiRefT::acctname");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, user) == 0x17ec, "LobbyApiRefT::user");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, callbacks) == 0x1ae4, "LobbyApiRefT::callbacks");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, CallbackQueue) == 0x1c64, "LobbyApiRefT::CallbackQueue");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, iQueueTail) == 0x1f64, "LobbyApiRefT::iQueueTail");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, serverStats) == 0x2064, "LobbyApiRefT::serverStats");
CGS_STATIC_ASSERT(sizeof(LobbyApiRefT) == 8388, "LobbyApiRefT");
CGS_STATIC_ASSERT(offsetof(LobbyCBQueueRefT, iId) == 0x00, "LobbyCBQueueRefT::iId");
CGS_STATIC_ASSERT(offsetof(LobbyCBQueueRefT, iKind) == 0x04, "LobbyCBQueueRefT::iKind");
CGS_STATIC_ASSERT(offsetof(LobbyCBQueueRefT, iTick) == 0x08, "LobbyCBQueueRefT::iTick");
CGS_STATIC_ASSERT(offsetof(LobbyCBQueueRefT, iSeqn) == 0x0c, "LobbyCBQueueRefT::iSeqn");
CGS_STATIC_ASSERT(offsetof(LobbyCBQueueRefT, pCallback) == 0x10,
                  "LobbyCBQueueRefT::pCallback");
CGS_STATIC_ASSERT(offsetof(LobbyCBQueueRefT, pUserData) == 0x14,
                  "LobbyCBQueueRefT::pUserData");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, uConnTimeout) == 0x38,
                  "LobbyApiRefT::uConnTimeout");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, lobbyport) == 0x1a7c,
                  "LobbyApiRefT::lobbyport");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, lobbyaddr) == 0x1a80,
                  "LobbyApiRefT::lobbyaddr");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, rdirport) == 0x1a84,
                  "LobbyApiRefT::rdirport");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, snapchan) == 0x1ad8,
                  "LobbyApiRefT::snapchan");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, snapopen) == 0x1adc,
                  "LobbyApiRefT::snapopen");
CGS_STATIC_ASSERT(offsetof(LobbyApiRefT, strServerName) == 0x2084,
                  "LobbyApiRefT::strServerName");

extern "C"
{
    LobbyApiUserT* LobbyApiGetUserInfo(LobbyApiRefT* pLobbyApi, const char* pUserName);
    void LobbyApiExtractUserRecord(LobbyApiUserT* pUser, const char* pData);
}

#endif // DIRTYSDK_X360_LOBBYAPI_INTERNAL_H
