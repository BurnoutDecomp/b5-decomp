#include "GameSource/Director/SharedIO/BrnDirectorOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Director/SharedIO/BrnDirectorOutputInterface.h"
// BrnDirector::DirectorOutputInterface -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, class:BrnDirector::DirectorOutputInterface):
//   DirectorOutputInterface::IsIntroCameraStartingToLookAtRival @0x823A7AB8

namespace BrnDirector
{

// @ 0x823A7AB8 -- h:101/:102 (both tripwires non-gating; the stores land anyway).
bool DirectorOutputInterface::IsIntroCameraStartingToLookAtRival(
    u32* lpuRivalryNumberOut, f32* lpfSecsRemaining) const
{
    CGS_ASSERT(lpuRivalryNumberOut != 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpuRivalryNumberOut != NULL", CGS_ARTIST_FILE, 101 /* ARTIST 0x823A7AE8 */));   // :101
    CGS_ASSERT(lpfSecsRemaining != 0,
        CGS_ASSERT_MESSAGE_AT("lpfSecsRemaining != NULL", CGS_ARTIST_FILE, 102 /* ARTIST 0x823A7B0C */));         // :102

    *lpuRivalryNumberOut = muIntroCameraRivalryNumber;
    *lpfSecsRemaining    = mfIntroCameraSecsRemaining;
    return mbIntroCameraStartingToLookAtRival;
}

}

#undef CGS_ARTIST_FILE
