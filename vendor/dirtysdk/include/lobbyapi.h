#ifndef DIRTYSDK_LOBBYAPI_H
#define DIRTYSDK_LOBBYAPI_H

#include "types.hpp"
#include "dirtyaddr.h"      // DirtyAddrT
#include "lobbydisplist.h"  // DispListRef

#include <stddef.h>         // offsetof

// DirtySDK 5.5.3 - core/include/lobbyapi.h
// The lobby connection: status / info queries, asynchronous requests, per-type event
// callbacks, the lobby display lists and the record types the game reads out of them.
// PC bodies: ../src/pc/lobbyapipc.cpp (offline = no server; LAN = the PC LAN lobby).
//
// The record layouts are the console build's (32-bit, big-endian on the console): persona
// names are 16 bytes on this platform, so LobbyApiUserT is 0x234 bytes, LobbyApiPlayerT 0xA4
// and LobbyApiPlayT 0x858. None of them holds a pointer, so every offset is the same on the
// host; the static_asserts below pin them.

struct LobbyApiRefT;   // opaque

// Callback types (the second argument of LobbyApiSetCallback).
enum LobbyApiCBTypeE
{
    LOBBYAPI_CBTYPE_INVALID = -1,
    LOBBYAPI_CBTYPE_RESP    = 0,   // request responses
    LOBBYAPI_CBTYPE_CHAT    = 1,   // chat / broadcast messages
    LOBBYAPI_CBTYPE_DISP    = 2,   // display-list changes
    LOBBYAPI_CBTYPE_CONN    = 3,   // connection state ('conn', 'disc')
    LOBBYAPI_CBTYPE_EVNT    = 4,   // lobby events ('game', 'play', 'kick', 'user', ...)
    LOBBYAPI_CBTYPE_IDLE    = 5,   // called once per LobbyApiUpdate
    LOBBYAPI_CBTYPE_TOTAL   = 6
};

// A lobby message handed to a callback: the request/event kind fourcc, the result code
// (0 or an error fourcc for responses) and the tagfield payload text.
struct LobbyApiMsgT
{
    s32         id;      // +0x00  request id (responses) / callback id
    s32         type;    // +0x04  LobbyApiCBTypeE
    s32         kind;    // +0x08  request / event fourcc
    s32         code;    // +0x0C  result code
    const char* pData;   // +0x10  tagfield payload (never null)
    void*       pMisc;   // +0x14
};

// Callback TYPE: (lobby ref, message, user data). Call sites use `LobbyApiCallbackT*`.
typedef void (LobbyApiCallbackT)(LobbyApiRefT* pLobbyApi, LobbyApiMsgT* pMsg, void* pUserData);

struct LobbyApiColorT
{
    unsigned char uRed;
    unsigned char uGreen;
    unsigned char uBlue;
    unsigned char uAlpha;
};

// A lobby user record ('self' status select, 564 bytes).
struct LobbyApiUserT
{
    s32            ident;          // +0x000
    s32            flags;          // +0x004
    char           name[16];       // +0x008
    char           ping[8];        // +0x018
    u32            addr;           // +0x020
    s32            rank;           // +0x024
    char           stat[260];      // +0x028
    char           aux[132];       // +0x12C
    s32            game;           // +0x1B0  ident of the game the user is in, 0 when none
    u32            attr;           // +0x1B4
    LobbyApiColorT Color;          // +0x1B8
    u32            uLevel;         // +0x1BC
    u32            uMedals;        // +0x1C0
    s32            iUserSetIdent;  // +0x1C4
    u32            uHwFlags;       // +0x1C8
    s32            iReputation;    // +0x1CC
    DirtyAddrT     MachineAddr;    // +0x1D0
    u32            uLocalAddr;     // +0x210
    u32            uLocality;      // +0x214
    char           strClubID[20];  // +0x218
    char           strClubTag[8];  // +0x22C
};

