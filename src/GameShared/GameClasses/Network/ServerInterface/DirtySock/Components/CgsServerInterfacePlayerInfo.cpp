#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.h" // ServerInterfacePlayerInfoDataBase
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"   // ServerInterfaceDirtySock (GetLobbyAPIRef/GetSettingRef/GetMessageBuffer)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h" // DSErrorToServerInterfaceError(Table)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include "lobbyapi.h"        // LobbyApiStatus / LobbyApiRequestCB / LobbyApiSetCallback / LobbyApiClearCallback
#include "lobbyfinduser.h"   // LobbyFindUser* family
#include "lobbystatbook.h"   // LobbyStatbook* family
#include "lobbysetting.h"    // LobbySetting* family
#include "lobbytagfield.h"   // TagFieldFind / TagFieldGetString / TagFieldSetString
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h"

#include <string.h>          // _strnicmp

#include "dirtyaddr.h"       // DirtyAddrToHostAddr

// ===========================================================================
// CgsNetwork::ServerInterfacePlayerInfo
//
// Reconstructed from the X360 ARTIST pseudocode + asm (the spine), the
// CgsServerInterfacePlayerInfo.h DWARF (declaration shape), and the Feb-2007 idioms.
// Only the 21 functions present in this TU's export are bodied here; the remaining
// declared members belong to functions not in this export.
//
// The three per-action tables below (names, lobby request codes, DirtySock-error
// mappings) hold the console rodata values word for word.
// ===========================================================================

namespace CgsNetwork
{
    // The per-action display names indexed by EAction (passed to StartActionCore).
    static const char* const KAPC_ACTION_NAMES[ServerInterfacePlayerInfo::E_ACTION_COUNT] =
    {
        "Finding User Data",          // 0
        "Updating auxiliary data",    // 1
        "Selecting stat view",        // 2
        "Downloading stats",          // 3
        "Downloading view info",      // 4
        "Uploading user feedback",    // 5
        "Update settings",            // 6
        "Load settings",              // 7
        "Getting user xuids"          // 8
    };

    // The lobby request code StartAction sends for each action. 0x20 marks the actions
    // that are not lobby requests.
    static const s32 KAI_ACTION_CODE_MAPPING[ServerInterfacePlayerInfo::E_ACTION_COUNT] =
    {
        0x20,
        0x61757869,   // 'auxi'
        0x20,
        0x20,
        0x20,
        0x72657074,   // 'rept'
        0x20,
        0x20,
        0x6D616472    // 'madr'
    };

