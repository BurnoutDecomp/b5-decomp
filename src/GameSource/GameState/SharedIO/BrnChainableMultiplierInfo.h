#pragma once

#include "types.hpp"

// Home of BrnGameState::GameStateModuleIO::ChainableMultiplierInfo -- the element of the online stunt
// scorer's Array<ChainableMultiplierInfo,8> (StuntModeScoringOnline +0x23F0) and of the
// Array_ChainableMultiplierInfo_8.cpp explicit instantiation. PROVISIONAL minimal record: sized to the
// X360 element stride (16); real field names land with the type's own TU. X360-only (the DecFIGS DWARF
// has no such type).
//
// [FX-GS 2026-09-23] Moved verbatim out of BrnGameStateLeafContainers.h (which still includes it), so
// BrnStuntModeScoringOnline.h -- now embedded by value in ScoringSystem -- can name its element type
// without dragging that header's other provisional leaf stubs (BufferedNewHighScore,
// OnlineFlybyManager) into every ScoringSystem includer, where they collide with the real
// BrnGameStateStreetManager.h / BrnGameStateOnlineFlybyManager.h definitions.
namespace BrnGameState
{
namespace GameStateModuleIO
{
struct ChainableMultiplierInfo { s32 maField[4]; };  // X360 stride 16 (provisional)
}
}
