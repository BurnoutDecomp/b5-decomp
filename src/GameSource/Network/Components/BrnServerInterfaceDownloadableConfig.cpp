#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"

#include <cstddef>                                        // offsetof
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // GetLobbyAPIRef
#include "lobbyapi.h"                                     // LobbyApiInfo
#include "lobbyname.h"                                    // LobbyNameCmp
#include "lobbytagfield.h"                                // TagFieldFind / TagFieldGetDelim

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnServerInterfaceDownloadableConfig::`vector deleting destructor'
//   BrnNetwork::BrnServerInterfaceDownloadableConfig::Construct
//   BrnNetwork::BrnServerInterfaceDownloadableConfig::Destruct
//   BrnNetwork::BrnServerInterfaceDownloadableConfig::Prepare
//   BrnNetwork::BrnServerInterfaceDownloadableConfig::Release
//   BrnNetwork::BrnServerInterfaceDownloadableConfig::GetConfigDataFromNews
//
// The field-descriptor offsets are console byte offsets into this object; the host
// registers offsetof() of the same named member.

// Platform memset (declared exactly as the other network TUs declare it).
extern "C"
{
    void* XMemSet(void* lpDest, s32 liValue, u32 luCount);
}

namespace BrnNetwork
{
    namespace
    {
        // The lobby info select for the downloaded client-config record ('conf').
        const s32  KI_LOBBY_INFO_CLIENT_CONFIG = 0x636F6E66;

        // One gamertag of the comma-separated fever-carrier list (buffer size, delimiter).
        const s32  KI_FEVER_GAMERTAG_LENGTH = 16;
        const s32  KI_FEVER_LIST_DELIMITER  = ',';
    }

    BrnServerInterfaceDownloadableConfig::BrnServerInterfaceDownloadableConfig()
    {
    }

    // The deleting destructor only restores the base vtable and conditionally frees.
    BrnServerInterfaceDownloadableConfig::~BrnServerInterfaceDownloadableConfig()
    {
    }

    // Base Construct, then clear the field table and load the built-in defaults every
    // tunable keeps until a config record overrides it.
    void BrnServerInterfaceDownloadableConfig::Construct()
    {
        CgsNetwork::ServerInterfaceDownloadableConfig::Construct();

        XMemSet(maIDsToMemAddrs, 0, sizeof(maIDsToMemAddrs));
        miSizeOfMemLookupTable = 0;

        miIdleTimeOut             = 30000;
        miSearchQueryTimeInterval = 30000;
        miNatTestPacketTimeout    = 30000;
        miTOSBufferSize           = 64000;
        miNewsBufferSize          = 16000;
        miNumDemanglePlayers      = 0;

        mfMinSyncingTime                        = 1.0f;
        mfMaxSyncingTime                        = 30.0f;
        mfWaitForStartTime                      = 30.0f;
        mfTimeToWaitForSilentClientReady        = 30.0f;
        mfTimeToWaitForCommunicatingClientReady = 45.0f;
        mfTimeGapToLeaveBeforeStartTime         = 5.0f;
        mfTimeBetweenStatChecks                 = 30.0f;
        mfTimeBetweenRoadRulesUploads           = 1.0f;
        mfTimeBetweenRoadRulesDownloads         = 900.0f;
        mfTimeUntilRetryAfterFailedBuddyUpload  = 600.0f;
        mfTimeBetweenOfflineProgressionUpload   = 600.0f;

        mbDoLogOffOnExitOnline = false;

        XMemSet(macTelemetryFiltersFirstUse, 0, sizeof(macTelemetryFiltersFirstUse));
        XMemSet(macTelemetryFiltersNormalUse, 0, sizeof(macTelemetryFiltersNormalUse));
    }

    // A pure forward to the base (the console body is a single branch).
    void BrnServerInterfaceDownloadableConfig::Destruct()
    {
        CgsNetwork::ServerInterfaceDownloadableConfig::Destruct();
    }

    // Base Prepare; on success register the twenty config keys, each with its type and
    // the member it is parsed into.
    bool BrnServerInterfaceDownloadableConfig::Prepare(CgsNetwork::ServerInterface* lpServerInterface)
    {
        if (!CgsNetwork::ServerInterfaceDownloadableConfig::Prepare(lpServerInterface))
        {
            return false;
        }

        miSizeOfMemLookupTable = 0;

        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("MIN_TIME_SPENT_SYNCYING_TIME"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfMinSyncingTime));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("MAX_TIME_SPENT_SYNCYING_TIME"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfMaxSyncingTime));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("MAX_TIME_TO_WAIT_FOR_START_TIME"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfWaitForStartTime));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("MAX_TIME_TO_WAIT_FOR_SILENT_CLIENT_READY"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfTimeToWaitForSilentClientReady));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("MAX_TIME_TO_WAIT_FOR_COMMUNICATING_CLIENT_READY"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfTimeToWaitForCommunicatingClientReady));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("TIME_GAP_TO_LEAVE_BEFORE_START_TIME"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfTimeGapToLeaveBeforeStartTime));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("IDLE_TIMEOUT"), CgsNetwork::E_TYPE_INTEGER,
            offsetof(BrnServerInterfaceDownloadableConfig, miIdleTimeOut));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("SEARCH_QUERY_TIME_INTERVAL"), CgsNetwork::E_TYPE_INTEGER,
            offsetof(BrnServerInterfaceDownloadableConfig, miSearchQueryTimeInterval));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("NAT_TEST_PACKET_TIMEOUT"), CgsNetwork::E_TYPE_INTEGER,
            offsetof(BrnServerInterfaceDownloadableConfig, miNatTestPacketTimeout));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("TOS_BUFFER_SIZE"), CgsNetwork::E_TYPE_INTEGER,
            offsetof(BrnServerInterfaceDownloadableConfig, miTOSBufferSize));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("NEWS_BUFFER_SIZE"), CgsNetwork::E_TYPE_INTEGER,
            offsetof(BrnServerInterfaceDownloadableConfig, miNewsBufferSize));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("NUM_DEMANGLE_PLAYERS"), CgsNetwork::E_TYPE_INTEGER,
            offsetof(BrnServerInterfaceDownloadableConfig, miNumDemanglePlayers));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("LOG_OFF_ON_EXIT_ONLINE_MENU"), CgsNetwork::E_TYPE_BOOL,
            offsetof(BrnServerInterfaceDownloadableConfig, mbDoLogOffOnExitOnline));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("TELEMETRY_FILTERS_FIRST_USE"), CgsNetwork::E_TYPE_STRING,
            offsetof(BrnServerInterfaceDownloadableConfig, macTelemetryFiltersFirstUse));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("TELEMETRY_FILTERS_NORMAL_USE"), CgsNetwork::E_TYPE_STRING,
            offsetof(BrnServerInterfaceDownloadableConfig, macTelemetryFiltersNormalUse));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("TIME_BETWEEN_STATS_CHECKS"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfTimeBetweenStatChecks));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("TIME_BETWEEN_ROAD_RULES_UPLOADS"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfTimeBetweenRoadRulesUploads));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("TIME_BETWEEN_ROAD_RULES_DOWNLOADS"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfTimeBetweenRoadRulesDownloads));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("TIME_BEFORE_RETRY_AFTER_FAILED_BUDDY_UPLOAD"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfTimeUntilRetryAfterFailedBuddyUpload));
        maIDsToMemAddrs[miSizeOfMemLookupTable++] = CgsNetwork::DataIDToMemoryAddr(
            const_cast<char*>("TIME_BETWEEN_OFFLINE_PROGRESSION_UPLOADS"), CgsNetwork::E_TYPE_FLOAT,
            offsetof(BrnServerInterfaceDownloadableConfig, mfTimeBetweenOfflineProgressionUpload));

        CGS_ASSERT(miSizeOfMemLookupTable == KI_MAX_ELEMENTS_IN_MEMADDR_LOOKUP,
                   "miSizeOfMemLookupTable == KI_MAX_ELEMENTS_IN_MEMADDR_LOOKUP");

        return true;
    }

    // Base Release; on success drop the field table and restore the defaults. The
    // road-rules download interval comes back as 600 here, not Construct's 900.
    bool BrnServerInterfaceDownloadableConfig::Release()
    {
        if (!CgsNetwork::ServerInterfaceDownloadableConfig::Release())
        {
            return false;
        }

        XMemSet(maIDsToMemAddrs, 0, sizeof(maIDsToMemAddrs));
        miSizeOfMemLookupTable = 0;

        miIdleTimeOut             = 30000;
        miSearchQueryTimeInterval = 30000;
        miNatTestPacketTimeout    = 30000;
        miTOSBufferSize           = 64000;
        miNewsBufferSize          = 16000;
        miNumDemanglePlayers      = 0;

        mfMinSyncingTime                        = 1.0f;
        mfMaxSyncingTime                        = 30.0f;
        mfWaitForStartTime                      = 30.0f;
        mfTimeToWaitForSilentClientReady        = 30.0f;
        mfTimeToWaitForCommunicatingClientReady = 45.0f;
        mfTimeGapToLeaveBeforeStartTime         = 5.0f;
        mfTimeBetweenStatChecks                 = 30.0f;
        mfTimeBetweenRoadRulesUploads           = 1.0f;
        mfTimeBetweenRoadRulesDownloads         = 600.0f;
        mfTimeUntilRetryAfterFailedBuddyUpload  = 600.0f;
        mfTimeBetweenOfflineProgressionUpload   = 600.0f;

        XMemSet(macTelemetryFiltersFirstUse, 0, sizeof(macTelemetryFiltersFirstUse));
        XMemSet(macTelemetryFiltersNormalUse, 0, sizeof(macTelemetryFiltersNormalUse));

        return true;
    }

    void BrnServerInterfaceDownloadableConfig::GetConfigDataFromNews(s32 liNewsIndex)
    {
        GetConfigurationDataFromNews(liNewsIndex, maIDsToMemAddrs, miSizeOfMemLookupTable);
    }

    // Walk the comma-separated FEVER_CARRIERS field of the client config record, comparing
    // each entry against lpcGamertag with the lobby's name comparison.
    bool BrnServerInterfaceDownloadableConfig::IsGamertagInFeverList(const char* lpcGamertag)
    {
        const char* lpcConfig = static_cast<const char*>(
            LobbyApiInfo(GetServerInterface()->GetLobbyAPIRef(), KI_LOBBY_INFO_CLIENT_CONFIG));
        const char* lpcFeverList = TagFieldFind(lpcConfig, "FEVER_CARRIERS");

        char lacGamertag[KI_FEVER_GAMERTAG_LENGTH];
        s32  liIndex = 0;

        while (TagFieldGetDelim(lpcFeverList, lacGamertag, KI_FEVER_GAMERTAG_LENGTH, "",
                                liIndex, KI_FEVER_LIST_DELIMITER) > 0)
        {
            ++liIndex;
            if (LobbyNameCmp(lpcGamertag, lacGamertag) == 0)
            {
                return true;
            }
        }

        return false;
    }

    // The TELE_DISABLE field of the client config record (the telemetry-disabled country list).
    const char* BrnServerInterfaceDownloadableConfig::GetTelemetryDisabledList()
    {
        const char* lpcConfig = static_cast<const char*>(
            LobbyApiInfo(GetServerInterface()->GetLobbyAPIRef(), KI_LOBBY_INFO_CLIENT_CONFIG));
        return TagFieldFind(lpcConfig, "TELE_DISABLE");
    }
}
