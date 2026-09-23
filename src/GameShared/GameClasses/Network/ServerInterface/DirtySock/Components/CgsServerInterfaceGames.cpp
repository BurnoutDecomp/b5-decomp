#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // full ServerInterfaceDirtySock (GetLobbyAPIRef/GetGameManagerRef/GetConnAPIRef/GetMessageBuffer/GetSKU)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h" // ServerInterfaceComponent (StartActionCore/EndActionCore/ConvertError)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h" // DSErrorToServerInterfaceError(Table)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameSearchParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceQuickJoinParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameResults.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePingRegions.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.h" // EConversionFlags
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint / CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf / SnPrintf / StrCat
#include <string.h>                                      // memcpy / strlen / strcmp

// Reconstructed from BURNOUT_X360_ARTIST.XEX (CgsServerInterfaceGames.cpp).
// The DirtySock "games" component: owns the local play record (mLastGameRecord,
// a LobbyApiPlayT) and drives create/join/quick-join/search/leave/kick/lock/
// start/results actions through the lobby + game-manager DirtySDK handles.
//
// Member access is by name throughout; the LobbyApiPlayT field offsets are pinned
// in the header (grounded against this TU's asm).
//
// FLAGGED placeholder (honest -- contents live in unrecovered .rdata):
//   * gServerInterfaceErrorData    -- the static error-string table (shared with the
//                                     other components; Construct points mpErrorData at it).
// The per-action name, request-code and DirtySock-error tables hold the console values.

// ---- External DirtySDK / lobby C-API entry points used by this component. -------
// These live in the DirtySDK vendor SDK (not yet fully reconstructed in vendor/).
// A not-yet-homed callee is satisfied by its declaration under cl /c; no body needed.
struct LobbyApiRefT;
#include "connapi.h"   // ConnApiControl, DirtySock::ConnApiRefT / ConnApiCbInfoT

extern "C"
{
    s32  LobbyNameCmp(const char* pNameA, const char* pNameB);
    void* XMemSet(void* pDest, s32 iValue, u32 uCount);

    // lobbytagfield C-API (vendor/dirtysdk/include/lobbytagfield.h homes some of these;
    // the rest are declared here so the component links under cl /c).
    void* TagFieldFind(const char* pRecord, const char* pKey);
    s32  TagFieldGetNumber(const char* pField, s32 iDefault);
    s32  TagFieldSetNumber(char* pRecord, s32 iRecLen, const char* pKey, s32 iValue);
    s32  TagFieldSetString(char* pRecord, s32 iRecLen, const char* pKey, const char* pValue);
    s32  TagFieldSetRaw(char* pRecord, s32 iRecLen, const char* pKey, const u8* pValue);
    s32  TagFieldSetEpoch(char* pRecord, s32 iRecLen, const char* pKey, s32 iEpoch);
}

namespace CgsNetwork
{
    // ConvertFlags: decode a LobbyApiPlayT's uSysflags into the logical flag word the
    // queries test (locked == bit 5, started == bit 11). Homed elsewhere in CgsNetwork.
    u32 ConvertFlags(u32 luSysflags, EConversionFlags leDirection);

    // The shared debug-log stream (X360 off_82F335C8). The component routes its
    // "DirtySock: ..." progress lines through this global StrStream. Modelled as a free
    // helper so the call sites stay readable; the real sink is the network debug channel.
    void DirtySockDebugLog(const char* lpcText);

    // Raise a server-interface event up through the owning DirtySock interface. The X360
    // build invokes the interface's OnEvent vtable slot (this+0, +0x20); modelled here as
    // a free helper that forwards to OnEvent so the games component stays member-by-name.
    void RaiseServerInterfaceEvent(ServerInterfaceDirtySock* lpInterface, s32 liEvent, void* lpData);

    namespace
    {
        // ---- Selector / control / request fourccs (big-endian packed, as the asm
        //      immediates show). ----
        const s32 KI_SELECT_USERINFO   = 1936026726; // 'self'-style self-info status select
        const s32 KI_STATUS_BUF_SIZE   = 564;

        const s32 KI_GM_SELECT_ISSERVER = 1735616353; // GameManager "is server game" select
        const s32 KI_GM_SELECT_HASSRV   = 1735619190; // GameManager game-server select/control
        const s32 KI_GM_SELECT_PLAYREC  = 1735288431; // GameManager play-record status select
        const s32 KI_GM_REQ_NULL        = 1768386159; // synthesised request-failure tag

        const s32 KI_CONN_CTRL_DISCONNECT = 1936028531; // ConnApi disconnect control
        const s32 KI_CONN_CTRL_LATENCY    = 1936486260; // ConnApi latency-update control
        const s32 KI_CONN_CTRL_CONNTYPE   = 1735620146; // ConnApi connection-type control

        const s32 KI_LIST_FOUND_GAMES = 3;  // LobbyApi list id for the found-games display list

        const s32 KI_CB_CHANNEL_EVENT = 4;  // LobbyApi event-status callback channel
        const s32 KI_CB_CHANNEL_RESP  = 0;  // LobbyApi response callback channel

        // EventStatusCallback dispatch tags (asm switch cases).
        const s32 KI_EVENT_TAG_PLAYERS = 1886151033;
        const s32 KI_EVENT_TAG_RESET   = 1734438245;
        const s32 KI_EVENT_TAG_KICKED  = 1802068843;

        // SearchForGamesCallback "no results" synthesised error fourcc.
        const s32 KI_ERR_NO_RESULTS = 1852206692;

        // The shared message-buffer size.
        const s32 KI_MESSAGE_BUFFER_LEN = 2048;
        const s32 KI_PLAYER_RECORD_SIZE = 2136;

        // EventStatusCallback / OnEvent event ids passed up to the server interface.
        // (Values are E_SERVER_INTERFACE_EVENT enumerators; pinned from the asm `li r4,N`.)
        const s32 KI_EVT_PLAYER_PARAMS_CHANGED = 8;
        const s32 KI_EVT_GAME_RESET            = 9;
        const s32 KI_EVT_PLAYERS_CHANGED       = 14;
        const s32 KI_EVT_PARAM_CHANGED         = 15;
        const s32 KI_EVT_GAME_PARAMS_CHANGED   = 16;
        const s32 KI_EVT_DISCONNECTED          = 17;
        const s32 KI_EVT_HOST_MIGRATED         = 18;
    }

    // ---- Per-action tables (console rodata). ----
    // The per-action display names indexed by EAction (passed to StartActionCore).
    const char* const KAPC_ACTION_NAMES[ServerInterfaceGames::E_ACTION_COUNT] =
    {
        "Create Game",                // 0
        "Join game",                  // 1
        "Quick join",                 // 2
        "Search for games",           // 3
        "Cancel Search for games",    // 4
        "Leave game",                 // 5
        "Kick player",                // 6
        "Update game parameters",     // 7
        "Update player parameters",   // 8
        "Lock game",                  // 9
        "Unlock game",                // 10
        "Start game",                 // 11
        "Send Game Results"           // 12
    };

    // The DirtySDK request message code each action sends.
    const s32 KAU_ACTION_MESSAGE_TYPES[ServerInterfaceGames::E_ACTION_COUNT] =
    {
        0x67637265,   // 'gcre'
        0x676A6F69,   // 'gjoi'
        0x6771776B,   // 'gqwk'
        0x67736561,   // 'gsea'
        0x67736561,   // 'gsea'
        0x676C6561,   // 'glea'
        0x67736574,   // 'gset'
        0x67736574,   // 'gset'
        0x67736574,   // 'gset'
        0x67736574,   // 'gset'
        0x67736574,   // 'gset'
        0x67737461,   // 'gsta'
        0x72616E6B    // 'rank'
    };

