#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 05.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom only -- and for all three of
// these bodies retail CONTRADICTS Feb-2007, see the per-function notes).
//
// Functions in this partfile:
//   BoostBurnout5::OnStuntCompletion  @ 0x822A6C68   (vtable slot 10)
//   BoostBurnout5::OnTrafficCheck     @ 0x822A6F18   (vtable slot 12)
//   BoostBurnout5::OnTakedown         @ 0x822A6F60   (vtable slot 3)
//
// All three are one-line "earn a tuning-table amount" handlers: each loads a
// f32 out of the BoostStrategy tuning block and dispatches vtable slot 49
// (+0xC4 == 49 * 4) -- BoostStrategy::AddBoost, pinned as slot 49 by the
// keystone (OnModeStart @0x822A6030 dispatches the same +0xC4). BoostBurnout5
// does NOT override AddBoost (no such DWARF declaration, no X360 identity), so
// the dispatch lands on the base body @0x822C0E10; it is emitted as a virtual
// call because AddBoost is a non-final virtual, which is exactly what plain
// `AddBoost(x)` compiles to.
//
// Offset -> member map used below (base layout from the wave-P keystone header
// BrnBoostStrategy.h; every one of these is a BoostStrategy member, this
// partfile touches NO BoostBurnout5-own member):
//   +0x028 BoostStrategy::mfTakedownEarning       (attrib rec+0x08 TakedownEarning)
//   +0x048 BoostStrategy::mfTrafficCheck          (attrib rec+0x00 TrafficCheck)
//   +0x080 BoostStrategy::mfStuntJumpEarning      (attrib rec+0x14 StuntJumpEarning)
//   +0x088 BoostStrategy::mfStuntBillBoardEarning (attrib rec+0x18 StuntBillBoardEarning)
//   +0x0C2 BoostStrategy::mbJustTrafficChecked    (IsATrafficCheckPending @0x822A5E98)
//
// Compile-validated out of tree (b5-decomp/.../BrnBoostBurnout5.h does not
// exist yet and this pass may not create headers) against the scratchpad probe
// header carrying the full DWARF member list:
// scratchpad/waveP/probe5g05/ in the workflow repo.
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnStuntCompletion @ 0x822A6C68  (vtable slot 10; pure in the base)
//
//   0x822A6C68  cmpwi  cr6, r4, 0
//   0x822A6C6C  beq    cr6, loc_822A6C8C     # element == JUMP
//   0x822A6C70  cmpwi  cr6, r4, 2
//   0x822A6C74  bnelr  cr6                   # element != BILLBOARD -> return
//   0x822A6C78  lwz    r11, 0(r3)            # vptr
//   0x822A6C7C  lfs    f1,  0x88(r3)         # mfStuntBillBoardEarning
//   0x822A6C80  lwz    r11, 0xC4(r11)        # slot 49 = AddBoost
//   0x822A6C84  mtctr  r11
//   0x822A6C88  bctr                         # tail call
//   loc_822A6C8C:
//   0x822A6C8C  lwz    r11, 0(r3)
//   0x822A6C90  lfs    f1,  0x80(r3)         # mfStuntJumpEarning
//   0x822A6C94  lwz    r11, 0xC4(r11)
//   0x822A6C98  mtctr  r11
//   0x822A6C9C  bctr                         # tail call
//
// r4 is the element type (an int-width enum; no FPR is consumed before it, so
// no PPC float-arg GPR skip is in play here). DWARF pins the parameter type:
// `virtual void OnStuntCompletion(BrnGameState::StuntElementType)`
// (BrnBoostBurnout5.h, DWARF body @cpp:251), and the enumerators are
// JUMP = 0 / SMASH = 1 / BILLBOARD = 2 (BrnGameStateTypes.h). The two tested
// values 0 and 2 therefore map exactly onto the two tuning params whose names
// match -- StuntJumpEarning and StuntBillBoardEarning -- which is the
// independent confirmation that this really is the element-type enum.
//
// SMASH (1) falls through and earns NOTHING in retail, even though
// mfStuntSmashEarning (+0x84) exists in the tuning block: the asm has no third
// arm and no load of +0x84 anywhere in this body. (Consistent with
// BoostBurnout5::Prepare @0x822C1F8C-94, which re-zeroes all three Stunt*
// earning params right after the attrib load -- B5 pays stunt boost through
// UpdateStuntBoost instead.)
//
// FEB-2007 DIVERGENCE: the Feb-2007 body (BrnBoostBurnout5.cpp:226) takes NO
// parameter at all and unconditionally does `AddBoostB5(mfBoostAmountOnStunt)`
// -- a per-B5 static const and a B5-private AddBoostB5 helper. Retail has
// neither: the signature grew the element-type argument, the reward moved into
// the shared boostparamsasset tuning block, and the earn goes through the base
// virtual AddBoost. The asm wins.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnStuntCompletion(
    BrnGameState::StuntElementType leElementType )
{
    switch( leElementType )
    {
    case BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP:
        AddBoost( mfStuntJumpEarning );
        break;

    case BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD:
        AddBoost( mfStuntBillBoardEarning );
        break;

    default:
        // E_STUNT_ELEMENT_TYPE_SMASH earns nothing in retail (no arm in the asm).
        break;
    }
}

