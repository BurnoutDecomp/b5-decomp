#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeWorldToX.h
//
// Public declarations for the World->X bridge TU. The DWARF home for this family is
// GameSource/Game/GameBridgeWorldToX.cpp -- every assert in
// BrnGameModule::BridgeWorldToDirector @0x823E3AB0 names
// "d:\p4\b5_main\burnout\main\code\gamesource\unity\../Game/GameBridgeWorldToX.cpp",
// so the bridge lives here rather than in BrnGameModule.cpp.
//
// Only the surface this batch needs is declared; the sibling BridgeWorldTo* entry points
// (ToGui / ToSound / ToNetwork / ToGameState / ToResource / ToEffects_Dispatch) are added
// by their owning batches. The member function itself is declared on BrnGameModule
// (BrnGameModule.hpp) -- this header exists so the TU has a home of its own and so future
// siblings have somewhere to hang shared declarations.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<1536,16>

namespace BrnWorldIO { struct UpdateOutputBuffer; }

namespace BrnGame
{
    // [FX-BRIDGES CC-11, 2026-09-24] LEG 10 of BrnGameModule::BridgeWorldToGameState @0x823E5368
    // (0x823E5494..0x823E554C): every route response the MODE MANAGER asked for becomes game event
    // 174 (GameStateModuleIO::ModeManagerRouteInfoEvent { the response's event id, the route's
    // length }) on the game-state post-world game-event queue. De-inlined because this build has
    // TWO callers of it: BridgeWorldToGameState itself (the console home, with no call site until
    // DoUpdate_GameStatePostWorld lands) and the one-feed post-world seam in BrnGameModule.cpp, which
    // builds the queue the console's PostWorldInputBuffer would carry (legs 2 + 10, in that order).
    // Body: GameBridgeWorldToX.cpp.
    void BridgeWorldToGameState_RouteInfo(CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
                                          const BrnWorldIO::UpdateOutputBuffer*     lpWorldOutput);
}