    // The DirtySock-error -> EServerInterfaceError mappings, laid out as one contiguous
    // block because that is how the console reads them: the load-settings lookup entry
    // below carries a count of 4 over a 3-entry table, so ConvertError also reads the
    // following 8 rodata bytes (the string "NEWS_URL"). That word pair is reproduced as
    // the block's last entry; its DirtySock code 'NEWS' is never returned by the lobby.
    static const DSErrorToServerInterfaceError KA_PLAYER_INFO_DS_ERROR_MAPPINGS[32] =
    {
        // [0] find user (the console pairs 'soon' with 55 and 'many' with 56)
        { 0x736F6F6E, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_FIND_BUSY },                 // 'soon'
        { 0x6D616E79, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_REQUEST_TOO_SOON },          // 'many'
        { 0x696E7670, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_FIND_INVALID_PARAMETERS },   // 'invp'
        // [3] update auxiliary data
        { 0x736F6F6E, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_FIND_BUSY },                 // 'soon'
        { 0x6D616E79, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_REQUEST_TOO_SOON },          // 'many'
        { 0x696E7670, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_FIND_INVALID_PARAMETERS },   // 'invp'
        // [6] stat-book actions (negative stat-book result codes)
        { -1, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_STAT_MISC },
        { -2, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_STAT_INVALID_VIEW },
        { -3, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_STAT_PLAYER_NOT_FOUND },
        { -4, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_STAT_VIEW_NOT_SELECTED },
        { -5, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_STAT_BAD_SLOT },
        { -6, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_STAT_BUSY },
        { -7, E_SERVER_INTERFACE_PLAYER_INFO_ERROR_STAT_TIMEOUT },
        // [13] user feedback
        { 0,          E_SERVER_INTERFACE_ERROR_NONE },
        { 0x6D617574, E_SERVER_INTERFACE_FEEDBACK_ERROR_MASTER_AUTH },          // 'maut'
        { 0x70617574, E_SERVER_INTERFACE_FEEDBACK_ERROR_PERSONA_AUTH },         // 'paut'
        { 0x64697361, E_SERVER_INTERFACE_FEEDBACK_ERROR_REPORTING_DISABLED },   // 'disa'
        { 0x69706572, E_SERVER_INTERFACE_FEEDBACK_ERROR_INVALID_PLAYER },       // 'iper'
        { 0x73656C66, E_SERVER_INTERFACE_FEEDBACK_ERROR_SELF },                 // 'self'
        { 0x776F6C66, E_SERVER_INTERFACE_FEEDBACK_ERROR_PLAYER_NOT_POSTED },    // 'wolf'
        // [20] update settings
        { 0,          E_SERVER_INTERFACE_ERROR_NONE },
        { 0x6D697373, E_SERVER_INTERFACE_PLAYER_INFO_UPDATE_SETTINGS_ERROR_MISSING_PARAM },     // 'miss'
        { 0x64626572, E_SERVER_INTERFACE_PLAYER_INFO_UPDATE_SETTINGS_ERROR_DATABASE_ERROR },    // 'dber'
        { 0x69706572, E_SERVER_INTERFACE_PLAYER_INFO_UPDATE_SETTINGS_ERROR_INVALID_PERSONA },   // 'iper'
        // [24] load settings
        { 0,          E_SERVER_INTERFACE_ERROR_NONE },
        { 0x6D697373, E_SERVER_INTERFACE_PLAYER_INFO_LOAD_SETTINGS_ERROR_MISSING_PARAM },       // 'miss'
        { 0x64626572, E_SERVER_INTERFACE_PLAYER_INFO_LOAD_SETTINGS_ERROR_DATABASE_ERROR },      // 'dber'
        { 0x69706572, E_SERVER_INTERFACE_PLAYER_INFO_LOAD_SETTINGS_ERROR_INVALID_PERSONA },     // 'iper'
        // [28] get user xuids
        { 0,          E_SERVER_INTERFACE_ERROR_NONE },
        { 0x6D697373, E_SERVER_INTERFACE_GENERAL_ERROR_MISSING_PARAMS },        // 'miss'
        { 0x6E666E64, E_SERVER_INTERFACE_GENERAL_ERROR_INVALID_PERSONA },       // 'nfnd'
        // [31] the 8 bytes after the block ("NEWS_URL"), read through the count of 4
        { 0x4E455753, static_cast<EServerInterfaceError>(0x5F55524C) },
    };

    // Per-action { mapping table, count }. The three stat-book actions share one table.
    static const DSErrorToServerInterfaceErrorTable
        KA_DS_ERROR_TABLE_LOOKUP[ServerInterfacePlayerInfo::E_ACTION_COUNT] =
    {
        { &KA_PLAYER_INFO_DS_ERROR_MAPPINGS[0],  3 },
        { &KA_PLAYER_INFO_DS_ERROR_MAPPINGS[3],  3 },
        { &KA_PLAYER_INFO_DS_ERROR_MAPPINGS[6],  7 },
        { &KA_PLAYER_INFO_DS_ERROR_MAPPINGS[6],  7 },
        { &KA_PLAYER_INFO_DS_ERROR_MAPPINGS[6],  7 },
        { &KA_PLAYER_INFO_DS_ERROR_MAPPINGS[13], 7 },
        { &KA_PLAYER_INFO_DS_ERROR_MAPPINGS[20], 4 },
        { &KA_PLAYER_INFO_DS_ERROR_MAPPINGS[24], 4 },
        { &KA_PLAYER_INFO_DS_ERROR_MAPPINGS[28], 4 }
    };

    // -----------------------------------------------------------------------------------
    // Construct @ 0x82878B78  [EXECUTED in goal trace]
    //   Reset to the idle leaf state. The base fields are set the same way the shared
    //   ServerInterfaceComponent::Construct would (vtable action ptr, status idle, error 0).
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::Construct()
    {
        // Base (+0x04..+0x0C): meStatus = 2 (idle), mpcCurrentAction set, miLastError = 0.
        ServerInterfaceComponent::Construct();

        mpServerInterface = 0;                       // +0x10
        meCurrentAction   = E_ACTION_COUNT;          // +0x14 (9)
        mpPlayerInfo      = 0;                        // +0x18
        mpFindUser        = 0;                        // +0x1C
        mpStatbook        = 0;                        // +0x20
        miEventCallback   = -1;                       // +0x24
        mbUseCachedAccountSettings = false;           // +0x28: asm zeroes all four flag bytes (one stw)
        mbAgreeToShareInfoEA       = false;
        mbAgreeToShareInfoPartners = false;
        mbTelemetryEnable          = false;
    }

