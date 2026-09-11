#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeNetworkToX.h
//
// Support types for the BrnGame::BrnGameModule network-bridge family
// (GameSource/Unity/../Game/GameBridgeNetworkToX.cpp). Each per-frame bridge reads the
// network module's OUTPUT buffer (BrnNetwork::BrnNetworkModuleIO::OutputBuffer, the
// committed home) and republishes its contents into the GUI + game-state subsystems --
// the mirror image of the controller/replay bridges in GameBridgeControllerToX.cpp /
// GameBridgeReplayToX.cpp.
//
// This header carries ONLY the bridge-local scaffolding the four reconstructed functions
// need that is not homed elsewhere:
//
//   * The GUI event records the ToGui translators synthesise are the CANONICAL types now.
//     They used to be re-defined here as alignas(16) opaque byte spans, which forked against
//     their real homes AND rounded every record UP to a multiple of 16 -- and the publisher
//     bakes sizeof(T) as the queued record size, so every one of those posts was the wrong
//     length. The homes are included below; nothing GUI-side is re-declared here.
//
//   * A small set of by-name network accessors (the un-homed network helpers) the bridge
//     reaches on the network interfaces; declared here as free functions in namespace
//     BrnNetwork so the bridge references them by name. FLAGGED: these correspond to
//     committed OutputBuffer/interface accessors (see .cpp) reached by their byte offset.
//
//   * The three file-scope debug switches that inject the synthetic "Mole4Avril" test roster.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"          // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"  // CgsGui::GuiEventNetworkLaunching
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"        // the BrnGui GUI event records
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // GuiLiveRevengeUpdateEvent / GuiEventNetworkPlayerLeftLobby
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"  // GuiEventNetworkGameParams

// -------------------------------------------------------------------------
// File-scope debug switches (X360 byte_82FB5090 / _5091 / _5092). Inject the synthetic
// "Mole4Avril" roster into the player-list / player-status snapshots. FLAG: file-scope
// here (their real home is the network debug TU); default-zero (retail path unchanged).
// -------------------------------------------------------------------------
namespace BrnGame
{
    extern bool byte_82FB5090;
    extern bool byte_82FB5091;
    extern u8   byte_82FB5092;
}

// -------------------------------------------------------------------------
// Game-state event tags the ToGameState translator (0x823DF9E0) synthesises. Each is the
// AddEvent record built store-for-store then queued into the PreWorld game-event queue.
// The X360 calls a de-inlined <Event>::SetNetworkPlayerID(record, id) single-word setter
// whose exact within-record offset is owned by the real event type (not attested here);
// modelled as a static setter on the opaque tag (HARD RULE 3 -- no field names invented).
// FLAG: real homes are BrnGameStateModuleIO.h event-type-defs; promote when reconstructed.
// -------------------------------------------------------------------------
namespace BrnGameState
{
    struct OnlinePlayerAddedEvent
    { u8 maOpaque[40]; static void SetNetworkPlayerID(void* lpRecord, s32 liId); };
    struct OnlinePlayerRemovedEvent
    { u8 maOpaque[8];  static void SetNetworkPlayerID(void* lpRecord, s32 liId); };
    struct OnlinePlayerFinalisedEvent
    { u8 maOpaque[4];  static void SetNetworkPlayerID(void* lpRecord, s32 liId); };
    struct ChangeNetworkCarEvent
    { u8 maOpaque[32]; static void SetNetworkPlayerID(void* lpRecord, s32 liId); };
    struct RemotePlayerDisconnectedEvent
    { u8 maOpaque[4];  static void SetNetworkPlayerID(void* lpRecord, s32 liId); };
}

// -------------------------------------------------------------------------
// By-name network accessors the bridge reaches on the network interfaces. FLAG: these are
// the committed OutputBuffer / interface accessors (BrnNetworkModuleIO.h) that the X360
// Hex-Rays surfaced as free-function shims (Net / In / BrnNetwo / BrnNetworkMo /
// GetOnlineLobbyPlayerStatusInterface). Declared here so the bridge references them by
// name; the .cpp notes the committed accessor + byte offset each corresponds to. Bodies
// land with the OutputBuffer / NetworkToGuiInterface / status-interface TUs.
// -------------------------------------------------------------------------
namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        // NetworkToGuiInterface::GetLiveRevengeRecord(index) (Hex-Rays "Net").
        const s32* GetLiveRevengeRecord(const void* lpNetworkToGuiInterface, int liIndex);
        // InGamePlayerStatusInterface::GetPlayerStatusData(index) (Hex-Rays "In").
        const void* In(const void* lpStatusInterface, int liIndex);
    }
    // record-heading-type accessor (Hex-Rays "BrnNetwo") + online-lobby interface base
    // (Hex-Rays "BrnNetworkMo").
    unsigned int GetScoreboardResponseHeadingType(const void* lpRecord);
    const void*  GetOnlineLobbyPlayerStatusInterface(const void* lpOutputBuffer);
}