// ---------------------------------------------------------------------------
// OnTrafficCheck @ 0x822A6F18  (vtable slot 12; pure in the base)
//
//   0x822A6F18  mflr   r12
//   0x822A6F1C  stw    r12, var_8(r1)
//   0x822A6F20  std    r31, var_10(r1)
//   0x822A6F24  stwu   r1, back_chain(r1)
//   0x822A6F28  mr     r31, r3               # this
//   0x822A6F2C  lwz    r11, 0(r31)           # vptr
//   0x822A6F30  lfs    f1,  0x48(r31)        # mfTrafficCheck
//   0x822A6F34  lwz    r11, 0xC4(r11)        # slot 49 = AddBoost
//   0x822A6F38  mtctr  r11
//   0x822A6F3C  bctrl                        # CALL (not a tail call)
//   0x822A6F40  li     r11, 1
//   0x822A6F44  stb    r11, 0xC2(r31)        # mbJustTrafficChecked = true
//   0x822A6F48..58  epilogue
//
// The store happens AFTER the AddBoost call returns -- that is why this body
// (unlike OnTakedown) sets up a frame and uses bctrl rather than bctr.
//
// FEB-2007 DIVERGENCE: Feb-2007 (BrnBoostBurnout5.cpp:456) is
// `AddBoostB5( mfOnTrafficCheckBoostReward )` -- a B5-private helper and a
// member that no longer exists. Retail earns the shared tuning param
// mfTrafficCheck through the base virtual AddBoost, and the
// mbJustTrafficChecked latch (read by BoostStrategy::IsATrafficCheckPending
// @0x822A5E98, slot 23) is NEW: Feb-2007 does not set it here.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnTrafficCheck()
{
    AddBoost( mfTrafficCheck );

    mbJustTrafficChecked = true;
}

// ---------------------------------------------------------------------------
// OnTakedown @ 0x822A6F60  (vtable slot 3; pure in the base)
//
//   0x822A6F60  lwz    r11, 0(r3)            # vptr
//   0x822A6F64  lfs    f1,  0x28(r3)         # mfTakedownEarning
//   0x822A6F68  lwz    r11, 0xC4(r11)        # slot 49 = AddBoost
//   0x822A6F6C  mtctr  r11
//   0x822A6F70  bctr                         # tail call, no frame
//
// The whole body: five instructions, no frame, no branch. Nothing is read or
// written besides the tuning param and the dispatch.
//
// FEB-2007 DIVERGENCE (the largest of the three): Feb-2007
// (BrnBoostBurnout5.cpp:470) switches on meBoostMode, and in RED mode bumps
// miBoostLevel up to KI_BOOST_LEVELS_IF_RED_MODE - 1, calls UpdateMaxBoost(),
// then AddBoostB5( mfMaxMaxBoost ); in BLUE mode AddBoostB5( mfMaxMaxBoost );
// otherwise asserts "Unsuported boost mode". Retail has NONE of that -- no
// load of meBoostMode (+0x130), no miBoostLevel/UpdateMaxBoost, no assert --
// just one virtual AddBoost of the shared tuning param mfTakedownEarning.
// The asm wins.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnTakedown()
{
    AddBoost( mfTakedownEarning );
}

} // namespace BrnWorld
