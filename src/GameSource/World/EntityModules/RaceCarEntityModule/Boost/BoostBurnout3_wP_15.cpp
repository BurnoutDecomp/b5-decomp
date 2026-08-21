// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 15.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::OnTakedown              @ 0x822A6638   (vtable slot 3)
//   BoostBurnout3::OnTakenDownByAIOrPlayer @ 0x822A6930   (vtable slot 4)
//   BoostBurnout3::OnWrecked               @ 0x822C2438   (vtable slot 8)
//
// Slot numbers are the base's (BrnBoostStrategy.h slots 3 / 4 / 8); all three
// are pure in the base, so BoostBurnout3 must define them.
//
// Member layout / vtable slot order come from the wave-P keystone header
// BrnBoostStrategy.h and from BrnBoostBurnout3.h. Offsets touched here:
//   +0x028 BoostStrategy::mfTakedownEarning        (attrib rec+0x08)
//   +0x094 BoostStrategy::mfOnWrecked              (attrib rec+0x30)
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x0A4 BoostStrategy::mfMaxBoost
//   +0x0CA BoostStrategy::mbJustLostBoostChunk
//   +0x100 BoostStrategy::mfMinBoostAllowedAmount
//   +0x134 BoostBurnout3::miBoostLevel   -- B3's OWN level member; it HIDES the
//          base's miBoostLevel at +0x104, and every B3 body uses +0x134.
//
// Two rodata literals are used below; both are pinned, not guessed:
//   flt_82001CC0 == 0.0f  -- BoostBurnout3::UpdateMaxBoost @0x822C1CC4 loads it
//          into f31 and compares mfMaxBoost against it at 0x822C1CD0; Hex-Rays
//          renders that compare as `== 0.0`.
//   flt_82001C98 == 1.0f  -- the same function loads it @0x822C1D04 and stores
//          it to mfMaxBoost (+0xA4) @0x822C1D08 as the "max boost came out
//          zero" fallback; Hex-Rays renders that store as `= 1.0`.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnTakedown @ 0x822A6638 -- vtable slot 3.
//
//   0x822A664C  lwz   r11, 0x134(r31)     ; miBoostLevel
//   0x822A6650  cmpwi cr6, r11, 3         ; KI_BOOST_LEVELS
//   0x822A6654  bge   cr6, loc_822A6660
//   0x822A6658  addi  r11, r11, 1
//   0x822A665C  stw   r11, 0x134(r31)
//   0x822A6660  lwz   r11, 0(r31)
//   0x822A6664  li    r4, 0
//   0x822A666C  lwz   r11, 0xC8(r11)      ; slot 50 -> B3::UpdateMaxBoost(false)
//   0x822A6674  bctrl
//   0x822A6678  lwz   r11, 0(r31)
//   0x822A667C  lfs   f1, 0x28(r31)       ; mfTakedownEarning -> f1 (PPC float arg)
//   0x822A6684  lwz   r11, 0xC4(r11)      ; slot 49 -> BoostStrategy::AddBoost
//   0x822A668C  bctrl
//
// `cmpwi` is the SIGNED compare, matching the signed s32 miBoostLevel (the same
// member is sign-extended with `extsw` in UpdateMaxBoost @0x822C1CA4).
//
// CALL ORDER IS LOAD-BEARING: UpdateMaxBoost first, AddBoost second. The sibling
// OnStuntCompletion @0x822A6830 (partfile 04) raises the same level but calls
// them the other way round; do not normalise one against the other.
//
// DIVERGENCES FROM FEB-2007 (asm wins):
//   * Feb-2007 (BrnBoostBurnout3.cpp:117) guards with
//     `if(miBoostLevel<KI_BOOST_LEVELS-1)` and defines KI_BOOST_LEVELS as 5
//     (Feb-2007 BrnBoostBurnout3.h:109). Retail compares against the bare
//     constant and DecFIGS pins KI_BOOST_LEVELS == 3 (DWARF
//     BrnBoostBurnout3.h:134), which is exactly the `cmpwi cr6, r11, 3` above.
//   * Feb-2007's tail (cpp:122-123) is `UpdateMaxBoost(); mfBoostAmount =
//     mfMaxBoost;` -- an instant full bar. Retail neither writes mfBoostAmount (+0xA0) nor
//     mfMaxBoost (+0xA4) here; it earns the tuned takedown reward through the
//     virtual AddBoost, which applies the speed-scaling and earning gates.
// ---------------------------------------------------------------------------
void
BoostBurnout3::OnTakedown()
{
    if( miBoostLevel < KI_BOOST_LEVELS )
    {
        ++miBoostLevel;
    }

    // Unqualified: name lookup finds BoostBurnout3's OWN virtual
    // UpdateMaxBoost(bool) (its vtable slot 50, +0xC8), which HIDES the base's
    // non-virtual one. Qualifying this would call the wrong function.
    UpdateMaxBoost( false );

    AddBoost( mfTakedownEarning );
}

