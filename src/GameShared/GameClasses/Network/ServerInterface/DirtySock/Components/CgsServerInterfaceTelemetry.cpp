#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"

#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceTelemetry::EnableTemetry @ 0x82541968
//
// EnableTemetry turns the DirtySDK telemetry feed on or off for both telemetry handles
// the component holds. The X360 asm reads mpTelemetryFirstUsage (this+0x18) and
// mpTelemetryNormalUsage (this+0x1C); for each non-null handle it issues the DirtySDK
// telemetry control selector 'unbl' (0x756E626C) with iValue set to the requested
// enable flag (1 when enabling, 0 when disabling) and a null pValue. The disassembly
// hoists the first-usage handle once (lwz r3,0x18) before branching on the enable flag,
// and re-reads the normal-usage handle (lwz r3,0x1C) on both arms; the structure below
// preserves that store/control ordering. The return value is the result of the last
// TelemetryApiControl call that ran (the normal-usage handle when present, else the
// first-usage handle's), or the loaded normal-usage handle word when neither control
// fired -- the callers (BrnNetwork::StateManager UpdateLogin / Update) discard it.
//
// TelemetryApiControl is the DirtySDK telemetry API entry
// (telemetryapi.h: int32_t TelemetryApiControl(TelemetryApiRefT*, int32_t iKind,
// int32_t iValue, void* pValue)); declared extern here and homed in the DirtySDK
// telemetryapi TU. The remaining ServerInterfaceTelemetry lifecycle/behavioural methods
// and its static error/lookup tables are owned by their own dossiers and are not bodied
// in this TU.

namespace CgsNetwork
{
    namespace DirtySock
    {
        // telemetryapi.h -- DirtySDK telemetry control entry. 'iKind' selects the
        // control; 'iValue' / 'pValue' carry the control argument.
        extern "C" s32 TelemetryApiControl(TelemetryApiRefT* lpRef, s32 liKind, s32 liValue, void* lpValue);
    }

    namespace
    {
        // DirtySDK telemetry control selector 'unbl' (X360 immediate 0x756E626C) -- the
        // user-enable/disable selector EnableTemetry drives on each telemetry handle.
        const s32 KI_TELEMETRY_CONTROL_USER_ENABLE = 0x756E626C;
    }

    void ServerInterfaceTelemetry::EnableTemetry(bool lbEnable)
    {
        const s32 liValue = lbEnable ? 1 : 0;

        if (mpTelemetryFirstUsage != 0)
        {
            DirtySock::TelemetryApiControl(mpTelemetryFirstUsage,
                                           KI_TELEMETRY_CONTROL_USER_ENABLE, liValue, 0);
        }

        if (mpTelemetryNormalUsage != 0)
        {
            DirtySock::TelemetryApiControl(mpTelemetryNormalUsage,
                                           KI_TELEMETRY_CONTROL_USER_ENABLE, liValue, 0);
        }
    }
}
