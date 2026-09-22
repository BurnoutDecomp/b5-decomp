#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeNetworkToX.h
//
// Includes for the BrnGame::BrnGameModule network-bridge family
// (GameSource/Unity/../Game/GameBridgeNetworkToX.cpp). Each per-frame bridge reads the
// network module's OUTPUT buffer (BrnNetwork::BrnNetworkModuleIO::OutputBuffer) and
// republishes its contents into the GUI + game-state subsystems -- the mirror image of the
// controller/replay bridges in GameBridgeControllerToX.cpp / GameBridgeReplayToX.cpp.
//
// Every type the bridges touch has its own home: the network IO buffer and its interfaces
// (BrnNetworkModuleIO.h), the game-state events (BrnGameEvents.h) and the GUI event records
// below. This header declares nothing of its own; it only gathers the GUI event homes the
// translators post, so a TU can include it beside the network module headers.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"          // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"  // CgsGui::GuiEventNetworkLaunching
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"        // the BrnGui GUI event records
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // GuiLiveRevengeUpdateEvent / GuiEventNetworkPlayerLeftLobby
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"  // GuiEventNetworkGameParams
