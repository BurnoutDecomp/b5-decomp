#ifndef DIRTYSDK_LOBBYRANK_H
#define DIRTYSDK_LOBBYRANK_H

#include "types.hpp"
#include "lobbyapi.h"   // LobbyApiRefT, LobbyApiMsgT

// DirtySDK 5.5.3 - lobby/include/lobbyrank.h
// The "rank" lobby module: downloads the rank category / index / variation tree for a rank
// view (LobbyRankCategoryFetch), then one rank list (scoreboard) at a time for the selected
// variation (LobbyRankSelect + LobbyRankFetch), and answers column / row / cell queries over
// the downloaded list. The CGS rankings component is the only caller.
// PC body: ../src/pc/lobbymiscpc.cpp.

// Opaque rank module handle.
struct LobbyRankT;

// Fetch completion callback TYPE: (rank ref, completion message, user data). The message
// kind is 'cate' for a category fetch and 'list' for a rank list fetch; its code is 0 on
// success or one of the LOBBYRANK_ERROR_* values.
typedef void (LobbyRankCallbackT)(LobbyRankT* pRanker, LobbyApiMsgT* pMsg, void* pUserData);

// Result codes (the CGS rankings component maps each one to a server-interface error).
#define LOBBYRANK_ERROR_INVALID_CATEGORY         (-1)
#define LOBBYRANK_ERROR_INVALID_INDEX            (-2)
#define LOBBYRANK_ERROR_INVALID_VARIATION        (-3)
#define LOBBYRANK_ERROR_INVALID_VIEW             (-4)
#define LOBBYRANK_ERROR_LIST_NOT_DOWNLOADED      (-5)   // no category list
#define LOBBYRANK_ERROR_RANKINGS_NOT_DOWNLOADED  (-6)   // no scoreboard selected / no rank list
#define LOBBYRANK_ERROR_MISC                     (-7)
#define LOBBYRANK_ERROR_TIMEOUT                  (-8)
#define LOBBYRANK_ERROR_OUT_OF_MEMORY            (-9)
#define LOBBYRANK_ERROR_LOBBY_INVALID            (-10)  // the request could not be sent
#define LOBBYRANK_ERROR_DOWNLOAD_IN_PROGRESS     (-11)
#define LOBBYRANK_ERROR_INVALID_COL              (-12)
#define LOBBYRANK_ERROR_INVALID_ROW              (-13)

// LobbyRankSelect's category argument that selects a scoreboard directly by index /
// variation number, without a downloaded category tree.
#define LOBBYRANK_SELECT_DIRECT                  (-90)

#ifdef __cplusplus
extern "C" {
#endif

// Create / destroy a rank module bound to the lobby.
LobbyRankT* LobbyRankCreate(LobbyApiRefT* pLobby, s32 iCache);
void        LobbyRankDestroy(LobbyRankT* pRanker);

// Category tree for the rank view pView (at most 11 characters). Returns 0 when the request
// was sent (pCallback reports completion) or a LOBBYRANK_ERROR_* value.
s32         LobbyRankCategoryFetch(LobbyRankT* pRanker, const char* pView, LobbyRankCallbackT* pCallback,
                                   void* pUserData);
s32         LobbyRankCategoryCount(LobbyRankT* pRanker);
const char* LobbyRankCategoryName(LobbyRankT* pRanker, s32 iCategory, char* pBuffer, s32 iBuflen);
s32         LobbyRankIndexCount(LobbyRankT* pRanker, s32 iCategory);
const char* LobbyRankIndexName(LobbyRankT* pRanker, s32 iCategory, s32 iIndex, char* pBuffer, s32 iBuflen);
s32         LobbyRankVariationCount(LobbyRankT* pRanker, s32 iCategory, s32 iIndex);
const char* LobbyRankVariationName(LobbyRankT* pRanker, s32 iCategory, s32 iIndex, s32 iVariation,
                                   char* pBuffer, s32 iBuflen);
s32         LobbyRankVariationUserType(LobbyRankT* pRanker, s32 iCategory, s32 iIndex, s32 iVariation);

// Select the scoreboard later fetches and queries address. 0 on success.
s32         LobbyRankSelect(LobbyRankT* pRanker, s32 iCategory, s32 iIndex, s32 iVariation);

// Drop the selected scoreboard's rank list (bAll != 0: every cached list).
void        LobbyRankFlush(LobbyRankT* pRanker, s32 bAll);

// The selected scoreboard's iIndex-th parameter into *pData; 0 on success.
s32         LobbyRankParm(LobbyRankT* pRanker, s32 iIndex, s32* pData);

// Rank list of the selected scoreboard: rows iOffset.. (iRows, -1 = all) or the named users
// in pBuddyList. Returns 0 when the request was sent or a LOBBYRANK_ERROR_* value.
s32         LobbyRankFetch(LobbyRankT* pRanker, s32 iTimeout, s32 iOffset, s32 iRows, const char** pBuddyList,
                           s32 iBuddyCount, LobbyRankCallbackT* pCallback, void* pUserData);

// Queries over the downloaded rank list.
s32         LobbyRankColumnCount(LobbyRankT* pRanker);
s32         LobbyRankColumnWidth(LobbyRankT* pRanker, s32 iColumn);
s32         LobbyRankColumnType(LobbyRankT* pRanker, s32 iColumn);
s32         LobbyRankColumnStyle(LobbyRankT* pRanker, s32 iColumn);
const char* LobbyRankColumnName(LobbyRankT* pRanker, s32 iColumn, char* pBuffer, s32 iBuflen);
s32         LobbyRankRowAttrib(LobbyRankT* pRanker, s32 iRow, s32* pAttrib);
s32         LobbyRankRowCount(LobbyRankT* pRanker);
const char* LobbyRankData(LobbyRankT* pRanker, s32 iColumn, s32 iRow, char* pBuffer, s32 iBuflen);

// Forget the pending fetch callback. Returns 1 when an outstanding request was cancelled.
s32         LobbyRankCancelCB(LobbyRankT* pRanker);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYRANK_H
