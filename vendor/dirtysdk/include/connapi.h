#ifndef DIRTYSDK_CONNAPI_H
#define DIRTYSDK_CONNAPI_H

#include "types.hpp"
#include "dirtyaddr.h"     // DirtyAddrT
#include "netgamelink.h"   // NetGameLinkRefT

// DirtySDK 5.5.3 - core/include/connapi.h
// ConnApi keeps the peer list of the game session the local player is in and opens one game
// connection (a NetGameLink) to every other member. The game manager hands it the members
// (ConnApiHost / ConnApiConnect with the whole list, ConnApiAddClient / ConnApiRemoveClient
// afterwards); the game reads the list back through ConnApiGetClientList and is told about every
// connection status change through the ConnApiCreate callback.
// Bodies (PC backend): ../src/pc/connapipc.cpp.
//
// The types are declared in CgsNetwork::DirtySock (the game's name for them) and aliased at
// global scope with the DirtySDK names; both spellings are the same type.
//
// Client entry layout. Offsets are the console's; the game reads every field by name.
//   ConnApiUserInfoT (0x90): the member as the game manager describes it.
//   ConnApiClientT (0xBC): user info, then the game and voice connection states, then the
//   connection objects.

namespace CgsNetwork
{
    namespace DirtySock
    {
        // Connection status (GameInfo.eStatus / VoipInfo.eStatus, and the status pair a
        // callback reports).
        enum ConnApiConnStatusE
        {
            CONNAPI_STATUS_INIT = 0,   // not started
            CONNAPI_STATUS_CONN = 1,   // connecting
            CONNAPI_STATUS_MNGL = 2,   // demangling
            CONNAPI_STATUS_ACTV = 3,   // active: the link is up
            CONNAPI_STATUS_CLSE = 4,   // closing
            CONNAPI_STATUS_DISC = 5,   // disconnected
            CONNAPI_STATUS_DEST = 6    // connection destroyed
        };

        // Which connection a callback reports.
        enum ConnApiCbTypeE
        {
            CONNAPI_CBTYPE_GAMEEVENT = 0,
            CONNAPI_CBTYPE_VOIPEVENT = 1
        };

        // One connection of a client (game or voice).
        struct ConnApiConnInfoT
        {
            u16 uLocalPort;            // +0x00
            u16 uMnglPort;             // +0x02  port after demangling
            u8  bDemangling;           // +0x04  demangling in progress
            u8  uConnFlags;            // +0x05  bit 1: routed through the game server
            u8  eStatus;               // +0x06  ConnApiConnStatusE
            u8  uGameServerFallback;   // +0x07  set when the connection fell back to the game server
        };

        // A session member as the game manager describes it.
        struct ConnApiUserInfoT
        {
            u32        uAddr;          // +0x00  external address (0 until the connection resolves it)
            u32        uLocalAddr;     // +0x04  address behind the NAT
            u32        uClientId;      // +0x08  client id (ConnApi assigns one when 0)
            u16        uLocalGamePort; // +0x0C  0 = the session's game port
            u16        uLocalVoipPort; // +0x0E  0 = the session's voice port
            DirtyAddrT DirtyAddr;      // +0x10  machine address ("$" + 16 hex digits of the XUID)
            char       strName[32];    // +0x50  persona
            char       strTunnelKey[32];  // +0x70  short key built from the machine address
        };

        struct ConnApiClientT
        {
            ConnApiUserInfoT UserInfo;       // +0x00
            ConnApiConnInfoT GameInfo;       // +0x90
            ConnApiConnInfoT VoipInfo;       // +0x98
            void*            pGameUtilRef;   // +0xA0  connection negotiator (unused on this platform)
            NetGameLinkRefT* pGameLinkRef;   // +0xA4  the game link; set while the game connection is up
            u32              uReservedA8;    // +0xA8
            u32              uReservedAC;    // +0xAC
            s32              iTunnelId;      // +0xB0
            s16              iVoipConnId;    // +0xB4  voice connection id, -1 when none
            u16              uConnFlags;     // +0xB6  which connections this client gets (bit 0 game, bit 1 voice)
            u16              uFlags;         // +0xB8  bit 0: removal pending
            u16              uPadBA;         // +0xBA
        };

        // The session's member list; Clients[] holds iMaxClients entries.
        struct ConnApiClientListT
        {
            s32            iNumClients;      // +0x00
            s32            iMaxClients;      // +0x04
            ConnApiClientT Clients[1];       // +0x08
        };

        // Status-change callback payload.
        struct ConnApiCbInfoT
        {
            s32 iClientIndex;   // +0x00  index into the client list
            s32 eType;          // +0x04  ConnApiCbTypeE
            s32 eOldStatus;     // +0x08  ConnApiConnStatusE
            s32 eNewStatus;     // +0x0C  ConnApiConnStatusE
        };

