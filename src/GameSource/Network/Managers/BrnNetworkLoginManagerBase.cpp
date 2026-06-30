// ===================================================================================
// BrnNetwork::LoginManagerBase
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkLoginManagerBase.cpp
//
// The network login state machine base. Reconstructed from the X360 binary
// (BURNOUT_X360_ARTIST.XEX). Each method is grounded in its postmortem asm; the
// per-method X360 address is given at the function head. The state-machine cursor
// (meSubState) is walked by Update(); the answer/cancel entry points are driven by
// BrnNetwork::StateManager::ProcessGuiEvents / ProcessNetworkEvents.
//
// The DirtySock server-interface components the asm reaches as raw
// *(mpNetworkManager + 0x38EC + 8*slot) pointers are accessed BY NAME through the
// committed accessor convention (GetServerInterface()->Get<Component>Component() /
// ->GetStatus(component) / ->IsSuspended()), matching BrnNetworkAutoLoginManager /
// BrnNetworkConnectionManager. The 0x38E8 DirtySock base is GetServerInterface().
//
// Every literal (state values, event codes, error codes 18/45, the 15000 ms connect
// timeout, the 2100 ms HTTPS timeout, the news index 8, the TOS url) comes from a stored
// immediate / cmpwi operand / rodata string in the asm. The dev-only debug-stream prints
// the X360 emits on some error paths (a global compiled out of retail) are noted in
// comments rather than fabricating the un-homed dev-stream global; the asserts are
// preserved via CGS_ASSERT.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkLoginManagerBase.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/BrnNetworkModule.h"        // GetNetworkManager / AddOutputGuiEvent
#include "GameSource/Network/BrnNetworkManager.h"        // BrnNetworkManager (TriggerEventFromLogin / GetServerInterface / GetSuspensionManager)
#include "GameSource/Network/BrnServerInterface.h"       // BrnServerInterface + component accessors
#include "GameSource/Network/BrnNetworkServers.h"        // NetworkServers::GetServerIP / GetServerPort
#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"  // SuspensionManager::Resume / Update
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"   // PlayerInfoData
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"  // EComponents / MemAlloc / MemFree
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceHttp.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePingRegions.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"     // CgsUnicode::Copy
#include "GameShared/GameClasses/Gui/CgsGuiEventAddLocalisedTextPointer.h"

#include <cstring>   // strncpy / memset

// DirtySDK connection-status query (vendor SDK; declared at global scope to match the SDK
// header, mirroring BrnNetworkGamerPictureManagerX360.cpp). The selector is a FourCC.
extern "C" int NetConnStatus(int luSelector, int liData, void* lpBuf, int liBufSize);

namespace BrnNetwork
{
    namespace
    {
        // The DirtySDK 'onln' connection-status selector (X360 lis/ori 0x6F6E6C6E). NetConnStatus
        // returns 1 when the console has reached the online service.
        const int KI_NETCONNSTATUS_ONLINE_SELECTOR = 0x6F6E6C6E;   // 'onln'
        // The DirtySDK 'conn' connection-status selector (X360 0x636F6E6E). Its high byte is '-'
        // (0x2D) while the connection is erroring.
        const int KI_NETCONNSTATUS_CONNECT_SELECTOR = 0x636F6E6E;  // 'conn'
        // High byte that marks a '-err' style DirtySDK connect status ('-' == 0x2D == 45).
        const int KI_NETCONNSTATUS_ERROR_HIGH_BYTE = 45;

        // The connect-to-DS-server timeout (X360 immediate 15000 ms at the ConnectToServer call).
        const s32 KI_DS_CONNECT_TIMEOUT_MS = 15000;
        // The HTTPS terms-of-service download timeout (X360 immediate 2100 ms).
        const s16 KI16_TOS_DOWNLOAD_TIMEOUT_MS = 2100;
        // The news index the configuration data is re-requested from (X360 immediate 8 at every
        // GetConfigDataFromNews call site).
        const s32 KI_CONFIG_NEWS_INDEX = 8;

        // The DirtySock connection-status the asm special-cases on the connecting-DS error path
        // (X360 cmpwi 18 after GetAndClearLastError); a non-18 error prints a dev-only diagnostic.
        const s32 KI_CONNECTING_BENIGN_ERROR = 18;

