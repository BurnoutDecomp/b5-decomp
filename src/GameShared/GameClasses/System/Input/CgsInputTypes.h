#pragma once

// Input-system result enums. Recovered from the DecFIGS DWARF (CgsInputTypes.h).
#include "types.hpp"

namespace CgsInput
{
    // Number of physical controller pads the input system supports. The pad-index asserts in the
    // input IO accessors + the controller bridges check ports against this (X360 GetPadInfo asserts
    // `iPort < 4`; GetPadInfoForPlayer0 asserts `miPlayer0ControllerPort <= CgsInput::KU_NUMBER_OF_PADS`).
    const u32 KU_NUMBER_OF_PADS = 4;

    enum EBindResult : s32
    {
        E_BINDRESULTOK                 = 0,
        E_BINDRESULTPLAYERALREADYBOUND = 1,
        E_BINDRESULTPORTALREADYBOUND   = 2,
        E_BINDRESULTINVALIDPLAYER      = 3,
        E_BINDRESULTINVALIDPORT        = 4
    };

    enum EUnbindResult : s32
    {
        E_UNBINDRESULTOK            = 0,
        E_UNBINDRESULTINVALIDPLAYER = 1,
        E_UNBINDRESULTPLAYERNOTBOUND = 2
    };
}
