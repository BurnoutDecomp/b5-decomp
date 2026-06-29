#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.h" // ServerInterfacePlayerInfoDataBase
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"   // ServerInterfaceDirtySock (GetLobbyAPIRef/GetSettingRef/GetMessageBuffer)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h" // DSErrorToServerInterfaceError(Table)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include "lobbyapi.h"        // LobbyApiStatus / LobbyApiRequestCB / LobbyApiSetCallback / LobbyApiClearCallback
#include "lobbyfinduser.h"   // LobbyFindUser* family
#include "lobbystatbook.h"   // LobbyStatbook* family
#include "lobbysetting.h"    // LobbySetting* family

// ===========================================================================
// CgsNetwork::ServerInterfacePlayerInfo
//
// Reconstructed from the X360 ARTIST pseudocode + asm (the spine), the
// CgsServerInterfacePlayerInfo.h DWARF (declaration shape), and the Feb-2007 idioms.
// Only the 21 functions present in this TU's export are bodied here; the remaining
// declared members belong to functions not in this export.
//
// FLAGGED rodata: the X360 build keeps several class-static tables in rodata that the
// available exports do NOT dump as values:
//   * KAPC_ACTION_NAMES[9]        -- the human-readable per-action names. The asm
//                                    comments reveal a handful of the strings (recovered
//                                    below); the unrevealed entries are left as a clearly
//                                    flagged empty placeholder. These strings are
//                                    display-only (passed to StartActionCore).
//   * KAI_ACTION_CODE_MAPPING[9]  -- the per-action lobby request fourcc codes
//                                    (dword_820E90A0). Their values are NOT recoverable
//                                    from the available exports -> flagged 0 placeholders.
//   * KA_DS_ERROR_TABLE_LOOKUP[9] -- per-action { error-mapping table, count } pairs
//                                    (off_820E91C8 / dword_820E91CC). The mapping tables'
//                                    fourcc/EServerInterfaceError contents are NOT
//                                    recoverable -> flagged empty (null table, 0 count),
//                                    which makes ConvertError fall back to the shared
//                                    default table at runtime.
// The bodies index these tables exactly as the asm does (behaviour preserved); only the
// literal table CONTENTS are flagged-unknown, never fabricated.
// ===========================================================================

namespace CgsNetwork
{
    // The per-action display names indexed by EAction. Recovered strings come from the
    // X360 asm string comments (off_82F3350C base); the rest are FLAGGED placeholders.
    static const char* const KAPC_ACTION_NAMES[ServerInterfacePlayerInfo::E_ACTION_COUNT] =
    {
        "Finding User Data",      // E_ACTION_FIND_USER             (off_82F3350C)
        "",                       // E_ACTION_UPDATE_AUXI           FLAGGED: not in export
        "",                       // E_ACTION_SELECT_VIEW           FLAGGED: not in export
        "Downloading stats",      // E_ACTION_DOWNLOADING_STATS     (off_82F33518)
        "Downloading view info",  // E_ACTION_DOWNLOADING_VIEW_INFO (off_82F3351C)
        "",                       // E_ACTION_SENDING_FEEDBACK      FLAGGED: not in export
        "",                       // E_ACTION_UPDATE_ACCOUNT        FLAGGED: not in export
        "Load settings",          // E_ACTION_UPDATE_SETTINGS       (off_82F33528)
        ""                        // E_ACTION_LOAD_SETTINGS         FLAGGED: not in export
    };

    // FLAGGED: per-action lobby request fourcc codes (dword_820E90A0). The values are not
    // recoverable from the available exports; zeroed placeholders preserve the indexing.
    static const s32 KAI_ACTION_CODE_MAPPING[ServerInterfacePlayerInfo::E_ACTION_COUNT] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0   // FLAGGED unrecoverable rodata
    };

    // FLAGGED: per-action error-mapping lookup (off_820E91C8 / dword_820E91CC). The
    // mapping-table contents are not recoverable from the available exports; an empty
    // entry (null table, 0 count) makes ConvertError fall back to the shared default
    // table, matching the not-found behaviour for an unknown code.
    static const DSErrorToServerInterfaceErrorTable
        KA_DS_ERROR_TABLE_LOOKUP[ServerInterfacePlayerInfo::E_ACTION_COUNT] =
    {
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }   // FLAGGED unrecoverable rodata
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
}