        struct ConnApiRefT;     // opaque (defined by the backend)

        typedef void (ConnApiCallbackT)(ConnApiRefT* pConnApi, ConnApiCbInfoT* pCbInfo, void* pUserData);
    }
}

#if !defined(_WIN64)
static_assert(sizeof(CgsNetwork::DirtySock::ConnApiUserInfoT) == 0x90, "ConnApiUserInfoT is 0x90 console bytes");
static_assert(sizeof(CgsNetwork::DirtySock::ConnApiClientT) == 0xBC, "ConnApiClientT is 0xBC console bytes");
#endif

typedef CgsNetwork::DirtySock::ConnApiRefT        ConnApiRefT;
typedef CgsNetwork::DirtySock::ConnApiConnInfoT   ConnApiConnInfoT;
typedef CgsNetwork::DirtySock::ConnApiUserInfoT   ConnApiUserInfoT;
typedef CgsNetwork::DirtySock::ConnApiClientT     ConnApiClientT;
typedef CgsNetwork::DirtySock::ConnApiClientListT ConnApiClientListT;
typedef CgsNetwork::DirtySock::ConnApiCbInfoT     ConnApiCbInfoT;
typedef CgsNetwork::DirtySock::ConnApiCallbackT   ConnApiCallbackT;

#ifdef __cplusplus
extern "C" {
#endif

// Allocate the module. iGamePort / iMaxClients size the session; pCallback (null = log only)
// receives every connection status change. Registers ConnApiUpdate as a NetConn idle handler.
ConnApiRefT* ConnApiCreate(s32 iGamePort, s32 iMaxClients, ConnApiCallbackT* pCallback, void* pUserData);

// Close every connection and free the module.
void ConnApiDestroy(ConnApiRefT* pConnApi);

// The local player came online: remember the game name, persona and machine address.
void ConnApiOnline(ConnApiRefT* pConnApi, const char* pGameName, const char* pSelfName, const DirtyAddrT* pSelfAddr);

// Start a session as host / as a member with the given members (pClientList is iNumClients
// ConnApiUserInfoT entries, the local player among them). iSessFlags bit 0 = ranked. The fourth
// argument is not used. Returns 0, or a negative value when a session is already running.
s32 ConnApiHost(ConnApiRefT* pConnApi, ConnApiUserInfoT* pClientList, s32 iNumClients, s32 iUnused, s32 iSessFlags);
s32 ConnApiConnect(ConnApiRefT* pConnApi, ConnApiUserInfoT* pClientList, s32 iNumClients, s32 iUnused, s32 iSessFlags);

// Add a member to / remove a member from the running session. RemoveClient finds the member
// by name, or by iClientIndex when pClientName is null. Return 0, or a negative value.
s32 ConnApiAddClient(ConnApiRefT* pConnApi, ConnApiUserInfoT* pUserInfo);
s32 ConnApiRemoveClient(ConnApiRefT* pConnApi, const char* pClientName, s32 iClientIndex);

// Start / stop the game on the session (arbitration); connections stay up.
s32  ConnApiStart(ConnApiRefT* pConnApi, s32 iFlags);
void ConnApiStop(ConnApiRefT* pConnApi);

// Close every connection and leave the session.
s32 ConnApiDisconnect(ConnApiRefT* pConnApi);

// The session's member list (never null for a live module).
const ConnApiClientListT* ConnApiGetClientList(ConnApiRefT* pConnApi);

// Controls: 'cbfp' / 'cbup' (pValue = the new callback / user data), 'peer', 'mwid', 'mngl',
// 'gsv2', 'gsrv', 'gprt', 'vprt', 'slot', 'sflg', 'sess', 'nonc', 'skil', 'type', 'lbuf',
// 'time', 'tunl', 'hsti', 'rcbk', 'migr', 'minp', 'mout'. Returns 0, or -1 for an unknown selector.
s32 ConnApiControl(ConnApiRefT* pConnApi, s32 iControl, s32 iValue, s32 iValue2, const void* pValue);

// Status: 'cbfp' / 'cbup' (copy the callback / user data pointer into pBuf), 'gprt', 'vprt',
// 'self', 'host', 'ingm' (1 while a session runs), 'idle', 'fail', 'lbuf', 'mwid', 'peer',
// 'sess', 'sflg', 'type', 'tunl', 'minp', 'mout'. -1 for an unknown selector.
//
// Session events reach the callback with eType 2: on the host, 0 -> 3 on the update after
// ConnApiHost (session created); 3 -> 5 on the update after ConnApiDisconnect (torn down).
s32 ConnApiStatus(ConnApiRefT* pConnApi, s32 iSelect, void* pBuf, s32 iBufSize);

// Run the module (also run from the NetConn idle handler).
void ConnApiUpdate(ConnApiRefT* pConnApi);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_CONNAPI_H