// One player of a play record.
struct LobbyApiPlayerT
{
    s32  iIdent;              // +0x00
    char strPers[16];         // +0x04
    u32  uAddr;               // +0x14
    u32  uLocalAddr;          // +0x18
    char strMachineAddr[64];  // +0x1C  DirtyAddrT text
    s32  iPartition;          // +0x5C
    u32  uFlags;              // +0x60
    s32  iPresence;           // +0x64
    char strParams[60];       // +0x68
};

struct LobbyApiPartitionT
{
    s32  iSize;
    char strParams[36];
};

// A game ("play") record: the lobby's game list entries and the game-manager's current game.
struct LobbyApiPlayT
{
    s32                iIdent;             // +0x000
    char               strName[36];        // +0x004
    char               strHost[16];        // +0x028
    char               strGPSHost[16];     // +0x038
    s32                iRoom;              // +0x048
    char               strSess[128];       // +0x04C
    char               strParams[260];     // +0x0CC
    char               strPlatParams[16];  // +0x1D0
    u32                uCustflags;         // +0x1E0
    u32                uSysflags;          // +0x1E4
    s8                 iMinsize;           // +0x1E8
    s8                 iMaxsize;           // +0x1E9
    s8                 iGameMode;          // +0x1EA
    u8                 uPad;               // +0x1EB
    u32                uSeed;              // +0x1EC
    u32                uWhen;              // +0x1F0
    u16                uGamePort;          // +0x1F4
    u16                uVoipPort;          // +0x1F6
    char               strAuth[64];        // +0x1F8
    s32                iNumPartitions;     // +0x238
    LobbyApiPartitionT aPartitions[2];     // +0x23C
    s32                iCount;             // +0x28C
    s32                iPrivSlots;         // +0x290
    LobbyApiPlayerT    aOpponents[9];      // +0x294
};

// A userset record.
struct LobbyApiUserSetT
{
    s32  iIdent;          // +0x00
    s32  iType;           // +0x04
    u32  uSysFlags;       // +0x08
    u32  uCustFlags;      // +0x0C
    char strOwner[16];    // +0x10
    s32  iSize;           // +0x20
    s32  iCount;          // +0x24
    char strName[36];     // +0x28
    char strDesc[68];     // +0x4C
    char strParams[68];   // +0x90
};

#ifdef __cplusplus
static_assert(sizeof(LobbyApiUserT) == 0x234, "LobbyApiUserT");
static_assert(offsetof(LobbyApiUserT, game) == 0x1B0, "LobbyApiUserT::game");
static_assert(offsetof(LobbyApiUserT, MachineAddr) == 0x1D0, "LobbyApiUserT::MachineAddr");
static_assert(sizeof(LobbyApiPlayerT) == 0xA4, "LobbyApiPlayerT");
static_assert(offsetof(LobbyApiPlayerT, strMachineAddr) == 0x1C, "LobbyApiPlayerT::strMachineAddr");
static_assert(offsetof(LobbyApiPlayerT, strParams) == 0x68, "LobbyApiPlayerT::strParams");
static_assert(offsetof(LobbyApiPlayT, strSess) == 0x4C, "LobbyApiPlayT::strSess");
static_assert(offsetof(LobbyApiPlayT, uSysflags) == 0x1E4, "LobbyApiPlayT::uSysflags");
static_assert(offsetof(LobbyApiPlayT, uGamePort) == 0x1F4, "LobbyApiPlayT::uGamePort");
static_assert(offsetof(LobbyApiPlayT, iCount) == 0x28C, "LobbyApiPlayT::iCount");
static_assert(offsetof(LobbyApiPlayT, aOpponents) == 0x294, "LobbyApiPlayT::aOpponents");
static_assert(sizeof(LobbyApiPlayT) == 0x858, "LobbyApiPlayT");
static_assert(sizeof(LobbyApiUserSetT) == 0xD4, "LobbyApiUserSetT");
#endif