    // The DirtySock-error -> EServerInterfaceError mappings, laid out as one contiguous
    // block because that is how the console reads them: the search and cancel-search
    // lookup entries carry a count of 22 over the 2-entry search table, so ConvertError
    // walks on through the leave, kick/params/lock and start tables and the first four
    // entries of the results table.
    const DSErrorToServerInterfaceError KA_GAMES_DS_ERROR_MAPPINGS[76] =
    {
        // [0] create game
        { 0x6D697373, E_SERVER_INTERFACE_GENERAL_ERROR_MISSING_PARAMS },        // 'miss'
        { 0x696E7670, E_SERVER_INTERFACE_GENERAL_ERROR_INVALID_PARAMS },        // 'invp'
        { 0x6D617574, E_SERVER_INTERFACE_GENERAL_ERROR_MASTER_NOT_AUTH },       // 'maut'
        { 0x696E676D, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_IN_GAME },         // 'ingm'
        { 0x6475706C, E_SERVER_INTERFACE_GAMES_ERROR_NAME_ALREADY_EXISTS },     // 'dupl'
        { 0x75726F6D, E_SERVER_INTERFACE_GAMES_ERROR_UNKNOWN_ROOM },            // 'urom'
        { 0x75757374, E_SERVER_INTERFACE_GAMES_ERROR_UNKNOWN_USERSET },         // 'uust'
        { 0x6E6C6F6B, E_SERVER_INTERFACE_GAMES_ERROR_USERSET_NOT_LOCKED },      // 'nlok'
        { 0x61757468, E_SERVER_INTERFACE_GAMES_ERROR_NOT_USERSET_HOST },        // 'auth'
        { 0x6D616E79, E_SERVER_INTERFACE_GAMES_ERROR_TOO_MANY_GAMES_IN_ROOM },  // 'many'
        // [10] join game
        { 0x6D697373, E_SERVER_INTERFACE_GENERAL_ERROR_MISSING_PARAMS },        // 'miss'
        { 0x7567616D, E_SERVER_INTERFACE_GAMES_ERROR_UNKOWN_GAME },             // 'ugam'
        { 0x6D617574, E_SERVER_INTERFACE_GENERAL_ERROR_MASTER_NOT_AUTH },       // 'maut'
        { 0x616A6F69, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_JOINED },          // 'ajoi'
        { 0x696E676D, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_IN_GAME },         // 'ingm'
        { 0x66756C6C, E_SERVER_INTERFACE_GAMES_ERROR_GAME_FULL },               // 'full'
        { 0x6E737063, E_SERVER_INTERFACE_GAMES_ERROR_GAME_FULL },               // 'nspc'
        { 0x70617373, E_SERVER_INTERFACE_GAMES_ERROR_INVALID_PASSWORD },        // 'pass'
        { 0x61737461, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_STARTED },         // 'asta'
        { 0x6C6F636B, E_SERVER_INTERFACE_GAMES_ERROR_GAME_LOCKED },             // 'lock'
        { 0x70617274, E_SERVER_INTERFACE_GAMES_ERROR_INVALID_PARTITION },       // 'part'
        { 0x75757374, E_SERVER_INTERFACE_GAMES_ERROR_UNKNOWN_USERSET },         // 'uust'
        { 0x6E6C6F6B, E_SERVER_INTERFACE_GAMES_ERROR_USERSET_NOT_LOCKED },      // 'nlok'
        { 0x62616E64, E_SERVER_INTERFACE_GAMES_ERROR_PLAYER_BANNED },           // 'band'
        { 0x6A6F696E, E_SERVER_INTERFACE_GAMES_ERROR_USER_NOT_JOINED },         // 'join'
        // [25] quick join
        { 0x6D697373, E_SERVER_INTERFACE_GENERAL_ERROR_MISSING_PARAMS },        // 'miss'
        { 0x7567616D, E_SERVER_INTERFACE_GAMES_ERROR_UNKOWN_GAME },             // 'ugam'
        { 0x6D617574, E_SERVER_INTERFACE_GENERAL_ERROR_MASTER_NOT_AUTH },       // 'maut'
        { 0x616A6F69, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_JOINED },          // 'ajoi'
        { 0x696E676D, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_IN_GAME },         // 'ingm'
        { 0x66756C6C, E_SERVER_INTERFACE_GAMES_ERROR_GAME_FULL },               // 'full'
        { 0x6E737063, E_SERVER_INTERFACE_GAMES_ERROR_GAME_FULL },               // 'nspc'
        { 0x70617373, E_SERVER_INTERFACE_GAMES_ERROR_INVALID_PASSWORD },        // 'pass'
        { 0x61737461, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_STARTED },         // 'asta'
        { 0x6C6F636B, E_SERVER_INTERFACE_GAMES_ERROR_GAME_LOCKED },             // 'lock'
        { 0x70617274, E_SERVER_INTERFACE_GAMES_ERROR_INVALID_PARTITION },       // 'part'
        { 0x75757374, E_SERVER_INTERFACE_GAMES_ERROR_UNKNOWN_USERSET },         // 'uust'
        { 0x6E6C6F6B, E_SERVER_INTERFACE_GAMES_ERROR_USERSET_NOT_LOCKED },      // 'nlok'
        { 0x62616E64, E_SERVER_INTERFACE_GAMES_ERROR_PLAYER_BANNED },           // 'band'
        { 0x6A6F696E, E_SERVER_INTERFACE_GAMES_ERROR_USER_NOT_JOINED },         // 'join'
        { 0x6E666E64, E_SERVER_INTERFACE_GAMES_ERROR_NO_GAMES_FOUND },          // 'nfnd'
        { 0x78697374, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_QUICK_JOINING },   // 'xist'
        { 0x696E7670, E_SERVER_INTERFACE_GENERAL_ERROR_INVALID_PARAMS },        // 'invp'
        { 0x6475706C, E_SERVER_INTERFACE_GAMES_ERROR_NAME_ALREADY_EXISTS },     // 'dupl'
        { 0x75726F6D, E_SERVER_INTERFACE_GAMES_ERROR_UNKNOWN_ROOM },            // 'urom'
        { 0x61757468, E_SERVER_INTERFACE_GAMES_ERROR_NOT_USERSET_HOST },        // 'auth'
        { 0x6D616E79, E_SERVER_INTERFACE_GAMES_ERROR_TOO_MANY_GAMES_IN_ROOM },  // 'many'
        // [47] search / cancel search
        { 0x6E666E64, E_SERVER_INTERFACE_GAMES_ERROR_NO_GAMES_FOUND },          // 'nfnd'
        { 0x75726F6D, E_SERVER_INTERFACE_GAMES_ERROR_UNKNOWN_ROOM },            // 'urom'
        // [49] leave game
        { 0x6D697373, E_SERVER_INTERFACE_GENERAL_ERROR_MISSING_PARAMS },        // 'miss'
        { 0x7567616D, E_SERVER_INTERFACE_GAMES_ERROR_UNKOWN_GAME },             // 'ugam'
        { 0x6D617574, E_SERVER_INTERFACE_GENERAL_ERROR_MASTER_NOT_AUTH },       // 'maut'
        { 0x6A6F696E, E_SERVER_INTERFACE_GAMES_ERROR_USER_NOT_JOINED },         // 'join'
        { 0x67687374, E_SERVER_INTERFACE_GAMES_ERROR_HOST_CANT_LEAVE },         // 'ghst'
        { 0x61737461, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_STARTED },         // 'asta'
        { 0x6C6F636B, E_SERVER_INTERFACE_GAMES_ERROR_GAME_LOCKED },             // 'lock'
        { 0x61757468, E_SERVER_INTERFACE_GAMES_ERROR_NOT_USERSET_HOST },        // 'auth'
        // [57] kick player, update game/player parameters, lock, unlock
        { 0x6D697373, E_SERVER_INTERFACE_GENERAL_ERROR_MISSING_PARAMS },        // 'miss'
        { 0x7567616D, E_SERVER_INTERFACE_GAMES_ERROR_UNKOWN_GAME },             // 'ugam'
        { 0x6D617574, E_SERVER_INTERFACE_GENERAL_ERROR_MASTER_NOT_AUTH },       // 'maut'
        { 0x6E676874, E_SERVER_INTERFACE_GAMES_ERROR_NOT_HOST },                // 'nght'
        { 0x6E67616D, E_SERVER_INTERFACE_GAMES_ERROR_NOT_IN_GAME },             // 'ngam'
        { 0x61737461, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_STARTED },         // 'asta'
        { 0x696E7670, E_SERVER_INTERFACE_GENERAL_ERROR_INVALID_PARAMS },        // 'invp'
        { 0x6E6F776E, E_SERVER_INTERFACE_GAMES_ERROR_NOT_OWNER_OF_GUEST },      // 'nown'
        // [65] start game
        { 0x6D697373, E_SERVER_INTERFACE_GENERAL_ERROR_MISSING_PARAMS },        // 'miss'
        { 0x7567616D, E_SERVER_INTERFACE_GAMES_ERROR_UNKOWN_GAME },             // 'ugam'
        { 0x6D617574, E_SERVER_INTERFACE_GENERAL_ERROR_MASTER_NOT_AUTH },       // 'maut'
        { 0x6E676874, E_SERVER_INTERFACE_GAMES_ERROR_NOT_HOST },                // 'nght'
        { 0x6E67616D, E_SERVER_INTERFACE_GAMES_ERROR_NOT_IN_GAME },             // 'ngam'
        { 0x61737461, E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_STARTED },         // 'asta'
        { 0x6E65706C, E_SERVER_INTERFACE_GAMES_ERROR_NOT_ENOUGH_PLAYERS },      // 'nepl'
        // [72] send game results
        { 0x75757372, E_SERVER_INTERFACE_GAMES_ERROR_UNKNOWN_USER },            // 'uusr'
        { 0x626F6775, E_SERVER_INTERFACE_GAMES_ERROR_BOGUS_RESULT },            // 'bogu'
        { 0x71756974, E_SERVER_INTERFACE_GAMES_ERROR_QUIT },                    // 'quit'
        { 0x736F6F6E, E_SERVER_INTERFACE_GAMES_ERROR_RESULT_TOO_SOON },         // 'soon'
    };

