// ============================================================================
// BrnWorld::BoostBurnout2 -- wave P partfile 06.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX; the
// ARTIST asm is rung 1 and arbitrates every branch/constant/store below):
//   BoostBurnout2::RemoveAllBoostAndChunks @ 0x822A63F0   (vtable slot 41)
//
// NOT here -- PARKED complete-but-unlandable, see the wave report and
// scratchpad/waveP/parked/BoostBurnout2_wP_6.parked.cpp (workflow repo):
//   BoostBurnout2::Prepare          @ 0x822C0F38  (vtable slot 0)
//       needs 34 attribute accessors on Attrib::Gen::boostparamsasset -- the
//       committed generated class derives Attrib::Instance PRIVATELY and
//       re-exports nothing, so its 0x88-byte data area is unreachable from
//       this TU. (Same blocker the BoostBurnout3 ApplyUpdate park hit.)
//   BoostBurnout2::UpdateStuntBoost @ 0x822A6478  (vtable slot 48)
//       needs the complete type BrnGameState::GameStateModuleIO::
//       CompletedStuntAction; BrnBoostStrategy.h only forward-declares it and
//       no header in b5-decomp/src defines it.
//
// Offset -> member map used below (base layout from the wave-P keystone header
// BrnBoostStrategy.h; BoostBurnout2's own members start at +0x130 because
// sizeof(BoostStrategy) == 0x130 on BOTH console and host):
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x134 BoostBurnout2::mfHiddenBoost   (DWARF BrnBoostBurnout2.h:134)
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// RemoveAllBoostAndChunks @ 0x822A63F0 -- vtable slot 41 (pure in the base;
// all three concrete strategies override it).
//
//   0x822A63F0  lis   r11, flt_82001CC0@ha
//   0x822A63F4  lfs   f0,  flt_82001CC0@l(r11)   # 0.0f
//   0x822A63F8  stfs  f0,  0x134(r3)             # mfHiddenBoost = 0.0f
//   0x822A63FC  stfs  f0,  0xA0(r3)              # mfBoostAmount = 0.0f
//   0x822A6400  blr
//
// Five instructions, no branches, no `this` reads: both stores are the single
// shared 0.0f constant flt_82001CC0. The Hex-Rays `int __fastcall ...(int
// result)` / `return result` is the usual PPC artifact of r3 (the incoming
// `this`) surviving to the blr -- the DWARF declares `virtual void
// RemoveAllBoostAndChunks()` (BrnBoostBurnout2.h:102 / cpp:709) and nothing in
// the image consumes a return value from slot 41, so the return type is void.
//
// The live bar AND the parked stash both go to zero: mfHiddenBoost is the
// accumulator BoostBurnout2 fills while a boost run is in flight (OnDriveThru
// @0x822C1648, the OnSlam-family adds) and drains when the run ends, so
// clearing mfBoostAmount alone would let the stash refill the bar on the next
// drain. Retail and Feb-2007 agree here verbatim, store order included
// (Feb-2007 BrnBoostBurnout2.cpp:393-397).
// ---------------------------------------------------------------------------
void
BoostBurnout2::RemoveAllBoostAndChunks()
{
    mfHiddenBoost = 0.0f;
    mfBoostAmount = 0.0f;
}

}   // namespace BrnWorld
