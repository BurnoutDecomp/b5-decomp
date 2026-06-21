#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<RecentCrash,64> maRecentCrashes

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
    // Fixed capacity of the recent-hit-cars set (BrnCrashModeScoring.h:44). 64 * 8-byte stride
    // == 0x200 == the count-word offset within the Array<> instantiation.
    static const u32 KI_MAX_RECENTLY_HIT_CARS = 64;

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

    // --- slice grown so BrnGameState::ScoringSystem can embed CrashModeScoring BY VALUE and compile ---
    // ScoringSystem holds a CrashModeScoring sub-scorer member; it drives the lifecycle and reads the
    // remaining live-score accessors through it. Declared-only (bodies + full layout live in the
    // BrnCrashModeScoring.cpp TU -- grow this home in place when that lands, do NOT fork).
    //
    // Only the methods whose signatures use already-available types (primitives + the nested RecentCrash)
    // are sliced in here so the header stays self-contained / leak-free. The remaining DWARF methods take
    // not-yet-homed event/traffic types (Update's interface params, DealWith* event ptrs,
    // DealWithScoreForVehicleClass / GetVehicleScoreData's BrnTraffic::VehicleClass &
    // VehicleScoreCategory & CgsID, DealWithShowtimeStunt's WorldStuntAction); those land with this
    // type's own TU rather than dragging their includes into the keystone's by-value embed.
    void Construct();                       // BrnCrashModeScoring.h:70
    bool Prepare();                         // BrnCrashModeScoring.h:74
    bool Release();                         // BrnCrashModeScoring.h:85
    void Destruct();                        // BrnCrashModeScoring.h:89
    void ClearData();                       // BrnCrashModeScoring.h:93
    void DealWithHitOverheadSign();         // BrnCrashModeScoring.h:125
    void DealWithDetachedWheel();           // BrnCrashModeScoring.h:141
    bool HasCrashModeEnded(f32 lfTime) const; // BrnCrashModeScoring.h:161
    s32  GetRawScore() const;               // BrnCrashModeScoring.h:165
    s32  GetOverallScore() const;           // BrnCrashModeScoring.h:168
    s32  GetNumCarsCrashed() const;         // BrnCrashModeScoring.h:177

    // --- slice grown for this type's own TU (BrnCrashModeScoring.cpp) ---
    // GetRecentCrash linearly scans the live recent-hit-cars set for the element whose
    // muTrafficCarIndex matches luTrafficCarIndex, returning a mutable pointer to it (or
    // null if absent). The X360 body (0x8232BEF8) walks maRecentCrashes via the bounds-
    // checked Array<>::operator[] (the explicit instantiation in Array_RecentCrash_64.cpp).
    // Called by DealWithHitTrafficCar / DealWithScoreForVehicleClass.
    RecentCrash* GetRecentCrash(u16 luTrafficCarIndex);   // BrnCrashModeScoring.h (X360 0x8232BEF8)

    // NOMINAL reserved storage. The committed home above names only the handful of members the debug
    // overlay reads by hand; the by-value embed in ScoringSystem needs a non-trivial footprint, so the
    // remaining ~0x320 bytes of live-scoring state (the CrashScoreDebugComponent sub-object, the
    // FixedRingBuffer/Array sets, the timers, the per-class crash counters, the air-time accumulators,
    // ...) are reserved here as opaque bytes. NOT byte-verified -- the real layout/order lands with the
    // BrnCrashModeScoring.cpp TU (DWARF members run BrnCrashModeScoring.h:215-258).
    //
    // The recent-hit-cars set IS now named (GetRecentCrash needs it): the X360 body reaches it at
    // this+0x7C and its count word at this+0x7C+0x200 == this+0x27C, so the opaque reserve is split
    // around it -- a 0x7C-byte head, then maRecentCrashes (64*8 elements + the trailing count word ==
    // 0x204 bytes), then the 0xA0-byte tail. Total footprint preserved at 0x320. Offsets/order around
    // the array remain NOMINAL until the full BrnCrashModeScoring.cpp layout lands; only maRecentCrashes
    // is anchored.
    u8 maReservedHead[0x7C];                  // NOMINAL -- members before the recent-hit set (BrnCrashModeScoring.h:215..)
    Array<RecentCrash, KI_MAX_RECENTLY_HIT_CARS> maRecentCrashes; // +0x7C  the live recent-hit-cars set
    u8 maReservedTail[0x320 - 0x7C - 0x204];  // NOMINAL -- remaining live-scoring state past the recent-hit set

    friend class CrashScoreDebugComponent;
};
}
