#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 06.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom/naming only).
//
// Functions ASSIGNED to this partfile:
//   BoostBurnout5::Prepare                  @ 0x822C1D58   (vtable slot 0)  -- PARKED
//   BoostBurnout5::RemoveAllBoostAndChunks  @ 0x822C2410   (vtable slot 41) -- below
//   BoostBurnout5::UpdateStuntBoost         @ 0x822A6F98   (vtable slot 48) -- PARKED
//
// The two parked bodies are COMPLETE and asm-walked but need declarations that
// do not exist in b5-decomp/src and that this pass may not create. They live,
// with the verbatim unblock text, at (workflow repo, not this tree):
//   scratchpad/waveP/parked/BoostBurnout5_wP_6.parked.cpp
//     * Prepare          -- Attrib::Gen::boostparamsasset exposes no attribute
//                           accessors (it derives Attrib::Instance PRIVATELY and
//                           re-exports nothing), and rw::core::stdc::ConvertI64ToA
//                           is not declared anywhere in the tree.
//     * UpdateStuntBoost -- BrnGameState::GameStateModuleIO::CompletedStuntAction
//                           is an incomplete type everywhere in b5-decomp/src;
//                           the body dereferences four of its fields.
//
// SHARED WAVE DEPENDENCY (blocks this partfile and every other BoostBurnout5 /
// BoostBurnout2 / BoostBurnout3 partfile landed this wave): the class header
//   .../RaceCarEntityModule/Boost/BrnBoostBurnout5.h
// does not exist yet, so `selfcheck` on this file reports C1083 on line 1. The
// class is `BoostBurnout5 : public BoostStrategy`, adds NO virtuals to the base
// 50 slots, and its own members -- every offset pinned by Prepare @0x822C1D58 --
// are, in DecFIGS DWARF declaration order:
//   +0x130 BoostMode meBoostMode                      (stw  0 -> 304)
//   +0x134 bool      mbLeaveBlueDueToCrash            (stb  0 -> 308)
//   +0x135 bool      mbLeaveBlueDueToInsufficientHidden
//   +0x136 bool      mbSwicthBlueToRed                (sic -- DWARF spelling)
//   +0x137 bool      mbAddRemoveChunkMode
//   +0x138 bool      mbAllowBoostEarning
//   +0x13C f32       mfHiddenBoost                    (stfs 0.0f -> 316)
//   +0x140 bool      mbBoostInterrupted
//   +0x144 s32       miChainSize
//   +0x148 f32       mfBlueCrashDecrease              (stfs 50.0f -> 328)
//   +0x150 Vector3   mStuntRollInProgress             (stvx128 zero)
//   +0x160 Vector3   mvPositionLastFrame              (stvx128 zero)
//   +0x170 Vector3   mvTakeOffAtVec                   (stvx128 zero)
//   +0x180 Vector3   mvLandAtVec                      (stvx128 zero)
//   +0x190 f32       mfBearingLastFrame
//   +0x194 f32       mfAngleSoFar
//   +0x198 f32       mfTimeElapsed
//   +0x19C f32       mfTimeBoosting
//   +0x1A0 bool      mbHandbreakTurnAttempting
//   +0x1A1 bool      mbWasJustInTheAir
//   +0x1A2 bool      mbTestForCleanLanding
//   sizeof 0x1B0, align 16.
// plus `enum BoostMode { E_BOOSTMODE_B3_RED = 0, E_BOOSTMODE_B2_BLUE = 1 };`
// (DWARF BrnBoostBurnout5.h:143). Base offsets come from the wave-P keystone
// header BrnBoostStrategy.h; sizeof(BoostStrategy) == 0x130 on BOTH console and
// host, which is why the console offsets above are directly assertable here.
//
// Base members touched by the body below (BrnBoostStrategy.h):
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x104 BoostStrategy::miBoostLevel
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// RemoveAllBoostAndChunks @ 0x822C2410  (vtable slot 41; pure in the base)
//
//   0x822C2410  lis    r11, flt_82001CC0@ha
//   0x822C2414  li     r10, 0
//   0x822C2418  li     r4,  0                  # UpdateMaxBoost's lbFillToMax
//   0x822C241C  lfs    f0,  flt_82001CC0@l(r11)   # 0.0f
//   0x822C2420  stfs   f0,  0xA0(r3)           # mfBoostAmount   = 0.0f
//   0x822C2424  stw    r10, 0x104(r3)          # miBoostLevel    = 0
//   0x822C2428  stfs   f0,  0x13C(r3)          # mfHiddenBoost   = 0.0f
//   0x822C242C  stw    r10, 0x130(r3)          # meBoostMode     = E_BOOSTMODE_B3_RED
//   0x822C2430  b      BrnWorld__BoostStrategy__UpdateMaxBoost
//
// Four stores and a TAIL CALL. Three details the asm pins:
//   * the branch to UpdateMaxBoost is a plain `b` to the base symbol, not a
//     vtable dispatch -- BoostStrategy::UpdateMaxBoost is NON-virtual (the
//     `virtual void UpdateMaxBoost(bool)` that DecFIGS shows on BoostBurnout3
//     is B3's OWN new virtual, which hides rather than overrides, and does not
//     exist on BoostBurnout5 at all).
//   * r4 is set to 0 BEFORE the branch, so the tail call is UpdateMaxBoost
//     (false) -- i.e. clamp mfBoostAmount into the recomputed [0, mfMaxBoost],
//     do NOT refill to max. That is the whole point here: the bar was just
//     emptied and must stay empty.
//   * meBoostMode is reset to the RED (Burnout-3 rules) mode, which is also the
//     value Prepare @0x822C1DC0 seeds -- blue mode cannot survive having its
//     hidden reservoir taken away.
//
// Feb-2007 (BrnBoostBurnout5.cpp:729-737) is the same four assignments in a
// different order followed by `UpdateMaxBoost();`. DIVERGENCE (asm wins, but it
// is cosmetic): retail's UpdateMaxBoost takes an explicit lbFillToMax argument
// that the Feb-2007 signature did not have; the retail call passes false, which
// is the behaviour the Feb-2007 no-arg form had.
// ---------------------------------------------------------------------------
void
BoostBurnout5::RemoveAllBoostAndChunks()
{
    mfBoostAmount = 0.0f;
    miBoostLevel  = 0;
    mfHiddenBoost = 0.0f;
    meBoostMode   = E_BOOSTMODE_B3_RED;

    UpdateMaxBoost(false);
}

} // namespace BrnWorld
