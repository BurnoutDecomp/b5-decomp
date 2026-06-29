#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // full ServerInterfaceDirtySock (GetLobbyAPIRef/GetGameManagerRef/GetConnAPIRef/GetMessageBuffer/GetSKU)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h" // ServerInterfaceComponent (StartActionCore/EndActionCore/ConvertError)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameSearchParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceQuickJoinParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceEndGameData.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePingRegions.h"
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
// FLAGGED placeholders (honest -- contents live in unrecovered .rdata):
//   * gServerInterfaceErrorData    -- the static error-string table (shared with the
//                                     other components; Construct points mpErrorData at it).
//   * KAPC_ACTION_NAMES[13]        -- per-action human-readable names ("Create Game" ...).
//   * KAU_ACTION_MESSAGE_TYPES[13] -- per-action DirtySDK request fourcc table.
//   * KA_DS_ERROR_TABLE_LOOKUP[13] -- per-action { DS-error-table*, count } pairs used by
//                                     EndAction -> ConvertError.
// Their element TYPES are reconstructed; their byte contents are not fabricated.

// ---- External DirtySDK / lobby C-API entry points used by this component. -------
// These live in the DirtySDK vendor SDK (not yet fully reconstructed in vendor/).
// A not-yet-homed callee is satisfied by its declaration under cl /c; no body needed.
struct LobbyApiRefT;
namespace CgsNetwork { namespace DirtySock { struct ConnApiRefT; struct GameManagerRefT; } }

