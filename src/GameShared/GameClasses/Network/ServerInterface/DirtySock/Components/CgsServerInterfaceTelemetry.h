#ifndef CGS_SERVER_INTERFACE_TELEMETRY_H
#define CGS_SERVER_INTERFACE_TELEMETRY_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"
#include "telemetryapi.h"   // TelemetryApiRefT, TelemetryApiEventT, the TelemetryApi* C API

// ===========================================================================
// CgsNetwork::ServerInterfaceTelemetry
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceTelemetry.{h,cpp}
//
// The telemetry server-interface component. Derives from
// CgsNetwork::ServerInterfaceComponent (CgsServerInterfaceTelemetry.h DWARF:
// `ServerInterfaceTelemetry : public CgsNetwork::ServerInterfaceComponent`).
//
// Member layout after the 4-word ServerInterfaceComponent base, in
// CgsServerInterfaceTelemetry.h DWARF order:
//     +0x10  muMaxFirstUsageBufferSize        (u32)
//     +0x14  mbDidSuspendHaltCurrentBuffer    (bool; word-padded)
//     +0x18  mpTelemetryFirstUsage            (DirtySock::TelemetryApiRefT*)
//     +0x1C  mpTelemetryNormalUsage           (DirtySock::TelemetryApiRefT*)
//     +0x20  mpTelemetryCurrent               (DirtySock::TelemetryApiRefT*)
//     +0x24  mpServerInterface                (ServerInterfaceDirtySock*)
//     +0x28  mpaEventIDsToKeysMapping         (EventDataKeys*)
//
// The component keeps two DirtySDK telemetry buffers: the optional first-usage buffer
// (filled once, sent, then abandoned into the normal buffer) and the normal-usage buffer.
// mpTelemetryCurrent is the one CaptureEvent records into.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceDirtySock;         // forward; pointer member only

    namespace DirtySock
    {
        using ::TelemetryApiRefT;
        using ::TelemetryApiEventT;
    }

    // CgsServerInterfaceTelemetry.h:49 -- event-id -> telemetry-keys mapping record.
    struct EventDataKeys
    {
        u32  luModuleID;       // +0x00
        u32  luGroupID;        // +0x04
        char macDescription[16]; // +0x08
    };

    class ServerInterfaceTelemetry : public ServerInterfaceComponent
    {
    public:
        // telemetryapi.h:47 -- member alias used by the DWARF member declarations.
        typedef DirtySock::TelemetryApiRefT TelemetryApiRefT;

        virtual ~ServerInterfaceTelemetry();

        // Turn the telemetry feed on or off on both buffers.
        void EnableTemetry(bool lbEnable);

        // Configure and connect (LoginManagerBase::PrepareConnectTelemetry): the downloaded
        // telemetry-disabled country list, the first-usage / normal-usage event filter rules,
        // then authenticate and connect (lbAbandonFirstUsage folds the first-usage buffer
        // into the normal one first).
        void SetDisabledCountryList(const char* lpcDisabledCountryList);
        void SetEventFilters(const char* lpcFirstUsageFilters, const char* lpcNormalUsageFilters);
        EServerInterfaceError Connect(bool lbAbandonFirstUsage);

        // Vtable: the leaf appends Destruct, Prepare, Release and Update (in that order) after
        // the five component slots.
        virtual void Construct();
        virtual void Destruct();
        virtual bool Prepare(ServerInterfaceDirtySock* lpServerInterface, bool lbUseFirstUsageBuffer,
                             const char* lpcDisabledCountryList,
                             s32 liMaxFirstUsageBufferSize, s32 liMaxNormalUsageBufferSize);
        virtual bool Release();
        virtual void Update();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);

        // Record one telemetry event {id, payload}.
        void CaptureEvent(s32 liEventID, const char* lpacEventData);

    protected:
        void SetEventMappingTable(EventDataKeys* lpaEventIDsToKeysMapping);

    private:
        void AbandonFirstUsageBuffer();
        EServerInterfaceError AuthAndConnect();
        // DirtySDK telemetry buffer callbacks (user data = the component).
        static void _FirstUsageBufferFull(TelemetryApiRefT* lpTelemetry, void* lpUserData);
        static void _FirstUsageBufferSendComplete(TelemetryApiRefT* lpTelemetry, void* lpUserData);

        u32                muMaxFirstUsageBufferSize;        // +0x10
        bool               mbDidSuspendHaltCurrentBuffer;    // +0x14
        TelemetryApiRefT*  mpTelemetryFirstUsage;            // +0x18
        TelemetryApiRefT*  mpTelemetryNormalUsage;           // +0x1C
        TelemetryApiRefT*  mpTelemetryCurrent;               // +0x20
        ServerInterfaceDirtySock* mpServerInterface;         // +0x24
        EventDataKeys*     mpaEventIDsToKeysMapping;          // +0x28
    };
}

#endif // CGS_SERVER_INTERFACE_TELEMETRY_H
