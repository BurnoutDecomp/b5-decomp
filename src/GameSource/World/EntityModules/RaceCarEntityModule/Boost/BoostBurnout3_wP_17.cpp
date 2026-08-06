// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 17.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::UpdateStuntBoost @ 0x822A6708   (base vtable slot 48, +0xC0)
//
// This body was parked in wave P because
// BrnGameState::GameStateModuleIO::CompletedStuntAction was only FORWARD-declared
// in the tree (BrnBoostStrategy.h:87). It now has a real definition in its home,
// GameSource/GameState/BrnGameActions.h:766, so the four payload reads below name
// real members. Nothing is forked here.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"
#include "GameSource/GameState/BrnGameActions.h"             // BrnGameState::GameStateModuleIO::CompletedStuntAction
#include "GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManagerShared.h" // BrnPhysics::EStuntActionComplete
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT (the X360 Begin/Fire/EndAssert triple)

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// UpdateStuntBoost @ 0x822A6708 -- the base's vtable slot 48 (+0xC0), overridden.
//
// Pays out the four one-shot stunt awards carried by a CompletedStuntAction, each
// gated on its own bit of the action's muStuntActionComplete bitfield, each paid
// through the base's protected virtual AddBoost (slot 49, +0xC4).
//
// Full asm (re-derived from the export at 0x822A6708; r31 == this, r30 == the
// action pointer, set up by `mr r30, r4` / `mr r31, r3` @0x822A671C-20):
//
//   0x822A6724  cmplwi cr6, r30, 0                 ; lpCompletedStuntAction == NULL?
//   0x822A6728  bne    cr6, loc_822A674C
//   0x822A672C  bl     CgsDev::Assert::BeginAssert
//   0x822A6734  li     r5, 0x14A                   ; line 330
//   0x822A6738  addi   r4, r11, aDP4B5MainBurno_531 ; "d:\p4\b5_main\burnout\main\code\g"...
//   0x822A6740  addi   r3, r11, aLpcompletedstu_0  ; "lpCompletedStuntAction != NULL"
//   0x822A6744  bl     CgsDev::Assert::FireAssert
//   0x822A6748  bl     CgsDev::Assert::EndAssert
// loc_822A674C:                                    ; NOTE: NO early-out. The assert
//                                                  ; falls straight through and the
//                                                  ; code dereferences r30 anyway --
//                                                  ; CGS_ASSERT is report-and-continue
//                                                  ; on this build, so a plain
//                                                  ; CGS_ASSERT with no `return`
//                                                  ; reproduces it exactly.
//   0x822A674C  lwz    r11, 0(r30)                 ; muStuntActionComplete
//   0x822A6750  clrlwi r11, r11, 31                ; & 0x1  -> BARREL_ROLL
//   0x822A6758  beq    cr6, loc_822A6790
//   0x822A675C  lwz    r11, 0x18(r30)              ; miCompletedBarrelRolls
//   0x822A6760  lfs    f0,  0x5C(r31)              ; mfBarrelRollEarning
//   0x822A6764  lwz    r10, 0(r31)                 ; vptr
//   0x822A676C  extsw  r11, r11                    ; SIGNED 32-bit -> s32, not u32
//   0x822A6770  lwz    r10, 0xC4(r10)              ; +0xC4 == slot 49 == AddBoost
//   0x822A677C  fcfid  f13, f13                    ; int -> f64
//   0x822A6780  frsp   f13, f13                    ; -> f32
//   0x822A6784  fmuls  f1,  f13, f0                ; count * mfBarrelRollEarning
//   0x822A678C  bctrl
// loc_822A6790:
//   0x822A6790  lwz    r11, 0(r30)                 ; RE-READ (see the re-read note)
//   0x822A6794  rlwinm r11, r11, 0,30,30           ; & 0x2  -> AIR_SPIN
//   0x822A679C  beq    cr6, loc_822A67C0
//   0x822A67A4  lfs    f0,  0x58(r31)              ; mfAirSpinEarning
//   0x822A67A8  lfs    f13, 8(r30)                 ; mfCompletedAirSpinAngle
//   0x822A67B0  fmuls  f1,  f0, f13                ; earning * angle (operand order)
//   0x822A67B4  lwz    r11, 0xC4(r11)              ; AddBoost
//   0x822A67BC  bctrl
// loc_822A67C0:
//   0x822A67C0  lwz    r11, 0(r30)
//   0x822A67C4  rlwinm r11, r11, 0,29,29           ; & 0x4  -> HANDBREAK_TURN
//   0x822A67CC  beq    cr6, loc_822A67F0
//   0x822A67D4  lfs    f0,  0x50(r31)              ; mfHandbrake180Earning
//   0x822A67D8  lfs    f13, 0xC(r30)               ; mfCompletedHandbreakTurnAngle
//   0x822A67E0  fmuls  f1,  f0, f13
//   0x822A67EC  bctrl                              ; AddBoost
// loc_822A67F0:
//   0x822A67F0  lwz    r11, 0(r30)
//   0x822A67F4  rlwinm r11, r11, 0,28,28           ; & 0x8  -> CLEANLANDING
//   0x822A67FC  beq    cr6, loc_822A6818
//   0x822A6804  lfs    f1,  0x60(r31)              ; mfCleanLanding -- FLAT award,
//                                                  ; reads no payload and applies no
//                                                  ; scale (there is no fmuls here)
//   0x822A6814  bctrl                              ; AddBoost
// loc_822A6818: epilogue.
//
// SIGNEDNESS. The barrel-roll payload is `lwz` + `extsw` + `fcfid` @0x822A675C /
// 0x822A676C / 0x822A677C -- a SIGN-EXTENDED word converted by the signed
// int-to-float instruction, i.e. an s32. That is what pins CompletedStuntAction's
// +0x18 as `s32 miCompletedBarrelRolls` (BrnGameActions.h:774) and it is why the
// cast below is on a signed member. `fcfid` on a zero-extended word would be the
// unsigned form; it is not what the compiler emitted.
//
// THE RE-READ IS REAL, NOT AN ARTEFACT. muStuntActionComplete is re-loaded from
// 0(r30) before EACH of the four tests (0x822A674C / 0x822A6790 / 0x822A67C0 /
// 0x822A67F0) rather than being held in a register across the AddBoost calls.
// Reading the member expression fresh in each `if` below reproduces that; hoisting
// it into a local would not. (AddBoost is an out-of-line virtual and the action is
// a `const` pointer to non-const-qualified storage, so the compiler had to reload.)
//
// THE FOUR BITS. The masks are the physics detector's completed-stunt bitfield,
// whose DWARF-verbatim enumerators already have a home in
// GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManagerShared.h
// (`BrnPhysics::EStuntActionComplete`, values 1/2/4/8 for BARREL_ROLL / AIR_SPIN /
// HANDBREAK_TURN / CLEANLANDING). Those are pre-existing in-tree declarations, NOT
// minted here; what the asm attests on its own is only bit positions 0/1/2/3 of the
// u32. The binding is corroborated by which earning member each bit pays:
// bit0 -> mfBarrelRollEarning (+0x5C), bit1 -> mfAirSpinEarning (+0x58),
// bit2 -> mfHandbrake180Earning (+0x50), bit3 -> mfCleanLanding (+0x60) -- the four
// names line up one-for-one with the four enumerators, in the same bit order.
// (Offsets from BrnBoostStrategy.h:453/455/456/457.)
//
// OPERAND ORDER. Kept as emitted: the barrel-roll multiply is `fmuls f1, f13, f0`
// (count * earning) while the air-spin and handbrake ones are `fmuls f1, f0, f13`
// (earning * angle). Float multiply is not associative-safe to reorder in general,
// and the differing order is itself evidence of how the two source expressions were
// written; there is no reason to normalise it.
//
// SIBLINGS. BoostBurnout2 @0x822A6478 and BoostBurnout5 @0x822A6F98 are the same
// body -- all three are 74 instructions and differ only in the two assert operands,
// which are per-file by construction:
//   B3 (here) `li r5, 0x14A` = line 330, file string aDP4B5MainBurno_531
//   B2        `li r5, 0x304` = line 772, file string aDP4B5MainBurno_530
//   B5        `li r5, 0x33E` = line 830, file string aDP4B5MainBurno_532
// The expression string aLpcompletedstu_0 ("lpCompletedStuntAction != NULL") is the
// SAME pooled literal in all three. So the three overrides share this source text.
//
// FEB-2007 has no UpdateStuntBoost at all -- the whole slot-48 stunt-boost path
// postdates it -- so there is nothing to reconcile against; this body is purely
// asm-derived.
// ---------------------------------------------------------------------------
void BoostBurnout3::UpdateStuntBoost(const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpCompletedStuntAction)
{
    CGS_ASSERT(lpCompletedStuntAction != 0, "lpCompletedStuntAction != NULL");

    if( (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_BARREL_ROLL) != 0 )
    {
        AddBoost(static_cast<f32>(lpCompletedStuntAction->miCompletedBarrelRolls) * mfBarrelRollEarning);
    }

    if( (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_AIR_SPIN) != 0 )
    {
        AddBoost(mfAirSpinEarning * lpCompletedStuntAction->mfCompletedAirSpinAngle);
    }

    if( (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_HANDBREAK_TURN) != 0 )
    {
        AddBoost(mfHandbrake180Earning * lpCompletedStuntAction->mfCompletedHandbreakTurnAngle);
    }

    if( (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_CLEANLANDING) != 0 )
    {
        AddBoost(mfCleanLanding);
    }
}

}