        // The login-event codes raised to the rest of the manager via TriggerEventFromLogin
        // (X360 r4 immediates at the call sites). The event-code enum's home is the manager's own
        // TU; the grounded raw values live here.
        const s32 KI_LOGIN_EVENT_TOS_DOWNLOADED   = 0;  // UpdateDownloadingTOS
        const s32 KI_LOGIN_EVENT_SELECT_ACCOUNT   = 1;  // UpdateLoggingIn (case 6, non-silent)
        const s32 KI_LOGIN_EVENT_DECLINED         = 4;  // Answer*/CancelLogin decline path
        const s32 KI_LOGIN_EVENT_AGREED           = 7;  // Answer* agree path

        // The default terms-of-service URL used when the server provides none (X360 rodata).
        const char* const KPC_DEFAULT_TOS_URL =
            "http://sdevbesl01.online.ea.com/easo/editorial/common/2006/tos/tos.jsp"
            "?style=accept&lang=%25s&platform=xbl2";   // asm rodata: %25 (URL-escaped %), not %s

        // The localised-string id the downloaded terms-of-service text is registered under
        // (X360 rodata "TOS_TEXT", strncpy'd with a 128 cap at the publish site).
        const char* const KPC_TOS_TEXT_ID = "TOS_TEXT";

        // The UTF-8 byte-order-mark bytes / blank-line skip the downloaded TOS is stripped of.
        const u8 KU8_BOM_0 = 0xEF;   // 239
        const u8 KU8_BOM_1 = 0xBB;   // 187
        const u8 KU8_BOM_2 = 0xBF;   // 191
        const u8 KU8_NEWLINE = 0x0A; // 10

        // Publish the downloaded terms-of-service text to the front end under KPC_TOS_TEXT_ID.
        void PublishTosText(BrnNetworkModule* lpNetworkModule, CgsUnicode::CgsUtf8* lpTosText)
        {
            CgsGui::GuiEventAddLocalisedTextPointer lEvent;
            lEvent.miMarker = 1;
            std::strncpy(lEvent.macId, KPC_TOS_TEXT_ID,
                         CgsGui::GuiEventAddLocalisedTextPointer::KI_MAX_ADD_ID_STRING_LENGTH);
            lEvent.mpText = lpTosText;
            lpNetworkModule->AddOutputGuiEvent<CgsGui::GuiEventAddLocalisedTextPointer>(lEvent);
        }
    }

    // ---- lifecycle --------------------------------------------------------------------------

    // X360 0x82543808
    void LoginManagerBase::Construct(BrnNetworkModule* lpNetworkModule)
    {
        CGS_ASSERT(lpNetworkModule != nullptr, "lpNetworkModule");
        mpNetworkModule = lpNetworkModule;

        mpNetworkManager = lpNetworkModule->GetNetworkManager();
        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");

        meSignInState        = E_SIGN_IN_STATE_COUNT;
        meSubState           = E_SUBSTATE_COUNT;
        meSignInType         = E_SIGN_IN_TYPE_COUNT;
        meCompletedSignInType = E_SIGN_IN_TYPE_COUNT;
        mpTOS                = nullptr;
    }

    // X360 0x8256CC60
    bool LoginManagerBase::Update(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        switch (meSubState)
        {
        case E_SUBSTATE_PLATFORM_SPECIFIC_UPDATE:
            PlatformSpecificUpdate(lpInput);
            return false;

        case E_SUBSTATE_CONNECTING_ONLINE:
            UpdateConnectingOnline();
            return false;

        case E_SUBSTATE_CONNECTING_DS:
            UpdateConnectingDS();
            return false;

        case E_SUBSTATE_LOGGING_IN:
            UpdateLoggingIn();
            return false;

        case E_SUBSTATE_DOWNLOADING_TOS:
            UpdateDownloadingTOS();
            return false;

        case E_SUBSTATE_GET_CONFIGURATION_DATA:
        case E_SUBSTATE_WAITING_FOR_PLAYER_ID:
            UpdateGetConfigurationData();
            return false;

        case E_SUBSTATE_CONNECTING_TELEMETRY:
            UpdateWaitingForPlayerID();
            return false;

        case E_SUBSTATE_DOWNLOADING_SCOREBOARD_HEADERS:
            UpdateConnectingTelemetry();
            return false;

        case E_SUBSTATE_WAITING_SELECTION:
        case E_SUBSTATE_NO_AGREEMENT:
            return false;

        case E_SUBSTATE_RESUMING_SERVER_INTERFACE:
            mpNetworkManager->GetSuspensionManager()->Update();
            return false;

        case E_SUBSTATE_LOAD_SETTINGS:
            UpdateLoadSettings();
            return false;

        case E_SUBSTATE_PING_REGIONS:
            UpdatePingRegions();
            return false;

        case E_SUBSTATE_DONE:
            return true;

        default:
            CGS_ASSERT(false, "Invalid login sub state");
            return false;
        }
    }

