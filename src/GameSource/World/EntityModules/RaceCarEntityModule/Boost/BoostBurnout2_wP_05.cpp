// ============================================================================
// BrnWorld::BoostBurnout2 -- wave-P partfile 05.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Bodies reconstructed in this partfile (X360 ARTIST addresses):
//   OnTakedown      @0x822A6288   (vtable slot 3)
//   OnTrafficCheck  @0x822A6358   (vtable slot 12)
//   OnWrecked       @0x822C15D8   (vtable slot 8)
//
// Authority: the X360 ARTIST asm quoted inline below (rung 1). Feb-2007 is used
// for idiom/naming only and CONTRADICTS retail on OnTakedown; it has no
// OnWrecked at all (the whole wreck path post-dates that drop). See the
// per-function DIVERGENCE notes.
//
// Offset -> member map used below. Base layout from the wave-P keystone header
// BrnBoostStrategy.h; BoostBurnout2's own members start at +0x130 because
// sizeof(BoostStrategy) == 0x130 on BOTH console and host:
//   +0x028 BoostStrategy::mfTakedownEarning     (attrib rec+0x08 TakedownEarning)
//   +0x048 BoostStrategy::mfTrafficCheck        (attrib rec+0x00 TrafficCheck)
//   +0x094 BoostStrategy::mfOnWrecked           (attrib rec+0x30 OnWrecked)
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x0A4 BoostStrategy::mfMaxBoost
//   +0x0C5 BoostStrategy::mbBoosting
//   +0x134 BoostBurnout2::mfHiddenBoost         (DWARF BrnBoostBurnout2.h:134)
//   +0x138 BoostBurnout2::mbBoostInterrupted    (DWARF BrnBoostBurnout2.h:135)
//
// Shared rodata literal used by all three: flt_82001CC0 == 0.0f, pinned by
// RemoveAllBoostAndChunks @0x822A63F0 (Feb-2007 body: mfHiddenBoost = 0.0f;
// mfBoostAmount = 0.0f; the asm stores flt_82001CC0 to both +0x134 and +0xA0).
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnTakedown -- vtable slot 3 (pure in the base). @0x822A6288
//
//   0x822A6288  lbz    r11, 0xC5(r3)      ; mbBoosting
//   0x822A628C  cmplwi cr6, r11, 0
//   0x822A6290  beq    cr6, loc_822A62A8
//   0x822A6294  lfs    f0,  0x134(r3)     ; mfHiddenBoost
//   0x822A6298  lfs    f13, 0x28(r3)      ; mfTakedownEarning
//   0x822A629C  fadds  f0, f13, f0
//   0x822A62A0  stfs   f0,  0x134(r3)     ; mfHiddenBoost += mfTakedownEarning
//   0x822A62A4  blr
//   0x822A62A8  lwz    r11, 0(r3)         ; vtable
//   0x822A62AC  lfs    f1,  0x28(r3)      ; mfTakedownEarning -> f1 (PPC float arg)
//   0x822A62B0  lwz    r11, 0xC4(r11)     ; +0xC4 == slot 49 == AddBoost
//   0x822A62B4  mtctr  r11
//   0x822A62B8  bctr                      ; tail virtual call
//
// The two-way "bank it while boosting, otherwise put it on the bar" shape is
// BoostBurnout2's house idiom -- Feb-2007 writes exactly this in OnNearMiss and
// OnSlam, so the member name mfHiddenBoost and the `+=` are style-confirmed.
// BoostBurnout2 does not override AddBoost, so the in-class call still goes
// through the vtable, which is what the `bctr` on slot 49 shows.
//
// DIVERGENCE FROM FEB-2007: the Feb-2007 body is a one-liner,
// `mfBoostAmount = mfMaxBoost;` (BrnBoostBurnout2.cpp:172) -- an instant full
// bar. Retail replaced it with a tuned earning that respects AddBoost's
// speed-scaling/earning gates. The asm wins: neither mfBoostAmount (+0xA0) nor
// mfMaxBoost (+0xA4) is touched anywhere in this body.
// ---------------------------------------------------------------------------
void
BoostBurnout2::OnTakedown()
{
    if (mbBoosting)
    {
        // Cannot grow the live bar mid-boost; bank it in the hidden pool.
        mfHiddenBoost += mfTakedownEarning;
    }
    else
    {
        AddBoost(mfTakedownEarning);
    }
}

