#pragma once

#include "types.hpp"

// Canonical RenderWare SDK home for the rwcore utility inlines (EARenderWare rwcore,
// rw/core/stdc/util.h). DWARF (references/DecFIGS/dwarfdump/SDKs/EATech/include/rw/core/stdc/
// util.h) attests `int Min(int, int)` (util.h:55), `unsigned int Min(unsigned int, unsigned int)`
// (util.h:56) and `int Max(int, int)` (util.h:66) as header inlines that game code calls
// (OnlineGameRoomPlayerInfo::HandleGameParamsChangedEvent inlines Min then Max, ARTIST
// 0x824A53EC..0x824A5418). The bodies are the platform header's plain conditional form
// (LegacySrc EARenderWare/stable/platform/include/native/ps3-ppu-gcc/rw/core/stdc/util.h).
//
// MINIMAL SLICE: the int and unsigned int overloads only. GROW this header in place for the
// other widths (int16_t/uint16_t/int64_t/uint64_t in the same platform header) when a caller
// needs them -- do NOT fork it.

namespace rw
{
namespace core
{
namespace stdc
{
    inline int Min(int a, int b)
    {
        return (((a) < (b)) ? (a) : (b));
    }
    inline int Max(int a, int b)
    {
        return (((a) > (b)) ? (a) : (b));
    }
    inline unsigned int Min(unsigned int a, unsigned int b)
    {
        return static_cast<unsigned int>(((a) < (b)) ? (a) : (b));
    }
    inline unsigned int Max(unsigned int a, unsigned int b)
    {
        return static_cast<unsigned int>(((a) > (b)) ? (a) : (b));
    }
}
}
}
