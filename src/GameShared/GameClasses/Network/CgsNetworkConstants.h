#pragma once

#include "types.hpp"   // s32

namespace CgsNetwork
{
    enum EServerType
    {
        E_SERVER_TYPE_LOCAL = 0,
        E_SERVER_TYPE_DEV,
        E_SERVER_TYPE_TEST,
        E_SERVER_TYPE_JUICE,
        E_SERVER_TYPE_ARTIST,
        E_SERVER_TYPE_DEMO_1,
        E_SERVER_TYPE_DEMO_2,
        E_SERVER_TYPE_COUNT
    };

    // DWARF CgsNetworkConstants.h:76. The telemetry parameter-buffer cap (the width of
    // BrnNetwork::BrnNetworkModuleIO::TelemetryData::macBuffer); the assert in
    // TelemetryData::AddParameter @0x82354010 compares against 16 (`cmpwi cr6, r11, 0x10`).
    // [takedown P1 wave 2026-09-03, additive.]
    const s32 KI_MAX_TELEMETRY_DATA_SIZE = 16;

    // The session-wide player handle and its "no player" sentinel (all bits set; the
    // console compares against -1).
    typedef s32 NetworkPlayerID;
    const NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}
