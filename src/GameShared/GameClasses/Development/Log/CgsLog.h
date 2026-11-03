#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// Shared native debug-stream declarations.
namespace CgsDev
{
namespace Log
{


    extern StrStreamBase* gpDebugPrint;
    extern StrStreamBase* gpAssertPrint;
}

namespace Message
{
    // ARTIST initial filter is the 64-bit value 1.
    extern u64 gxMessageFilterFlags;
}
}
