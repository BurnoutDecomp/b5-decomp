#pragma once

// BrnAI shared constants/enumerations. Enumerator names/values recovered from the
// DecFIGS DWARF (BrnAISharedConstants.h). This home currently provides the enums the
// reconstructed AI-module IO request/result types reference; sibling AI enums/constants
// are added additively by their own consumers.

namespace BrnAI
{
    // DWARF BrnAISharedConstants.h:40 -- how an AI car should be reset back onto the track.
    enum EResetType
    {
        E_RESET_TYPE_INVALID                  = 0,
        E_RESET_TYPE_STANDARD                 = 1,
        E_RESET_TYPE_BEHIND_PLAYER            = 2,
        E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE  = 3,
        E_RESET_TYPE_AHEAD_PLAYER_ON_COMING   = 4,
        E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE  = 5,
        E_RESET_TYPE_BEHIND_PLAYER_RACE_START = 6,
        E_RESET_TYPE_AWAY_FROM_PLAYER         = 7,
        E_RESET_TYPE_COUNT                    = 8,
    };
}
