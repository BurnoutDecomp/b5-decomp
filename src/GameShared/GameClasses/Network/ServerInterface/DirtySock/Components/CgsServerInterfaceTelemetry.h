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

        // ---- ADDITIVE GROW (BrnServerInterfaceTelemetry TU) -------------------------------
        // The base lifecycle/behaviour entries the game leaf BrnServerInterfaceTelemetry
        // (GameSource/Network/Components/BrnServerInterfaceTelemetry.{h,cpp}) chains into.
        // Signatures are DWARF-authoritative (CgsServerInterfaceTelemetry.h/.cpp DWARF):
        //   Prepare  @ cpp:125 -- component base Prepare gate. Extra args are the telemetry-
        //                          disabled country list + the first-usage/normal-usage buffer
        //                          caps (KI_FIRST_USAGE_BUFFER_SIZE 250 / KI_NORMAL_USAGE_BUFFER_SIZE 100).
        //   Release  @ cpp:182 -- component base Release gate.
        //   Update   @ cpp:217 -- pump the DirtySock telemetry feed.
        //   CaptureEvent @ cpp:363 -- record one telemetry event {id, payload}.
        //   SetEventMappingTable @ cpp:779 (protected) -- latch the event-id -> keys table.
        // Declared-only here; bodies live in this component's own (DirtySock) TU.
        virtual bool Prepare(ServerInterfaceDirtySock* lpServerInterface, bool lbConnectImmediately,
                             const char* lpcDisabledCountryList,
                             s32 liMaxFirstUsageBufferSize, s32 liMaxNormalUsageBufferSize);
        virtual bool Release();
        virtual void Update();
        void CaptureEvent(s32 liEventID, const char* lpacEventData);

    protected:
        void SetEventMappingTable(EventDataKeys* lpaEventIDsToKeysMapping);

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
