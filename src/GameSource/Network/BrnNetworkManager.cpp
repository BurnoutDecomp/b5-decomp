#include "types.hpp"

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameSource/Network/BrnServerInterface.h"                                          // GetTelemetryComponent
#include "GameSource/Network/Components/BrnServerInterfaceTelemetry.h"                      // BrnServerInterfaceTelemetry
#include "GameShared/GameClasses/Core/CgsAssert.h"                                          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                     // CgsCore::SPrintf
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                             // KI_MAX_TELEMETRY_DATA_SIZE
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- the online hub.
//
// The console functions of the class are declared in BrnNetworkManager.h with their
// reconstructed signatures, and the class layout is homed there by name. Most bodies live in
// the partfiles beside this one.
// ============================================================================================

namespace BrnNetwork
{
    // ARTIST 0x82582128. Serialise a NetworkPlayerID over the full signed 32-bit range.
    BrnNetworkManager::PackOrUnpackResult BrnNetworkManager::PackOrUnpack(
        CgsNetwork::Message* lpMessage, NetworkPlayerID* lpNetworkPlayerID)
    {
        s32 liValue = *lpNetworkPlayerID;
        const PackOrUnpackResult lxResult = lpMessage->PackOrUnpack(&liValue, -0x7FFFFFFF - 1, 0x7FFFFFFF);
        *lpNetworkPlayerID = liValue;
        return lxResult;
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::CaptureTelemetryEvent (integer payload)
    //
    // The integer form of the telemetry hook: print the value as decimal text into a
    // telemetry-sized buffer and record the event with that text as its payload.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::CaptureTelemetryEvent(ETelemetryHook leHook, s32 liData)
    {
        CGS_ASSERT(GetServerInterface(), "GetServerInterface()");
        CGS_ASSERT(GetServerInterface()->GetTelemetryComponent(), "GetServerInterface()->GetTelemetryComponent()");

        char lacData[CgsNetwork::KI_MAX_TELEMETRY_DATA_SIZE];
        CgsCore::SPrintf(lacData, sizeof(lacData), "%i", liData);
        GetServerInterface()->GetTelemetryComponent()->CaptureEvent(leHook, lacData);
    }
}