    // X360 0x8254F788
    void LoginManagerBase::Start(ESignInType leSignInType)
    {
        // Only (re)adopt the requested type when we are idle or already signing in silently.
        if (meSignInType == E_SIGN_IN_TYPE_SILENT || meSignInType == E_SIGN_IN_TYPE_COUNT)
        {
            meSignInType = leSignInType;
        }

        // Only (re)start the flow when it is not already mid-flight (idle or just done).
        if (meSubState != E_SUBSTATE_COUNT && meSubState != E_SUBSTATE_DONE)
        {
            return;
        }

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        if (lpServerInterface->GetConnectionComponent()->IsLoggedIn())
        {
            if (lpServerInterface->IsSuspended())
            {
                meSubState = E_SUBSTATE_RESUMING_SERVER_INTERFACE;
                mpNetworkManager->GetSuspensionManager()->Resume(&LoginManagerBase::ResumeCompleteCallback, this);
            }
            else
            {
                Finished(true);
            }
        }
        else
        {
            meSubState = E_SUBSTATE_PLATFORM_SPECIFIC_UPDATE;
        }
    }

    // X360 0x82543D18
    void LoginManagerBase::Disconnected()
    {
        CgsUnicode::CgsUtf8* lpTos = mpTOS;

        meSignInState        = E_SIGN_IN_STATE_COUNT;
        meSignInType         = E_SIGN_IN_TYPE_COUNT;
        meSubState           = E_SUBSTATE_COUNT;
        meCompletedSignInType = E_SIGN_IN_TYPE_COUNT;
        mbAgreeShare1        = false;
        mbAgreeShare2        = false;

        if (lpTos != nullptr)
        {
            CgsNetwork::ServerInterfaceDirtySock::MemFree(lpTos, 0, 0);
            mpTOS = nullptr;
        }
    }

    // ---- front-end answers ------------------------------------------------------------------

    // X360 0x82543B30
    void LoginManagerBase::AnswerOpenUsAccount(bool lbAgree)
    {
        if (lbAgree)
        {
            meSubState = E_SUBSTATE_LOGGING_IN;
            mpNetworkManager->TriggerEventFromLogin(KI_LOGIN_EVENT_AGREED, 0);
            mpNetworkManager->GetServerInterface()->GetConnectionComponent()->AgreeShareInfo(mbAgreeShare1, mbAgreeShare2);
        }
        else
        {
            meSubState = E_SUBSTATE_NO_AGREEMENT;
            mpNetworkManager->TriggerEventFromLogin(KI_LOGIN_EVENT_DECLINED, 0);
        }
    }

    // X360 0x82566478
    void LoginManagerBase::AnswerAgreeTOS(bool lbAgree)
    {
        if (lbAgree)
        {
            meSubState = E_SUBSTATE_LOGGING_IN;
            mpNetworkManager->TriggerEventFromLogin(KI_LOGIN_EVENT_AGREED, 0);
            mpNetworkManager->GetServerInterface()->GetConnectionComponent()->AgreeTOS(true);
        }
        else
        {
            meSubState = E_SUBSTATE_NO_AGREEMENT;
            mpNetworkManager->TriggerEventFromLogin(KI_LOGIN_EVENT_DECLINED, 0);
        }

        // Re-publish the terms-of-service text to the front end and suspend the config download.
        PublishTosText(mpNetworkModule, mpTOS);
        mpNetworkManager->GetServerInterface()->GetDownloadableConfigComponent()->Suspend();
    }

