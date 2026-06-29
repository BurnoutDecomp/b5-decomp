#ifndef CGS_SERVER_INTERFACE_GAMES_H
#define CGS_SERVER_INTERFACE_GAMES_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

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
// The LobbyApiPlayT play record is a DirtySDK vendor type. Its X360 build differs
// slightly from the DecFIGS DWARF copy (the DWARF is explicitly "not offset
// authority"); the field offsets named here are pinned to the load/store
// displacements in this TU's asm (e.g. uSysflags @ rec+484 from `lwz r3,0x1F4`
// where the record base is this+0x10, iCount @ rec+652, the player array @
// rec+660 with a 164-byte stride). Fields the TU does not touch are reserved with
// explicit padding so the named fields keep their grounded offsets.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceDirtySock;
    struct ServerInterfaceGameParamsBase;
    struct ServerInterfacePlayerParamsBase;
    struct ServerInterfaceGameSearchParamsBase;
    struct ServerInterfaceQuickJoinParamsBase;
    struct ServerInterfaceEndGameDataBase;
    enum   EServerInterfaceEvent : s32;

    namespace DirtySock
    {
        // A single player record inside a LobbyApiPlayT (X360 stride = 164 bytes;
        // the DWARF generic copy is 168). Only the fields this TU reads/compares are
        // named; the rest is reserved with padding to keep their offsets exact.
        struct LobbyApiPlayerT
        {
            s32  iIdent;        // rec-player +0   (player ID; compared in *ById)
            char strPers[20];   // rec-player +4   (player name; LobbyNameCmp arg)
            u8   mPad24[80];     // +24 .. +104
            char strParams[60]; // rec-player +104 (compared in CheckForPlayerParameterChange)
        };

        // The lobby "play" / game record (X360 size 2136 bytes). Field offsets are
        // record-relative and pinned to this TU's asm displacements.
        struct LobbyApiPlayT
        {
            s32  iIdent;        // +0   game ID            (GetGameID)
            char strName[36];   // +4   game name          (GetGameName)
            char strHost[20];   // +40  host name          (GetHostPlayerID LobbyNameCmp)
            u8   mPad60[144];   // +60 .. +204
            char strParams[16]; // +204 game-params string (asm record+0xCC; GAME_PARAMS_CHANGED strcmp)
            u8   mPad220[260];  // +220 .. +480
            u32  uCustflags;    // +480 (asm comp+496)
            u32  uSysflags;     // +484 (asm comp+500; ConvertFlags -> locked/started bits)
            u8   bMinsize;      // +488
            u8   bMaxsize;      // +489 (asm comp+505; compared as SBYTE1 of the dword @+488)
            u8   mPad490[6];    // +490 .. +496
            u32  uWhen;         // +496 (asm comp+512; "WHEN" epoch in SendGameResult)
            u8   mPad500[4];    // +500 .. +504
            char strAuth[64];   // +504 .. +568 (asm comp+520; SendGameResult AUTH + strlen check)
            u8   mPad568[84];   // +568 .. +652 (strParams moved to +204)
            s32  iCount;        // +652 number of players  (asm comp+668)
            s32  iPrivSlots;    // +656
            // +660: player array (stride 164). Sized so the whole record is 2136 bytes:
            //   2136 - 660 = 1476 = 9 * 164.
            LobbyApiPlayerT aPlayers[9];
        };
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
        virtual ~ServerInterfaceGames();

        // ---- Lifecycle (vtable + plain) -------------------------------------------
        virtual void Construct();
        void Destruct();
        bool Prepare(ServerInterfaceDirtySock* lpServerInterface);
        bool Release();

        // ---- Actions --------------------------------------------------------------
        void CreateGame(ServerInterfaceGameParamsBase* lpGameParams,
                        ServerInterfacePlayerParamsBase* lpPlayerParams);
        void JoinGame(ServerInterfaceGameParamsBase* lpGameParams,
                      ServerInterfacePlayerParamsBase* lpPlayerParams);
        void QuickJoinGame(ServerInterfaceQuickJoinParamsBase* lpQuickJoinParams,
                           ServerInterfacePlayerParamsBase* lpPlayerParams);
        void SearchForGames(ServerInterfaceGameSearchParamsBase* lpSearchParams);
        void CancelSearchForGames();
        void UpdateGameParameters(ServerInterfaceGameParamsBase* lpGameParams);
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
        void LeaveGame(s32 liFlagsA, s32 liFlagsB);
        void* KickPlayerByID(s32 liPlayerID, s32 liReason, char lbBan);
        void SendGameResult(ServerInterfaceEndGameDataBase* lpEndGameData, void* lpUserData);
        void SetGameServerConnectionType(s32 liConnectionType);
        void RegisterGameSearchSortCallback(SearchResultsSortCallback lpfnCallback,
                                            void* lpUserData,
                                            ServerInterfaceGameParamsBase* lpParamsA,
                                            ServerInterfaceGameParamsBase* lpParamsB);

        // ---- Queries --------------------------------------------------------------
        s32  GetGameID();
        char* GetGameName();
        bool GetGameParameters(ServerInterfaceGameParamsBase* lpOut);
        s32  GetHostPlayerID();
        s32  GetNumberOfFoundGames();
        s32  GetNumberPlayersInGame();
        void* GetPlayerParametersByIndex(s32 liIndex, ServerInterfacePlayerParamsBase* lpOut);
        void* GetPlayerParametersByPlayerID(s32 liPlayerID, ServerInterfacePlayerParamsBase* lpOut);
        void* GetPlayerParametersByPlayerName(const char* lpcName,
                                              ServerInterfacePlayerParamsBase* lpOut);
        bool IsLocalPlayerInGame();
        bool IsLocalPlayerHost();
        bool IsPlayerInGame(const char* lpcName);
        bool IsGameLocked();
        bool IsGameStarted();
        bool IsGameServerGame();

        // ADDITIVE GROW (flagged by the BrnNetworkConnectionManager group): kick a player by
        // name with a reason/ban. X360 BrnNetwork::ConnectionManager (KickUnNATablePlayer @
        // 0x82566860, UpdateNATData @ 0x8256CFF0) calls this cross-class on the games component,
        // so it belongs to the public surface. Body is homed in the X360-derived
        // ServerInterfaceGamesX360 TU / a sibling; declared here so callers can reach it.
        void* KickPlayer(const char* lpcPlayerName, s32 liReason, char lbBan);

    private:
        // ---- Internal helpers (this TU) -------------------------------------------
        void StartAction(EAction leAction, void* lpfnCallback);
        s32  StartGameManagerAction(EAction leAction, void* lpfnCallback);
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
        static s32 OnlyFinishOnErrorCallback(s32 a1, s32* a2, ServerInterfaceGames* lpSelf);

        // Static action-thunks (used as DirtySDK callbacks). Match the X360 ABI shapes.
        static s32   DefaultCallback(s32 a1, s32* a2, ServerInterfaceGames* lpSelf);
        static s32   LeaveGameCallback(s32 a1, s32* a2, ServerInterfaceGames* lpSelf);
        static s32   SearchForGamesCallback(s32 a1, s32* a2, ServerInterfaceGames* lpSelf);
        static s32   ReceivedGameEvent(ServerInterfaceGames* lpSelf, s32* a2);
        static void  EventStatusCallback(s32 a1, s32* a2, ServerInterfaceGames** a3);
        static void* GameManagerCallback(s32 a1, s32* a2, ServerInterfaceGames* lpSelf);
        static s32   FoundGamesSort(ServerInterfaceGames* lpSelf, void* a2,
                                    ServerInterfaceGameParamsBase* lpA,
                                    ServerInterfaceGameParamsBase* lpB);

    public:
        // The per-action error-mapping table ({table-ptr, count} indexed by action),
        // and the per-action message fourcc / name tables, live in the .cpp.

    private:
        // ---- own members (the ServerInterfaceComponent base provides the +0x00..+0x0F
        //      vptr / mpcCurrentAction / meStatus / miLastError header) ---
        DirtySock::LobbyApiPlayT mLastGameRecord;   // +0x010 (2136 bytes)
        void*       mpFoundGames;           // +0x868  (DispListRef*)
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
