#ifndef CGS_SERVER_INTERFACE_TELEMETRY_H
#define CGS_SERVER_INTERFACE_TELEMETRY_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

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
// This TU bodies EnableTemetry @ 0x82541968 (called by BrnNetwork::StateManager
// UpdateLogin / Update). The X360 asm reads mpTelemetryFirstUsage (this+0x18) and
// mpTelemetryNormalUsage (this+0x1C) and, for each non-null handle, drives the DirtySDK
// telemetry "enable" control on the underlying TelemetryApiRefT. The remaining
// behavioural virtuals/methods (Construct / Prepare / Update / Connect / CaptureEvent /
// ...) and the static error/lookup tables are owned by their own dossiers and are NOT
// bodied here.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterfaceDirtySock;          // forward; pointer member only

    namespace DirtySock
    {
        // telemetryapi.h:47 -- the DirtySDK telemetry API ref. Forward-declared here;
        // the component only holds pointers to it.
        struct TelemetryApiRefT;
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

        // CgsServerInterfaceTelemetry.h:81 -- destructor (body owned by a dedicated
        // dossier; declared-only here so the vtable is well-formed).
        virtual ~ServerInterfaceTelemetry();

        // CgsServerInterfaceTelemetry.h:232 -- @ 0x82541968.
        void EnableTemetry(bool lbEnable);

        // ---- ADDITIVE GROW (BrnNetworkLoginManagerBase TU) --------------------------------
        // LoginManagerBase::PrepareConnectTelemetry @ 0x8254FEC8 configures and connects this
        // component (reached through *(mpNetworkManager+0x38EC) + telemetry slot):
        //   SetDisabledCountryList -- pass the downloadable-config's telemetry-disabled country
        //                             list (the asm value from BrnServerInterfaceDownloadableConfig::
        //                             GetTelemetryDisabledList).
        //   SetEventFilters        -- pass the two event-filter blobs that live inside the
        //                             downloadable-config component (the +280 / +536 fields).
        //   Connect                -- open the telemetry connection (r4 == 0 == not first-usage),
        //                             returning a non-zero error on failure.
        // Declared-only here; the bodies live in this component's own (DirtySock) TU.
        void SetDisabledCountryList(s32 liDisabledCountryList);
        void SetEventFilters(const u8* lpFirstUsageFilters, const u8* lpNormalUsageFilters);
        s32  Connect(bool lbFirstUsage);

    private:
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
