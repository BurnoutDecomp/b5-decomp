// ============================================================================
// Embedder for the InviteManager slice compile gate. Forces the manager's full
// layout to instantiate and exercises a couple of X360-exact invariants.
//
// NOTE on sizes: the per-event-queue strides differ between MSVC (host) and the X360
// target (e.g. EventQueue<BaseInputEvent,8> is 76B on X360, 80B under MSVC due to base
// padding), so the absolute struct size is host-specific and not asserted. The two
// invariants that ARE X360-exact and that the bodies depend on are checked: the game
// event queue footprint (0x610) and the invite-params block (0x9C, port @ +0x14,
// change-flag @ +0x98).
// ============================================================================
#include "GameSource/GameState/InviteManager/BrnGameStateInviteManager.h"
#include <cstddef>

using BrnGameState::GameStateInviteManager;

static_assert(sizeof(CgsModule::VariableEventQueue<1536,16>) == 0x610, "GameEventQueue == 0x610");
static_assert(sizeof(BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams) == 0x9C, "InviteOrJoinParams == 0x9C");
static_assert(offsetof(BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams, miUserControllerPort) == 0x14,
              "InviteOrJoinParams::miUserControllerPort @ +0x14");
static_assert(offsetof(BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams, mbIsLocalUserChangeNeeded) == 0x98,
              "InviteOrJoinParams::mbIsLocalUserChangeNeeded @ +0x98");

char gEmbedStorage[sizeof(GameStateInviteManager)];
GameStateInviteManager* gpInviteManager =
    reinterpret_cast<GameStateInviteManager*>(gEmbedStorage);
