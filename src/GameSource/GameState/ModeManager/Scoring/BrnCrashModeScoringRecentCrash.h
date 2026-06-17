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
// The crash-score debug overlay is embedded as CrashModeScoring's first member and reads its private
// scoring state directly (BrnCrashModeScoring.h:215); declared here for the friend grant below.
class CrashScoreDebugComponent;

struct CrashModeScoring
{
    struct RecentCrash
    {
        u16 muTrafficCarIndex; // 0x00  BrnCrashModeScoring.h:202
        u16 muCrashChainCount; // 0x02  BrnCrashModeScoring.h:203
        f32 mfTimeOfCrash;     // 0x04  BrnCrashModeScoring.h:204
    };

    // --- slice grown for BrnCrashScoreDebugComponent.cpp (the "Showtime score" debug HUD) ---
    // Live crash-scoring accessors the debug readout reads (DWARF BrnCrashModeScoring.h:171-186).
    // Declared-only: the bodies (and CrashModeScoring's full member layout / remaining methods) belong
    // to the BrnCrashModeScoring.cpp TU -- grow this home in place when that lands, do NOT fork.
    s32 GetScoreMultiplier() const;     // BrnCrashModeScoring.h:171
    s32 GetCurrentComboCount() const;   // BrnCrashModeScoring.h:174
    s32 GetNumCarsLeapt() const;        // BrnCrashModeScoring.h:180
    f32 GetBestAirTime() const;         // BrnCrashModeScoring.h:183 (reads mfLongestJumpAirTime, +0x31C)
    f32 GetDistanceTravelled() const;   // BrnCrashModeScoring.h:186 (+0x308)

    // Members the debug overlay touches that have no accessor in the DWARF: OnActivate registers the
    // infinite-crash flag, and DisplayScores reads mfHighestJump directly (the X360 reads +0x320 inline;
    // CrashModeScoring's getter set is closed -- there is no GetHighestJump). The full member layout is
    // the BrnCrashModeScoring.cpp TU's responsibility; only the fields this debug TU names are sliced in.
    bool mbInfiniteCrashMode;           // BrnCrashModeScoring.h:227
    f32  mfHighestJump;                 // BrnCrashModeScoring.h:256 (+0x320)

    friend class CrashScoreDebugComponent;
};
}