    // Per-action { mapping table, count } used by EndAction -> ConvertError.
    const DSErrorToServerInterfaceErrorTable KA_DS_ERROR_TABLE_LOOKUP[ServerInterfaceGames::E_ACTION_COUNT] =
    {
        { &KA_GAMES_DS_ERROR_MAPPINGS[0],  10 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[10], 15 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[25], 22 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[47], 22 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[47], 22 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[49], 8 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[57], 8 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[57], 8 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[57], 8 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[57], 8 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[57], 8 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[65], 7 },
        { &KA_GAMES_DS_ERROR_MAPPINGS[72], 4 }
    };
    static_assert(sizeof(KA_GAMES_DS_ERROR_MAPPINGS) / sizeof(KA_GAMES_DS_ERROR_MAPPINGS[0]) == 47 + 22 + 7,
                  "the search entry's count of 22 ends inside the results table");

    // ===================================================================
    // Lifecycle
    // ===================================================================

    void ServerInterfaceGames::Construct()
    {
        // The base's +0x04 slot (mpcCurrentAction) starts at the empty string and the
        // component is marked idle.
        mpcCurrentAction     = "";                          // +0x04
        meStatus             = 2;                           // +0x08
        miLastError          = 0;                           // +0x0C
        mpFoundGames         = 0;                            // +0x868
        mpServerInterface    = 0;                            // +0x86C
        meCurrentAction      = E_ACTION_COUNT;               // +0x870
        miEventCallback      = -1;                           // +0x874
        miRespCallback       = -1;                           // +0x878
        miRequestCallbackID  = -1;                           // +0x87C
        XMemSet(&mLastGameRecord, 0, KI_PLAYER_RECORD_SIZE);
        mpSearchSortCallback  = 0;                           // +0x880
        mpSearchSortUserData  = 0;                           // +0x884
        mpGameParamsA         = 0;                           // +0x888
        mpGameParamsB         = 0;                           // +0x88C
    }

    void ServerInterfaceGames::Destruct()
    {
        miRequestCallbackID = -1;
        mpFoundGames        = 0;
        mpServerInterface   = 0;
        meCurrentAction     = E_ACTION_COUNT;
        miEventCallback     = -1;
        miRespCallback      = -1;
        XMemSet(&mLastGameRecord, 0, KI_PLAYER_RECORD_SIZE);
        mpSearchSortCallback = 0;
        mpSearchSortUserData = 0;
        mpGameParamsA        = 0;
        mpGameParamsB        = 0;
    }

    bool ServerInterfaceGames::Prepare(ServerInterfaceDirtySock* lpServerInterface)
    {
        CGS_ASSERT(miEventCallback == -1, "miEventCallback == -1");
        CGS_ASSERT(miRespCallback == -1, "miRespCallback == -1");
        CGS_ASSERT(mpFoundGames == 0, "!mpFoundGames");

        mpServerInterface = lpServerInterface;
        meCurrentAction   = E_ACTION_COUNT;

        miEventCallback = LobbyApiSetCallback(lpServerInterface->GetLobbyAPIRef(),
                                              KI_CB_CHANNEL_EVENT,
                                              &EventStatusCallback,
                                              this);
        miRespCallback  = LobbyApiSetCallback(mpServerInterface->GetLobbyAPIRef(),
                                              KI_CB_CHANNEL_RESP,
                                              0,
                                              this);
        AllocDisplayLists();

        // Register the per-game ConnAPI callback on the server interface and the
        // game-manager play-record callback.
        CGS_ASSERT(mpServerInterface->IsGameComponentRegistered(),
                   "The component hasn't been registered!");
        mpServerInterface->SetConnApiGameCallback(&ServerInterfaceGames::ConnApiCallback);
        GameManagerSetCallback(mpServerInterface->GetGameManagerRef(),
                               &GameManagerCallback,
                               this);

        XMemSet(&mLastGameRecord, 0, KI_PLAYER_RECORD_SIZE);
        mpSearchSortCallback = 0;
        mpSearchSortUserData = 0;
        mpGameParamsA        = 0;
        mpGameParamsB        = 0;
        return true;
    }