// ---------------------------------------------------------------------------
// OnTrafficCheck -- vtable slot 12 (pure in the base). @0x822A6358
//
//   0x822A6358  lwz   r11, 0(r3)          ; vtable
//   0x822A635C  lfs   f1,  0x48(r3)       ; mfTrafficCheck -> f1
//   0x822A6360  lwz   r11, 0xC4(r11)      ; +0xC4 == slot 49 == AddBoost
//   0x822A6364  mtctr r11
//   0x822A6368  bctr                      ; tail virtual call
//
// Five instructions, no `this` state touched: a bare AddBoost. NOTE the
// asymmetry with OnTakedown above -- there is NO mbBoosting / mfHiddenBoost
// branch here, and mbJustTrafficChecked (+0xC2) is NOT set (the flag is raised
// by the traffic-check detector, not by this reward hook). Do not "normalise"
// this body against OnTakedown's shape.
//
// FEB-2007 agrees on shape (BrnBoostBurnout2.cpp:219,
// `AddBoost( mfOnTrafficCheckBoostReward );`); only the member NAME changed --
// the retail/DecFIGS base member for attrib record offset rec+0x00
// ("TrafficCheck") is BoostStrategy::mfTrafficCheck at +0x48.
// ---------------------------------------------------------------------------
void
BoostBurnout2::OnTrafficCheck()
{
    AddBoost(mfTrafficCheck);
}

