#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"   // GetMessageBuffer / GetComponent / OnEvent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

#include <string.h>   // strlen, _strnicmp

// Case-insensitive bounded compare (console strnicmp -> MSVC _strnicmp).
#if defined(_MSC_VER)
#  define strnicmp _strnicmp
#endif

// The telemetry component over the DirtySDK telemetry API. Prepare creates the normal-usage
// buffer (and, when asked, the first-usage buffer that records first until it fills or is
// sent); Connect authenticates both with the lobby's telemetry auth string; CaptureEvent
// records {module, group, "description.data"} into the current buffer.

namespace CgsNetwork
{
    namespace
    {
        // TelemetryApiBufferTypeE.
        const s32 KI_TELEMETRY_BUFFERTYPE_FILLANDSTOP       = 0;
        const s32 KI_TELEMETRY_BUFFERTYPE_CIRCULAROVERWRITE = 1;

        // TelemetryApiCBTypeE.
        const s32 KI_TELEMETRY_CBTYPE_BUFFERFULL       = 0;
        const s32 KI_TELEMETRY_CBTYPE_FULLSENDCOMPLETE = 2;

        // TelemetryApiControl selectors.
        const s32 KI_TELEMETRY_CONTROL_USER_ENABLE      = 0x756E626C;   // 'unbl'
        const s32 KI_TELEMETRY_CONTROL_DISABLED_COUNTRY = 0x6364626C;   // 'cdbl'
        const s32 KI_TELEMETRY_CONTROL_HALT             = 0x68616C74;   // 'halt'
        const s32 KI_TELEMETRY_CONTROL_LOG_CHAR         = 0x6C6F6763;   // 'logc'

        // TelemetryApiStatus selectors.
        const s32 KI_TELEMETRY_STATUS_SEND_SIZE  = 0x7373697A;   // 'ssiz'
        const s32 KI_TELEMETRY_STATUS_NUM_EVENTS = 0x6E756D65;   // 'nume'
        const s32 KI_TELEMETRY_STATUS_MAX_EVENTS = 0x6D617865;   // 'maxe'

        // Log prefix characters of the two buffers.
        const s32 KI_FIRST_USAGE_LOG_CHAR  = 'F';
        const s32 KI_NORMAL_USAGE_LOG_CHAR = 'N';

        // Size of a telemetry event string (TelemetryApiEventT::strEvent).
        const s32 KI_EVENT_STRING_LENGTH = 16;

        const DSErrorToServerInterfaceError KA_TELEMETRY_SERVER_INTERFACE_ERROR_MAPPING[5] =
        {
            { 0x65636F6E, E_SERVER_INTERFACE_TELEMETRY_ERROR_NOT_CONNECTED },        // 'econ'
            { 0x696E7670, E_SERVER_INTERFACE_TELEMETRY_ERROR_NOT_INITIALISED },      // 'invp'
            { -1,         E_SERVER_INTERFACE_TELEMETRY_ERROR_NOT_INITIALISED },
            { -2,         E_SERVER_INTERFACE_TELEMETRY_ERROR_BUFFER_FULL },
            { 0x62757379, E_SERVER_INTERFACE_TELEMETRY_ERROR_UPLOAD_IN_PROGRESS },   // 'busy'
        };
    }

    using namespace DirtySock;

    ServerInterfaceTelemetry::~ServerInterfaceTelemetry()
    {
    }

    void ServerInterfaceTelemetry::Construct()
    {
        meStatus                      = 2;
        mpcCurrentAction              = "";
        miLastError                   = 0;
        muMaxFirstUsageBufferSize     = 0;
        mpTelemetryFirstUsage         = 0;
        mpTelemetryNormalUsage        = 0;
        mpTelemetryCurrent            = 0;
        mpServerInterface             = 0;
        mpaEventIDsToKeysMapping      = 0;
        mbDidSuspendHaltCurrentBuffer = false;
    }

    void ServerInterfaceTelemetry::Destruct()
    {
        muMaxFirstUsageBufferSize     = 0;
        mpTelemetryFirstUsage         = 0;
        mpTelemetryNormalUsage        = 0;
        mpTelemetryCurrent            = 0;
        mpServerInterface             = 0;
        mpaEventIDsToKeysMapping      = 0;
        mbDidSuspendHaltCurrentBuffer = false;
    }

