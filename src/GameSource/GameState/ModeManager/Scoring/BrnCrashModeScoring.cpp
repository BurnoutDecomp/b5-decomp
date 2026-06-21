// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp
// ============================================================================
// Reconstructed BODIES (store-for-store from the X360 dossiers) for the crash-mode
// scorer BrnGameState::CrashModeScoring (home header BrnCrashModeScoringRecentCrash.h).
// This is a PARTIAL TU for the type -- the remaining lifecycle / DealWith* / Update
// methods + the full member layout land later when the rest of the type is recovered;
// grow the home in place, do NOT fork.
//
// Methods bodied here (X360 ARTIST.XEX addresses):
//   GetRecentCrash    0x8232BEF8
//
// Each body is a clean (de-optimized) translation of its X360 pseudocode/asm. Members
// are accessed BY NAME against the committed layout in the home; semantic parity, NOT
// byte-matching.
//
// NOTE on the OTHER ledger function in this TU (X360 0x8231ADB0): that address is the
// non-const Array<CrashModeScoring::RecentCrash,64>::operator[](u32) -- the bounds-
// checked indexed accessor of the recent-hit-cars set (the truncated ledger name
// "BrnGameState::Cras"). It is NOT a CrashModeScoring method; its body is the generic
// inline Array<T,N>::operator[] in CgsArray.h and it is already emitted via the explicit
// instantiation in Array_RecentCrash_64.cpp (the same instantiation the catch-all noted
// at 0x8231AEB8). GetRecentCrash below drives it directly. No separate body is needed
// here.
// ============================================================================

#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"
// (no CGS_ASSERT here -- GetRecentCrash itself asserts only INSIDE the Array<>::operator[]
//  it calls; the recovered body issues no asserts of its own.)

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // GetRecentCrash  (X360 0x8232BEF8)
    // Linear search of the live recent-hit-cars set (maRecentCrashes, reached in the X360
    // at this+0x7C; its count word at +0x200 within the Array). Walk indices 0..count-1,
    // comparing each element's muTrafficCarIndex (u16 @ element+0) against the requested
    // index; return a mutable pointer to the first match, or null if none (the X360 returns
    // 0 both when the set is empty/count<=0 and when the scan falls off the end).
    //
    // The X360 body indexes through Array<>::operator[] (the bounds-checked accessor); the
    // 16-bit compare (`clrlwi r29,r29,16` then `lhz` of the element) is the u16 vs u16 match.
    // ------------------------------------------------------------------------
    CrashModeScoring::RecentCrash* CrashModeScoring::GetRecentCrash(u16 luTrafficCarIndex)
    {
        const s32 liCount = maRecentCrashes.GetCount();   // *(&set + 0x200)
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            RecentCrash& lrCrash = maRecentCrashes[static_cast<u32>(liIndex)];
            if (lrCrash.muTrafficCarIndex == luTrafficCarIndex)
            {
                return &lrCrash;
            }
        }
        return nullptr;
    }
}
