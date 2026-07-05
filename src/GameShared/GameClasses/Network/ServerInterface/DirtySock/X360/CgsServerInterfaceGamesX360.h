#ifndef CGS_SERVER_INTERFACE_GAMES_X360_H
#define CGS_SERVER_INTERFACE_GAMES_X360_H

#include "types.hpp"
#include "../Components/CgsServerInterfaceGames.h"   // ServerInterfaceGames base

// ===========================================================================
// CgsNetwork::ServerInterfaceGamesX360
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/
//         CgsServerInterfaceGamesX360.{h,cpp}
//
// The X360 (and PC-remaster) platform leaf of the DirtySock "games" server-
// interface component. It layers the Xbox LIVE matchmaking search-context
// plumbing on top of the shared ServerInterfaceGames: CreateGame / JoinGame /
// QuickJoinGame / SearchForGames push the XNCONTEXTS / XNRANK / XNCONTEXTID%d /
// XNCONTEXTVALUE%d tagfields (and, for CreateGame, XUserSetContext + the ConnAPI
// "slot" control) before chaining to the base; the lifecycle overrides
// (Prepare / Release) are bare tail-call thunks; Update / Suspend / Resume drive
// the found-games display list; and it adds Xbox-specific queries
// (GetPlayerXUIDByID, SetSessionFlags) and the "SESS"-tagfield game-event handler.
//
// IMPORTANT: the leaf adds NO data members of its own -- every member lives in
// the ServerInterfaceGames base. The X360 game-parameter extension is the
// ALREADY-COMMITTED leaf CgsServerInterfaceGameParamsX360 (maRankedContexts[10] @
// +0xF8, miContextCount @ +0x148, miPropertyCount @ +0x14C) plus the base slot
// counters (miNumPublicSlots @ +224 / miNumPrivateSlots @ +228); the end-game
// payload is the committed ServerInterfaceEndGameDataX360::maResultWords[16]; and
// the play record is the committed CgsNetwork::DirtySock::LobbyApiPlayT. This leaf
// reuses those committed types rather than duplicating them.
//
// NOTE (base access): the base ServerInterfaceGames declares
// CreateGame / JoinGame / QuickJoinGame / SearchForGames / UpdateGameParameters
// (which these overrides call as Base::X) -- promoted to virtual on the base so
// the X360 leaf occupies the corresponding vtable slots. The base private members
// / helpers these overrides reach (mpServerInterface, mpFoundGames, meCurrentAction,
// miRequestCallbackID, mpSearchSortCallback, EndAction, FreeDisplayLists,
// AllocDisplayLists, the static FoundGamesSort / ReceivedGameEvent, and the
// GetServerInterface() / GetLastGameRecord() accessors) are promoted to protected.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceDirtySock;
    struct ServerInterfaceGameParamsBase;
    struct ServerInterfacePlayerParamsBase;
    struct ServerInterfaceGameSearchParamsBase;
    struct ServerInterfaceQuickJoinParamsBase;
    struct ServerInterfaceEndGameDataBase;

    // Xbox-Live cap: the search-context table holds fewer than 10 entries.
    const s32 KI_MAX_SEARCH_PARAMS = 10;

    // The lobby play record holds up to 9 player entries.
    const s32 KI_MAX_GAME_PLAYERS = 9;

    // EndGame walks 8 {playerId, value} result pairs out of maResultWords[16].
    const s32 KI_END_GAME_RESULT_PAIRS = 8;

    // ConnApi control fourccs used by this leaf (leaf-local; the base's identically
    // valued anon-namespace constants are not visible across TUs).
    const s32 KI_CONN_CTRL_SLOT     = 0x736C6F74; // 'slot' -- public/private slot counts
    const s32 KI_CONN_CTRL_SKILL    = 0x736B696C; // 'skil' -- per-client end-game stat
    const s32 KI_CONN_SESSION_FLAGS = 0x73666C67; // 'sflg' -- game-server session flags

    class ServerInterfaceGamesX360 : public ServerInterfaceGames
    {
    public:
        // Compiler-emitted scalar deleting destructor @ 0x827DFC88 stands in for the
        // plain virtual dtor (rebinds vptr to the X360 leaf vtable, frees this).
        virtual ~ServerInterfaceGamesX360();

        // ---- Forwarding lifecycle overrides (tail-call the base) ----------------
        virtual void Construct();                                          // 0x8287F738
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData); // 0x828997B8

        // @ 0x828997B0 -- bare tail-call thunk to the base.
        bool Prepare(ServerInterfaceDirtySock* lpServerInterface);
        // @ 0x8288C0C8 -- bare tail-call thunk to the base.
        bool Release();

        // @ 0x8287F748 -- drive the found-games display list.
        virtual void Update();
        // @ 0x8288C0D0 -- cancel the outstanding lobby request.
        virtual void Suspend(s32 liUpdateFlags);
        // @ 0x8288C128 -- (re)build the found-games display list; re-sort it when a
        // search-result sort callback is registered.
        virtual void* Resume();

        // ---- Action overrides (add the XNCONTEXT* / XNRANK matchmaking tagfields) --
        void CreateGame(ServerInterfaceGameParamsBase* lpGameParams,
                        ServerInterfacePlayerParamsBase* lpPlayerParams);  // 0x8288C178
        void JoinGame(ServerInterfaceGameParamsBase* lpGameParams,
                      ServerInterfacePlayerParamsBase* lpPlayerParams);    // 0x8288C268
        void QuickJoinGame(ServerInterfaceQuickJoinParamsBase* lpQuickJoinParams,
                           ServerInterfacePlayerParamsBase* lpPlayerParams); // 0x8288C300
        void SearchForGames(ServerInterfaceGameSearchParamsBase* lpSearchParams); // 0x8288C460
        void UpdateGameParameters(ServerInterfaceGameParamsBase* lpGameParams);   // 0x8288C770
        void EndGame(ServerInterfaceEndGameDataBase* lpEndGameData);       // 0x8288C5B8

        // Set the game-server "session flags" word through ConnApi ('sflg').
        void SetSessionFlags(s32 liFlags);                                 // 0x8287F7B0

        // ---- X360 game-event handling ---------------------------------------------
        // @ 0x8288C808 -- end the current create/join/quick-join action once the lobby
        // message carries a "SESS" tagfield; otherwise chain to the base.
        // lpauMsg points at the DirtySDK lobby message (msg[3]==error, msg[4]==pData).
        s32 ReceivedGameEvent(s32* lpauMsg);

        // ---- X360 queries ---------------------------------------------------------
        // @ 0x8288C880 -- resolve a player id to its XUID/host-address.
        bool GetPlayerXUIDByID(s32 liPlayerID, void* lpXUIDOut);
    };
}

#endif // CGS_SERVER_INTERFACE_GAMES_X360_H
