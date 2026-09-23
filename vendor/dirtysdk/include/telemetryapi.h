#ifndef DIRTYSDK_TELEMETRYAPI_H
#define DIRTYSDK_TELEMETRYAPI_H

#include "types.hpp"

// DirtySDK 5.5.3 - misc/include/telemetryapi.h
// Telemetry: a ring (or fill-and-stop) buffer of small events that is uploaded to the
// telemetry server once the ref is authenticated and connected. The CGS telemetry component
// keeps two refs (first-usage and normal-usage). PC body: ../src/pc/telemetryapipc.cpp.

// Opaque telemetry module handle.
struct TelemetryApiRefT;

// One buffered event (28 bytes).
struct TelemetryApiEventT
{
    u32  uModuleID;
    u32  uGroupID;
    s32  iTimestamp;
    char strEvent[16];
};

// Buffer / callback kinds.
enum TelemetryApiBufferTypeE
{
    TELEMETRY_BUFFERTYPE_FILLANDSTOP       = 0,
    TELEMETRY_BUFFERTYPE_CIRCULAROVERWRITE = 1
};

enum TelemetryApiCBTypeE
{
    TELEMETRY_CBTYPE_BUFFERFULL       = 0,
    TELEMETRY_CBTYPE_SENDCOMPLETE     = 1,
    TELEMETRY_CBTYPE_FULLSENDCOMPLETE = 2,
    TELEMETRY_CBTYPE_TOTAL            = 3
};

// Callback TYPE: (telemetry ref, user data).
typedef void (TelemetryApiCallbackT)(TelemetryApiRefT* pTelemetryRef, void* pUserData);

// TelemetryApiControl selectors.
#define TELEMETRY_CONTROL_USER_ENABLE      (('u' << 24) | ('n' << 16) | ('b' << 8) | 'l')
#define TELEMETRY_CONTROL_DISABLED_COUNTRY (('c' << 24) | ('d' << 16) | ('b' << 8) | 'l')
#define TELEMETRY_CONTROL_HALT             (('h' << 24) | ('a' << 16) | ('l' << 8) | 't')
#define TELEMETRY_CONTROL_LOG_CHAR         (('l' << 24) | ('o' << 16) | ('g' << 8) | 'c')

// TelemetryApiStatus selectors.
#define TELEMETRY_STATUS_SEND_SIZE         (('s' << 24) | ('s' << 16) | ('i' << 8) | 'z')
#define TELEMETRY_STATUS_NUM_EVENTS        (('n' << 24) | ('u' << 16) | ('m' << 8) | 'e')
#define TELEMETRY_STATUS_MAX_EVENTS        (('m' << 24) | ('a' << 16) | ('x' << 8) | 'e')
#define TELEMETRY_STATUS_HALTED            (('h' << 24) | ('a' << 16) | ('l' << 8) | 't')

#ifdef __cplusplus
extern "C" {
#endif

// Create a buffer of uNumEvents events (0: 64) of type eBufferType; destroy it.
TelemetryApiRefT*   TelemetryApiCreate(u32 uNumEvents, s32 eBufferType);
void                TelemetryApiDestroy(TelemetryApiRefT* pTelemetryRef);

// Authenticate with the lobby's telemetry auth string (0 on success), connect (1 on
// success, 0 on failure), disconnect.
s32                 TelemetryApiAuthent(TelemetryApiRefT* pTelemetryRef, const char* pAuth);
s32                 TelemetryApiConnect(TelemetryApiRefT* pTelemetryRef);
void                TelemetryApiDisconnect(TelemetryApiRefT* pTelemetryRef);

// Pump uploads.
void                TelemetryApiUpdate(TelemetryApiRefT* pTelemetryRef);

// Install the callback for eType (a TelemetryApiCBTypeE); pCallback NULL clears it.
void                TelemetryApiSetCallback(TelemetryApiRefT* pTelemetryRef, s32 eType,
                                            TelemetryApiCallbackT* pCallback, void* pUserData);

// Status query (TELEMETRY_STATUS_*); -1 for an unknown selector or a NULL ref.
s32                 TelemetryApiStatus(TelemetryApiRefT* pTelemetryRef, s32 iSelect, void* pBuf, s32 iBufSize);

// Control (TELEMETRY_CONTROL_*); 0 on success, -1 on a rejected value.
s32                 TelemetryApiControl(TelemetryApiRefT* pTelemetryRef, s32 iKind, s32 iValue, void* pValue);

// Install an event filter rule string.
void                TelemetryApiFilter(TelemetryApiRefT* pTelemetryRef, const char* pFilterRule);

// Record one event.
void                TelemetryApiEvent(TelemetryApiRefT* pTelemetryRef, u32 uModuleID, u32 uGroupID,
                                      const char* pEvent);

// Remove the newest event into *pEvent (returns pEvent, or NULL when the buffer is empty);
// insert an event at the oldest end (0 when accepted).
TelemetryApiEventT* TelemetryApiPopBack(TelemetryApiRefT* pTelemetryRef, TelemetryApiEventT* pEvent);
s32                 TelemetryApiPushFront(TelemetryApiRefT* pTelemetryRef, const TelemetryApiEventT* pEvent);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_TELEMETRYAPI_H
