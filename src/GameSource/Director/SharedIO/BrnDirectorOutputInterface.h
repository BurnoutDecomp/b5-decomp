#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnDirector::DirectorOutputInterface - the director module's per-frame output
// interface (what the game/GUI read back from the camera director). DWARF home
// GameSource/Director/SharedIO/BrnDirectorOutputInterface.h. FLAG: MINIMAL slice --
// only the intro-camera-rival record IsIntroCameraStartingToLookAtRival
// @0x823A7AB8 reads is modelled (bool @+0, rivalry number @+4, secs remaining
// @+8; field names inferred from the accessor's DWARF param names); the full
// interface lands with its own TU.
namespace BrnDirector
{
    class DirectorOutputInterface
    {
    public:
        // @0x823A7AB8 (class TU; body in BrnDirectorOutputInterface.cpp, DWARF
        // h:101/:102) -- report whether the intro camera has started its
        // look-at-rival move, with the rivalry number and the seconds remaining.
        bool IsIntroCameraStartingToLookAtRival(u32* lpuRivalryNumberOut,
                                                f32* lpfSecsRemaining) const;

    private:
        bool mbIntroCameraStartingToLookAtRival;   // +0x00 (FLAG: names inferred)
        u32  muIntroCameraRivalryNumber;           // +0x04
        f32  mfIntroCameraSecsRemaining;           // +0x08
    };
}
