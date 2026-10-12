#pragma once
#include "types.hpp"

namespace CgsSystem
{
    // ARTIST 0x828D75F0 / DecFIGS CgsTimeUtils.h: elapsed milliseconds.
    u32 GetSystemTimeMS();
    // ARTIST 0x828D75A0 / 0x828D75C8 return the full 64-bit counter/frequency.
    // Existing PC wrappers preserve that width for consumers with u64 arithmetic.
    u64 GetSystemTimerBaseTime64();
    u64 GetSystemTimerFrequency64();
}
