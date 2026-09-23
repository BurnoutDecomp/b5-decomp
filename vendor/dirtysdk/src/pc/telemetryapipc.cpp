// ============================================================================
// telemetryapipc.cpp -- TelemetryApi for the PC build.
//
// [PC platform layer] Telemetry uploads to the online service's telemetry server, which the
// PC build never reaches. A ref keeps the settings the game gives it (halt / user-enable
// flags, disabled-country list, log prefix, callbacks) and answers the size queries from its
// capacity, but it stores no events: TelemetryApiEvent / TelemetryApiPushFront drop them,
// the buffer always reads empty, so the buffer-full / send-complete callbacks never fire.
// Authenticate and connect succeed (nothing is sent). Offline these are only reached through
// the component's Prepare / Update / Release; the login flow that connects runs on the LAN.
// State lives in the ref TelemetryApiCreate returns (DirtyMemAlloc / DirtyMemFree).
// ============================================================================

#include "telemetryapi.h"
#include "dirtymem.h"   // DirtyMemAlloc / DirtyMemFree / DirtyMemGroupQuery

#include <cstring>      // memset / strncpy

namespace
{
    // DirtyMem module id.
    const s32 KI_MEMID_TELEMETRY = ('t' << 24) | ('e' << 16) | ('l' << 8) | 'm';

    // Buffer size used when TelemetryApiCreate is given 0 events.
    const u32 KU_TELEMETRY_DEFAULT_EVENTS = 64;

    // Size of the upload header in front of the event records ('ssiz').
    const s32 KI_TELEMETRY_SEND_HEADER_SIZE = 12;

    // Disabled-country list capacity.
    const s32 KI_TELEMETRY_COUNTRY_LIST_SIZE = 1024;
}

struct TelemetryApiRefT
{
    s32                    iMemGroup;
    u32                    uNumEvents;      // capacity
    s32                    eBufferType;     // TelemetryApiBufferTypeE
    bool                   bHalted;         // TELEMETRY_CONTROL_HALT
    bool                   bUserDisabled;   // TELEMETRY_CONTROL_USER_ENABLE <= 0
    bool                   bConnected;
    s32                    iLogChar;        // TELEMETRY_CONTROL_LOG_CHAR, 0 = none
    TelemetryApiCallbackT* apCallback[TELEMETRY_CBTYPE_TOTAL];
    void*                  apCallbackData[TELEMETRY_CBTYPE_TOTAL];
    char                   strDisabledCountries[KI_TELEMETRY_COUNTRY_LIST_SIZE];
};

extern "C" TelemetryApiRefT* TelemetryApiCreate(u32 uNumEvents, s32 eBufferType)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    if (uNumEvents == 0)
    {
        uNumEvents = KU_TELEMETRY_DEFAULT_EVENTS;
    }

    TelemetryApiRefT* pRef =
        static_cast<TelemetryApiRefT*>(DirtyMemAlloc(sizeof(TelemetryApiRefT), KI_MEMID_TELEMETRY, iMemGroup));
    if (pRef == NULL)
    {
        return NULL;
    }
    memset(pRef, 0, sizeof(*pRef));
    pRef->iMemGroup   = iMemGroup;
    pRef->uNumEvents  = uNumEvents;
    pRef->eBufferType = eBufferType;
    return pRef;
}

extern "C" void TelemetryApiDestroy(TelemetryApiRefT* pTelemetryRef)
{
    DirtyMemFree(pTelemetryRef, KI_MEMID_TELEMETRY, pTelemetryRef->iMemGroup);
}

extern "C" s32 TelemetryApiAuthent(TelemetryApiRefT* /*pTelemetryRef*/, const char* /*pAuth*/)
{
    return 0;
}

extern "C" s32 TelemetryApiConnect(TelemetryApiRefT* pTelemetryRef)
{
    pTelemetryRef->bConnected = true;
    return 1;
}

extern "C" void TelemetryApiDisconnect(TelemetryApiRefT* pTelemetryRef)
{
    pTelemetryRef->bConnected = false;
}