    bool ServerInterfaceTelemetry::Prepare(ServerInterfaceDirtySock* lpServerInterface, bool lbUseFirstUsageBuffer,
                                           const char* lpcDisabledCountryList,
                                           s32 liMaxFirstUsageBufferSize, s32 liMaxNormalUsageBufferSize)
    {
        void* lpDisabledCountryList = const_cast<char*>(lpcDisabledCountryList);

        mpServerInterface      = lpServerInterface;
        mpTelemetryNormalUsage = TelemetryApiCreate(liMaxNormalUsageBufferSize,
                                                    KI_TELEMETRY_BUFFERTYPE_CIRCULAROVERWRITE);
        CGS_ASSERT(mpTelemetryNormalUsage != 0, "NULL != mpTelemetryNormalUsage");

        mpTelemetryCurrent = mpTelemetryNormalUsage;
        TelemetryApiControl(mpTelemetryNormalUsage, KI_TELEMETRY_CONTROL_DISABLED_COUNTRY, 0, lpDisabledCountryList);
        TelemetryApiControl(mpTelemetryNormalUsage, KI_TELEMETRY_CONTROL_HALT, 0, 0);

        if (lbUseFirstUsageBuffer)
        {
            mpTelemetryFirstUsage = TelemetryApiCreate(liMaxFirstUsageBufferSize, KI_TELEMETRY_BUFFERTYPE_FILLANDSTOP);
            CGS_ASSERT(mpTelemetryFirstUsage != 0, "NULL != mpTelemetryFirstUsage");

            // Record into the first-usage buffer until it fills; the normal buffer is held.
            mpTelemetryCurrent = mpTelemetryFirstUsage;
            TelemetryApiControl(mpTelemetryFirstUsage, KI_TELEMETRY_CONTROL_DISABLED_COUNTRY, 0,
                                lpDisabledCountryList);
            TelemetryApiControl(mpTelemetryNormalUsage, KI_TELEMETRY_CONTROL_HALT, 1, 0);
            TelemetryApiControl(mpTelemetryFirstUsage, KI_TELEMETRY_CONTROL_HALT, 1, 0);
            TelemetryApiControl(mpTelemetryFirstUsage, KI_TELEMETRY_CONTROL_LOG_CHAR, KI_FIRST_USAGE_LOG_CHAR, 0);
            TelemetryApiControl(mpTelemetryNormalUsage, KI_TELEMETRY_CONTROL_LOG_CHAR, KI_NORMAL_USAGE_LOG_CHAR, 0);
            TelemetryApiSetCallback(mpTelemetryFirstUsage, KI_TELEMETRY_CBTYPE_BUFFERFULL,
                                    _FirstUsageBufferFull, this);
            TelemetryApiSetCallback(mpTelemetryFirstUsage, KI_TELEMETRY_CBTYPE_FULLSENDCOMPLETE,
                                    _FirstUsageBufferSendComplete, this);
            muMaxFirstUsageBufferSize = TelemetryApiStatus(mpTelemetryFirstUsage, KI_TELEMETRY_STATUS_SEND_SIZE,
                                                           0, 0);
        }
        return true;
    }

    bool ServerInterfaceTelemetry::Release()
    {
        if (mpTelemetryFirstUsage != 0)
        {
            TelemetryApiDisconnect(mpTelemetryFirstUsage);
            TelemetryApiDestroy(mpTelemetryFirstUsage);
            mpTelemetryFirstUsage = 0;
        }
        if (mpTelemetryNormalUsage != 0)
        {
            TelemetryApiDisconnect(mpTelemetryNormalUsage);
            TelemetryApiDestroy(mpTelemetryNormalUsage);
            mpTelemetryNormalUsage = 0;
        }
        return true;
    }

    void ServerInterfaceTelemetry::Update()
    {
        if (mpTelemetryFirstUsage != 0)
        {
            TelemetryApiUpdate(mpTelemetryFirstUsage);
        }

        CGS_ASSERT(mpTelemetryNormalUsage != 0, "NULL != mpTelemetryNormalUsage");
        TelemetryApiUpdate(mpTelemetryNormalUsage);
    }

    void ServerInterfaceTelemetry::OnEvent(EServerInterfaceEvent leEvent, void* /*lpData*/)
    {
        if (leEvent == E_SERVER_INTERFACE_CONNECTION_EVENT_DISCONNECTED)
        {
            if (mpTelemetryFirstUsage != 0)
            {
                TelemetryApiDisconnect(mpTelemetryFirstUsage);
            }
            if (mpTelemetryNormalUsage != 0)
            {
                TelemetryApiDisconnect(mpTelemetryNormalUsage);
            }
        }
    }

    void ServerInterfaceTelemetry::EnableTemetry(bool lbEnable)
    {
        const s32 liValue = lbEnable ? 1 : 0;

        if (mpTelemetryFirstUsage != 0)
        {
            TelemetryApiControl(mpTelemetryFirstUsage, KI_TELEMETRY_CONTROL_USER_ENABLE, liValue, 0);
        }

        if (mpTelemetryNormalUsage != 0)
        {
            TelemetryApiControl(mpTelemetryNormalUsage, KI_TELEMETRY_CONTROL_USER_ENABLE, liValue, 0);
        }
    }

