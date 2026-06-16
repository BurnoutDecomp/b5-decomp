#pragma once

#include "types.hpp"

// Minimal element-type home for the fixed-capacity
// Array<BrnGameState::CrashModeScoring::RecentCrash, 64> leaf instantiation (the IsFull/
// Append/Erase explicit instantiations of CrashModeScoring's recent-hit-cars set).
// RecentCrash is a nested record of the BrnGameState::CrashModeScoring struct; only the
// nested element type this Array needs is declared here (CrashModeScoring's own
// members/methods belong to the BrnCrashModeScoring.cpp TU -- grow this home in place,
// do not fork). Fields/offsets recovered from DecFIGS DWARF
// (BrnCrashModeScoring.h:200-204): X360 element stride 8 bytes, matching the X360
// Array layout (64 * 8 == 512 == count-word offset 0x200). Capacity 64 ==
// BrnGameState::KI_MAX_RECENTLY_HIT_CARS (BrnCrashModeScoring.h:44).
namespace BrnGameState
{
struct CrashModeScoring
{
    struct RecentCrash
    {
        u16 muTrafficCarIndex; // 0x00  BrnCrashModeScoring.h:202
        u16 muCrashChainCount; // 0x02  BrnCrashModeScoring.h:203
        f32 mfTimeOfCrash;     // 0x04  BrnCrashModeScoring.h:204
    };
};
}
