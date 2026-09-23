#ifndef DIRTYSDK_LOBBYSTATBOOK_H
#define DIRTYSDK_LOBBYSTATBOOK_H

#include "types.hpp"
#include "lobbyapi.h"   // LobbyApiRefT

// DirtySDK 5.5.3 - lobby/include/lobbystatbook.h
// The "statbook" lobby module: downloads the list of stat views (FetchViewInfo), the column
// layout of one view (FetchView) and a player's stat row for the selected view into one of
// the module's slots (Fetch). The CGS player-info component is the only caller.
// PC body: ../src/pc/lobbymiscpc.cpp.

// Opaque statbook module handle.
struct LobbyStatbookT;

// Per-row info record returned by LobbyStatbookRowInfo (0x114 bytes, one per view row).
// The CGS player-info component reads only the row type at +0x110; the preceding bytes
// are the row's name / description text.
struct LobbyStatbookRowInfoT
{
    u8  _reserved[0x110];   // +0x000  row name / description text
    s32 iType;              // +0x110  row type
};

// Result codes (the CGS player-info component maps each one to a server-interface error).
#define LOBBYSTATBOOK_ERROR_MISC             (-1)
#define LOBBYSTATBOOK_ERROR_INVALID_VIEW     (-2)
#define LOBBYSTATBOOK_ERROR_PLAYER_NOT_FOUND (-3)
#define LOBBYSTATBOOK_ERROR_VIEW_NOT_SELECTED (-4)
#define LOBBYSTATBOOK_ERROR_BAD_SLOT         (-5)
#define LOBBYSTATBOOK_ERROR_BUSY             (-6)
#define LOBBYSTATBOOK_ERROR_TIMEOUT          (-7)

#ifdef __cplusplus
extern "C" {
#endif

// Create a statbook module with uSlots player slots (1..1024; anything else becomes 10).
LobbyStatbookT* LobbyStatbookCreate(LobbyApiRefT* pLobbyApi, u32 uSlots);

// Destroy a statbook module previously created with LobbyStatbookCreate.
void LobbyStatbookDestroy(LobbyStatbookT* pStatbook);

// Fetch the stat row of persona pPersona (the local user when empty) for the selected view
// into slot uSlot. 0 when the request was sent, otherwise a LOBBYSTATBOOK_ERROR_* value.
s32 LobbyStatbookFetch(LobbyStatbookT* pStatbook, u32 uSlot, const u8* pPersona);

// Select the view pView and fetch its column layout. 0 when the request was sent (or the
// view is already selected), otherwise a LOBBYSTATBOOK_ERROR_* value.
s32 LobbyStatbookFetchView(LobbyStatbookT* pStatbook, const u8* pView);

// Fetch the list of available views. 0 when the request was sent (or the list is present),
// otherwise a LOBBYSTATBOOK_ERROR_* value.
s32 LobbyStatbookFetchViewInfo(LobbyStatbookT* pStatbook);

// Slot fetch status: > 0 complete, 0 pending, < 0 a LOBBYSTATBOOK_ERROR_* value.
s32 LobbyStatbookStatus(LobbyStatbookT* pStatbook, u32 uSlot);

// View fetch status: the view's row count once present, 0 pending, < 0 an error.
s32 LobbyStatbookViewStatus(LobbyStatbookT* pStatbook);

// View list fetch status: the view count once present, 0 pending, < 0 an error.
s32 LobbyStatbookViewInfoStatus(LobbyStatbookT* pStatbook);

// Number of views in the downloaded view list.
s32 LobbyStatbookViewCount(LobbyStatbookT* pStatbook);

// Stat text of row uRow in slot uSlot, copied into pBuffer when it is not NULL. Returns the
// module's copy of the text, NULL when the slot has no such row.
const char* LobbyStatbookRowData(LobbyStatbookT* pStatbook, u32 uSlot, u32 uRow, char* pBuffer, s32 iBuflen);

// Per-row info record of the selected view's row uRow (NULL when absent).
LobbyStatbookRowInfoT* LobbyStatbookRowInfo(LobbyStatbookT* pStatbook, u32 uRow);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYSTATBOOK_H