    EServerInterfaceError ServerInterfaceTelemetry::Connect(bool lbAbandonFirstUsage)
    {
        CGS_ASSERT(mpTelemetryNormalUsage != 0, "NULL != mpTelemetryNormalUsage");

        if (lbAbandonFirstUsage)
        {
            AbandonFirstUsageBuffer();
        }
        return AuthAndConnect();
    }

    void ServerInterfaceTelemetry::CaptureEvent(s32 liEventID, const char* lpacEventData)
    {
        CGS_ASSERT(mpaEventIDsToKeysMapping != 0, "NULL != mpaEventIDsToKeysMapping");
        CGS_ASSERT(mpTelemetryCurrent != 0, "NULL != mpTelemetryCurrent");

        const EventDataKeys& lrKeys = mpaEventIDsToKeysMapping[liEventID];

        const bool lbHasDescription = strnicmp(lrKeys.macDescription, "", KI_EVENT_STRING_LENGTH) != 0;
        const bool lbHasEventData   = (lpacEventData != 0) &&
                                      strnicmp(lpacEventData, "", KI_EVENT_STRING_LENGTH) != 0;

        char lacEvent[KI_EVENT_STRING_LENGTH];
        if (lbHasEventData)
        {
            if (lbHasDescription)
            {
                CGS_ASSERT(strlen(lrKeys.macDescription) + strlen(lpacEventData) + 1 <
                               static_cast<u32>(KI_EVENT_STRING_LENGTH),
                           " is greater than the max size of ");
                CgsCore::SPrintf(lacEvent, KI_EVENT_STRING_LENGTH, "%s.%s", lrKeys.macDescription, lpacEventData);
            }
            else
            {
                CgsCore::SPrintf(lacEvent, KI_EVENT_STRING_LENGTH, "%s", lpacEventData);
            }
        }
        else if (lbHasDescription)
        {
            CGS_ASSERT(strlen(lrKeys.macDescription) + 1 < static_cast<u32>(KI_EVENT_STRING_LENGTH),
                       " is greater than the max size of ");
            CgsCore::SPrintf(lacEvent, KI_EVENT_STRING_LENGTH, "%s", lrKeys.macDescription);
        }
        else
        {
            lacEvent[0] = 0;
        }

        TelemetryApiEvent(mpTelemetryCurrent, lrKeys.luModuleID, lrKeys.luGroupID, lacEvent);
    }

    void ServerInterfaceTelemetry::SetEventFilters(const char* lpcFirstUsageFilters,
                                                   const char* lpcNormalUsageFilters)
    {
        if (mpTelemetryFirstUsage != 0 && lpcFirstUsageFilters != 0 && lpcFirstUsageFilters[0] != 0)
        {
            TelemetryApiFilter(mpTelemetryFirstUsage, lpcFirstUsageFilters);
        }
        if (mpTelemetryNormalUsage != 0 && lpcNormalUsageFilters != 0 && lpcNormalUsageFilters[0] != 0)
        {
            TelemetryApiFilter(mpTelemetryNormalUsage, lpcNormalUsageFilters);
        }
    }

    void ServerInterfaceTelemetry::SetEventMappingTable(EventDataKeys* lpaEventIDsToKeysMapping)
    {
        mpaEventIDsToKeysMapping = lpaEventIDsToKeysMapping;
    }

    void ServerInterfaceTelemetry::SetDisabledCountryList(const char* lpcDisabledCountryList)
    {
        TelemetryApiControl(mpTelemetryNormalUsage, KI_TELEMETRY_CONTROL_DISABLED_COUNTRY, 0,
                            const_cast<char*>(lpcDisabledCountryList));
    }