    // -----------------------------------------------------------------------------------
    // Destruct @ 0x82878BC0
    //   Same field reset as Construct but without re-touching the base action/status.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::Destruct()
    {
        mpServerInterface = 0;                        // +0x10
        meCurrentAction   = E_ACTION_COUNT;           // +0x14 (9)
        mpPlayerInfo      = 0;                         // +0x18
        mpFindUser        = 0;                         // +0x1C
        mpStatbook        = 0;                         // +0x20
        miEventCallback   = -1;                        // +0x24
        mbUseCachedAccountSettings = false;            // +0x28: asm zeroes all four flag bytes (one stw)
        mbAgreeToShareInfoEA       = false;
        mbAgreeToShareInfoPartners = false;
        mbTelemetryEnable          = false;
    }

    // -----------------------------------------------------------------------------------
    // Prepare @ 0x82888408
    //   Bind to the server interface, create the find-user + statbook modules, register
    //   the event-status callback, and set the find-user request pacing.
    // -----------------------------------------------------------------------------------
    bool ServerInterfacePlayerInfo::Prepare(ServerInterfaceDirtySock* lpServerInterface)
    {
        CGS_ASSERT(miEventCallback == -1, "miEventCallback == -1");
        CGS_ASSERT(!mpFindUser, "!mpFindUser");
        CGS_ASSERT(!mpStatbook, "!mpStatbook");

        mpServerInterface = lpServerInterface;
        mpPlayerInfo      = 0;
        meCurrentAction   = E_ACTION_COUNT;
        mbUseCachedAccountSettings = false;   // +0x28: asm zeroes all four flag bytes (one stw)
        mbAgreeToShareInfoEA       = false;
        mbAgreeToShareInfoPartners = false;
        mbTelemetryEnable          = false;

        mpFindUser = LobbyFindUserCreate(mpServerInterface->GetLobbyAPIRef());
        CGS_ASSERT(mpFindUser, "mpFindUser");

        mpStatbook = LobbyStatbookCreate(mpServerInterface->GetLobbyAPIRef(), 1);
        CGS_ASSERT(mpStatbook, "mpStatbook");

        miEventCallback = LobbyApiSetCallback(mpServerInterface->GetLobbyAPIRef(), 4,
                                              &ServerInterfacePlayerInfo::EventStatusCallback,
                                              this);

        // KI_MS_BETWEEN_PLAYER_INFO_REQUESTS (1000) / KI_MS_FOR_LOBBY_CACHE_EXPIRE (20000).
        LobbyFindUserSetParams(mpFindUser, 1000, 20000);
        return true;
    }

    // -----------------------------------------------------------------------------------
    // Release @ 0x82878BF0
    //   Tear down the lobby callback + modules and return to the idle state.
    // -----------------------------------------------------------------------------------
    bool ServerInterfacePlayerInfo::Release()
    {
        if (miEventCallback != -1)
        {
            LobbyApiClearCallback(mpServerInterface->GetLobbyAPIRef(), miEventCallback);
            miEventCallback = -1;
        }
        if (mpFindUser)
        {
            LobbyFindUserDestroy(mpFindUser);
            mpFindUser = 0;
        }
        if (mpStatbook)
        {
            LobbyStatbookDestroy(mpStatbook);
            mpStatbook = 0;
        }

        mpServerInterface = 0;
        mpPlayerInfo      = 0;
        mbUseCachedAccountSettings = false;   // +0x28: asm zeroes all four flag bytes (one stw)
        mbAgreeToShareInfoEA       = false;
        mbAgreeToShareInfoPartners = false;
        mbTelemetryEnable          = false;
        meCurrentAction   = E_ACTION_COUNT;
        return true;
    }