// Display-list types LobbyApiListAlloc / Flush / Free accept.
#define LOBBYAPI_LIST_GAMES          (3)    // LobbyApiPlayT records
#define LOBBYAPI_LIST_USERSET_USERS  (10)
#define LOBBYAPI_LIST_MAX            (10)

#ifdef __cplusplus
extern "C" {
#endif

// Create / destroy the lobby ref. pTagField carries the client identity (VERS, SKU, SLUS,
// LOC, MID); the debug hooks and memory group may be 0.
LobbyApiRefT* LobbyApiCreate(const char* pTagField, void* pDebugRef,
                             void (*pDebugFnc)(void* pDebugRef, const char* pText), s32 iRefMemGroup);
void LobbyApiDestroy(LobbyApiRefT* pLobbyApi);

// Control selects ('rtim' request timeout in ms). Returns 0, or -1 for an unknown select.
s32 LobbyApiControl(LobbyApiRefT* pLobbyApi, s32 iKind, s32 iValue);

// Pump: completes queued requests and delivers queued events. Every callback runs from here.
void LobbyApiUpdate(LobbyApiRefT* pLobbyApi);

void LobbyApiSuspend(LobbyApiRefT* pLobbyApi);
void LobbyApiResume(LobbyApiRefT* pLobbyApi);

// Drop the lobby connection. iCode is the disconnect reason (0 == none).
void LobbyApiDisconnect(LobbyApiRefT* pLobbyApi, s32 iCode);

// Register a callback for one LobbyApiCBTypeE (passed as its integer value); returns the
// callback id (>= 0) or -1 when the table is full.
s32 LobbyApiSetCallback(LobbyApiRefT* pLobbyApi, s32 iType, LobbyApiCallbackT* pCallback, void* pUserData);
// Clear a callback registered by LobbyApiSetCallback; returns 0, -1 for an unknown id.
s32 LobbyApiClearCallback(LobbyApiRefT* pLobbyApi, s32 iCallbackID);

// Issue request iKind with tagfield pRequest; pCallback gets the response on a later
// LobbyApiUpdate. Returns the request id (> 0), or -1 when the request cannot be sent.
s32 LobbyApiRequestCB(LobbyApiRefT* pLobbyApi, s32 iKind, const char* pRequest,
                      LobbyApiCallbackT* pCallback, void* pUserData);
// Drop the pending response of request iId; returns 0.
s32 LobbyApiCancelCB(LobbyApiRefT* pLobbyApi, s32 iId);

// Status selects: 'logn' 'stat' 'susp' return a value; 'self' (564 bytes) 'part' 'pers'
// fill pBuf and return 0; an unknown select or a bad buffer returns -1.
s32 LobbyApiStatus(LobbyApiRefT* pLobbyApi, s32 iSelect, void* pBuf, s32 iBufLen);

// Info selects ('conf' 'perd' 'autd' 'lkey' 'list'): pointer to the record, 0 when unknown.
const void* LobbyApiInfo(LobbyApiRefT* pLobbyApi, s32 iSelect);

// The userset record of the userset iUserSetIdent, 0 when it is not the current one.
LobbyApiUserSetT* LobbyApiGetUserSetInfoByIdent(LobbyApiRefT* pLobbyApi, s32 iUserSetIdent);

// Display lists (iListType 0..LOBBYAPI_LIST_MAX).
DispListRef* LobbyApiListAlloc(LobbyApiRefT* pLobbyApi, s32 iListType, LobbyApiCallbackT* pCallback,
                               void* pUserData);
void LobbyApiListFlush(LobbyApiRefT* pLobbyApi, s32 iListType);
void LobbyApiListFree(LobbyApiRefT* pLobbyApi, s32 iListType, DispListRef* pDisp);

// Decode a tagfield game record (IDENT NAME HOST ... OPPO<n> ... SESS) into pPlay.
void LobbyApiExtractPlayRecord(LobbyApiPlayT* pPlay, const char* pData);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYAPI_H
