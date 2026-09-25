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
    // KAC_SPLASH_SCREEN_IDS -- game mode -> splash-screen overlay id (null == the mode
    // has no splash). Values read from the image: the table has EIGHTEEN slots (one per
    // console game mode); the offline modes 0..9 are null, the online modes carry the
    // splash overlay ids, and the next rodata table starts right after slot 17.
    // -------------------------------------------------------------------------------
    const char* const KAC_SPLASH_SCREEN_IDS[18] =
    {
        nullptr, nullptr, nullptr, nullptr, nullptr,            // [0]..[4]  offline modes
        nullptr, nullptr, nullptr, nullptr, nullptr,            // [5]..[9]  offline modes
        "OnSplshRace",                                          // [10]
        "OnSplshRoadR",                                         // [11]
        "OnSplshFug",                                           // [12]
        "OnSplshBHR",                                           // [13]
        "OnSplshFug",                                           // [14]
        "OnSplshFreeB",                                         // [15]
        "OnSplshFreeB",                                         // [16]
        "OnSplshFug",                                           // [17]
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
