#include "GameSource/Director/SharedIO/BrnDirectorOutputInterface.h"

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
    CGS_ASSERT(lpuRivalryNumberOut != 0, "lpuRivalryNumberOut != NULL");   // :101
    CGS_ASSERT(lpfSecsRemaining != 0, "lpfSecsRemaining != NULL");         // :102

    *lpuRivalryNumberOut = muIntroCameraRivalryNumber;
    *lpfSecsRemaining    = mfIntroCameraSecsRemaining;
    return mbIntroCameraStartingToLookAtRival;
}

}
