// ===================================================================================
// BrnGui screen-flow shared helpers -- implementation
//   class:BrnGui (free functions)
//
//   GetSplashScreenIDForGameMode @ 0x824845E8
// Reconstructed store-for-store from the X360 asm; DWARF-attested shape.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/Shared/BrnScreenShared.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // -------------------------------------------------------------------------------
    // KAC_SPLASH_SCREEN_IDS  (X360 dword_82F26638)  -- DEFINITION
    //   game-mode -> splash-screen resource-id STRING (null == "no splash for this mode").
    //   SHAPE (const char*[17]) is DWARF-attested (BrnScreenShared.h:28); the 17 string
    //   CONTENTS are runtime rodata pointers NOT recovered by this TU.
    //
    //   *** UNRECOVERED VALUES -- CONSOLIDATOR MUST FILL ***  Read the 17 little-endian
    //   32-bit pointers at X360 0x82F26638 and replace the placeholder nullptrs. A null
    //   entry means the mode has no splash screen (the X360 fires a streamed assert and
    //   returns the (null) entry anyway). Index space is EGameModeType.
    // -------------------------------------------------------------------------------
    const char* const KAC_SPLASH_SCREEN_IDS[17] =
    {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,   // [0]..[5]  TODO(consolidator)
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,   // [6]..[11] TODO(consolidator)
        nullptr, nullptr, nullptr, nullptr, nullptr,            // [12]..[16] TODO(consolidator)
    };

    // @ 0x824845E8 -- map a game-mode value to its splash-screen resource-id STRING via the
    // static KAC_SPLASH_SCREEN_IDS table. A null entry means the game mode has no splash
    // screen registered; the X360 fires a (streamed) assert and then returns the (null)
    // entry anyway. The streamed 'Can't find splash screen Id from game mode (<mode>)'
    // assert collapses to a single CGS_ASSERT (the runtime index + the ')\n' tail fragment
    // are dropped per project convention).
    const char* GetSplashScreenIDForGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameMode)
    {
        CGS_ASSERT(KAC_SPLASH_SCREEN_IDS[leGameMode] != nullptr,
                   "Can't find splash screen Id from game mode (");
        return KAC_SPLASH_SCREEN_IDS[leGameMode];
    }
}