extern "C"
{
    s32  LobbyApiStatus(LobbyApiRefT* pLobbyApi, s32 iSelect, void* pBuf, s32 iBufLen);
    s32  LobbyApiSetCallback(LobbyApiRefT* pLobbyApi, s32 iChannel, void* pCallback, void* pUserData);
    s32  LobbyApiClearCallback(LobbyApiRefT* pLobbyApi, s32 iCallback);
    s32  LobbyApiRequestCB(LobbyApiRefT* pLobbyApi, s32 iKind, const char* pRequest,
                           void* pCallback, void* pUserData);
    void* LobbyApiListFlush(LobbyApiRefT* pLobbyApi, s32 iList);
    s32  LobbyApiListFree(LobbyApiRefT* pLobbyApi, s32 iList, void* pList,
                          s32 a4, s32 a5, s32 a6, s32 a7, s32 a8);
    s32  LobbyNameCmp(const char* pNameA, const char* pNameB);
    void* XMemSet(void* pDest, s32 iValue, u32 uCount);

    s32  GameManagerStatus(CgsNetwork::DirtySock::GameManagerRefT* pGm, s32 iSelect, s32 a3, s32 a4);
    s32  GameManagerControl(CgsNetwork::DirtySock::GameManagerRefT* pGm, s32 iControl, s32 a3, s32 a4, s32 a5);
    s32  GameManagerSetCallback(CgsNetwork::DirtySock::GameManagerRefT* pGm, void* pCallback, void* pUserData);
    s32  GameManagerRequestCb(CgsNetwork::DirtySock::GameManagerRefT* pGm, s32 iKind, const char* pRequest,
                              void* pCallback, void* pUserData);

    s32  ConnApiControl(CgsNetwork::DirtySock::ConnApiRefT* pConn, s32 iControl, s32 a3, s32 a4, const void* a5);

    void* DispListClear(void* pList);
    s32  DispListOrder(void* pList);
    s32  DispListSort(void* pList, void* pUserData, s32 a3, void* pfnCompare);
    s32  AptRenderItem_Manager_GetMask(void* pList);

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
    // Static error-string data, defined in another (not-yet-reconstructed) TU (the same
    // table the sibling components point mpErrorData at; X360 unk_820046A7).
    extern const u8 gServerInterfaceErrorData[];

    // ConvertFlags: decode a LobbyApiPlayT's uSysflags into the logical flag word the
    // queries test (locked == bit 5, started == bit 11). Homed elsewhere in CgsNetwork.
    u32 ConvertFlags(u32 luSysflags, s32 liMode);

    // ConnApiCallback / AllocDisplayLists: referenced by Prepare; homed in the X360-derived
    // ServerInterfaceGamesX360 TU. Declared so Prepare can take their address.
    s32  ServerInterfaceGamesConnApiCallback(void* a1, void* a2, void* a3);

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

    // ---- FLAGGED placeholder rodata (contents in unrecovered .rdata). ----
    // Per-action human-readable name table (off_82F334D8, "Create Game" ...).
    extern const char* const KAPC_ACTION_NAMES[ServerInterfaceGames::E_ACTION_COUNT];
    // Per-action DirtySDK request message fourcc table (dword_820E8CAC).
    extern const s32 KAU_ACTION_MESSAGE_TYPES[ServerInterfaceGames::E_ACTION_COUNT];

    // Per-action { DS-error-table*, count } pair used by EndAction -> ConvertError
    // (off_820E9038 / dword_820E903C, interleaved as one array of pairs).
    struct ActionErrorTableEntry
    {
        const DSErrorToServerInterfaceError* mpTable;
        s32                                  miCount;
    };
    extern const ActionErrorTableEntry KA_DS_ERROR_TABLE_LOOKUP[ServerInterfaceGames::E_ACTION_COUNT];

    // ===================================================================
    // Lifecycle
    // ===================================================================

    void ServerInterfaceGames::Construct()
    {
        // The X360 stores the static error-string table ptr in the base's +0x04 slot
        // (mpcCurrentAction) and marks the component idle.
        mpcCurrentAction     = reinterpret_cast<const char*>(gServerInterfaceErrorData); // +0x04
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
                                              reinterpret_cast<void*>(&EventStatusCallback),
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
        mpServerInterface->SetConnApiGameCallback(
            reinterpret_cast<ServerInterfaceComponentData::ServerInterfaceConnApiCallback>(
                &ServerInterfaceGamesConnApiCallback));
        GameManagerSetCallback(mpServerInterface->GetGameManagerRef(),
                               reinterpret_cast<void*>(&GameManagerCallback),
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
            LobbyApiListFree(mpServerInterface->GetLobbyAPIRef(), KI_LIST_FOUND_GAMES,
                             mpFoundGames, 0, 0, 0, 0, 0);
            mpFoundGames = 0;
        }
        return mpFoundGames;
    }

    // ===================================================================
    // Action plumbing
    // ===================================================================

    void ServerInterfaceGames::StartAction(EAction leAction, void* lpfnCallback)
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

    s32 ServerInterfaceGames::StartGameManagerAction(EAction leAction, void* lpfnCallback)
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
            s32 lauMsg[6];
            XMemSet(lauMsg, 0, sizeof(lauMsg));
            lauMsg[3] = KI_GM_REQ_NULL;
            typedef s32 (*GmCallback)(void*, void*, ServerInterfaceGames*);
            GmCallback lpfn = reinterpret_cast<GmCallback>(lpfnCallback);
            liResult = lpfn(mpServerInterface->GetLobbyAPIRef(), lauMsg, this);
        }
        return liResult;
    }

    void ServerInterfaceGames::EndAction(s32 liError)
    {
        CGS_ASSERT(meCurrentAction != E_ACTION_COUNT, "meCurrentAction != E_ACTION_COUNT");

        const ActionErrorTableEntry& lrEntry = KA_DS_ERROR_TABLE_LOOKUP[meCurrentAction];
        EServerInterfaceError leMapped = ConvertError(liError, lrEntry.mpTable, lrEntry.miCount);
        EndActionCore(static_cast<int>(leMapped));

        meCurrentAction     = E_ACTION_COUNT;
        miRequestCallbackID = -1;
    }

    // ===================================================================
    // Static callbacks
    // ===================================================================

    s32 ServerInterfaceGames::DefaultCallback(s32 /*a1*/, s32* a2, ServerInterfaceGames* lpSelf)
    {
        // asm @0x82886920 tail-calls EndAction(self, msg[3]) and returns its result.
        // (EndAction -> EndActionCore returns the converted error code; the base
        // EndActionCore is declared void in this build, so we surface the success code.)
        lpSelf->EndAction(a2[3]);   // a2 + 12 -> the DirtySDK message error field
        return 0;
    }

    s32 ServerInterfaceGames::LeaveGameCallback(s32 /*a1*/, s32* a2, ServerInterfaceGames* lpSelf)
    {
        CGS_ASSERT(lpSelf != 0, "lpLobbyComponent");
        ConnApiControl(lpSelf->mpServerInterface->GetConnAPIRef(),
                       KI_CONN_CTRL_DISCONNECT, 0, 0, gServerInterfaceErrorData);
        if (lpSelf->IsLocalPlayerInGame())
        {
            s32 liError = a2[3];
            if (liError)
            {
                lpSelf->EndAction(liError);
                return 1;
            }
        }
        return lpSelf->IsLocalPlayerInGame() ? 1 : 0;
    }

    s32 ServerInterfaceGames::SearchForGamesCallback(s32 /*a1*/, s32* a2, ServerInterfaceGames* lpSelf)
    {
        s32 liError = a2[3];
        if (liError == 0)
        {
            void* lpField = TagFieldFind(reinterpret_cast<const char*>(a2[4]), "COUNT");
            if (TagFieldGetNumber(reinterpret_cast<const char*>(lpField), 0) == 0)
                liError = KI_ERR_NO_RESULTS;
        }
        lpSelf->EndAction(liError);
        return liError;
    }

    s32 ServerInterfaceGames::ReceivedGameEvent(ServerInterfaceGames* lpSelf, s32* a2)
    {
        if (static_cast<u32>(lpSelf->meCurrentAction) < 3u)
        {
            void* lpField = TagFieldFind(reinterpret_cast<const char*>(a2[4]), "IDENT");
            s32 liIdent = TagFieldGetNumber(reinterpret_cast<const char*>(lpField), -1);
            if (liIdent == lpSelf->mLastGameRecord.iIdent)
            {
                lpSelf->EndAction(a2[3]);
                return a2[3];
            }
            return liIdent;
        }
        return 0;
    }

    void ServerInterfaceGames::EventStatusCallback(s32 /*a1*/, s32* a2, ServerInterfaceGames** a3)
    {
        ServerInterfaceGames* lpSelf = *a3;
        switch (a2[2])     // a2 + 8 -> the lobby event tag
        {
        case KI_EVENT_TAG_PLAYERS:
            RaiseServerInterfaceEvent(lpSelf->mpServerInterface, KI_EVT_PLAYER_PARAMS_CHANGED, 0);
            break;
        case KI_EVENT_TAG_RESET:
            // Re-run the play-record processing (vtable slot 17 of the games component).
            lpSelf->ProcessGameManagerPlayRecord();
            break;
        case KI_EVENT_TAG_KICKED:
            {
                void* lpField = TagFieldFind(reinterpret_cast<const char*>(a2[4]), "REASON");
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

    void* ServerInterfaceGames::GameManagerCallback(s32 /*a1*/, s32* a2, ServerInterfaceGames* lpSelf)
    {
        switch (a2[0])
        {
        case 0:
        case 1:
        case 2:
            return lpSelf;
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
            ConnApiControl(lpSelf->mpServerInterface->GetConnAPIRef(), KI_CONN_CTRL_DISCONNECT, 0, 0, gServerInterfaceErrorData);
            return lpSelf;
        case 4:
            return lpSelf->ProcessGameManagerPlayRecord();
        default:
            CGS_ASSERT(false, "Unknown GameManager event");
            return lpSelf;
        }
    }

    s32 ServerInterfaceGames::FoundGamesSort(ServerInterfaceGames* lpSelf, void* /*a2*/,
                                             ServerInterfaceGameParamsBase* lpA,
                                             ServerInterfaceGameParamsBase* lpB)
    {
        if (!lpSelf->mpGameParamsA || !lpSelf->mpGameParamsB)
            return 0;
        lpSelf->mpGameParamsA->DeserialiseFromString(reinterpret_cast<const char*>(lpA));
        lpSelf->mpGameParamsB->DeserialiseFromString(reinterpret_cast<const char*>(lpB));
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

        StartGameManagerAction(E_ACTION_CREATE_GAME,
                               reinterpret_cast<void*>(&OnlyFinishOnErrorCallback));
    }

    void ServerInterfaceGames::JoinGame(ServerInterfaceGameParamsBase* lpGameParams,
                                        ServerInterfacePlayerParamsBase* lpPlayerParams)
    {
        DirtySockDebugLog("DirtySock: JoinGame\n");

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        lpGameParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);
        lpPlayerParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);

        StartGameManagerAction(E_ACTION_JOIN_GAME,
                               reinterpret_cast<void*>(&OnlyFinishOnErrorCallback));
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

        StartGameManagerAction(E_ACTION_QUICK_JOIN_GAME,
                               reinterpret_cast<void*>(&OnlyFinishOnErrorCallback));
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

        StartAction(E_ACTION_SEARCH_FOR_GAMES,
                    reinterpret_cast<void*>(&SearchForGamesCallback));
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
        StartAction(E_ACTION_CANCEL_SEARCH_FOR_GAMES, reinterpret_cast<void*>(&DefaultCallback));
    }

    void ServerInterfaceGames::UpdateGameParameters(ServerInterfaceGameParamsBase* lpGameParams)
    {
        CGS_ASSERT(IsLocalPlayerHost(), "IsLocalPlayerHost()");

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        lpGameParams->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);
        StartAction(E_ACTION_UPDATE_GAME_PARAMS, reinterpret_cast<void*>(&DefaultCallback));
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
        StartAction(E_ACTION_LOCK_GAME, reinterpret_cast<void*>(&DefaultCallback));
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
        StartAction(E_ACTION_UNLOCK_GAME, reinterpret_cast<void*>(&DefaultCallback));
    }

    void* ServerInterfaceGames::KickPlayerByID(s32 liPlayerID, s32 liReason, char lbBan)
    {
        for (s32 i = 0; ; ++i)
        {
            CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
            if (i >= mLastGameRecord.iCount)
                break;
            if (mLastGameRecord.aPlayers[i].iIdent == liPlayerID)
                return KickPlayer(mLastGameRecord.aPlayers[i].strPers, liReason, lbBan);
        }
        return reinterpret_cast<void*>(static_cast<intptr_t>(IsLocalPlayerInGame() ? 1 : 0));
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
        DispListSort(mpFoundGames, this, 0, reinterpret_cast<void*>(&FoundGamesSort));
    }

    void ServerInterfaceGames::SendGameResult(ServerInterfaceEndGameDataBase* lpEndGameData,
                                              void* /*lpUserData*/)
    {
        DirtySockDebugLog("DirtySockLobby: SendGameResult");

        char lacSelf[8];
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

        char lacReport[1] = { 0 };
        lpcBuffer[0] = 0;
        TagFieldSetEpoch(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "WHEN",
                         static_cast<s32>(mLastGameRecord.uWhen));
        TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "REPT", lacReport);
        TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "AUTH", mLastGameRecord.strAuth);
        TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "VENUE", 0);
        TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "SKU", mpServerInterface->GetSKU());

        for (s32 i = 0; i < mLastGameRecord.iCount; ++i)
        {
            char lacKey[32];
            CgsCore::SPrintf(lacKey, 32, "NAME%d", i);
            TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LEN, lacKey,
                              mLastGameRecord.aPlayers[i].strPers);
            CgsCore::SPrintf(lacKey, 32, "TEAM%d", i);
            TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, lacKey, i);
            CgsCore::SPrintf(lacKey, 32, "WEIGHT%d", i);
            TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, lacKey, 0);
        }

        lpEndGameData->SerialiseToString(lpcBuffer, KI_MESSAGE_BUFFER_LEN);
        DirtySockDebugLog("Uploading: ");
        DirtySockDebugLog(lpcBuffer);
        DirtySockDebugLog("\n");

        StartAction(E_ACTION_SEND_RESULTS, reinterpret_cast<void*>(&DefaultCallback));
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
            if (i >= mLastGameRecord.iCount || mLastGameRecord.aPlayers[i].iIdent == liPlayerID)
                break;
        }

        char lacSelf[576];
        LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_USERINFO,
                       lacSelf, KI_STATUS_BUF_SIZE);
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        CGS_ASSERT(i < mLastGameRecord.iCount, "Could not find PlayerID");

        const s32 liLocalPlayerID = *reinterpret_cast<s32*>(lacSelf);
        const bool lbDifferentPlayer = (mLastGameRecord.aPlayers[i].iIdent != liLocalPlayerID);

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
                                  mLastGameRecord.aPlayers[i].strPers);
            }
        }

        StartAction(E_ACTION_UPDATE_PLAYER_PARAMS, reinterpret_cast<void*>(&DefaultCallback));
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
                              reinterpret_cast<intptr_t>(&mLastGameRecord), KI_PLAYER_RECORD_SIZE)));

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

            // Slots / privilege change -> push a latency-update control.
            if (mLastGameRecord.bMaxsize != lNewRecord.bMaxsize ||
                mLastGameRecord.iPrivSlots != lNewRecord.iPrivSlots)
            {
                ConnApiControl(mpServerInterface->GetConnAPIRef(), KI_CONN_CTRL_LATENCY,
                               mLastGameRecord.bMaxsize - mLastGameRecord.iPrivSlots,
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
            const s32 liIdent = lpA->aPlayers[i].iIdent;
            bool lbStillPresent = false;
            for (s32 j = 0; j < lpB->iCount; ++j)
            {
                if (lpB->aPlayers[j].iIdent == liIdent)
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
            const s32 liIdent = lpA->aPlayers[i].iIdent;
            for (s32 j = 0; j < lpB->iCount; ++j)
            {
                if (lpB->aPlayers[j].iIdent == liIdent)
                {
                    if (::strcmp(lpA->aPlayers[i].strParams,
                                        lpB->aPlayers[j].strParams) != 0)
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

    bool ServerInterfaceGames::GetGameParameters(ServerInterfaceGameParamsBase* lpOut)
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        return lpOut->DeserialiseFromString(reinterpret_cast<const char*>(&mLastGameRecord.iIdent));
    }

    s32 ServerInterfaceGames::GetHostPlayerID()
    {
        char lacSelf[8];
        for (s32 i = 0; i < mLastGameRecord.iCount; ++i)
        {
            if (LobbyNameCmp(mLastGameRecord.strHost, mLastGameRecord.aPlayers[i].strPers) == 0)
                return mLastGameRecord.aPlayers[i].iIdent;
        }
        (void)lacSelf;
        CGS_ASSERT(false, "Host not found!");
        return -1;
    }

    s32 ServerInterfaceGames::GetNumberOfFoundGames()
    {
        CGS_ASSERT(mpFoundGames != 0, "mpFoundGames");
        return AptRenderItem_Manager_GetMask(mpFoundGames);
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
        lpOut->SerialiseFromPlayer(&mLastGameRecord.aPlayers[liIndex]); return lpOut;
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
            if (mLastGameRecord.aPlayers[i].iIdent == liPlayerID)
                lpOut->SerialiseFromPlayer(&mLastGameRecord.aPlayers[i]); return lpOut;
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
            if (LobbyNameCmp(lpcName, mLastGameRecord.aPlayers[i].strPers) == 0)
                lpOut->SerialiseFromPlayer(&mLastGameRecord.aPlayers[i]); return lpOut;
        }
        CGS_ASSERT(false, "This player is not in the game");
        return 0;
    }

    bool ServerInterfaceGames::IsLocalPlayerInGame()
    {
        char lacSelf[16];
        LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_USERINFO,
                       lacSelf, KI_STATUS_BUF_SIZE);
        if (mLastGameRecord.iCount <= 0)
            return false;
        for (s32 i = 0; i < mLastGameRecord.iCount; ++i)
        {
            if (LobbyNameCmp(mLastGameRecord.aPlayers[i].strPers, lacSelf + 8) == 0)
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
            if (LobbyNameCmp(mLastGameRecord.aPlayers[i].strPers, lpcName) == 0)
                return true;
        }
        return false;
    }

    bool ServerInterfaceGames::IsGameLocked()
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        return ((ConvertFlags(mLastGameRecord.uSysflags, 0) >> 5) & 1) != 0;
    }

    bool ServerInterfaceGames::IsGameStarted()
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");
        return ((ConvertFlags(mLastGameRecord.uSysflags, 0) >> 11) & 1) != 0;
    }

    bool ServerInterfaceGames::IsGameServerGame()
    {
        if (!IsLocalPlayerInGame())
            return false;
        CgsNetwork::DirtySock::GameManagerRefT* lpGm = mpServerInterface->GetGameManagerRef();
        if (lpGm)
            return GameManagerStatus(lpGm, KI_GM_SELECT_ISSERVER, 0, 0) == 1;
        // No game-manager: a "game server" game iff the first host name byte equals '@' (64).
        return mLastGameRecord.aPlayers[0].iIdent == 64;
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