// ---------------------------------------------------------------------------
// OnTakenDownByAIOrPlayer @ 0x822A6930 -- vtable slot 4.
//
//   0x822A6930  lwz   r11, 0x134(r3)      ; miBoostLevel
//   0x822A6934  cmpwi cr6, r11, 1
//   0x822A6938  ble   cr6, loc_822A694C
//   0x822A693C  addi  r11, r11, -1
//   0x822A6940  li    r10, 1
//   0x822A6944  stw   r11, 0x134(r3)
//   0x822A6948  stb   r10, 0xCA(r3)       ; mbJustLostBoostChunk = true
//   0x822A694C  lwz   r11, 0(r3)
//   0x822A6950  li    r4, 0
//   0x822A6954  lwz   r11, 0xC8(r11)      ; slot 50 -> B3::UpdateMaxBoost(false)
//   0x822A695C  bctr                      ; TAIL call
//
// No frame is set up at all -- the whole body is eleven instructions ending in a
// tail dispatch, so `this` stays in r3 throughout.
//
// This is the floor half of the level range: OnTakedown raises while below
// KI_BOOST_LEVELS (3), this lowers while above 1, so retail's live range is
// [1, KI_BOOST_LEVELS]. The `1` is a bare immediate; the DWARF declares no named
// lower-bound constant for it, so it stays a literal rather than an invented one.
//
// mbJustLostBoostChunk (+0xCA) is the latch BoostStrategy::HasJustLostBoostChunk
// @0x822A5EC0 reads-and-clears; setting it INSIDE the guard is what the asm does
// (the `stb` is on the taken-branch side of `ble`), so a takedown that finds the
// level already at 1 raises no chunk-lost notification.
//
// No Feb-2007 counterpart exists -- the method post-dates that drop.
// ---------------------------------------------------------------------------
void
BoostBurnout3::OnTakenDownByAIOrPlayer()
{
    if( miBoostLevel > 1 )
    {
        --miBoostLevel;
        mbJustLostBoostChunk = true;
    }

    UpdateMaxBoost( false );
}

