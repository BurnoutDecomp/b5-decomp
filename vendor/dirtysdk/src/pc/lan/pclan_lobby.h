#pragma once

#include "types.hpp"
#include "lobbyapi.h"

// ============================================================================
// pclan_lobby.h -- the seam between the PC lobby (lobbyapipc.cpp) and the PC LAN lobby
// authority (pclan_lobby.cpp).
//
// [PC platform layer] Not console code. On the console every game request went to the online
// lobby server, which kept the game records and pushed 'game' / 'play' / 'kick' events to the
// members. On PC with BP_LAN=1 the instance that creates a game keeps its record and plays that
// server for it over the PC LAN transport; the other instances find it with a DISCOVER sweep.
// Nothing here runs offline: lobbyapipc.cpp only calls in when PcLanActive().
// ============================================================================

// ---- lobbyapipc.cpp services the authority uses ---------------------------------------------

// Complete request iId with (iCode, pData); the response reaches its callback on the next
// LobbyApiUpdate. A cancelled or unknown id is ignored.
void LobbyApiPcComplete(LobbyApiRefT* pLobbyApi, s32 iId, s32 iCode, const char* pData);

// Queue a lobby event for every callback of type iType (a LobbyApiCBTypeE).
void LobbyApiPcPostEvent(LobbyApiRefT* pLobbyApi, s32 iType, s32 iKind, s32 iCode, const char* pData);

// The local user record ('self'); the authority keeps its game field current.
LobbyApiUserT* LobbyApiPcSelf(LobbyApiRefT* pLobbyApi);

// Add a copy of pPlay to display list iListType (when allocated) and mark the list changed.
void LobbyApiPcListAdd(LobbyApiRefT* pLobbyApi, s32 iListType, const LobbyApiPlayT* pPlay);

// LobbyLogin's connect step: LAN posts the 'conn' event (LOBBYAPI_CBTYPE_CONN) and returns 0;
// offline there is nothing to connect to and it returns -1.
s32 LobbyApiPcConnect(LobbyApiRefT* pLobbyApi);

// ---- pclan_lobby.cpp -------------------------------------------------------------------------

// Bind / unbind the lobby ref to the LAN transport (message handlers, state reset).
void PcLanLobbyAttach(LobbyApiRefT* pLobbyApi);
void PcLanLobbyDetach(LobbyApiRefT* pLobbyApi);

// Offer request iId to the authority. Returns true when iKind is a game request the
// authority answers (it then calls LobbyApiPcComplete, now or later).
bool PcLanLobbyRequest(LobbyApiRefT* pLobbyApi, s32 iId, s32 iKind, const char* pRequest);

// A request the authority owns was cancelled.
void PcLanLobbyCancel(LobbyApiRefT* pLobbyApi, s32 iId);

// Per-update work: search / join deadlines and the 1 Hz record refresh to members.
void PcLanLobbyUpdate(LobbyApiRefT* pLobbyApi);

// The local user leaves the current game (lobby disconnect).
void PcLanLobbyLeave(LobbyApiRefT* pLobbyApi);
