#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"

// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 02.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//     OnCrash        @ 0x822A68D8
//     OnDriveThru    @ 0x822A6A48
//     OnEndCrashPlay @ 0x822A6AF8
//
// Shared facts walked out of the asm for all three bodies:
//   * The vtable slot the three of them call is +0xC8 = ordinal 50, i.e. the FIRST
//     slot PAST the 50-slot BoostStrategy vtable (0x00..0xC4). That is B3's own
//     `virtual void UpdateMaxBoost(bool)` (@0x822C1C80, DWARF BrnBoostBurnout3.h:118),
//     which HIDES the base's non-virtual BoostStrategy::UpdateMaxBoost. An unqualified
//     `UpdateMaxBoost(...)` inside BoostBurnout3 therefore reproduces the observed
//     virtual dispatch; do NOT qualify it with BoostStrategy::.
//   * B3's own members sit past the 0x130-byte base: +0x130 mfBoostChunkAmount,
//     +0x134 miBoostLevel, +0x138 miOldBoostLevel, +0x13C mfTimeBoosting.
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnCrash @ 0x822A68D8   (vtable slot 7)
//
//   0x822A68EC  li    r4, 0                 -> UpdateMaxBoost(false)
//   0x822A68F4  lwz   r11, 0xC8(vtbl)          (virtual, B3 slot 50)
//   0x822A6910  stb   r11(=0), 0xC5(this)   -> mbBoosting = false  (AFTER the call:
//                                              the compiler could not hoist a member
//                                              store across the virtual dispatch, so
//                                              the source order is call-then-store)
//   0x822A690C  fcmpu f13(=+0xA0), f0(=+0xA4)
//   0x822A6914  ble   -> skip
//   0x822A6918  stfs  f0, 0xA0(this)        -> mfBoostAmount = mfMaxBoost
//
// NaN polarity: fcmpu+ble is TAKEN when unordered, i.e. the store is SKIPPED for a
// NaN mfBoostAmount -- which is exactly what plain `>` does in C++. No inversion
// needed here.
//
// DIVERGENCE from Feb-2007: the leaked body decrements miBoostLevel before the
// recompute and has no mbBoosting store. Retail does NEITHER decrement NOR anything
// to miBoostLevel here, and DOES clear mbBoosting. The asm wins.
// ---------------------------------------------------------------------------
void BoostBurnout3::OnCrash()
{
    UpdateMaxBoost( false );

    mbBoosting = false;

    if( mfBoostAmount > mfMaxBoost )
    {
        mfBoostAmount = mfMaxBoost;
    }
}

// ---------------------------------------------------------------------------
// OnDriveThru @ 0x822A6A48   (vtable slot 46 -- pure in the base)
//
//   0x822A6A48  lwz   r11, 0x134(r3)        -> miBoostLevel
//   0x822A6A4C  cmpwi r11, 3                   3 == KI_BOOST_LEVELS (DWARF
//   0x822A6A50  bge   -> skip                  BrnBoostBurnout3.h:134 == 3; note
//   0x822A6A54  addi  r11, r11, 1              Feb-2007 had 5 and compared
//   0x822A6A58  stw   r11, 0x134(r3)           `< KI_BOOST_LEVELS - 1`)
//   0x822A6A5C  lwz   r11, 0xC8(vtbl)       -> tail call UpdateMaxBoost(true)
//   0x822A6A60  li    r4, 1                    true == "fill mfBoostAmount to the
//   0x822A6A6C  bctr                            new mfMaxBoost" (see 0x822C1D34)
//
// The comparison is signed (cmpwi), so a negative miBoostLevel still increments.
// No Feb-2007 counterpart exists for this function -- it is retail-only.
// ---------------------------------------------------------------------------
void BoostBurnout3::OnDriveThru()
{
    if( miBoostLevel < KI_BOOST_LEVELS )
    {
        ++miBoostLevel;
    }

    UpdateMaxBoost( true );
}

// ---------------------------------------------------------------------------
// OnEndCrashPlay @ 0x822A6AF8   (vtable slot 14)
//
//   0x822A6B0C  lwz   r11, 0x138(this)      -> miOldBoostLevel
//   0x822A6B10  cmpwi r11, 1
//   0x822A6B14  bge   -> skip                  (signed)
//   0x822A6B18  li    r11, 1
//   0x822A6B1C  stw   r11, 0x138(this)      -> miOldBoostLevel = 1
//   0x822A6B20  lwz   r11, 0x138(this)         (re-load; plain assignment)
//   0x822A6B30  stw   r11, 0x134(this)      -> miBoostLevel = miOldBoostLevel
//   0x822A6B24  li    r4, 0                 -> UpdateMaxBoost(false)
//   0x822A6B34  lwz   r11, 0xC8(r10)           (B3 slot 50, NOT a tail call)
//   0x822A6B44  lfs   f13, 0xA4(this)       -> mfMaxBoost, read AFTER the recompute
//   0x822A6B48  lfs   f0, flt_820147E8         (= 0.15f; the same immediate is used by
//   0x822A6B4C  fmuls f0, f13, f0                BoostStrategy::SetCarStatBoostLevel
//   0x822A6B50  stfs  f0, 0x100(this)           @0x822D5360 and by BoostBurnout2::
//                                               OnEndCrashPlay for the same store)
//
// The pairing with OnStartCrashPlay @0x822A6AA0 confirms miOldBoostLevel's role:
// that function saves miBoostLevel into +0x138, forces miBoostLevel = 3, calls
// UpdateMaxBoost(false) and parks mfMinBoostAllowedAmount at FLT_EPSILON
// (flt_82014460). This one restores the saved level (floored at 1) and re-raises the
// floor to 15% of the recomputed maximum.
//
// The 0.15f literal is left as a literal on purpose: the DWARF for this .cpp declares
// both KF_BOOST_LOSS_SCALE (cpp:37) and KF_MIN_BOOST_LOSS_SCALE (cpp:38) and nothing
// in the image says which one owns this value, so naming it here would be a guess.
//
// DIVERGENCE from Feb-2007: the leaked body is `miBoostLevel = 0; UpdateMaxBoost();`.
// Retail restores miOldBoostLevel instead of zeroing, floors it at 1, and adds the
// mfMinBoostAllowedAmount store. The asm wins.
// ---------------------------------------------------------------------------
void BoostBurnout3::OnEndCrashPlay()
{
    if( miOldBoostLevel < 1 )
    {
        miOldBoostLevel = 1;
    }

    miBoostLevel = miOldBoostLevel;

    UpdateMaxBoost( false );

    mfMinBoostAllowedAmount = mfMaxBoost * 0.15f;   // flt_820147E8
}

} // namespace BrnWorld
