#ifndef CGS_SERVER_INTERFACE_GAMES_H
#define CGS_SERVER_INTERFACE_GAMES_H

#include "types.hpp"
#include <stddef.h>   // offsetof
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"
#include "lobbyapi.h"      // LobbyApiPlayT, LobbyApiPlayerT, LobbyApiMsgT, LobbyApiCallbackT
#include "gamemanager.h"   // GameManagerRefT, GameManagerCBDataT

struct LobbyApiRefT;
struct LobbyApiMsgT;

// ===========================================================================
// CgsNetwork::ServerInterfaceGames
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceGames.{h,cpp}
//
// The DirtySock "games" server-interface component: owns the local player's
// current game/play record (mLastGameRecord, a DirtySDK LobbyApiPlayT), drives
// create/join/quick-join/search/leave/kick/start/results actions through the
// lobby + game-manager DirtySDK handles, and answers queries about the game and
// the players in it. Derives from CgsNetwork::ServerInterfaceComponent.
//
// LAYOUT (X360, grounded against the function asm in this TU):
//   +0x000 vptr
//   +0x004 mpErrorData          (const void*)  static error-string table ptr
//   +0x008 miStatus             (s32)          2 == "no error"
//   +0x00C miLastError          (s32)
//   +0x010 mLastGameRecord      (LobbyApiPlayT, 2136 bytes)  -- Construct does
//                               XMemSet(this+0x10, 0, 2136); GetGameID returns
//                               *(this+0x10) = mLastGameRecord.iIdent.
//   +0x868 mpFoundGames         (DispListRef*)
//   +0x86C mpServerInterface    (ServerInterfaceDirtySock*)
//   +0x870 meCurrentAction      (EAction; 13 == E_ACTION_COUNT == idle)
//   +0x874 miEventCallback      (s32, -1 == none)
//   +0x878 miRespCallback       (s32, -1 == none)
//   +0x87C miRequestCallbackID  (s32, -1 == none)
//   +0x880 mpSearchSortCallback (SearchResultsSortCallback)
//   +0x884 mpSearchSortUserData (void*)
//   +0x888 mpGameParamsA        (ServerInterfaceGameParamsBase*)
//   +0x88C mpGameParamsB        (ServerInterfaceGameParamsBase*)
//
// ServerInterfaceComponent has no committed standalone layout we can include
// without forking, so (matching the sibling ServerInterfaceServerInfo home) the
// base's documented data layout is modelled here as the leading members.
//
// The LobbyApiPlayT play record is the DirtySDK type (lobbyapi.h, console layout).
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceDirtySock;
    struct ServerInterfaceGameParamsBase;
    struct ServerInterfacePlayerParamsBase;
    struct ServerInterfaceGameSearchParamsBase;
    struct ServerInterfaceQuickJoinParamsBase;
    struct ServerInterfaceEndGameDataBase;
    struct ServerInterfaceGameResultsBase;
    enum   EServerInterfaceEvent : s32;

    namespace DirtySock
    {
        struct ConnApiCbInfoT;

        // The DirtySDK record types, under the names this component's interface uses.
        using ::LobbyApiPlayerT;
        using ::LobbyApiPlayT;
    }

    class ServerInterfaceGames : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfaceGames.h:81
        enum EAction
        {
            E_ACTION_CREATE_GAME            = 0,
            E_ACTION_JOIN_GAME              = 1,
            E_ACTION_QUICK_JOIN_GAME        = 2,
            E_ACTION_SEARCH_FOR_GAMES       = 3,
            E_ACTION_CANCEL_SEARCH_FOR_GAMES= 4,
            E_ACTION_LEAVE_GAME             = 5,
            E_ACTION_KICK_PLAYER            = 6,
            E_ACTION_UPDATE_GAME_PARAMS     = 7,
            E_ACTION_UPDATE_PLAYER_PARAMS   = 8,
            E_ACTION_LOCK_GAME              = 9,
            E_ACTION_UNLOCK_GAME            = 10,
            E_ACTION_START_GAME             = 11,
            E_ACTION_SEND_RESULTS           = 12,
            E_ACTION_COUNT                  = 13,
        };

        // CgsServerInterfaceGames.h:100
        enum EGameServerConnectionType
        {
            E_GAME_SERVER_CONNECTION_TYPE_NONE              = 0,
            E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_BOTH     = 1,
            E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_VOIP_ONLY = 2,
            E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_GAME_ONLY = 3,
            E_GAME_SERVER_CONNECTION_TYPE_NO_FALLBACK       = 4,
            E_GAME_SERVER_CONNECTION_TYPE_COUNT             = 5,
        };

        // Search-result comparison callback (CgsServerInterfaceGames.h:67).
        typedef s32 (*SearchResultsSortCallback)(void*,
                                                 ServerInterfaceGameParamsBase*,
                                                 ServerInterfaceGameParamsBase*);

        // Establishes the vtable slot at +0 (the X360 type is polymorphic).
        // Empty: the leaf's deleting destructor only restores the component vtable.
        virtual ~ServerInterfaceGames() {}

        // ---- Lifecycle ------------------------------------------------------------
        // Vtable: after the five component slots the games component appends Destruct,
        // Prepare, Release, Update, Suspend, Resume, CreateGame, JoinGame, QuickJoinGame,
        // SearchForGames, UpdateGameParameters, EndGame and ReceivedGameEvent, in that
        // order. Update / Suspend / Resume / EndGame have no base bodies in the console
        // image (only the platform leaf implements them).
        virtual void Construct();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);
        virtual void Destruct();
        virtual bool Prepare(ServerInterfaceDirtySock* lpServerInterface);
        virtual bool Release();
        virtual void Update() = 0;
        virtual void Suspend() = 0;
        virtual void Resume() = 0;

        // ---- Actions --------------------------------------------------------------
        // CreateGame / JoinGame / QuickJoinGame / SearchForGames / UpdateGameParameters
        // are virtual: the X360 leaf ServerInterfaceGamesX360 (0x8288C178 / 0x8288C268 /
        // 0x8288C300 / 0x8288C460 / 0x8288C770) genuinely overrides these vtable slots and
        // chains back to Base::X after seeding the Xbox LIVE search contexts.
        virtual void CreateGame(ServerInterfaceGameParamsBase* lpGameParams,
                        ServerInterfacePlayerParamsBase* lpPlayerParams);
        virtual void JoinGame(ServerInterfaceGameParamsBase* lpGameParams,
                      ServerInterfacePlayerParamsBase* lpPlayerParams);
        virtual void QuickJoinGame(ServerInterfaceQuickJoinParamsBase* lpQuickJoinParams,
                           ServerInterfacePlayerParamsBase* lpPlayerParams);
        virtual void SearchForGames(ServerInterfaceGameSearchParamsBase* lpSearchParams);
        void CancelSearchForGames();
        virtual void UpdateGameParameters(ServerInterfaceGameParamsBase* lpGameParams);
        virtual void EndGame(const ServerInterfaceEndGameDataBase* lpEndGameData) = 0;
        // Lobby game event: ends the pending create/join/quick-join action when the event
        // carries our game ident. lpauMsg is the DirtySDK lobby message (word 3 the error,
        // word 4 the tagfield payload).
        virtual s32 ReceivedGameEvent(LobbyApiMsgT* lpMsg);
        void UpdatePlayerParameters(s32 liPlayerID,
                                    ServerInterfacePlayerParamsBase* lpPlayerParams);
        void LockGame();
        void UnlockGame();
        // ADDITIVE GROW (BrnNetworkLaunchManager TU): the host issues the lobby "start
        // game" action once stats have downloaded (UpdateDownloadingStats). Declared-only;
        // body lives in this component's own TU.
        void StartGame();
        // ADDITIVE GROW (BrnNetworkPostRoundManager TU): leave the current lobby game.
        // The post-round flow (ActionLeaveGame) calls LeaveGame(0, 1) -- the two trailing
        // args are the X360 LeaveGame(this, 0, 1) request flags. Declared-only; body lives
        // in this component's own TU.
        // The two flags become the request's "SET" / "FORCE" tags.
        void LeaveGame(bool lbSet, bool lbForce);
        void KickPlayerByID(s32 liPlayerID, s32 liReason, char lbBan);
        // Upload the local player's game result record (the tagged header fields, then
        // lpGameResults->SerialiseToString) as the lobby "send results" action.
        void SendGameResult(const ServerInterfaceGameResultsBase* lpGameResults);
        void SetGameServerConnectionType(s32 liConnectionType);
        void RegisterGameSearchSortCallback(SearchResultsSortCallback lpfnCallback,
                                            void* lpUserData,
                                            ServerInterfaceGameParamsBase* lpParamsA,
                                            ServerInterfaceGameParamsBase* lpParamsB);

        // ---- Queries --------------------------------------------------------------
        s32  GetGameID();
        char* GetGameName();
        // Hand the current play record to lpOut->SerialiseFromGame.
        void GetGameParameters(ServerInterfaceGameParamsBase* lpOut);
        s32  GetHostPlayerID();
        s32  GetNumberOfFoundGames();
        // Copy found game liIndex's parameters into lpOut; false if there is no such game.
        bool GetFoundGame(s32 liIndex, ServerInterfaceGameParamsBase* lpOut) const;
        s32  GetNumberPlayersInGame();
        void* GetPlayerParametersByIndex(s32 liIndex, ServerInterfacePlayerParamsBase* lpOut);
        void* GetPlayerParametersByPlayerID(s32 liPlayerID, ServerInterfacePlayerParamsBase* lpOut);
        void* GetPlayerParametersByPlayerName(const char* lpcName,
                                              ServerInterfacePlayerParamsBase* lpOut);
        bool IsLocalPlayerInGame();
        bool IsLocalPlayerHost();
        bool IsLocalPlayerLeavingGame() const;
        bool IsPlayerInGame(const char* lpcName);

        // The by-id overload: is a player with this network player id in the play record?
        bool IsPlayerInGame(s32 liPlayerID) const;

        bool IsGameLocked();
        bool IsGameStarted();
        bool IsGameServerGame();

        // ADDITIVE GROW (flagged by the BrnNetworkConnectionManager group): kick a player by
        // name with a reason/ban. X360 BrnNetwork::ConnectionManager (KickUnNATablePlayer @
        // 0x82566860, UpdateNATData @ 0x8256CFF0) calls this cross-class on the games component,
        // so it belongs to the public surface. Body is homed in the X360-derived
        // ServerInterfaceGamesX360 TU / a sibling; declared here so callers can reach it.
        void KickPlayer(const char* lpcPlayerName, s32 liReason, char lbBan);

        // The owning server interface (also reached from outside the component).
        ServerInterfaceDirtySock* GetServerInterface() const { return mpServerInterface; }

    protected:
        // Read accessor the platform leaf uses to reach the play record by name (rather
        // than widening data-member access). Layout unchanged.
        DirtySock::LobbyApiPlayT* GetLastGameRecord()        { return &mLastGameRecord; }

        // ---- Internal helpers (this TU) -------------------------------------------
        // Promoted private -> protected: the X360 leaf (Suspend / Resume / ReceivedGameEvent)
        // reaches EndAction / FreeDisplayLists / AllocDisplayLists / the static FoundGamesSort /
        // the static ReceivedGameEvent directly.
        void StartAction(EAction leAction, LobbyApiCallbackT* lpfnCallback);
        s32  StartGameManagerAction(EAction leAction, LobbyApiCallbackT* lpfnCallback);
        void EndAction(s32 liError);
        bool CheckForPlayerChange(DirtySock::LobbyApiPlayT* lpA, DirtySock::LobbyApiPlayT* lpB,
                                  EServerInterfaceEvent leAddEvent,
                                  EServerInterfaceEvent leRemoveEvent);
        bool CheckForPlayerParameterChange(DirtySock::LobbyApiPlayT* lpA,
                                           DirtySock::LobbyApiPlayT* lpB);
        void* ProcessGameManagerPlayRecord();
        void* FreeDisplayLists();

        // Homed in the X360-derived ServerInterfaceGamesX360 TU / a sibling; declared here so
        // this base TU can call them. (AllocDisplayLists creates the found-games display list;
        // OnlyFinishOnErrorCallback is the create/join action thunk that only ends the action on
        // error.) KickPlayer is now public (see above) -- it is reached cross-class.
        void  AllocDisplayLists();
        static void OnlyFinishOnErrorCallback(LobbyApiRefT* lpLobbyApi, LobbyApiMsgT* lpMsg, void* lpUserData);

        // Static action-thunks (used as DirtySDK callbacks). Match the X360 ABI shapes.
        static void  DefaultCallback(LobbyApiRefT* lpLobbyApi, LobbyApiMsgT* lpMsg, void* lpUserData);
        // ConnApi status callback registered in the games component slot of the server
        // interface (see ServerInterfaceDirtySock::SetConnApiGameCallback).
        static void  ConnApiCallback(DirtySock::ConnApiCbInfoT* lpCbInfo,
                                     ServerInterfaceComponent* lpComponent);
        static void  LeaveGameCallback(LobbyApiRefT* lpLobbyApi, LobbyApiMsgT* lpMsg, void* lpUserData);
        static void  SearchForGamesCallback(LobbyApiRefT* lpLobbyApi, LobbyApiMsgT* lpMsg, void* lpUserData);
        static void  EventStatusCallback(LobbyApiRefT* lpLobbyApi, LobbyApiMsgT* lpMsg, void* lpUserData);
        // The response-channel callback registered on lobby creation; it has an empty body.
        static void  RespCallback(LobbyApiRefT* lpLobbyApi, LobbyApiMsgT* lpMsg, void* lpUserData);
        static void  GameManagerCallback(GameManagerRefT* lpGameManager, GameManagerCBDataT* lpCBData,
                                         void* lpUserData);
        // DispListSortT: (sort ref == this component, sort con, two list-3 game records).
        static s32   FoundGamesSort(void* lpSortRef, s32 liSortCon, void* lpGameA, void* lpGameB);

    public:
        // The per-action error-mapping table ({table-ptr, count} indexed by action),
        // and the per-action message fourcc / name tables, live in the .cpp.

    protected:
        // ---- own members (the ServerInterfaceComponent base provides the +0x00..+0x0F
        //      vptr / mpcCurrentAction / meStatus / miLastError header) ---
        // Promoted private -> protected: the X360 leaf reads mpFoundGames / mpServerInterface /
        // meCurrentAction / miRequestCallbackID / mpSearchSortCallback by name. Order and
        // offsets are unchanged.
        DirtySock::LobbyApiPlayT mLastGameRecord;   // +0x010 (2136 bytes)
        DispListRef* mpFoundGames;          // +0x868
        ServerInterfaceDirtySock* mpServerInterface; // +0x86C
        EAction     meCurrentAction;        // +0x870
        s32         miEventCallback;        // +0x874
        s32         miRespCallback;         // +0x878
        s32         miRequestCallbackID;    // +0x87C
        SearchResultsSortCallback mpSearchSortCallback; // +0x880
        void*       mpSearchSortUserData;   // +0x884
        ServerInterfaceGameParamsBase* mpGameParamsA;   // +0x888
        ServerInterfaceGameParamsBase* mpGameParamsB;   // +0x88C
    };
}

#endif // CGS_SERVER_INTERFACE_GAMES_H
