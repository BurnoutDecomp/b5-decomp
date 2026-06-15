#pragma once

// Input-system result enums. Recovered from the DecFIGS DWARF (CgsInputTypes.h).
#include "types.hpp"

namespace CgsInput
{
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