    void ServerInterfaceTelemetry::AbandonFirstUsageBuffer()
    {
        CGS_ASSERT(mpTelemetryNormalUsage != 0, "NULL != mpTelemetryNormalUsage");

        if (mpTelemetryFirstUsage == 0)
        {
            return;
        }

        TelemetryApiSetCallback(mpTelemetryFirstUsage, KI_TELEMETRY_CBTYPE_BUFFERFULL, 0, this);
        TelemetryApiSetCallback(mpTelemetryFirstUsage, KI_TELEMETRY_CBTYPE_FULLSENDCOMPLETE, 0, this);

        // Move as many first-usage events as the normal buffer has room for.
        u32 luEventsToMove = TelemetryApiStatus(mpTelemetryFirstUsage, KI_TELEMETRY_STATUS_NUM_EVENTS, 0, 0);
        const s32 liMaxEvents = TelemetryApiStatus(mpTelemetryNormalUsage, KI_TELEMETRY_STATUS_MAX_EVENTS, 0, 0);
        const s32 liNumEvents = TelemetryApiStatus(mpTelemetryNormalUsage, KI_TELEMETRY_STATUS_NUM_EVENTS, 0, 0);
        const u32 luRoom = static_cast<u32>(liMaxEvents - liNumEvents);
        if (luEventsToMove > luRoom)
        {
            luEventsToMove = luRoom;
        }

        TelemetryApiEventT lEvent;
        while (luEventsToMove != 0)
        {
            --luEventsToMove;
            TelemetryApiPushFront(mpTelemetryNormalUsage, TelemetryApiPopBack(mpTelemetryFirstUsage, &lEvent));
        }

        TelemetryApiDisconnect(mpTelemetryFirstUsage);
        TelemetryApiDestroy(mpTelemetryFirstUsage);
        mpTelemetryFirstUsage = 0;
        mpTelemetryCurrent    = mpTelemetryNormalUsage;
        TelemetryApiControl(mpTelemetryNormalUsage, KI_TELEMETRY_CONTROL_HALT, 0, 0);
    }

    EServerInterfaceError ServerInterfaceTelemetry::AuthAndConnect()
    {
        CGS_ASSERT(mpServerInterface != 0, "NULL != mpServerInterface");

        char* lpcAuthString = mpServerInterface->GetMessageBuffer();
        lpcAuthString[0] = 0;

        ServerInterfaceServerInfo* lpServerInfo = static_cast<ServerInterfaceServerInfo*>(
            mpServerInterface->GetComponent(E_COMPONENTS_SERVERINFO));
        lpServerInfo->GetTelemetryAuthString(lpcAuthString, KI_MESSAGE_BUFFER_SIZE);

        if (lpcAuthString[0] == 0)
        {
            return E_SERVER_INTERFACE_TELEMETRY_ERROR_NO_AUTH_STRING;
        }

        EServerInterfaceError leError = ConvertError(TelemetryApiAuthent(mpTelemetryNormalUsage, lpcAuthString),
                                                     KA_TELEMETRY_SERVER_INTERFACE_ERROR_MAPPING, 5);
        if (leError != E_SERVER_INTERFACE_ERROR_NONE)
        {
            return leError;
        }
        if (TelemetryApiConnect(mpTelemetryNormalUsage) == 0)
        {
            return E_SERVER_INTERFACE_TELEMETRY_ERROR_NOT_CONNECTED;
        }

        if (mpTelemetryFirstUsage != 0)
        {
            leError = ConvertError(TelemetryApiAuthent(mpTelemetryFirstUsage, lpcAuthString),
                                   KA_TELEMETRY_SERVER_INTERFACE_ERROR_MAPPING, 5);
            if (leError != E_SERVER_INTERFACE_ERROR_NONE)
            {
                return leError;
            }
            if (TelemetryApiConnect(mpTelemetryFirstUsage) == 0)
            {
                return E_SERVER_INTERFACE_TELEMETRY_ERROR_NOT_CONNECTED;
            }
        }
        return E_SERVER_INTERFACE_ERROR_NONE;
    }

    void ServerInterfaceTelemetry::_FirstUsageBufferFull(TelemetryApiRefT* lpTelemetry, void* lpUserData)
    {
        ServerInterfaceTelemetry* lpServerInterfaceTelemetry = static_cast<ServerInterfaceTelemetry*>(lpUserData);
        CGS_ASSERT(lpServerInterfaceTelemetry != 0, "NULL != lpServerInterfaceTelemetry");

        // The first-usage buffer is full: record into the normal buffer and hold the full one.
        lpServerInterfaceTelemetry->mpTelemetryCurrent = lpServerInterfaceTelemetry->mpTelemetryNormalUsage;
        TelemetryApiControl(lpTelemetry, KI_TELEMETRY_CONTROL_HALT, 0, 0);
    }

    void ServerInterfaceTelemetry::_FirstUsageBufferSendComplete(TelemetryApiRefT* /*lpTelemetry*/,
                                                                 void* lpUserData)
    {
        ServerInterfaceTelemetry* lpServerInterfaceTelemetry = static_cast<ServerInterfaceTelemetry*>(lpUserData);
        CGS_ASSERT(lpServerInterfaceTelemetry != 0, "NULL != lpServerInterfaceTelemetry");

        lpServerInterfaceTelemetry->mpServerInterface->OnEvent(E_SERVER_INTERFACE_TELEMETRY_FIRST_USAGE_DATA_UPLOADED, 0);
        lpServerInterfaceTelemetry->AbandonFirstUsageBuffer();
    }
}