    // -----------------------------------------------------------------------------------
    // OnEvent @ 0x82888640
    //   Event 0 (CONNECTED): re-establish the callback + modules.
    //   Event 1 (DISCONNECTED): tear them down.
    //   Event 5 (RESET): force idle.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::OnEvent(EServerInterfaceEvent leEvent, void* /*lpData*/)
    {
        const s32 liEvent = static_cast<s32>(leEvent);

        if (liEvent == 0)
        {
            if (miEventCallback == -1)
            {
                miEventCallback = LobbyApiSetCallback(
                    mpServerInterface->GetLobbyAPIRef(), 4,
                    &ServerInterfacePlayerInfo::EventStatusCallback, this);
            }
            if (!mpFindUser)
            {
                mpFindUser = LobbyFindUserCreate(mpServerInterface->GetLobbyAPIRef());
                CGS_ASSERT(mpFindUser, "mpFindUser");
            }
            if (!mpStatbook)
            {
                mpStatbook = LobbyStatbookCreate(mpServerInterface->GetLobbyAPIRef(), 1);
                CGS_ASSERT(mpStatbook, "mpStatbook");
            }
        }
        else if (liEvent == 1)
        {
            if (miEventCallback != -1)
            {
                LobbyApiClearCallback(mpServerInterface->GetLobbyAPIRef(), miEventCallback);
                miEventCallback = -1;
            }
            if (mpFindUser)
            {
                LobbyFindUserDestroy(mpFindUser);
                mpFindUser = 0;
            }
            if (mpStatbook)
            {
                LobbyStatbookDestroy(mpStatbook);
                mpStatbook = 0;
            }
        }
        else if (liEvent == 5)
        {
            meCurrentAction = E_ACTION_COUNT;
            mpPlayerInfo    = 0;
        }
    }

    // -----------------------------------------------------------------------------------
    // EndAction @ 0x82878DD0
    //   Map the raw lobby error for the current action and finish the action.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::EndAction(int liError)
    {
        const DSErrorToServerInterfaceErrorTable& lEntry =
            KA_DS_ERROR_TABLE_LOOKUP[meCurrentAction];
        EServerInterfaceError leError =
            ServerInterfaceComponent::ConvertError(liError, lEntry.mpMappingTable,
                                                   lEntry.miNumMappings);
        EndActionCore(static_cast<int>(leError));
        meCurrentAction = E_ACTION_COUNT;
    }