// ---------------------------------------------------------------------------
// OnWrecked -- vtable slot 8 (pure in the base). @0x822C15D8
// Fired by BoostStrategy::SetWrecking on the false->true edge (X360: inlined
// into BoostManager::SetWrecking @0x822B8E40).
//
//   0x822C15D8  lbz    r11, 0xC5(r3)      ; mbBoosting
//   0x822C15DC  cmplwi cr6, r11, 0
//   0x822C15E0  lis    r11, flt_82001CC0@ha
//   0x822C15E4  lfs    f0,  flt_82001CC0@l(r11)  ; f0 = 0.0f
//   0x822C15E8  beq    cr6, loc_822C1608
//   0x822C15EC  lfs    f13, 0xA4(r3)      ; mfMaxBoost
//   0x822C15F0  li     r11, 0
//   0x822C15F4  fsubs  f12, f13, f0       ; mfMaxBoost - 0.0f
//   0x822C15F8  stfs   f0,  0x134(r3)     ; mfHiddenBoost      = 0.0f
//   0x822C15FC  stb    r11, 0x138(r3)     ; mbBoostInterrupted = false
//   0x822C1600  fsel   f13, f12, f0, f13  ; (mfMaxBoost-0 >= 0) ? 0.0f : mfMaxBoost
//   0x822C1604  stfs   f13, 0xA0(r3)      ; mfBoostAmount = that
//   loc_822C1608:
//   0x822C1608  lfs    f13, 0xA0(r3)      ; mfBoostAmount
//   0x822C160C  lfs    f12, 0x94(r3)      ; mfOnWrecked
//   0x822C1610  fsubs  f13, f13, f12
//   0x822C1614  stfs   f13, 0xA0(r3)      ; mfBoostAmount -= mfOnWrecked
//   0x822C1618  fcmpu  cr6, f13, f0
//   0x822C161C  bge    cr6, loc_822C1624
//   0x822C1620  stfs   f0,  0xA0(r3)      ; ...clamped at zero
//   loc_822C1624:
//   0x822C1624  lfs    f13, 0xA0(r3)
//   0x822C1628  fneg   f11, f13
//   0x822C162C  lfs    f12, 0xA4(r3)      ; mfMaxBoost
//   0x822C1630  fsel   f0,  f11, f0, f13  ; (-v >= 0) ? 0.0f : v      == Max(0, v)
//   0x822C1634  fsubs  f13, f12, f0
//   0x822C1638  fsel   f0,  f13, f0, f12  ; (max - t >= 0) ? t : max  == Min(t, max)
//   0x822C163C  stfs   f0,  0xA0(r3)
//   0x822C1640  blr
//
// THE PARAMETER IS UNUSED. DWARF declares `virtual void OnWrecked(bool)` and
// the base spells it lbInstantWreck, but r4 is never read anywhere in this
// body -- BoostBurnout2 wrecks identically either way. (Register check: the
// only GPR sourced is r3 = this; the two float operands come from `this`, not
// from f1, so there is no hidden PPC float argument being missed here.)
//
// THE SUBTRACT+CLAMP-AT-ZERO PAIR (0x822C1608..0x822C1620) IS
// BoostStrategy::RemoveBoost (DecFIGS BrnBoostStrategy.cpp:168) inlined -- it
// is byte-for-byte the same shape the compiler emitted in OnCrash @0x822A6374,
// where the same helper is inlined. Reversing the inline restores the call.
// NaN polarity there needs no inversion: `fcmpu`+`bge` is `bc 4,LT`, so the
// clamp store is reached only when LT is set; an unordered compare leaves LT
// clear and TAKES the branch -- exactly what C++ `if (x < 0.0f)` means.
//
// THE TAIL (0x822C1624..0x822C163C) IS THE SHARED [0, mfMaxBoost] CLAMP: it is
// instruction-for-instruction the same fsel pair the compiler emits at the tail
// of BoostStrategy::UpdateMaxBoost @0x822C0EF4..0x822C0F14 and
// BoostStrategy::AddBoost @0x822C0E88..0x822C0EA4. The f32 Min/Max helper it
// was inlined from is not present in any recovered header, so it is written out
// here. NaN polarity IS load-bearing on the upper half: fsel yields mfMaxBoost
// when the compare is unordered, so the ordered predicate must be negated -- a
// plain `if (x > mfMaxBoost)` would leave a NaN unclamped and diverge.
//
// The in-branch store at 0x822C1604 is that SAME clamp applied to a literal
// 0.0f: the compiler constant-folded the Max(0, 0) half away and kept only the
// Min against mfMaxBoost (hence the otherwise-pointless `fsubs f12, f13, f0`
// against zero). Written below in the fsel's own sense so the unordered case
// still yields mfMaxBoost.
//
// FEB-2007 HAS NO COUNTERPART: neither OnWrecked nor SetWrecking exists
// anywhere in the Feb-2007 Boost tree, so this body is read entirely from the
// retail asm.
// ---------------------------------------------------------------------------
void
BoostBurnout2::OnWrecked(bool /* lbInstantWreck -- unused; r4 never read */)
{
    if (mbBoosting)
    {
        // A wreck kills the run outright: the banked pool is forfeited (+0x134
        // = 0.0f) and the bar is emptied.
        //
        // CLEARING mbBoostInterrupted RE-ARMS the chain payout, it does not
        // suppress it -- CORRECTED 2026-08-06, this comment used to claim the
        // opposite. Re-derived from the asm: ApplyUpdate @0x822C1410 reads
        // +0x138 and `bne` @0x822C141C SKIPS the
        // `AddBoost(mfHiddenBoost + mfBoostChainBonus)` + mbChainNotifyPending +
        // ++miChainSize payout whenever the flag is NON-zero, and OnCrash
        // @0x822A639C stores 1 into it precisely to suppress that path. Storing
        // 0 here therefore leaves the payout path OPEN; what makes the wreck
        // cost nothing is mfHiddenBoost being zeroed on the line above, not the
        // flag. The code below is correct as written -- do not "fix" it.
        mfHiddenBoost      = 0.0f;
        mbBoostInterrupted = false;
        mfBoostAmount      = (mfMaxBoost >= 0.0f) ? 0.0f : mfMaxBoost;
    }

    RemoveBoost(mfOnWrecked);

    // Shared [0, mfMaxBoost] clamp (see the note above).
    if (mfBoostAmount < 0.0f)
    {
        mfBoostAmount = 0.0f;
    }
    if (!(mfBoostAmount <= mfMaxBoost))
    {
        mfBoostAmount = mfMaxBoost;
    }
}

} // namespace BrnWorld