extern "C" void TelemetryApiUpdate(TelemetryApiRefT* /*pTelemetryRef*/)
{
    // Nothing is buffered, so nothing is uploaded.
}

extern "C" void TelemetryApiSetCallback(TelemetryApiRefT* pTelemetryRef, s32 eType, TelemetryApiCallbackT* pCallback,
                                        void* pUserData)
{
    if ((pTelemetryRef == NULL) || (eType < 0) || (eType >= TELEMETRY_CBTYPE_TOTAL))
    {
        return;
    }
    pTelemetryRef->apCallback[eType]     = pCallback;
    pTelemetryRef->apCallbackData[eType] = pUserData;
}

extern "C" s32 TelemetryApiStatus(TelemetryApiRefT* pTelemetryRef, s32 iSelect, void* /*pBuf*/, s32 /*iBufSize*/)
{
    if (pTelemetryRef == NULL)
    {
        return -1;
    }

    switch (iSelect)
    {
    case TELEMETRY_STATUS_MAX_EVENTS:
        return static_cast<s32>(pTelemetryRef->uNumEvents);

    case TELEMETRY_STATUS_NUM_EVENTS:
        return 0;

    case TELEMETRY_STATUS_SEND_SIZE:
        return static_cast<s32>(sizeof(TelemetryApiEventT) * pTelemetryRef->uNumEvents) + KI_TELEMETRY_SEND_HEADER_SIZE;

    case TELEMETRY_STATUS_HALTED:
        return pTelemetryRef->bHalted ? 1 : 0;

    default:
        return -1;
    }
}

extern "C" s32 TelemetryApiControl(TelemetryApiRefT* pTelemetryRef, s32 iKind, s32 iValue, void* pValue)
{
    if (pTelemetryRef == NULL)
    {
        return 0;
    }

    switch (iKind)
    {
    case TELEMETRY_CONTROL_DISABLED_COUNTRY:
        if (pValue != NULL)
        {
            strncpy(pTelemetryRef->strDisabledCountries, static_cast<const char*>(pValue),
                    KI_TELEMETRY_COUNTRY_LIST_SIZE - 1);
            pTelemetryRef->strDisabledCountries[KI_TELEMETRY_COUNTRY_LIST_SIZE - 1] = 0;
        }
        return 0;

    case TELEMETRY_CONTROL_HALT:
        pTelemetryRef->bHalted = (iValue != 0);
        return 0;

    case TELEMETRY_CONTROL_LOG_CHAR:
        // The prefix must be printable: '+', '-', '~', '=', a digit or a letter (0 clears it).
        if ((iValue == 0) || (iValue == '+') || (iValue == '-') || (iValue == '~') || (iValue == '=') ||
            ((iValue >= '0') && (iValue <= '9')) || ((iValue >= 'A') && (iValue <= 'Z')) ||
            ((iValue >= 'a') && (iValue <= 'z')))
        {
            pTelemetryRef->iLogChar = iValue;
            return 0;
        }
        return -1;

    case TELEMETRY_CONTROL_USER_ENABLE:
        pTelemetryRef->bUserDisabled = (iValue <= 0);
        return 0;

    default:
        return 0;
    }
}

extern "C" void TelemetryApiFilter(TelemetryApiRefT* /*pTelemetryRef*/, const char* /*pFilterRule*/)
{
    // Events are not stored, so there is nothing to filter.
}

extern "C" void TelemetryApiEvent(TelemetryApiRefT* /*pTelemetryRef*/, u32 /*uModuleID*/, u32 /*uGroupID*/,
                                  const char* /*pEvent*/)
{
    // Dropped: there is no telemetry server to upload to.
}

extern "C" TelemetryApiEventT* TelemetryApiPopBack(TelemetryApiRefT* /*pTelemetryRef*/, TelemetryApiEventT* /*pEvent*/)
{
    // The buffer is always empty.
    return NULL;
}

extern "C" s32 TelemetryApiPushFront(TelemetryApiRefT* /*pTelemetryRef*/, const TelemetryApiEventT* /*pEvent*/)
{
    // Dropped, like every recorded event.
    return 0;
}
