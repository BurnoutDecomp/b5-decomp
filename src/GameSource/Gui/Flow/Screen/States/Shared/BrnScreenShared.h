#pragma once

#include "types.hpp"
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // BrnGameState::GameStateModuleIO::EGameModeType

// ============================================================================
// GameSource/Gui/Flow/Screen/States/Shared/BrnScreenShared.h
//
// Free-function home for the screen-flow shared helpers. Currently carries the
// game-mode -> splash-screen id lookup (GetSplashScreenIDForGameMode @0x824845E8).
//
// DWARF (DecFIGS BrnScreenShared.h) is AUTHORITATIVE on shape and REFUTES the
// earlier proposed int/int + s32-table reconstruction: the function returns a
// const char* (the X360 lwzx loads/returns a 32-bit pointer) and takes an
// EGameModeType, and the table is const char*[17].
// ============================================================================

namespace BrnGui
{
    // DWARF BrnScreenShared.h:28 -- game-mode -> splash-screen id STRING (null == none).
    // X360 dword_82F26638 (a 32-bit-pointer table of splash-screen id strings). The 17
    // string CONTENTS are runtime rodata pointers NOT recovered by the
    // GetSplashScreenIDForGameMode TU (values TBD -- see the .cpp TODO); the SHAPE
    // (element-type/count) IS DWARF-attested.
    extern const char* const KAC_SPLASH_SCREEN_IDS[17];

    // @ 0x824845E8 (DWARF h:77) -- look up the splash-screen id string for a game mode
    // (asserts a non-null entry, then returns it).
    const char* GetSplashScreenIDForGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameMode);
}
