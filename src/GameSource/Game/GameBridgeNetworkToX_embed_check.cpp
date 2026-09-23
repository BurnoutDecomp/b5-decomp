// Embed-check for the BrnGame::BrnGameModule network-bridge family. Forces the four bridge
// methods + the widest GUI event records to be referenced so the gate compiles them.
// Mirrors GameBridgeReplayToX_embed_check.cpp / GameBridgeControllerToX_embed_check.cpp.
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeNetworkToX.h"
#include "GameSource/Network/BrnNetworkModuleIO.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"

namespace
{
    // Take the address of each bridge method so it is emitted + type-checked.
    void ReferenceBridges()
    {
        void (BrnGame::BrnGameModule::*lpToGui)(
            CgsGui::CgsGuiModuleIO::InputBuffer*, const BrnNetwork::BrnNetworkModuleIO::OutputBuffer*) =
            &BrnGame::BrnGameModule::BridgeNetworkToGui;
        void (BrnGame::BrnGameModule::*lpToGameEvents)(
            BrnGameState::GameStateModuleIO::PreWorldInputBuffer*, const BrnNetwork::BrnNetworkModuleIO::OutputBuffer*) =
            &BrnGame::BrnGameModule::TranslateNetworkEventsToGameEvents;
        void (BrnGame::BrnGameModule::*lpToGuiEvents)(
            CgsGui::CgsGuiModuleIO::InputBuffer*, const BrnNetwork::BrnNetworkModuleIO::OutputBuffer*) =
            &BrnGame::BrnGameModule::TranslateNetworkEventsToGuiEvents;
        void (BrnGame::BrnGameModule::*lpIfToGui)(
            CgsGui::CgsGuiModuleIO::InputBuffer*, const BrnNetwork::BrnNetworkModuleIO::NetworkToGuiInterface*) =
            &BrnGame::BrnGameModule::TranslateNetworkInterfaceToGuiEvents;
        (void)lpToGui; (void)lpToGameEvents; (void)lpToGuiEvents; (void)lpIfToGui;
    }

    // Pin the widest GUI event records the bridge posts: the publisher bakes sizeof(T) as the
    // queued record size, so each one must match the size the console bakes.
    void ExerciseEventTags()
    {
        static_assert(sizeof(BrnGui::GuiEventScoreboardResponseTableEvent) == 2924, "scoreboard table record");
        static_assert(sizeof(BrnGui::GuiEventNetworkPlayerStatus) == 2544,           "player status record");
        static_assert(sizeof(BrnGui::GuiEventNetworkPlayerList) == 168,              "player list record");
        static_assert(sizeof(BrnGui::GuiEventNetworkLobbyPlayerList) == 456,         "lobby player list record");
        static_assert(sizeof(BrnGui::GuiLiveRevengeUpdateEvent) == 16,               "live revenge update record");
    }
}

extern "C" void GameBridgeNetworkToX_embed_check()
{
    ReferenceBridges();
    (void)&ExerciseEventTags;
}
