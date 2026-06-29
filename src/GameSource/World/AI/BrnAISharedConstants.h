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

    // DWARF BrnAISharedConstants.h:100 -- the route-finding/game-mode discriminator an AI car
    // runs under. AICar stores this at AICar+0x14C0; the aggression state machine reads it to
    // decide RACE vs ROAD_RAGE vs PURSUIT vs MARKED_MAN behaviour (BrnAIAggression bodies
    // compare it to 1/2/3/6).
    enum ERouteFindingStyle
    {
        E_ROUTE_FINDING_FREE_ROAM       = 0,
        E_ROUTE_FINDING_RACE            = 1,
        E_ROUTE_FINDING_ROAD_RAGE       = 2,
        E_ROUTE_FINDING_PURSUIT         = 3,
        E_ROUTE_FINDING_AVOID_PLAYER    = 4,
        E_ROUTE_FINDING_ALWAYS_STRAIGHT = 5,
        E_ROUTE_FINDING_MARKED_MAN      = 6,
        E_ROUTE_FINDING_COUNT           = 7,
    };

    // DWARF BrnAISharedConstants.h:112 -- the per-frame behaviour an AI car is running. Stored
    // at AICar+0x14B4 (the previous one at +0x14B8); AICar::SetBehaviour asserts the new value is
    // in [0, E_AI_BEHAVIOUR_COUNT). Enumerator names/values are the DecFIGS DWARF.
    enum EAIBehaviour
    {
        E_AI_BEHAVIOUR_STOP            = 0,
        E_AI_BEHAVIOUR_ROLLING_START   = 1,
        E_AI_BEHAVIOUR_DRIVE_THRU      = 2,
        E_AI_BEHAVIOUR_CRUISING        = 3,
        E_AI_BEHAVIOUR_FIGHTING        = 4,
        E_AI_BEHAVIOUR_QUICK_TURN      = 5,
        E_AI_BEHAVIOUR_SLOW_TURN       = 6,
        E_AI_BEHAVIOUR_CRASHING        = 7,
        E_AI_BEHAVIOUR_DONUT           = 8,
        E_AI_BEHAVIOUR_POST_RACE_WIN   = 9,
        E_AI_BEHAVIOUR_POST_RACE_LOSE  = 10,
        E_AI_BEHAVIOUR_COUNT           = 11,
    };

    // DWARF BrnAISharedConstants.h:92 -- whether an AI car is currently active and in range of
    // the player. Stored at AICar+0x14C8; the aggression machine treats any non-IN_RANGE state
    // as "not suitable for aggression".
    enum EAICarState
    {
        E_AI_CAR_STATE_IN_RANGE     = 0,
        E_AI_CAR_STATE_OUT_OF_RANGE = 1,
        E_AI_CAR_STATE_INACTIVE     = 2,
        E_AI_CAR_STATE_COUNT        = 3,
    };
}