    // X360 0x8254FDD0
    void LoginManagerBase::AnswerNoAgreement(bool lbRestart)
    {
        if (lbRestart)
        {
            ESignInType leSignInType = meSignInType;
            meSubState = E_SUBSTATE_COUNT;
            // Re-enter the flow through the (virtual) Start of the most-derived leaf.
            Start(leSignInType);
        }
        else
        {
            mpNetworkManager->GetServerInterface()->GetConnectionComponent()->DisconnectFromServer();
            ESignInType leSignInType = meSignInType;
            meSignInState        = E_SIGN_IN_STATE_COUNT;
            meSubState           = E_SUBSTATE_DONE;
            meCompletedSignInType = leSignInType;
            meSignInType         = E_SIGN_IN_TYPE_COUNT;
        }
    }

    // X360 0x82543BB8
    void LoginManagerBase::AnswerSignIn(bool lbSuccess)
    {
        CGS_ASSERT(meSignInState == E_SIGN_IN_STATE_SIGNING_IN,
                   "meSignInState == E_SIGN_IN_STATE_SIGNING_IN");
        // X360: meSignInState = !lbSuccess + 1 -- success -> SUCCESS(1), failure -> FAILED(2).
        meSignInState = lbSuccess ? E_SIGN_IN_STATE_SUCCESS : E_SIGN_IN_STATE_FAILED;
    }

    // X360 0x8254FE60
    void LoginManagerBase::CancelLogin()
    {
        if (meSubState == E_SUBSTATE_NO_AGREEMENT)
        {
            AnswerNoAgreement(false);
        }
        else
        {
            meSubState = E_SUBSTATE_NO_AGREEMENT;
            mpNetworkManager->TriggerEventFromLogin(KI_LOGIN_EVENT_DECLINED, 0);
        }

        mpNetworkManager->GetServerInterface()->GetDownloadableConfigComponent()->Suspend();
    }

    // ---- protected helpers ------------------------------------------------------------------

    // X360 0x82543C30
    void LoginManagerBase::Finished(bool lbSuccess)
    {
        if (!lbSuccess)
        {
            mpNetworkManager->GetServerInterface()->GetConnectionComponent()->DisconnectFromServer();
        }

        ESignInType leSignInType = meSignInType;
        meSignInState        = E_SIGN_IN_STATE_COUNT;
        meSubState           = E_SUBSTATE_DONE;
        meCompletedSignInType = leSignInType;
        meSignInType         = E_SIGN_IN_TYPE_COUNT;
    }

    // ---- private steps ----------------------------------------------------------------------

    // X360 0x82543AD0
    void LoginManagerBase::LogInToServer()
    {
        char macUserName[16];
        char macPassword[32];

        GetUserNameAndPassword(macUserName, macPassword);
        meSubState = E_SUBSTATE_LOGGING_IN;
        mpNetworkManager->GetServerInterface()->GetConnectionComponent()->LogInToServer(macUserName, macPassword);
    }

    // X360 0x825438A8
    void LoginManagerBase::PrepareDownloadingTOS()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();

        const char* lpcUrl = KPC_DEFAULT_TOS_URL;
        char macServerUrl[256];
        std::memset(macServerUrl, 0, sizeof(macServerUrl));
        lpServerInterface->GetServerInfoComponent()->GetTosUrl(macServerUrl, sizeof(macServerUrl));
        if (macServerUrl[0] != '\0')
        {
            lpcUrl = macServerUrl;
        }

