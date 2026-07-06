// Embed-check for the BrnGame::BrnGameModule network-bridge family. Forces the four bridge
// methods + the opaque GUI/game-state event tag layouts to be referenced so the gate compiles
// them. Mirrors GameBridgeReplayToX_embed_check.cpp / GameBridgeControllerToX_embed_check.cpp.
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeNetworkToX.h"
#include "GameSource/Network/BrnNetworkModuleIO.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"

namespace
{
    // Take the address of each bridge method so it is emitted + type-checked.
    void ReferenceBridges()
    {
        int (BrnGame::BrnGameModule::*lpToGui)(
            void*, const BrnNetwork::BrnNetworkModuleIO::OutputBuffer*) =
            &BrnGame::BrnGameModule::BridgeNetworkToGui;
        int (BrnGame::BrnGameModule::*lpToGameEvents)(
            BrnGameState::GameStateModule*, const BrnNetwork::BrnNetworkModuleIO::OutputBuffer*) =
            &BrnGame::BrnGameModule::TranslateNetworkEventsToGameEvents;
        int (BrnGame::BrnGameModule::*lpToGuiEvents)(
            void*, const BrnNetwork::BrnNetworkModuleIO::OutputBuffer*) =
            &BrnGame::BrnGameModule::TranslateNetworkEventsToGuiEvents;
        int (BrnGame::BrnGameModule::*lpIfToGui)(void*, const void*) =
            &BrnGame::BrnGameModule::TranslateNetworkInterfaceToGuiEvents;
        (void)lpToGui; (void)lpToGameEvents; (void)lpToGuiEvents; (void)lpIfToGui;
    }

    // Exercise the widest opaque event tags so their sizes are pinned.
    void ExerciseEventTags()
    {
        volatile s32 li =
            static_cast<s32>(sizeof(BrnGui::GuiEventScoreboardResponseTableEvent))       // 2924-byte record
            + static_cast<s32>(sizeof(BrnGui::GuiEventNetworkPlayerStatus))              // 8 x 312 + header
            + static_cast<s32>(sizeof(BrnGui::GuiEventNetworkLobbyPlayerList))           // 8 x 56 + count
            + static_cast<s32>(sizeof(BrnGui::GuiLiveRevengeUpdateEvent));               // 16-byte record
        (void)li;
    }
}

extern "C" void GameBridgeNetworkToX_embed_check()
{
    ReferenceBridges();
    (void)&ExerciseEventTags;
}