// ---------------------------------------------------------------------------
// OnWrecked @ 0x822C2438 -- vtable slot 8 (r3 = this, r4 = lbIsInOnlineGameMode).
// Fired by BoostStrategy::SetWrecking on the false->true edge (X360: inlined
// into BoostManager::SetWrecking @0x822B8E40).
//
//   0x822C2438  lfs   f0,  0xA0(r3)       ; mfBoostAmount
//   0x822C243C  lis   r11, flt_82001CC0@ha
//   0x822C2440  lfs   f13, 0x94(r3)       ; mfOnWrecked
//   0x822C2444  fsubs f0, f0, f13
//   0x822C2448  stfs  f0,  0xA0(r3)
//   0x822C244C  lfs   f12, flt_82001CC0@l(r11)   ; f12 = 0.0f
//   0x822C2450  fcmpu cr6, f0, f12
//   0x822C2454  bge   cr6, loc_822C245C
//   0x822C2458  stfs  f12, 0xA0(r3)
//        ^-- BoostStrategy::RemoveBoost (DecFIGS BrnBoostStrategy.cpp:168)
//            inlined: subtract, then clamp at zero. Byte-for-byte the same shape
//            the compiler emitted for the same helper in BoostBurnout2::OnWrecked
//            (@0x822C1608..0x822C1620) and BoostBurnout2::OnCrash
//            (@0x822A6370..0x822A6390). Reversing the inline restores the call.
//   loc_822C245C:
//   0x822C245C  lfs    f13, 0xA0(r3)      ; v = mfBoostAmount
//   0x822C2460  clrlwi r11, r4, 24        ; r11 = (u8)lbIsInOnlineGameMode
//   0x822C2464  fneg   f11, f13
//   0x822C2468  lfs    f0,  0xA4(r3)      ; f0 = mfMaxBoost (LIVE to the end)
//   0x822C246C  cmplwi cr6, r11, 0        ; ...the compare, hoisted here
//   0x822C2470  fsel   f13, f11, f12, f13 ; (-v  >= 0) ? 0.0f : v
//   0x822C2474  fsubs  f11, f0,  f13
//   0x822C2478  fsel   f13, f11, f13, f0  ; (max-v >= 0) ? v    : max
//   0x822C247C  stfs   f13, 0xA0(r3)
//   0x822C2480  beqlr  cr6                ; ...and its branch: offline -> return
//   0x822C2484  lfs    f11, 0x100(r3)     ; mfMinBoostAllowedAmount
//   0x822C2488  fcmpu  cr6, f13, f11
//   0x822C248C  bgelr  cr6
//   0x822C2490  lis    r11, flt_82001C98@ha
//   0x822C2494  lfs    f13, flt_82001C98@l(r11)  ; 1.0f
//   0x822C2498  fadds  f13, f11, f13      ; t = mfMinBoostAllowedAmount + 1.0f
//   0x822C249C  fneg   f11, f13
//   0x822C24A0  fsel   f13, f11, f12, f13 ; (-t  >= 0) ? 0.0f : t
//   0x822C24A4  fsubs  f12, f0,  f13
//   0x822C24A8  fsel   f0,  f12, f13, f0  ; (max-t >= 0) ? t    : max
//   0x822C24AC  stfs   f0,  0xA0(r3)
//   0x822C24B0  blr
//
// THE PARAMETER IS LIVE, AND THE BODY IS TWO NESTED CONDITIONS. r4 is narrowed
// to a byte at 0x822C2460 and compared at 0x822C246C, but its branch is the
// `beqlr cr6` five instructions later at 0x822C2480 -- the compiler hoisted the
// compare into the floating-point clamp to fill slots, so a straight read of the
// listing can miss that lbIsInOnlineGameMode is tested at all. It is: everything from
// 0x822C2484 onward runs ONLY in an online mode. The identical
// hoisting appears in BoostBurnout3::UpdateMaxBoost, where `clrlwi r11,r30,24`
// @0x822C1D10 / `cmplwi cr6,r11,0` @0x822C1D1C sit three instructions apart from
// their `beq cr6` @0x822C1D30. DWARF agrees the parameter exists
// (`virtual void OnWrecked(bool);`, BrnBoostBurnout3.h / cpp:445).
//
// NaN POLARITY -- the reason this is NOT rw::math::fpu::Clamp. `fsel frD,frA,frB,frC`
// yields frB when frA >= 0 and frC otherwise, and a NaN frA is NOT >= 0, so:
//   * lower half (0x822C2470, 0x822C24A0): frA = -v, frC = v, so a NaN input
//     falls through UNCHANGED;
//   * upper half (0x822C2478, 0x822C24A8): frA = max - v, frC = mfMaxBoost, so a
//     NaN input comes out as mfMaxBoost -- the bar SELF-HEALS on console.
// A three-argument Clamp written as `(v<lo)?lo:((hi<v)?hi:v)` returns NaN
// unchanged and would make a NaN stick forever, so the upper half must be spelled
// as a NEGATED ORDERED predicate. Same spelling as BoostBurnout2::OnWrecked
// @0x822C15D8 (partfile 05), which clamps the same pair of members.
// (Lower half: `if (v < 0.0f)` differs from the fsel only in the sign of a zero
// result -- the fsel turns -0.0f into +0.0f, the `if` leaves it -0.0f. Every
// other input, NaN included, agrees. The sibling spells it `<`; so does this.)
//
// The `bgelr` at 0x822C248C needs no such rewrite in the other direction: `bge`
// after `fcmpu` is taken on greater-or-equal AND on unordered, so the guarded
// body is reached only on a strictly-ordered less-than -- which is exactly what
// C++ `<` means (false for NaN).
//
// mfMaxBoost is loaded ONCE, at 0x822C2468, and f0 is still that value when the
// second clamp uses it at 0x822C24A4; nothing in between can change +0xA4, so
// naming the member twice below is faithful.
//
// THE `+ 1.0f` IS THE SHARED IMAGE-WIDE 1.0f, NOT A NAMED B3 CONSTANT. It is
// flt_82001C98, the same rodata slot BoostBurnout3::UpdateMaxBoost @0x822C1D04
// loads for its mfMaxBoost fallback. It is specifically NOT
// KF_BOOST_CHUNK_AMOUNT: that constant is flt_82014A18 == 26.666666f (the
// multiplier UpdateMaxBoost applies at @0x822C1CC8), a different slot with a
// different value.
//
// miBoostLevel (+0x134) IS NOT TOUCHED HERE. No instruction in this body
// references +0x134; the level floor of 1 is OnTakenDownByAIOrPlayer's
// (@0x822A6934), not this function's.
//
// No Feb-2007 counterpart exists -- Feb-2007's BoostBurnout3 has no OnWrecked
// (the whole wreck path post-dates that drop).
// ---------------------------------------------------------------------------
void
BoostBurnout3::OnWrecked(
    bool lbIsInOnlineGameMode )
{
    RemoveBoost( mfOnWrecked );

    // Shared [0, mfMaxBoost] clamp -- see the NaN POLARITY note above.
    if( mfBoostAmount < 0.0f )
    {
        mfBoostAmount = 0.0f;
    }
    if( !( mfBoostAmount <= mfMaxBoost ) )
    {
        mfBoostAmount = mfMaxBoost;
    }

    if( lbIsInOnlineGameMode )
    {
        // An instant wreck must not leave the car below the amount the mode
        // guarantees, so top the bar back up to just over that floor.
        if( mfBoostAmount < mfMinBoostAllowedAmount )
        {
            f32 lfRestoredBoost = mfMinBoostAllowedAmount + 1.0f;

            if( lfRestoredBoost < 0.0f )
            {
                lfRestoredBoost = 0.0f;
            }
            if( !( lfRestoredBoost <= mfMaxBoost ) )
            {
                lfRestoredBoost = mfMaxBoost;
            }

            mfBoostAmount = lfRestoredBoost;
        }
    }
}

} // namespace BrnWorld