    // -----------------------------------------------------------------------------------
    // StartAction @ 0x82878D40
    //   Begin the action: record it, name it, and issue the lobby request.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::StartAction(EAction leAction, LobbyApiCallbackT* lpfCallback)
    {
        meCurrentAction = leAction;
        StartActionCore(KAPC_ACTION_NAMES[leAction]);

        CGS_ASSERT(mpServerInterface->GetMessageBuffer() != 0, "mpacMessageBuffer");

        LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(),
                          KAI_ACTION_CODE_MAPPING[meCurrentAction],
                          mpServerInterface->GetMessageBuffer(),
                          lpfCallback, this);
    }

    // -----------------------------------------------------------------------------------
    // GetPlayerXUIDByName @ 0x82888918
    //   ADDITIVE GROW (BrnNetwork::NetworkGamerCardManagerX360 TU): resolve a player's
    //   64-bit XUID from a gamer name. Stash the destination XUID slot at +0x28 (which
    //   otherwise holds the four account-settings flag bytes -- the two paths never
    //   coexist), seed the shared message buffer under the "PERS" tag, and issue the lobby
    //   request via the E_ACTION_LOAD_SETTINGS slot with the XUID completion callback.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::GetPlayerXUIDByName(const PlayerName* lpcPlayerName,
                                                        u64* lpXuidOut)
    {
        CGS_ASSERT(lpXuidOut, "lpXUID");
        CGS_ASSERT(lpcPlayerName, "lpPlayerName");

        mpXUID = lpXuidOut;   // +0x28 (aliases the account-settings flag bytes; stw @0x82888984)

        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");
        lpcBuffer[0] = 0;

        // KI_MESSAGE_BUFFER_LEN (2048). The persona name reaches TagFieldSetString as a
        // const char* (the name buffer).
        TagFieldSetString(mpServerInterface->GetMessageBuffer(), 2048, "PERS",
                          reinterpret_cast<const char*>(lpcPlayerName));

        StartAction(E_ACTION_LOAD_SETTINGS, &ServerInterfacePlayerInfo::_GetPlayerXUIDCallback);
    }

    // -----------------------------------------------------------------------------------
    // _GetPlayerXUIDCallback @ 0x828796A0 (static; LobbyApiCallbackT-shaped)
    //   Lobby reports the resolved record for the XUID lookup. The component arrives via
    //   the void* user-data param (r5). On success (result == 0) decode the "MADDR" tag
    //   field into the caller's XUID slot via DirtyAddrToHostAddr; otherwise zero it. Then
    //   clear the pending XUID pointer, map the lobby result for the action, and finish.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::_GetPlayerXUIDCallback(LobbyApiRefT* /*lpRef*/,
                                                           LobbyApiMsgT* lpMsg, void* lpData)
    {
        ServerInterfacePlayerInfo* lpComponent =
            static_cast<ServerInterfacePlayerInfo*>(lpData);
        CGS_ASSERT(lpComponent->mpXUID, "lpServerInterface->mpXUID");

        // The lobby result code and the record text.
        const s32 liResult = lpMsg->code;
        if (liResult == 0)
        {
            const char* lpField = TagFieldFind(lpMsg->pData, "MADDR");
            DirtyAddrToHostAddr(lpComponent->mpXUID, 8, reinterpret_cast<const DirtyAddrT*>(lpField));
        }
        else
        {
            *lpComponent->mpXUID = 0;
        }

        const EAction leAction = lpComponent->meCurrentAction;   // read +0x14 before clearing
        lpComponent->mpXUID = 0;                                 // stw 0,0x28

        const DSErrorToServerInterfaceErrorTable& lEntry = KA_DS_ERROR_TABLE_LOOKUP[leAction];
        EServerInterfaceError leError =
            lpComponent->ServerInterfaceComponent::ConvertError(
                liResult, lEntry.mpMappingTable, lEntry.miNumMappings);
        lpComponent->EndActionCore(static_cast<int>(leError));
        lpComponent->meCurrentAction = E_ACTION_COUNT;
    }

    // -----------------------------------------------------------------------------------
    // FindUserCallback @ 0x82878EC0 (static)
    //   Lobby reports a resolved user record; serialise it into the pending player-info
    //   destination and finish the FIND_USER action.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::FindUserCallback(void* lpUserData, const char* /*lpcName*/,
                                                     LobbyApiUserT* lpUser)
    {
        CGS_ASSERT(lpUser, "lpUser");

        ServerInterfacePlayerInfo* lpComponent =
            static_cast<ServerInterfacePlayerInfo*>(lpUserData);
        CGS_ASSERT(lpUserData, "lpUserData");                 // asm assert @737
        CGS_ASSERT(lpComponent, "lpPlayerInfoComponent");     // asm assert @741

        lpComponent->mpPlayerInfo->SerialiseFromUser(lpUser);
        lpComponent->EndActionCore(0);
        lpComponent->meCurrentAction = E_ACTION_COUNT;
    }

    // -----------------------------------------------------------------------------------
    // GetLocalPlayerInfo @ 0x82878E30
    //   Query the local 'self' lobby user record and serialise it into lpPlayerInfo.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::GetLocalPlayerInfo(ServerInterfacePlayerInfoDataBase* lpPlayerInfo)
    {
        CGS_ASSERT(lpPlayerInfo, "lpPlayerInfo");

        // Selector fourcc 0x73656C66 == 'self', buffer 0x234 (564) bytes into a local
        // scratch (recovered from the asm immediates @ 0x82878E78/0x82878E7C/0x82878E7C).
        u8 lacUserData[568];
        LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), 0x73656C66, lacUserData, 564);
        lpPlayerInfo->SerialiseFromUser(lacUserData);
    }

    // -----------------------------------------------------------------------------------
    // GetPlayerInfoByName @ 0x828887A0
    //   Begin an async find-user lookup of lpcPlayerName, routing the result into
    //   lpPlayerInfo. Translates the immediate find-user reject codes to action errors.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::GetPlayerInfoByName(const PlayerName* lpcPlayerName,
                                                        ServerInterfacePlayerInfoDataBase* lpPlayerInfo)
    {
        CGS_ASSERT(lpcPlayerName, "lpcPlayerName");
        CGS_ASSERT(lpPlayerInfo, "lpPlayerInfo");
        CGS_ASSERT(mpFindUser, "Prepare the component before trying to use it");

        mpPlayerInfo    = lpPlayerInfo;
        meCurrentAction = E_ACTION_FIND_USER;
        StartActionCore(KAPC_ACTION_NAMES[E_ACTION_FIND_USER]);

        LobbyFindUserCancel(mpFindUser);

        // r4 is the persona name string; the PlayerName operand reaches LobbyFindUser as a
        // const char* (the name buffer). iFlags = 0, iWhatever = 1.
        s32 liResult = LobbyFindUser(mpFindUser,
                                     reinterpret_cast<const char*>(lpcPlayerName),
                                     &ServerInterfacePlayerInfo::FindUserCallback, this,
                                     0, 1);
        switch (liResult)
        {
        case 0:
            break;
        case -1:
            EndAction(0x696E7670);   // 'invp' (invalid player)
            break;
        case -2:
            EndAction(0x6D616E79);   // 'many' (too many requests)
            break;
        case -3:
            EndAction(0x736F6F6E);   // 'soon' (too soon)
            break;
        default:
            CGS_ASSERT(false, "Unexpected return code from LobbyFindUser");
            break;
        }
    }

    // -----------------------------------------------------------------------------------
    // SelectStatView @ 0x82878FA0
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::SelectStatView(const char* lpcView)
    {
        CGS_ASSERT(mpStatbook, "mpStatbook");

        meCurrentAction = E_ACTION_SELECT_VIEW;
        StartActionCore(KAPC_ACTION_NAMES[E_ACTION_SELECT_VIEW]);

        s32 liResult = LobbyStatbookFetchView(mpStatbook,
                                              reinterpret_cast<const u8*>(lpcView));
        if (liResult < 0)
        {
            const DSErrorToServerInterfaceErrorTable& lEntry =
                KA_DS_ERROR_TABLE_LOOKUP[meCurrentAction];
            EServerInterfaceError leError =
                ServerInterfaceComponent::ConvertError(liResult, lEntry.mpMappingTable,
                                                       lEntry.miNumMappings);
            EndActionCore(static_cast<int>(leError));
            meCurrentAction = E_ACTION_COUNT;
        }
    }

    // -----------------------------------------------------------------------------------
    // DownloadPlayersStats @ 0x82879070
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::DownloadPlayersStats(const char* lpacStatList)
    {
        CGS_ASSERT(mpStatbook, "mpStatbook");

        meCurrentAction = E_ACTION_DOWNLOADING_STATS;
        StartActionCore(KAPC_ACTION_NAMES[E_ACTION_DOWNLOADING_STATS]);  // asm: off_82F33518 = table[3]

        s32 liResult = LobbyStatbookFetch(mpStatbook, 0,
                                          reinterpret_cast<const u8*>(lpacStatList));
        if (liResult < 0)
        {
            const DSErrorToServerInterfaceErrorTable& lEntry =
                KA_DS_ERROR_TABLE_LOOKUP[meCurrentAction];
            EServerInterfaceError leError =
                ServerInterfaceComponent::ConvertError(liResult, lEntry.mpMappingTable,
                                                       lEntry.miNumMappings);
            EndActionCore(static_cast<int>(leError));
            meCurrentAction = E_ACTION_COUNT;
        }
    }

    // -----------------------------------------------------------------------------------
    // DownloadStatViews @ 0x82879258
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::DownloadStatViews()
    {
        CGS_ASSERT(mpStatbook, "mpStatbook");

        meCurrentAction = E_ACTION_DOWNLOADING_VIEW_INFO;
        StartActionCore(KAPC_ACTION_NAMES[E_ACTION_DOWNLOADING_VIEW_INFO]);  // asm: off_82F3351C = table[4]

        s32 liResult = LobbyStatbookFetchViewInfo(mpStatbook);
        if (liResult < 0)
        {
            const DSErrorToServerInterfaceErrorTable& lEntry =
                KA_DS_ERROR_TABLE_LOOKUP[meCurrentAction];
            EServerInterfaceError leError =
                ServerInterfaceComponent::ConvertError(liResult, lEntry.mpMappingTable,
                                                       lEntry.miNumMappings);
            EndActionCore(static_cast<int>(leError));
            meCurrentAction = E_ACTION_COUNT;
        }
    }

    // -----------------------------------------------------------------------------------
    // GetPlayerStats @ 0x82879140
    // -----------------------------------------------------------------------------------
    bool ServerInterfacePlayerInfo::GetPlayerStats(s32 liRow, char* lpacStatDataBuffer,
                                                   s32 liBufferLength)
    {
        CGS_ASSERT(lpacStatDataBuffer, "lpacStatDataBuffer");

        return LobbyStatbookRowData(mpStatbook, 0, static_cast<u32>(liRow),
                                    lpacStatDataBuffer, liBufferLength) != 0;
    }

    // -----------------------------------------------------------------------------------
    // GetRowType @ 0x828791B0
    //   Read the per-row info record and return its "type" field (record +0x110).
    // -----------------------------------------------------------------------------------
    bool ServerInterfacePlayerInfo::GetRowType(s32 liRow, s32* lpnType)
    {
        CGS_ASSERT(lpnType, "lpnType");

        LobbyStatbookRowInfoT* lpRowInfo =
            LobbyStatbookRowInfo(mpStatbook, static_cast<u32>(liRow));
        if (!lpRowInfo)
            return false;

        // The row "type" lives at +0x110 in the DirtySDK row-info record (named field).
        *lpnType = lpRowInfo->iType;
        return true;
    }

    // -----------------------------------------------------------------------------------
    // HasStatViewInfoBeenDownloaded @ 0x82879220
    // -----------------------------------------------------------------------------------
    bool ServerInterfacePlayerInfo::HasStatViewInfoBeenDownloaded()
    {
        return LobbyStatbookViewCount(mpStatbook) > 0;
    }

    // -----------------------------------------------------------------------------------
    // Update @ 0x82888560
    //   Poll any in-flight statbook action and finish it once complete (>0) or failed (<0).
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::Update()
    {
        if (meCurrentAction == E_ACTION_SELECT_VIEW && mpStatbook)
        {
            s32 liStatus = LobbyStatbookViewStatus(mpStatbook);
            if (liStatus > 0)
                EndAction(0);
            else if (liStatus < 0)
                EndAction(liStatus);
        }

        if (meCurrentAction == E_ACTION_DOWNLOADING_STATS && mpStatbook)
        {
            s32 liStatus = LobbyStatbookStatus(mpStatbook, 0);
            if (liStatus > 0)
                EndAction(0);
            else if (liStatus < 0)
                EndAction(liStatus);
        }

        if (meCurrentAction == E_ACTION_DOWNLOADING_VIEW_INFO && mpStatbook)
        {
            s32 liStatus = LobbyStatbookViewInfoStatus(mpStatbook);
            if (liStatus > 0)
                EndAction(0);
            else if (liStatus < 0)
                EndAction(liStatus);
        }
    }

    // -----------------------------------------------------------------------------------
    // EditAccount @ 0x82879458
    //   Write the three account-settings flags, then async-save them.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::EditAccount(bool lbAgreeToShareInfoEA,
                                                bool lbAgreeToShareInfoPartners,
                                                bool lbTelemetryEnable)
    {
        DirtySock::LobbySettingRefT* lpSettings = mpServerInterface->GetSettingRef();

        LobbySettingSetNumber(lpSettings, "TELE_NABL", lbTelemetryEnable ? 1 : 0);
        LobbySettingSetNumber(lpSettings, "SPM_EA",   lbAgreeToShareInfoEA ? 1 : 0);
        LobbySettingSetNumber(lpSettings, "SPM_PART", lbAgreeToShareInfoPartners ? 1 : 0);

        meCurrentAction = E_ACTION_UPDATE_ACCOUNT;
        StartActionCore(KAPC_ACTION_NAMES[E_ACTION_UPDATE_ACCOUNT]);

        LobbySettingSave(lpSettings,
                         &ServerInterfacePlayerInfo::UpdateSettingsCallback, this);
    }

    // -----------------------------------------------------------------------------------
    // GetAccountSettings @ 0x82879528
    //   Read the three account-settings flags back out of the lobby settings store.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::GetAccountSettings(bool* lpbAgreeToShareInfoEA,
                                                       bool* lpbAgreeToShareInfoPartners,
                                                       bool* lpbTelemetryEnable)
    {
        CGS_ASSERT(lpbAgreeToShareInfoEA, "lpbAgreeToShareInfoEA");
        CGS_ASSERT(lpbAgreeToShareInfoPartners, "lpbAgreeToShareInfoPartners");
        CGS_ASSERT(lpbTelemetryEnable, "lpbTelemetryEnable");

        DirtySock::LobbySettingRefT* lpSettings = mpServerInterface->GetSettingRef();

        // asm decodes all three flags identically: (cntlzw(GetNumber-1) & 0x20), i.e. == 1.
        *lpbAgreeToShareInfoEA       = LobbySettingGetNumber(lpSettings, "SPM_EA", 0) == 1;
        *lpbAgreeToShareInfoPartners = LobbySettingGetNumber(lpSettings, "SPM_PART", 0) == 1;
        *lpbTelemetryEnable          = LobbySettingGetNumber(lpSettings, "TELE_NABL", 1) == 1;
    }

    // -----------------------------------------------------------------------------------
    // LoadSettings @ 0x82879638
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::LoadSettings()
    {
        meCurrentAction = E_ACTION_UPDATE_SETTINGS;   // asm stores 7 here
        StartActionCore(KAPC_ACTION_NAMES[E_ACTION_UPDATE_SETTINGS]);  // off_82F33528 "Load settings"

        LobbySettingLoad(mpServerInterface->GetSettingRef(),
                         &ServerInterfacePlayerInfo::LoadSettingsCallback, this);
    }

    // -----------------------------------------------------------------------------------
    // UpdateSettingsCallback @ 0x82879318 (static)
    //   Settings save/load completed: map the result and finish the action.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::UpdateSettingsCallback(DirtySock::LobbySettingRefT* /*lpRef*/,
                                                           s32 liResult, void* lpData)
    {
        ServerInterfacePlayerInfo* lpComponent =
            static_cast<ServerInterfacePlayerInfo*>(lpData);
        CGS_ASSERT(lpComponent, "lpData");

        const DSErrorToServerInterfaceErrorTable& lEntry =
            KA_DS_ERROR_TABLE_LOOKUP[lpComponent->meCurrentAction];
        EServerInterfaceError leError =
            lpComponent->ServerInterfaceComponent::ConvertError(
                liResult, lEntry.mpMappingTable, lEntry.miNumMappings);
        lpComponent->EndActionCore(static_cast<int>(leError));
        lpComponent->meCurrentAction = E_ACTION_COUNT;
    }

    // Both empty: the owning server interface's suspend / resume fan-out reaches the shared
    // empty body for this component.
    void ServerInterfacePlayerInfo::Suspend()
    {
    }

    void ServerInterfacePlayerInfo::Resume()
    {
    }

    // -----------------------------------------------------------------------------------
    // LoadSettingsCallback (static)
    //   Settings load completed: map the result for the current action and finish it.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::LoadSettingsCallback(DirtySock::LobbySettingRefT* /*lpRef*/,
                                                         s32 liResult, void* lpData)
    {
        CGS_ASSERT(lpData, "lpData");
        ServerInterfacePlayerInfo* lpPlayerInfoComponent =
            static_cast<ServerInterfacePlayerInfo*>(lpData);
        CGS_ASSERT(lpPlayerInfoComponent, "lpPlayerInfoComponent");

        const DSErrorToServerInterfaceErrorTable& lEntry =
            KA_DS_ERROR_TABLE_LOOKUP[lpPlayerInfoComponent->meCurrentAction];
        EServerInterfaceError leError =
            lpPlayerInfoComponent->ServerInterfaceComponent::ConvertError(
                liResult, lEntry.mpMappingTable, lEntry.miNumMappings);
        lpPlayerInfoComponent->ServerInterfaceComponent::EndActionCore(static_cast<int>(leError));
        lpPlayerInfoComponent->meCurrentAction = E_ACTION_COUNT;
    }

    // -----------------------------------------------------------------------------------
    // EventStatusCallback (static)
    //   A lobby 'user' event about someone other than the local user: tell the component
    //   the player stats changed. The local user record ('self') carries its ident at +0
    //   and its name at +0x28.
    // -----------------------------------------------------------------------------------
    void ServerInterfacePlayerInfo::EventStatusCallback(LobbyApiRefT* /*lpRef*/, LobbyApiMsgT* lpMsg,
                                                        void* lpData)
    {
        ServerInterfacePlayerInfo* lpComponent = static_cast<ServerInterfacePlayerInfo*>(lpData);
        if (lpMsg->kind != 0x75736572)   // 'user'
        {
            return;
        }

        u32 lauSelf[0x234 / sizeof(u32)];
        LobbyApiStatus(lpComponent->mpServerInterface->GetLobbyAPIRef(), 0x73656C66 /* 'self' */,
                       lauSelf, sizeof(lauSelf));

        char lacName[260];
        TagFieldGetString(TagFieldFind(lpMsg->pData, "S"), lacName, sizeof(lacName), "");

        if (static_cast<s32>(lauSelf[0]) > 0
            && _strnicmp(lacName, reinterpret_cast<const char*>(lauSelf) + 0x28, sizeof(lacName)) != 0)
        {
            lpComponent->OnEvent(E_SERVER_INTERFACE_PLAYER_INFO_EVENT_STATS_CHANGED, 0);
        }
    }
}
