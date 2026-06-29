#ifndef DIRTYSDK_LOBBYSTATBOOK_H
#define DIRTYSDK_LOBBYSTATBOOK_H

#include "types.hpp"
#include "lobbyapi.h"   // LobbyApiRefT

// DirtySDK 5.5.3 - core/include/lobbystatbook.h
// The "statbook" lobby module: fetches and caches a player's persistent statistics and
// the stat-view metadata. Only the prototypes the reconstructed CGS player-info
// component calls are declared here; signatures mirror the DirtySDK sources
// (lobbystatbook.c) as recovered from the X360 ARTIST call sites.

// Opaque statbook module handle.
struct LobbyStatbookT;

// Per-row info record returned by LobbyStatbookRowInfo. The CGS player-info component
// reads the row "type" field at +0x110 (recovered from the X360 asm @ 0x82879210,
// `lwz r11, 0x110(r3)`); the preceding bytes are SDK-internal and modelled as opaque
// reserved storage so the named iType field lands at the exact recovered offset.
struct LobbyStatbookRowInfoT
{
    u8  _reserved[0x110];   // +0x000  SDK-internal row data (not modelled)
    s32 iType;              // +0x110  row "type" the component reads back
};

#ifdef __cplusplus
extern "C" {
#endif

// Create a statbook module bound to the supplied lobby API ref. uiFlags selects the
// statbook category/mode. Returns the module handle (0 on failure).
LobbyStatbookT* LobbyStatbookCreate(LobbyApiRefT* pLobbyApi, u32 uiFlags);

// Destroy a statbook module previously created with LobbyStatbookCreate.
void LobbyStatbookDestroy(LobbyStatbookT* pStatbook);

// Begin fetching the stat rows for a player. uiFlags selects options; pacStatList is the
// (null-terminated) list of stat ids to fetch. Returns 0 on success or a negative error.
s32 LobbyStatbookFetch(LobbyStatbookT* pStatbook, u32 uiFlags, const u8* pacStatList);

// Begin fetching the metadata for a single stat view selected by pacView. Returns 0 on
// success or a negative error.
s32 LobbyStatbookFetchView(LobbyStatbookT* pStatbook, const u8* pacView);

// Begin fetching the stat-view info (the available views). Returns 0 on success or a
// negative error.
s32 LobbyStatbookFetchViewInfo(LobbyStatbookT* pStatbook);

// Poll the status of an in-flight row fetch (iWhich selects which request). Returns >0
// when complete, 0 while pending, <0 on error.
s32 LobbyStatbookStatus(LobbyStatbookT* pStatbook, s32 iWhich);

// Poll the status of an in-flight stat-view fetch. Returns >0 when complete, 0 while
// pending, <0 on error.
s32 LobbyStatbookViewStatus(LobbyStatbookT* pStatbook);

// Poll the status of an in-flight stat-view-info fetch. Returns >0 when complete, 0 while
// pending, <0 on error.
s32 LobbyStatbookViewInfoStatus(LobbyStatbookT* pStatbook);

// Number of stat views currently cached in the module.
s32 LobbyStatbookViewCount(LobbyStatbookT* pStatbook);

// Copy the data for stat row uiRow (request uiWhich) into pacBuffer (iBufLen bytes).
// Returns the number of bytes written (0 when the row is absent).
s32 LobbyStatbookRowData(LobbyStatbookT* pStatbook, u32 uiWhich, u32 uiRow,
                         char* pacBuffer, s32 iBufLen);

// Return the per-row info record for row uiRow (0 when absent).
LobbyStatbookRowInfoT* LobbyStatbookRowInfo(LobbyStatbookT* pStatbook, u32 uiRow);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYSTATBOOK_H
