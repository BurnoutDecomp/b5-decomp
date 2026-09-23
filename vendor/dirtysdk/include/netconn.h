#ifndef DIRTYSDK_NETCONN_H
#define DIRTYSDK_NETCONN_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/netconn.h
// NetConn brings the platform network stack up and down, reports its state through
// four-character status selectors, and pumps every DirtySDK module that registered an idle
// handler (NetConnIdle -> each handler, at most every 5 ms).
// Bodies (PC backend): ../src/pc/netconnpc.cpp.
//
// Status selectors the game uses:
//   'onln'  1 while connected to the network service, else 0
//   'conn'  connection state word; its top byte is '-' after a failure ('-dup' = signed in
//           elsewhere), '+' while up
//   'plug'  1 while the network cable is connected
//   'gtag'  copy the gamertag of user iData into pBuf (>= 16 bytes); returns 1 on success
//   'open'  1 while NetConnStartup has run and NetConnShutdown has not

// Platform configuration record (unused by this platform; the game passes none).
typedef struct NetConfigRecT NetConfigRecT;

// Idle handler: called with the registration's data pointer and the current tick (ms).
typedef void (NetConnIdleProcT)(void* pData, u32 uTick);

#ifdef __cplusplus
extern "C" {
#endif

// Bring the stack up. pParams is the option string ("-nosecure" or ""). Returns 0.
s32 NetConnStartup(const char* pParams);

// Tear the stack down: drops every idle handler; uShutdownFlags bit 1 keeps the connection
// up. Returns 0, or -1 when NetConnStartup never ran.
s32 NetConnShutdown(u32 uShutdownFlags);

// Start connecting to the network service named by pOption (a hex service id, "dev" or "prod").
// Returns 1 when the connect started, a negative value when it could not.
s32 NetConnConnect(const NetConfigRecT* pConfig, const char* pOption);

// Drop the service connection ('onln' becomes 0). Returns 0.
s32 NetConnDisconnect(void);

// Configuration controls ('frrt' connect timeout, 'xlsp', 'serv' service id). Returns 0, or -1
// when NetConnStartup never ran.
s32 NetConnControl(s32 iControl, s32 iValue, s32 iValue2, void* pValue, void* pValue2);

// Status query; see the selector list above.
s32 NetConnStatus(s32 iKind, s32 iData, void* pBuf, s32 iBufSize);

// Pump the stack and every registered idle handler.
void NetConnIdle(void);

// "$" followed by the 12 hex digits of the adapter address; "" when it is unknown.
const char* NetConnMAC(void);

// Register / remove an idle handler (32 slots). Return 0, or -1 when full / not registered.
s32 NetConnIdleAdd(NetConnIdleProcT* pProc, void* pData);
s32 NetConnIdleDel(NetConnIdleProcT* pProc, void* pData);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_NETCONN_H