        const s32 liBufferSize = lpServerInterface->GetDownloadableConfigComponent()->GetTosDownloadBufferSize();
        lpServerInterface->GetHttpComponent()->StartHttpsDownload(lpcUrl, liBufferSize, KI16_TOS_DOWNLOAD_TIMEOUT_MS);
    }

    // X360 0x8254FEC8
    void LoginManagerBase::PrepareConnectTelemetry()
    {
        CGS_ASSERT(mpNetworkManager != nullptr, "NULL != mpNetworkManager");

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        BrnServerInterfaceDownloadableConfig* lpConfig = lpServerInterface->GetDownloadableConfigComponent();
        CgsNetwork::ServerInterfaceTelemetry* lpTelemetry =
            reinterpret_cast<CgsNetwork::ServerInterfaceTelemetry*>(lpServerInterface->GetTelemetryComponent());

        lpTelemetry->SetDisabledCountryList(lpConfig->GetTelemetryDisabledList());
        lpTelemetry->SetEventFilters(lpConfig->GetTelemetryFirstUsageEventFilters(),
                                     lpConfig->GetTelemetryNormalUsageEventFilters());

        const s32 liError = lpTelemetry->Connect(false);
        if (liError != 0)
        {
            // X360: dev-only diagnostic ("WARING: Could not connect to telemetry server, error :"
            // << error) streamed to the debug text stream; compiled out of retail.
        }

        meSubState = E_SUBSTATE_DOWNLOADING_SCOREBOARD_HEADERS;
    }

    // X360 0x82543D88
    void LoginManagerBase::PrepareLoadSettings()
    {
        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_PLAYER_INFO) == BrnServerInterface::E_STATUS_IDLE)
        {
            lpServerInterface->GetPlayerInfoComponent()->LoadSettings();
            meSubState = E_SUBSTATE_LOAD_SETTINGS;
        }
    }

    // X360 0x82543E08
    void LoginManagerBase::PreparePingRegions()
    {
        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_PING_REGIONS) == BrnServerInterface::E_STATUS_IDLE)
        {
            lpServerInterface->GetPingRegionsComponent()->StartPingRegions();
            meSubState = E_SUBSTATE_PING_REGIONS;
        }
    }

    // X360 0x82543E88 (private virtual)
    void LoginManagerBase::ShowChatRestrictionPopup()
    {
        ESignInType leSignInType = meSignInType;
        meSignInState        = E_SIGN_IN_STATE_COUNT;
        meSubState           = E_SUBSTATE_DONE;
        meCompletedSignInType = leSignInType;
        meSignInType         = E_SIGN_IN_TYPE_COUNT;
    }

    // X360 0x82543C90 (static; lpUserData == the LoginManagerBase)
    void LoginManagerBase::ResumeCompleteCallback(bool lbSuccess, void* lpUserData)
    {
        LoginManagerBase* lpThis = static_cast<LoginManagerBase*>(lpUserData);
        if (lbSuccess)
        {
            // X360: the success path calls vtable slot +0x10 on the user-data object -- the same
            // (private virtual) slot UpdatePingRegions dispatches, i.e. ShowChatRestrictionPopup.
            lpThis->ShowChatRestrictionPopup();
        }
        else
        {
            lpThis->mpNetworkManager->GetServerInterface()->GetConnectionComponent()->DisconnectFromServer();
            ESignInType leSignInType = lpThis->meSignInType;
            lpThis->meSignInState        = E_SIGN_IN_STATE_COUNT;
            lpThis->meSubState           = E_SUBSTATE_DONE;
            lpThis->meCompletedSignInType = leSignInType;
            lpThis->meSignInType         = E_SIGN_IN_TYPE_COUNT;
        }
    }

    // ---- per-substate update steps ----------------------------------------------------------

    // X360 0x8254F880
    void LoginManagerBase::UpdateConnectingOnline()
    {
        if (NetConnStatus(KI_NETCONNSTATUS_ONLINE_SELECTOR, 0, nullptr, 0) == 1)
        {
            BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
            meSubState = E_SUBSTATE_CONNECTING_DS;

            NetworkServers* lpServers = mpNetworkManager->GetNetworkServers();
            const s32 liServerPort = lpServers->GetServerPort();
            const char* lpcServerIP = lpServers->GetServerIP();
            lpServerInterface->GetConnectionComponent()->ConnectToServer(lpcServerIP, liServerPort,
                                                                         KI_DS_CONNECT_TIMEOUT_MS);
        }
        else if ((NetConnStatus(KI_NETCONNSTATUS_CONNECT_SELECTOR, 0, nullptr, 0) >> 24) == KI_NETCONNSTATUS_ERROR_HIGH_BYTE)
        {
            Finished(false);
        }
    }

    // X360 0x8254F938
    void LoginManagerBase::UpdateConnectingDS()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        const u32 luStatus = static_cast<u32>(lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_CONNECTION));
        if (luStatus == 0)
        {
            return;
        }

        if (luStatus == 1)
        {
            if (lpServerInterface->GetAndClearLastError(CgsNetwork::E_COMPONENTS_CONNECTION) != KI_CONNECTING_BENIGN_ERROR)
            {
                // X360: dev-only diagnostic ("Connecting error\n") to the debug stream; retail no-op.
            }
            Finished(false);
        }
        else if (luStatus < 3)
        {
            LogInToServer();
        }
        else
        {
            CGS_ASSERT(false, "Invalid server interface status!");
        }
    }

    // X360 0x8254FA10
    void LoginManagerBase::UpdateLoggingIn()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        CgsNetwork::ServerInterfaceConnection* lpConnection = lpServerInterface->GetConnectionComponent();

        // X360: while the connection component is not in its error state (status != ERROR/1) keep
        // polling the login step; when it errors the connection dropped -> disconnect and abandon.
        if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_CONNECTION) == 1)
        {
            lpConnection->DisconnectFromServer();
            meCompletedSignInType = meSignInType;
            meSignInState        = E_SIGN_IN_STATE_COUNT;
            meSubState           = E_SUBSTATE_DONE;
            meSignInType         = E_SIGN_IN_TYPE_COUNT;
            return;
        }

        switch (lpConnection->GetLoginStatus())
        {
        case CgsNetwork::ServerInterfaceConnection::E_LOGIN_STATUS_LOGIN:
            CGS_ASSERT(false, "Probably shouldn't get here but just in case try logging in again");
            LogInToServer();
            break;

        case CgsNetwork::ServerInterfaceConnection::E_LOGIN_STATUS_AGREE_TOS:
            if (meSignInType == E_SIGN_IN_TYPE_SILENT)
            {
                Finished(false);
                break;
            }
            if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == 1)
            {
                lpServerInterface->ClearLastError(CgsNetwork::E_COMPONENTS_HTTP);
                Finished(false);
            }
            else if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == 2)
            {
                meSubState = E_SUBSTATE_WAITING_FOR_PLAYER_ID;
                lpServerInterface->GetDownloadableConfigComponent()->GetConfigDataFromNews(KI_CONFIG_NEWS_INDEX);
            }
            break;

        case CgsNetwork::ServerInterfaceConnection::E_LOGIN_STATUS_AGREE_SHARE_INFO:
            if (meSignInType == E_SIGN_IN_TYPE_SILENT)
            {
                Finished(false);
            }
            else
            {
                mbAgreeShare1 = false;
                mbAgreeShare2 = false;
                meSubState = E_SUBSTATE_WAITING_SELECTION;
                mpNetworkManager->TriggerEventFromLogin(KI_LOGIN_EVENT_SELECT_ACCOUNT, 0);
            }
            break;

        case CgsNetwork::ServerInterfaceConnection::E_LOGIN_STATUS_LOGIN_DONE:
            if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_DOWNLOADABLE_CONFIG) == 2)
            {
                meSubState = E_SUBSTATE_GET_CONFIGURATION_DATA;
                lpServerInterface->GetDownloadableConfigComponent()->GetConfigDataFromNews(KI_CONFIG_NEWS_INDEX);
            }
            break;

        case CgsNetwork::ServerInterfaceConnection::E_LOGIN_STATUS_BUSY:
            return;

        default:
            CGS_ASSERT(false, "Unexpected Login status");
            break;
        }
    }

    // X360 0x82566320
    void LoginManagerBase::UpdateDownloadingTOS()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        const s32 liStatus = lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_HTTP);

        if (liStatus == 2)
        {
            CgsNetwork::ServerInterfaceHttp* lpHttp = lpServerInterface->GetHttpComponent();

            // Skip a leading UTF-8 byte-order-mark, then any blank lines.
            u8* lpText = lpHttp->GetHttpsDownloadBuffer();
            if (lpText[0] == KU8_BOM_0 && lpText[1] == KU8_BOM_1 && lpText[2] == KU8_BOM_2)
            {
                lpText += 3;
            }
            while (*lpText == KU8_NEWLINE)
            {
                ++lpText;
            }

            if (mpTOS != nullptr)
            {
                CgsNetwork::ServerInterfaceDirtySock::MemFree(mpTOS, 0, 0);
                mpTOS = nullptr;
            }

            const s32 liBufferSize = lpServerInterface->GetDownloadableConfigComponent()->GetTosDownloadBufferSize();
            mpTOS = static_cast<CgsUnicode::CgsUtf8*>(CgsNetwork::ServerInterfaceDirtySock::MemAlloc(liBufferSize, 0, 0));
            CgsUnicode::Copy(mpTOS, lpText);

            PublishTosText(mpNetworkModule, mpTOS);
            lpHttp->DestroyHttpsDownload();
            mpNetworkManager->TriggerEventFromLogin(KI_LOGIN_EVENT_TOS_DOWNLOADED, 0);
            meSubState = E_SUBSTATE_WAITING_SELECTION;
        }
        else if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == 1)
        {
            lpServerInterface->GetHttpComponent()->DestroyHttpsDownload();
            meSubState = E_SUBSTATE_WAITING_SELECTION;
            Finished(false);
        }
    }

    // X360 0x82543938
    void LoginManagerBase::UpdateGetConfigurationData()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        const u32 luStatus = static_cast<u32>(lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_DOWNLOADABLE_CONFIG));
        if (luStatus == 0)
        {
            return;
        }

        ESubState leNextSubState;
        if (luStatus == 1)
        {
            CGS_ASSERT(false, "Error getting configuration data");
            leNextSubState = E_SUBSTATE_WAITING_SELECTION;
        }
        else
        {
            if (luStatus >= 3)
            {
                CGS_ASSERT(false, "Invalid server interface status!");
                return;
            }
            if (meSubState == E_SUBSTATE_WAITING_FOR_PLAYER_ID)
            {
                PrepareDownloadingTOS();
                leNextSubState = E_SUBSTATE_DOWNLOADING_TOS;
            }
            else
            {
                leNextSubState = E_SUBSTATE_CONNECTING_TELEMETRY;
            }
        }
        meSubState = leNextSubState;
    }

    // X360 0x82561A10
    void LoginManagerBase::UpdateWaitingForPlayerID()
    {
        PlayerInfoData lPlayerInfo;
        CGS_ASSERT(lPlayerInfo.Prepare(), "lPlayerInfo.Prepare()");

        mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
        if (lPlayerInfo.GetID() != -1)
        {
            PrepareConnectTelemetry();
        }
    }

    // X360 0x8254FC50
    void LoginManagerBase::UpdateConnectingTelemetry()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        const u32 luStatus = static_cast<u32>(lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_TELEMETRY));
        if (luStatus == 0)
        {
            return;
        }

        if (luStatus == 1)
        {
            CGS_ASSERT(false, "Error getting configuration data");
            meSubState = E_SUBSTATE_WAITING_SELECTION;
        }
        else if (luStatus < 3)
        {
            PrepareLoadSettings();
        }
        else
        {
            CGS_ASSERT(false, "Invalid server interface status!");
        }
    }

    // X360 0x8254FD18
    void LoginManagerBase::UpdateLoadSettings()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        const u32 luStatus = static_cast<u32>(lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_PLAYER_INFO));
        if (luStatus == 0)
        {
            return;
        }

        if (luStatus == 1)
        {
            CGS_ASSERT(false, "Error loading settings");
            lpServerInterface->ClearLastError(CgsNetwork::E_COMPONENTS_PLAYER_INFO);
        }
        else if (luStatus >= 3)
        {
            CGS_ASSERT(false, "Invalid server interface status!");
            return;
        }

        PreparePingRegions();
    }

    // X360 0x82543A08
    void LoginManagerBase::UpdatePingRegions()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        const u32 luStatus = static_cast<u32>(lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_PING_REGIONS));
        if (luStatus == 0)
        {
            return;
        }

        if (luStatus == 1)
        {
            CGS_ASSERT(false, "Error pinging regions");
            lpServerInterface->ClearLastError(CgsNetwork::E_COMPONENTS_PING_REGIONS);
        }
        else if (luStatus >= 3)
        {
            CGS_ASSERT(false, "Invalid server interface status!");
            return;
        }

        // X360 vtable slot +0x10: the (private virtual) chat-restriction popup step.
        ShowChatRestrictionPopup();
    }
}
