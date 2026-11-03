#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// Shared native debug-stream declarations.
namespace CgsDev
{
namespace Log
{


    extern StrStreamBase* gpDebugPrint;
}

namespace Message
{
    // Log-category filter. Call sites gate logging on `(gxMessageFilterFlags & bit)`; set broad
    // so the engine's logging is captured. (The real per-category default differs; tune later.)
    // Width is 64-bit: the X360 asm loads/stores the full 64-bit value and the DecFIGS DWARF
    // (CgsMessage.h:40) types it `uint64_t`. Matches CgsMessage.h's `extern FilterFlag (u64)`
    // declaration of the same CgsDev::Message::gxMessageFilterFlags symbol (no ODR conflict).
    extern u64 gxMessageFilterFlags;
}
}
