// ============================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkOutGameParamsChanged.h
// ============================================================================
// BrnNetwork::BrnNetworkModuleIO::NetworkOutGameParamsChanged -- the network-out record the
// host posts when the game parameters change (tag 15, 480 bytes). OutputGameParameters builds
// it on the stack: Construct, the selected routes into maEvents (SelectedRoutesManager::
// GetRouteData), then every scalar from the packed GameParams words.
//
// The record is pointer-free, so the host layout is the console layout (pinned below).
// Construct's defaults: every round event reset (trigger id = the light-trigger tag, no
// landmarks, event id 0), meGameMode 10, mePreviousGameMode 18, miNumRounds 3,
// miVehicleClass 9, miNumRunnerCrashes 3, the other words 0 and all four flags true.
#pragma once

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // NetworkEvent<N>
#include "GameSource/GameState/BrnGameStateSharedIO.h"        // SpecificGameModeEventInterface::Event (44-byte round record)

#include <cstddef>   // offsetof (layout pins)

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    struct NetworkOutGameParamsChanged : public NetworkEvent<15>
    {
        static const s32 KI_NUM_EVENTS = 10;

        typedef BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event Event;

        Event maEvents[KI_NUM_EVENTS];   // +0x000 (stride 44)
        s32   meGameMode;                // +0x1B8 GameStateModuleIO::EGameModeType
        s32   mePreviousGameMode;        // +0x1BC GameStateModuleIO::EGameModeType
        s32   meSecurity;                // +0x1C0 BrnNetwork::EBrnGameSecurity
        s32   meBoostType;               // +0x1C4 BrnNetwork::EBoostType
        s32   meVehicleChoice;           // +0x1C8 BrnNetwork::EVehicleChoice
        s32   miNumRounds;               // +0x1CC
        s32   miVehicleClass;            // +0x1D0
        s32   miNumRunnerCrashes;        // +0x1D4
        s32   miTimeLimit;               // +0x1D8
        bool  mbRanked;                  // +0x1DC
        bool  mbInfiniteBoost;           // +0x1DD
        bool  mbTrafficOn;               // +0x1DE
        bool  mbTrafficCheckingOn;       // +0x1DF

        // Reset to the defaults listed above.
        void Construct();

    private:
        // Never called.
        static void _AssertLayout()
        {
            static_assert(sizeof(Event) == 44, "round event stride");
            static_assert(offsetof(NetworkOutGameParamsChanged, meGameMode) == 0x1B8, "meGameMode @+0x1B8");
            static_assert(offsetof(NetworkOutGameParamsChanged, mePreviousGameMode) == 0x1BC, "mePreviousGameMode @+0x1BC");
            static_assert(offsetof(NetworkOutGameParamsChanged, meSecurity) == 0x1C0, "meSecurity @+0x1C0");
            static_assert(offsetof(NetworkOutGameParamsChanged, meBoostType) == 0x1C4, "meBoostType @+0x1C4");
            static_assert(offsetof(NetworkOutGameParamsChanged, meVehicleChoice) == 0x1C8, "meVehicleChoice @+0x1C8");
            static_assert(offsetof(NetworkOutGameParamsChanged, miNumRounds) == 0x1CC, "miNumRounds @+0x1CC");
            static_assert(offsetof(NetworkOutGameParamsChanged, miVehicleClass) == 0x1D0, "miVehicleClass @+0x1D0");
            static_assert(offsetof(NetworkOutGameParamsChanged, miNumRunnerCrashes) == 0x1D4, "miNumRunnerCrashes @+0x1D4");
            static_assert(offsetof(NetworkOutGameParamsChanged, miTimeLimit) == 0x1D8, "miTimeLimit @+0x1D8");
            static_assert(offsetof(NetworkOutGameParamsChanged, mbRanked) == 0x1DC, "mbRanked @+0x1DC");
            static_assert(offsetof(NetworkOutGameParamsChanged, mbTrafficCheckingOn) == 0x1DF, "mbTrafficCheckingOn @+0x1DF");
            static_assert(sizeof(NetworkOutGameParamsChanged) == 480, "tag 15 is queued as 480 bytes");
        }
    };
}
}