    bool ServerInterfaceGames::Release()
    {
        meCurrentAction = E_ACTION_COUNT;
        if (miEventCallback != -1)
        {
            LobbyApiClearCallback(mpServerInterface->GetLobbyAPIRef(), miEventCallback);
            miEventCallback = -1;
        }
        if (miRespCallback != -1)
        {
            LobbyApiClearCallback(mpServerInterface->GetLobbyAPIRef(), miRespCallback);
            miRespCallback = -1;
        }
        FreeDisplayLists();
        XMemSet(&mLastGameRecord, 0, KI_PLAYER_RECORD_SIZE);
        mpSearchSortCallback = 0;
        mpSearchSortUserData = 0;
        mpGameParamsA        = 0;
        mpGameParamsB        = 0;
        mpServerInterface    = 0;
        return true;
    }

    void* ServerInterfaceGames::FreeDisplayLists()
    {
        if (mpFoundGames)
        {
            LobbyApiListFree(mpServerInterface->GetLobbyAPIRef(), KI_LIST_FOUND_GAMES, mpFoundGames);
            mpFoundGames = 0;
        }
        return mpFoundGames;
    }

    // ===================================================================
    // Action plumbing
    // ===================================================================

    void ServerInterfaceGames::StartAction(EAction leAction, LobbyApiCallbackT* lpfnCallback)
    {
        meCurrentAction = leAction;
        StartActionCore(KAPC_ACTION_NAMES[leAction]);

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        s32 liResult = LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(),
                                         KAU_ACTION_MESSAGE_TYPES[meCurrentAction],
                                         lpcBuffer, lpfnCallback, this);
        miRequestCallbackID = liResult;
        if (liResult <= 0)
            miRequestCallbackID = -1;
    }

    s32 ServerInterfaceGames::StartGameManagerAction(EAction leAction, LobbyApiCallbackT* lpfnCallback)
    {
        meCurrentAction = leAction;
        StartActionCore(KAPC_ACTION_NAMES[leAction]);

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        s32 liResult = GameManagerRequestCb(mpServerInterface->GetGameManagerRef(),
                                            KAU_ACTION_MESSAGE_TYPES[meCurrentAction],
                                            lpcBuffer, lpfnCallback, this);
        if (liResult < 0)
        {
            // Synthesise an immediate failure callback when the request could not be sent.
            LobbyApiMsgT lMsg;
            XMemSet(&lMsg, 0, sizeof(lMsg));
            lMsg.code = KI_GM_REQ_NULL;
            lpfnCallback(mpServerInterface->GetLobbyAPIRef(), &lMsg, this);
        }
        return liResult;
    }

    void ServerInterfaceGames::EndAction(s32 liError)
    {
        CGS_ASSERT(meCurrentAction != E_ACTION_COUNT, "meCurrentAction != E_ACTION_COUNT");

        const DSErrorToServerInterfaceErrorTable& lrEntry = KA_DS_ERROR_TABLE_LOOKUP[meCurrentAction];
        EServerInterfaceError leMapped = ConvertError(liError, lrEntry.mpMappingTable, lrEntry.miNumMappings);
        EndActionCore(static_cast<int>(leMapped));

        meCurrentAction     = E_ACTION_COUNT;
        miRequestCallbackID = -1;
    }

    // ===================================================================
    // Static callbacks
    // ===================================================================

    void ServerInterfaceGames::DefaultCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* lpMsg, void* lpUserData)
    {
        static_cast<ServerInterfaceGames*>(lpUserData)->EndAction(lpMsg->code);
    }

    void ServerInterfaceGames::LeaveGameCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* lpMsg, void* lpUserData)
    {
        ServerInterfaceGames* lpSelf = static_cast<ServerInterfaceGames*>(lpUserData);
        CGS_ASSERT(lpSelf != 0, "lpLobbyComponent");
        ConnApiControl(lpSelf->mpServerInterface->GetConnAPIRef(),
                       KI_CONN_CTRL_DISCONNECT, 0, 0, "");
        if (lpSelf->IsLocalPlayerInGame())
        {
            s32 liError = lpMsg->code;
            if (liError)
            {
                lpSelf->EndAction(liError);
            }
        }
    }

    void ServerInterfaceGames::SearchForGamesCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* lpMsg,
                                                      void* lpUserData)
    {
        ServerInterfaceGames* lpSelf = static_cast<ServerInterfaceGames*>(lpUserData);
        s32 liError = lpMsg->code;
        if (liError == 0)
        {
            void* lpField = TagFieldFind(lpMsg->pData, "COUNT");
            if (TagFieldGetNumber(reinterpret_cast<const char*>(lpField), 0) == 0)
                liError = KI_ERR_NO_RESULTS;
        }
        lpSelf->EndAction(liError);
    }

    s32 ServerInterfaceGames::ReceivedGameEvent(LobbyApiMsgT* lpMsg)
    {
        ServerInterfaceGames* lpSelf = this;
        if (static_cast<u32>(lpSelf->meCurrentAction) < 3u)
        {
            void* lpField = TagFieldFind(lpMsg->pData, "IDENT");
            s32 liIdent = TagFieldGetNumber(reinterpret_cast<const char*>(lpField), -1);
            if (liIdent == lpSelf->mLastGameRecord.iIdent)
            {
                lpSelf->EndAction(lpMsg->code);
                return lpMsg->code;
            }
            return liIdent;
        }
        return 0;
    }

    void ServerInterfaceGames::EventStatusCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* lpMsg,
                                                   void* lpUserData)
    {
        // Prepare registers `this` directly as the callback user data.
        ServerInterfaceGames* lpSelf = static_cast<ServerInterfaceGames*>(lpUserData);
        switch (lpMsg->kind)
        {
        case KI_EVENT_TAG_PLAYERS:
            RaiseServerInterfaceEvent(lpSelf->mpServerInterface, KI_EVT_PLAYER_PARAMS_CHANGED, 0);
            break;
        case KI_EVENT_TAG_RESET:
            // The lobby 'game' event goes to the (virtual) ReceivedGameEvent with the message.
            lpSelf->ReceivedGameEvent(lpMsg);
            break;
        case KI_EVENT_TAG_KICKED:
            {
                void* lpField = TagFieldFind(lpMsg->pData, "REASON");
                s32 liReason = TagFieldGetNumber(reinterpret_cast<const char*>(lpField), 5);
                s32 laReason[4];
                laReason[0] = liReason;
                RaiseServerInterfaceEvent(lpSelf->mpServerInterface, KI_EVT_GAME_RESET, laReason);
            }
            break;
        default:
            break;
        }
    }

    void ServerInterfaceGames::GameManagerCallback(GameManagerRefT* /*lpGameManager*/, GameManagerCBDataT* lpCBData,
                                                   void* lpUserData)
    {
        ServerInterfaceGames* lpSelf = static_cast<ServerInterfaceGames*>(lpUserData);
        switch (lpCBData->eType)
        {
        case 0:
        case 1:
        case 2:
            return;
        case 3:
            XMemSet(&lpSelf->mLastGameRecord, 0, KI_PLAYER_RECORD_SIZE);
            if (lpSelf->meCurrentAction == E_ACTION_LEAVE_GAME)
            {
                lpSelf->EndAction(0);
            }
            else
            {
                RaiseServerInterfaceEvent(lpSelf->mpServerInterface, KI_EVT_DISCONNECTED, 0);
            }
            ConnApiControl(lpSelf->mpServerInterface->GetConnAPIRef(), KI_CONN_CTRL_DISCONNECT, 0, 0, "");
            return;
        case 4:
            lpSelf->ProcessGameManagerPlayRecord();
            return;
        default:
            CGS_ASSERT(false, "Unknown GameManager event");
            return;
        }
    }

    s32 ServerInterfaceGames::FoundGamesSort(void* lpSortRef, s32 /*liSortCon*/, void* lpGameA, void* lpGameB)
    {
        ServerInterfaceGames* lpSelf = static_cast<ServerInterfaceGames*>(lpSortRef);
        if (!lpSelf->mpGameParamsA || !lpSelf->mpGameParamsB)
            return 0;
        lpSelf->mpGameParamsA->SerialiseFromGame(lpGameA);
        lpSelf->mpGameParamsB->SerialiseFromGame(lpGameB);
        return lpSelf->mpSearchSortCallback(lpSelf->mpSearchSortUserData,
                                            lpSelf->mpGameParamsA, lpSelf->mpGameParamsB);
    }

    // ===================================================================
    // Actions
    // ===================================================================

    void ServerInterfaceGames::CreateGame(ServerInterfaceGameParamsBase* lpGameParams,
                                          ServerInterfacePlayerParamsBase* lpPlayerParams)
    {
        DirtySockDebugLog("DirtySock: CreateGame\n");

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        lpGameParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);
        lpPlayerParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);

        // Game-server games append the ping-region preference list to the request.
        if (GameManagerStatus(mpServerInterface->GetGameManagerRef(), KI_GM_SELECT_HASSRV, 0, 0) == 1)
        {
            char lacRegions[368];
            lacRegions[0] = 0;

            ServerInterfacePingRegions* lpPingRegions =
                static_cast<ServerInterfacePingRegions*>(mpServerInterface->GetPingRegionsComponent());
            if (lpPingRegions)
            {
                s32 liCount = lpPingRegions->GetNumberOfPingRegions();
                if (liCount > 0)
                {
                    for (s32 i = 0; i < liCount; ++i)
                    {
                        char lacValue[6];
                        s32 liPing = lpPingRegions->GetPingValue(i);
                        if (liPing <= -1)
                            CgsCore::SnPrintf(lacValue, 6, "9999");
                        else
                            CgsCore::SnPrintf(lacValue, 6, "%d", liPing);
                        CgsCore::StrCat(lacRegions, 0xF9, lacValue);
                        if (i < liCount - 1)
                            CgsCore::StrCat(lacRegions, 0xF9, ",");
                    }
                    lacRegions[249] = 0;
                    TagFieldSetRaw(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "REGIONS",
                                   reinterpret_cast<const u8*>(lacRegions));
                }
            }
        }

        StartGameManagerAction(E_ACTION_CREATE_GAME, &OnlyFinishOnErrorCallback);
    }

    void ServerInterfaceGames::JoinGame(ServerInterfaceGameParamsBase* lpGameParams,
                                        ServerInterfacePlayerParamsBase* lpPlayerParams)
    {
        DirtySockDebugLog("DirtySock: JoinGame\n");

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        lpGameParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);
        lpPlayerParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);

        StartGameManagerAction(E_ACTION_JOIN_GAME, &OnlyFinishOnErrorCallback);
    }

    void ServerInterfaceGames::QuickJoinGame(ServerInterfaceQuickJoinParamsBase* lpQuickJoinParams,
                                             ServerInterfacePlayerParamsBase* lpPlayerParams)
    {
        DirtySockDebugLog("DirtySock: QuickJoinGame\n");

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "MODE", 0);
        lpQuickJoinParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);
        lpPlayerParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);

        StartGameManagerAction(E_ACTION_QUICK_JOIN_GAME, &OnlyFinishOnErrorCallback);
    }

    void ServerInterfaceGames::SearchForGames(ServerInterfaceGameSearchParamsBase* lpSearchParams)
    {
        DirtySockDebugLog("DirtySock: SearchForGames \n");
        CGS_ASSERT(mpFoundGames != 0, "mpFoundGames");

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        LobbyApiListFlush(mpServerInterface->GetLobbyAPIRef(), KI_LIST_FOUND_GAMES);
        DispListClear(mpFoundGames);
        DispListOrder(mpFoundGames);
        lpSearchParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);

        StartAction(E_ACTION_SEARCH_FOR_GAMES, &SearchForGamesCallback);
    }

    void ServerInterfaceGames::CancelSearchForGames()
    {
        DirtySockDebugLog("DirtySock: CancelSearchForGames \n");
        if (meCurrentAction == E_ACTION_CANCEL_SEARCH_FOR_GAMES)
            return;

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        lpcBuffer[0] = 0;
        TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "CANCEL", 1);
        StartAction(E_ACTION_CANCEL_SEARCH_FOR_GAMES, &DefaultCallback);
    }

    void ServerInterfaceGames::UpdateGameParameters(ServerInterfaceGameParamsBase* lpGameParams)
    {
        CGS_ASSERT(IsLocalPlayerHost(), "IsLocalPlayerHost()");

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        lpGameParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);
        StartAction(E_ACTION_UPDATE_GAME_PARAMS, &DefaultCallback);
    }

    void ServerInterfaceGames::LockGame()
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        CGS_ASSERT(IsLocalPlayerHost(), "IsLocalPlayerHost()");

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        lpcBuffer[0] = 0;
        TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "SYSFLAGS",
                          static_cast<s32>(mLastGameRecord.uSysflags | 0x1000));
        StartAction(E_ACTION_LOCK_GAME, &DefaultCallback);
    }

    void ServerInterfaceGames::UnlockGame()
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        CGS_ASSERT(IsLocalPlayerHost(), "IsLocalPlayerHost()");

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        lpcBuffer[0] = 0;
        TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "SYSFLAGS",
                          static_cast<s32>(mLastGameRecord.uSysflags & 0xFFFFEFFF));
        StartAction(E_ACTION_UNLOCK_GAME, &DefaultCallback);
    }

    void ServerInterfaceGames::KickPlayerByID(s32 liPlayerID, s32 liReason, char lbBan)
    {
        for (s32 i = 0; ; ++i)
        {
            CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
            if (i >= mLastGameRecord.iCount)
                break;
            if (mLastGameRecord.aOpponents[i].iIdent == liPlayerID)
            {
                KickPlayer(mLastGameRecord.aOpponents[i].strPers, liReason, lbBan);
                return;
            }
        }
    }

    void ServerInterfaceGames::SetGameServerConnectionType(s32 liConnectionType)
    {
        CGS_ASSERT(mpServerInterface->GetGameManagerRef() != 0,
                   "mpServerInterface->GetGameManagerRef()");
        CGS_ASSERT(mpServerInterface->GetConnAPIRef() != 0,
                   "mpServerInterface->GetConnAPIRef()");

        CgsNetwork::DirtySock::GameManagerRefT* lpGm = mpServerInterface->GetGameManagerRef();
        CgsNetwork::DirtySock::ConnApiRefT*     lpConn = mpServerInterface->GetConnAPIRef();
        switch (liConnectionType)
        {
        case E_GAME_SERVER_CONNECTION_TYPE_NONE:
            GameManagerControl(lpGm, KI_GM_SELECT_HASSRV, 0, 0, 0);
            ConnApiControl(lpConn, KI_CONN_CTRL_CONNTYPE, 3, 0, 0);
            break;
        case E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_BOTH:
            GameManagerControl(lpGm, KI_GM_SELECT_HASSRV, 1, 0, 0);
            ConnApiControl(lpConn, KI_CONN_CTRL_CONNTYPE, 3, 3, 0);
            break;
        case E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_VOIP_ONLY:
            GameManagerControl(lpGm, KI_GM_SELECT_HASSRV, 1, 0, 0);
            ConnApiControl(lpConn, KI_CONN_CTRL_CONNTYPE, 3, 2, 0);
            break;
        case E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_GAME_ONLY:
            GameManagerControl(lpGm, KI_GM_SELECT_HASSRV, 1, 0, 0);
            ConnApiControl(lpConn, KI_CONN_CTRL_CONNTYPE, 3, 1, 0);
            break;
        case E_GAME_SERVER_CONNECTION_TYPE_NO_FALLBACK:
            GameManagerControl(lpGm, KI_GM_SELECT_HASSRV, 1, 0, 0);
            ConnApiControl(lpConn, KI_CONN_CTRL_CONNTYPE, 3, 0, 0);
            break;
        default:
            CGS_ASSERT(false, "Invalid game server connection type\n");
            break;
        }
    }

    void ServerInterfaceGames::RegisterGameSearchSortCallback(SearchResultsSortCallback lpfnCallback,
                                                              void* lpUserData,
                                                              ServerInterfaceGameParamsBase* lpParamsA,
                                                              ServerInterfaceGameParamsBase* lpParamsB)
    {
        mpSearchSortCallback = lpfnCallback;
        mpSearchSortUserData = lpUserData;
        mpGameParamsA        = lpParamsA;
        mpGameParamsB        = lpParamsB;
        DispListSort(mpFoundGames, this, 0, &FoundGamesSort);
    }

    void ServerInterfaceGames::SendGameResult(const ServerInterfaceGameResultsBase* lpGameResults)
    {
        DirtySockDebugLog("DirtySockLobby: SendGameResult");

        char lacSelf[576];
        LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_USERINFO,
                       lacSelf, KI_STATUS_BUF_SIZE);

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");

        // When the game has a server-auth string, it must be present.
        if (((mLastGameRecord.uSysflags >> 18) & 1) != 0)
        {
            CGS_ASSERT(::strlen(mLastGameRecord.strAuth) > 0,
                       "strlen( mLastGameRecord.strAuth ) > 0");
        }

        lpcBuffer[0] = 0;
        TagFieldSetEpoch(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "WHEN",
                         static_cast<s32>(mLastGameRecord.uWhen));
        // REPT is the reporting (local) player's persona name -- lacSelf+8, same offset
        // IsLocalPlayerInGame reads for its own LobbyNameCmp against the self-status buffer.
        TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "REPT", lacSelf + 8);
        TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "AUTH", mLastGameRecord.strAuth);
        TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "VENUE", 0);
        TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "SKU", mpServerInterface->GetSKU());

        for (s32 i = 0; i < mLastGameRecord.iCount; ++i)
        {
            char lacKey[32];
            CgsCore::SPrintf(lacKey, 32, "NAME%d", i);
            TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, lacKey,
                              mLastGameRecord.aOpponents[i].strPers);
            CgsCore::SPrintf(lacKey, 32, "TEAM%d", i);
            TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, lacKey, i);
            CgsCore::SPrintf(lacKey, 32, "WEIGHT%d", i);
            TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, lacKey, 0);
        }

        lpGameResults->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);
        DirtySockDebugLog("Uploading: ");
        DirtySockDebugLog(lpcBuffer);
        DirtySockDebugLog("\n");

        StartAction(E_ACTION_SEND_RESULTS, &DefaultCallback);
    }

    void ServerInterfaceGames::UpdatePlayerParameters(s32 liPlayerID,
                                                      ServerInterfacePlayerParamsBase* lpPlayerParams)
    {
        CGS_ASSERT(lpPlayerParams != 0, "lpPlayerParams");
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        CGS_ASSERT(liPlayerID != -1, "Invalid player ID");

        s32 i = 0;
        for (; ; ++i)
        {
            CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
            if (i >= mLastGameRecord.iCount || mLastGameRecord.aOpponents[i].iIdent == liPlayerID)
                break;
        }

        char lacSelf[576];
        LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_USERINFO,
                       lacSelf, KI_STATUS_BUF_SIZE);
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        CGS_ASSERT(i < mLastGameRecord.iCount, "Could not find PlayerID");

        const s32 liLocalPlayerID = *reinterpret_cast<s32*>(lacSelf);
        const bool lbDifferentPlayer = (mLastGameRecord.aOpponents[i].iIdent != liLocalPlayerID);

        if (lbDifferentPlayer)
        {
            char lacSelf2[576];
            LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_USERINFO,
                           lacSelf2, KI_STATUS_BUF_SIZE);
            bool lbIsHost = false;
            if (IsLocalPlayerInGame() && *reinterpret_cast<s32*>(lacSelf2) == GetHostPlayerID())
                lbIsHost = true;
            CGS_ASSERT(lbIsHost,
                       "Not the host but trying to modify another player's parameters");
        }

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");
        lpcBuffer[0] = 0;

        lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");
        lpPlayerParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);

        if (lbDifferentPlayer)
        {
            char lacSelf3[576];
            LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_USERINFO,
                           lacSelf3, KI_STATUS_BUF_SIZE);
            bool lbIsHost = false;
            if (IsLocalPlayerInGame() && *reinterpret_cast<s32*>(lacSelf3) == GetHostPlayerID())
                lbIsHost = true;
            if (lbIsHost)
            {
                lpcBuffer = mpServerInterface->GetMessageBuffer();
                CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");
                TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "PERS",
                                  mLastGameRecord.aOpponents[i].strPers);
            }
        }

        StartAction(E_ACTION_UPDATE_PLAYER_PARAMS, &DefaultCallback);
    }

    // ===================================================================
    // Play-record diffing
    // ===================================================================

    void* ServerInterfaceGames::ProcessGameManagerPlayRecord()
    {
        DirtySock::LobbyApiPlayT lNewRecord;
        memcpy(&lNewRecord, &mLastGameRecord, KI_PLAYER_RECORD_SIZE);
        void* lpResult = reinterpret_cast<void*>(static_cast<intptr_t>(
            GameManagerStatus(mpServerInterface->GetGameManagerRef(), KI_GM_SELECT_PLAYREC,
                              &mLastGameRecord, KI_PLAYER_RECORD_SIZE)));

        if (mLastGameRecord.strName[0])
        {
            bool lbPlayersLeft = CheckForPlayerChange(&mLastGameRecord, &lNewRecord,
                                                      static_cast<EServerInterfaceEvent>(10),
                                                      static_cast<EServerInterfaceEvent>(13));
            bool lbPlayersJoined = CheckForPlayerChange(&lNewRecord, &mLastGameRecord,
                                                        static_cast<EServerInterfaceEvent>(11),
                                                        static_cast<EServerInterfaceEvent>(12));
            if (lbPlayersJoined || lbPlayersLeft)
                RaiseServerInterfaceEvent(mpServerInterface, KI_EVT_PLAYERS_CHANGED, 0);

            // Slots / privilege change -> push a latency-update control. asm sign-extends
            // bMaxsize (extsb) before comparing/subtracting.
            const s8 liMaxsize    = mLastGameRecord.iMaxsize;
            const s8 liNewMaxsize = lNewRecord.iMaxsize;
            if (liMaxsize != liNewMaxsize ||
                mLastGameRecord.iPrivSlots != lNewRecord.iPrivSlots)
            {
                ConnApiControl(mpServerInterface->GetConnAPIRef(), KI_CONN_CTRL_LATENCY,
                               liMaxsize - mLastGameRecord.iPrivSlots,
                               mLastGameRecord.iPrivSlots, 0);
            }

            if (::strcmp(mLastGameRecord.strParams, lNewRecord.strParams) != 0 ||
                mLastGameRecord.uSysflags != lNewRecord.uSysflags ||
                mLastGameRecord.uCustflags != lNewRecord.uCustflags)
            {
                RaiseServerInterfaceEvent(mpServerInterface, KI_EVT_GAME_PARAMS_CHANGED, 0);
            }

            lpResult = reinterpret_cast<void*>(static_cast<intptr_t>(
                CheckForPlayerParameterChange(&lNewRecord, &mLastGameRecord) ? 1 : 0));

            if (mLastGameRecord.iIdent != lNewRecord.iIdent &&
                mLastGameRecord.iIdent && lNewRecord.iIdent)
            {
                RaiseServerInterfaceEvent(mpServerInterface, KI_EVT_HOST_MIGRATED, 0);
                lpResult = mpServerInterface;
            }
        }
        return lpResult;
    }

    bool ServerInterfaceGames::CheckForPlayerChange(DirtySock::LobbyApiPlayT* lpA, DirtySock::LobbyApiPlayT* lpB,
                                                    EServerInterfaceEvent leAddEvent,
                                                    EServerInterfaceEvent leRemoveEvent)
    {
        char lacSelf[576];
        LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_USERINFO,
                       lacSelf, KI_STATUS_BUF_SIZE);
        const s32 liLocalPlayerID = *reinterpret_cast<s32*>(lacSelf);

        bool lbChanged = false;
        for (s32 i = 0; i < lpA->iCount; ++i)
        {
            const s32 liIdent = lpA->aOpponents[i].iIdent;
            bool lbStillPresent = false;
            for (s32 j = 0; j < lpB->iCount; ++j)
            {
                if (lpB->aOpponents[j].iIdent == liIdent)
                {
                    lbStillPresent = true;
                    break;
                }
            }
            if (!lbStillPresent)
            {
                lbChanged = true;
                RaiseServerInterfaceEvent(mpServerInterface, leAddEvent,
                                          reinterpret_cast<void*>(static_cast<intptr_t>(liIdent)));
                if (liIdent == liLocalPlayerID)
                    RaiseServerInterfaceEvent(mpServerInterface, leRemoveEvent, 0);
            }
        }
        return lbChanged;
    }

    bool ServerInterfaceGames::CheckForPlayerParameterChange(DirtySock::LobbyApiPlayT* lpA, DirtySock::LobbyApiPlayT* lpB)
    {
        bool lbChanged = false;
        for (s32 i = 0; i < lpA->iCount; ++i)
        {
            const s32 liIdent = lpA->aOpponents[i].iIdent;
            for (s32 j = 0; j < lpB->iCount; ++j)
            {
                if (lpB->aOpponents[j].iIdent == liIdent)
                {
                    if (::strcmp(lpA->aOpponents[i].strParams,
                                        lpB->aOpponents[j].strParams) != 0)
                    {
                        RaiseServerInterfaceEvent(mpServerInterface, 
                            static_cast<EServerInterfaceEvent>(KI_EVT_PARAM_CHANGED), 0);
                        lbChanged = true;
                    }
                    break;
                }
            }
        }
        return lbChanged;
    }

    // ===================================================================
    // Queries
    // ===================================================================

    s32 ServerInterfaceGames::GetGameID()
    {
        if (!IsLocalPlayerInGame())
            return -1;
        return mLastGameRecord.iIdent;
    }

    char* ServerInterfaceGames::GetGameName()
    {
        return mLastGameRecord.strName;
    }

    void ServerInterfaceGames::GetGameParameters(ServerInterfaceGameParamsBase* lpOut)
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        lpOut->SerialiseFromGame(&mLastGameRecord);
    }

    s32 ServerInterfaceGames::GetHostPlayerID()
    {
        char lacSelf[8];
        for (s32 i = 0; i < mLastGameRecord.iCount; ++i)
        {
            if (LobbyNameCmp(mLastGameRecord.strHost, mLastGameRecord.aOpponents[i].strPers) == 0)
                return mLastGameRecord.aOpponents[i].iIdent;
        }
        (void)lacSelf;
        CGS_ASSERT(false, "Host not found!");
        return -1;
    }

    s32 ServerInterfaceGames::GetNumberOfFoundGames()
    {
        CGS_ASSERT(mpFoundGames != 0, "mpFoundGames");
        return DispListCount(mpFoundGames);
    }

    s32 ServerInterfaceGames::GetNumberPlayersInGame()
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        return mLastGameRecord.iCount;
    }

    void* ServerInterfaceGames::GetPlayerParametersByIndex(s32 liIndex,
                                                           ServerInterfacePlayerParamsBase* lpOut)
    {
        CGS_ASSERT(lpOut != 0, "lpPlayerParams");
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        if (liIndex >= 0)
        {
            CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        }
        CGS_ASSERT(liIndex >= 0 && liIndex < mLastGameRecord.iCount, "Invalid player index");
        lpOut->SerialiseFromPlayer(&mLastGameRecord.aOpponents[liIndex]); return lpOut;
    }

    void* ServerInterfaceGames::GetPlayerParametersByPlayerID(s32 liPlayerID,
                                                             ServerInterfacePlayerParamsBase* lpOut)
    {
        CGS_ASSERT(lpOut != 0, "lpPlayerParams");
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        for (s32 i = 0; ; ++i)
        {
            CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
            if (i >= mLastGameRecord.iCount)
                break;
            if (mLastGameRecord.aOpponents[i].iIdent == liPlayerID)
            {
                lpOut->SerialiseFromPlayer(&mLastGameRecord.aOpponents[i]);
                return lpOut;
            }
        }
        CGS_ASSERT(false, "This player is not in the game");
        return 0;
    }

    void* ServerInterfaceGames::GetPlayerParametersByPlayerName(const char* lpcName,
                                                               ServerInterfacePlayerParamsBase* lpOut)
    {
        CGS_ASSERT(lpOut != 0, "lpPlayerParams");
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        for (s32 i = 0; ; ++i)
        {
            CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
            if (i >= mLastGameRecord.iCount)
                break;
            if (LobbyNameCmp(lpcName, mLastGameRecord.aOpponents[i].strPers) == 0)
            {
                lpOut->SerialiseFromPlayer(&mLastGameRecord.aOpponents[i]);
                return lpOut;
            }
        }
        CGS_ASSERT(false, "This player is not in the game");
        return 0;
    }

    bool ServerInterfaceGames::IsLocalPlayerInGame()
    {
        char lacSelf[576];
        LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_USERINFO,
                       lacSelf, KI_STATUS_BUF_SIZE);
        if (mLastGameRecord.iCount <= 0)
            return false;
        for (s32 i = 0; i < mLastGameRecord.iCount; ++i)
        {
            if (LobbyNameCmp(mLastGameRecord.aOpponents[i].strPers, lacSelf + 8) == 0)
                return true;
        }
        return false;
    }

    bool ServerInterfaceGames::IsLocalPlayerHost()
    {
        char lacSelf[576];
        LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_USERINFO,
                       lacSelf, KI_STATUS_BUF_SIZE);
        if (!IsLocalPlayerInGame())
            return false;
        s32 liHostID = GetHostPlayerID();
        return *reinterpret_cast<s32*>(lacSelf) == liHostID;
    }

    bool ServerInterfaceGames::IsPlayerInGame(const char* lpcName)
    {
        if (mLastGameRecord.iCount <= 0)
            return false;
        for (s32 i = 0; i < mLastGameRecord.iCount; ++i)
        {
            if (LobbyNameCmp(mLastGameRecord.aOpponents[i].strPers, lpcName) == 0)
                return true;
        }
        return false;
    }

    bool ServerInterfaceGames::IsGameLocked()
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        return ((ConvertFlags(mLastGameRecord.uSysflags, E_CONVERSION_FROM_WIRE) >> 5) & 1) != 0;
    }

    bool ServerInterfaceGames::IsGameStarted()
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        return ((ConvertFlags(mLastGameRecord.uSysflags, E_CONVERSION_FROM_WIRE) >> 11) & 1) != 0;
    }

    bool ServerInterfaceGames::IsGameServerGame()
    {
        if (!IsLocalPlayerInGame())
            return false;
        CgsNetwork::DirtySock::GameManagerRefT* lpGm = mpServerInterface->GetGameManagerRef();
        if (lpGm)
            return GameManagerStatus(lpGm, KI_GM_SELECT_ISSERVER, 0, 0) == 1;
        // No game-manager: a "game server" game iff the first player's persona name starts
        // with '@' (64) -- asm reads the SIGNED first byte of aOpponents[0].strPers.
        return static_cast<s8>(mLastGameRecord.aOpponents[0].strPers[0]) == 64;
    }

    // ===================================================================
    // Display list, events, leave / kick / start
    // ===================================================================

    void ServerInterfaceGames::AllocDisplayLists()
    {
        if (mpFoundGames == 0)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint
                    << "********++++++++++\nALLOCING FOUND GAMES\n********++++++++++\n";
            }
            mpFoundGames = LobbyApiListAlloc(mpServerInterface->GetLobbyAPIRef(), KI_LIST_FOUND_GAMES, NULL, NULL);
            CGS_ASSERT(mpFoundGames != 0, "Failed to create found game list");
        }
    }

    void ServerInterfaceGames::OnlyFinishOnErrorCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* lpMsg,
                                                         void* lpUserData)
    {
        // The create / join / quick-join requests finish on the game event instead; only
        // a failed request ends the action here.
        if (lpMsg->code != 0)
        {
            static_cast<ServerInterfaceGames*>(lpUserData)->EndAction(lpMsg->code);
        }
    }

    void ServerInterfaceGames::RespCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* /*lpMsg*/,
                                            void* /*lpUserData*/)
    {
    }

    void ServerInterfaceGames::OnEvent(EServerInterfaceEvent leEvent, void* /*lpData*/)
    {
        if (leEvent == static_cast<EServerInterfaceEvent>(0))   // lobby API created
        {
            AllocDisplayLists();
            if (mpSearchSortCallback != 0)
            {
                DispListSort(mpFoundGames, this, 0, &FoundGamesSort);
            }
            if (miEventCallback == -1)
            {
                miEventCallback = LobbyApiSetCallback(mpServerInterface->GetLobbyAPIRef(),
                                                      KI_CB_CHANNEL_EVENT,
                                                      &EventStatusCallback,
                                                      this);
            }
            if (miRespCallback == -1)
            {
                miRespCallback = LobbyApiSetCallback(mpServerInterface->GetLobbyAPIRef(),
                                                     KI_CB_CHANNEL_RESP,
                                                     &RespCallback,
                                                     this);
            }
            GameManagerSetCallback(mpServerInterface->GetGameManagerRef(),
                                   &GameManagerCallback, this);
        }
        else if (leEvent == static_cast<EServerInterfaceEvent>(1))   // lobby API destroying
        {
            if (miEventCallback != -1)
            {
                LobbyApiClearCallback(mpServerInterface->GetLobbyAPIRef(), miEventCallback);
                miEventCallback = -1;
            }
            if (miRespCallback != -1)
            {
                LobbyApiClearCallback(mpServerInterface->GetLobbyAPIRef(), miRespCallback);
                miRespCallback = -1;
            }
            FreeDisplayLists();
        }
        else if (leEvent == static_cast<EServerInterfaceEvent>(5))   // disconnected
        {
            meCurrentAction = E_ACTION_COUNT;
            XMemSet(&mLastGameRecord, 0, KI_PLAYER_RECORD_SIZE);
        }
    }

    bool ServerInterfaceGames::GetFoundGame(s32 liIndex, ServerInterfaceGameParamsBase* lpOut) const
    {
        if (liIndex >= 0)
        {
            CGS_ASSERT(mpFoundGames != 0, "mpFoundGames");
            if (liIndex < DispListCount(mpFoundGames))
            {
                void* lpRecord = DispListIndex(mpFoundGames, liIndex);
                if (lpRecord == 0)
                {
                    return false;
                }
                lpOut->SerialiseFromGame(lpRecord);
                return true;
            }
        }

        // The streamed text also reports the found-game total (an inlined
        // GetNumberOfFoundGames, with its own assert).
        CGS_ASSERT(mpFoundGames != 0, "mpFoundGames");
        CGS_ASSERT(liIndex >= 0 && liIndex < DispListCount(mpFoundGames), "Invalid game index specified: ");
        return false;
    }

    bool ServerInterfaceGames::IsLocalPlayerLeavingGame() const
    {
        return meCurrentAction == E_ACTION_LEAVE_GAME;
    }

    bool ServerInterfaceGames::IsPlayerInGame(s32 liPlayerID) const
    {
        for (s32 i = 0; i < mLastGameRecord.iCount; ++i)
        {
            if (mLastGameRecord.aOpponents[i].iIdent == liPlayerID)
                return true;
        }
        return false;
    }

    void ServerInterfaceGames::LeaveGame(bool lbSet, bool lbForce)
    {
        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        lpcBuffer[0] = 0;
        TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "SET", lbSet ? "1" : "0");
        TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "FORCE", lbForce ? "1" : "0");
        StartGameManagerAction(E_ACTION_LEAVE_GAME, &LeaveGameCallback);
    }

    void ServerInterfaceGames::KickPlayer(const char* lpcPlayerName, s32 liReason, char lbBan)
    {
        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        if (IsLocalPlayerInGame() && IsLocalPlayerHost())
        {
            lpcBuffer[0] = 0;
            TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "KICK", lpcPlayerName);
            TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "KICK_REASON", liReason);
            TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "KICK_SET", lbBan ? "1" : "0");
            StartAction(E_ACTION_KICK_PLAYER, &DefaultCallback);
        }
    }

    void ServerInterfaceGames::StartGame()
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        CGS_ASSERT(IsLocalPlayerHost(), "IsLocalPlayerHost()");

        mpServerInterface->GetMessageBuffer()[0] = 0;
        StartAction(E_ACTION_START_GAME, &DefaultCallback);
    }

    // ===================================================================
    // Free helpers (debug log + event raising). The X360 routes debug lines through a
    // global StrStream and raises component events through the interface's OnEvent slot.
    // ===================================================================

    void DirtySockDebugLog(const char* /*lpcText*/)
    {
        // The X360 forwards to off_82F335C8 (the network debug StrStream). Cosmetic only;
        // the sink lives in the development log subsystem (not reconstructed here).
    }

    void RaiseServerInterfaceEvent(ServerInterfaceDirtySock* lpInterface, s32 liEvent, void* lpData)
    {
        lpInterface->OnEvent(static_cast<EServerInterfaceEvent>(liEvent), lpData);
    }
}
