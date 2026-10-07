#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnDirector::DirectorOutputInterface - the director module's per-frame output
// interface (what the game/GUI read back from the camera director). DWARF home
// GameSource/Director/SharedIO/BrnDirectorOutputInterface.h. The rival record
// is pinned by IsIntroCameraStartingToLookAtRival @0x823A7AB8. The four trailing
// bools follow the DecFIGS member order; OutputBuffer::Construct @0x8225C960
// confirms the reset stores at +0x00/+0x0D/+0x0E/+0x0C and the 16-byte stride.
namespace BrnDirector
{
    class DirectorOutputInterface
    {
    public:
        // Header-inline on ARTIST, expanded at 0x8225C9B8..C8. The rival payload
        // and mbPauseRequestState are intentionally retained on buffer reuse.
        void Construct()
        {
            mbIntroCameraStartingToLookAtRival = false;
            mbFinishedPostRacePresentation = false;
            mbPauseRequest = false;
            mbStartRivalPresentation = false;
        }

        // @0x823A7AB8 (class TU; body in BrnDirectorOutputInterface.cpp, DWARF
        // h:101/:102) -- report whether the intro camera has started its
        // look-at-rival move, with the rivalry number and the seconds remaining.
        bool IsIntroCameraStartingToLookAtRival(u32* lpuRivalryNumberOut,
                                                f32* lpfSecsRemaining) const;

    private:
        bool mbIntroCameraStartingToLookAtRival;   // +0x00
        u32  muIntroCameraRivalryNumber;           // +0x04
        f32  mfIntroCameraSecsRemaining;           // +0x08
        bool mbStartRivalPresentation;             // +0x0C
        bool mbFinishedPostRacePresentation;       // +0x0D
        bool mbPauseRequest;                       // +0x0E
        bool mbPauseRequestState;                  // +0x0F
    };

    static_assert(sizeof(DirectorOutputInterface) == 0x10,
                  "DirectorOutputInterface has a pointer-free 16-byte payload");
}
