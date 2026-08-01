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
