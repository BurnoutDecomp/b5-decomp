// ============================================================================
// BrnWorld::BoostBurnout2 -- wave P partfile 16.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX; the
// ARTIST asm is rung 1 and arbitrates every branch, constant and store below):
//   BoostBurnout2::Prepare          @ 0x822C0F38   (base vtable slot 0)
//   BoostBurnout2::UpdateStuntBoost @ 0x822A6478   (base vtable slot 48)
//
// Both were parked earlier in wave P on two header blockers that are now gone:
//   * Attrib::Gen::boostparamsasset now exposes the 34 named tuning-parameter
//     accessors (GameSource/AttribSys/Generated/classes/boostparamsasset.h) and
//     its constructor key is u64, so the collection actually resolves. This TU
//     goes through those accessors -- it does NOT re-export or poke
//     Instance::GetLayoutPointer.
//   * BrnGameState::GameStateModuleIO::CompletedStuntAction is now a complete
//     type in GameSource/GameState/BrnGameActions.h, with the X360 field order
//     (miCompletedBarrelRolls at +0x18, mbSuccessfulLanding at +0x1C).
//
// Offset -> member map used below. BoostBurnout2's own members start at +0x130
// because sizeof(BoostStrategy) == 0x130 on BOTH console and host; the 34
// tuning params and the two flags live in the base (BrnBoostStrategy.h).
//   +0x010..+0x094 BoostStrategy:: the 34 tuning params (see Prepare)
//   +0x0C3 BoostStrategy::mbIsBoostFull
//   +0x0CB BoostStrategy::mbChainNotifyPending
//   +0x138 BoostBurnout2::mbBoostInterrupted
//   +0x13C BoostBurnout2::miChainSize
//   +0x140 BoostBurnout2::mfTimeBasedBoostingBonus
//   +0x144 BoostBurnout2::mfContinuousBoostingTimeAdd
//   +0x148 BoostBurnout2::mfTimeBoosting
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"

#include "GameSource/GameState/BrnGameActions.h"                                  // BrnGameState::GameStateModuleIO::CompletedStuntAction
#include "GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManagerShared.h" // BrnPhysics::EStuntActionComplete
#include "GameSource/AttribSys/Generated/classes/boostparamsasset.h"              // Attrib::Gen::boostparamsasset
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT (the X360 Begin/Fire/EndAssert triple)
#include "rw/core/stdc/stdc.h"                                                    // rw::core::stdc::ConvertI64ToA

namespace BrnWorld
{

namespace
{
    // The boostparamsasset collection GUID BoostBurnout2 tunes itself from.
    // X360-attested: Prepare @0x822C0F74 `lis r3,8` + @0x822C0F80
    // `ori r3,r3,0xCA05` == 0x0008CA05 == 576005, handed to ConvertI64ToA with
    // r5 == 0xA so the key text is "576005".
    //
    // NAME AND TYPE are the DecFIGS DWARF's, verbatim -- BrnBoostBurnout2.cpp:4
    // of the dwarfdump declares `const int64_t KI_DEFAULT_DANGER_BOOST_PARAMS_
    // GUID = 576005;` at .cpp scope (BrnBoostBurnout2.h:84 records that this is
    // a .cpp-scope constant, not a header declaration).
    const s64 KI_DEFAULT_DANGER_BOOST_PARAMS_GUID = 576005;

    // The stack text buffer ConvertI64ToA writes the decimal GUID into. Hex-Rays
    // renders it as `_BYTE v9[512]` at sp+0x70 of Prepare's 0x280-byte frame.
    // A char count is width-invariant, so this is not a console-size hazard.
    const u32 KU_BOOST_KEY_TEXT_SIZE = 512;
}

// ---------------------------------------------------------------------------
// Prepare @ 0x822C0F38 -- base vtable slot 0.
//
// Shape, walked end to end from the asm:
//   0x822C0F4C  bl BoostStrategy::Prepare ; clrlwi r11,r3,24 ; cmplwi ; bne
//               -> continue, else `li r3,0 ; blr`          # EARLY-OUT on false
//   0x822C0F74  r3 = 0x0008CA05 (576005) ; r5 = 0xA ; r4 = sp+0x70 ;
//               bl rw__core__stdc__ConvertI64ToA           # -> "576005"
//   0x822C0F8C  bl Attrib__StringToKey                     # r3 = the FULL u64
//   0x822C0F90  mr r4,r3 (64-bit move, no clrldi) ; li r5,0 ; r3 = sp+0x60 ;
//               bl Attrib::Gen::boostparamsasset::boostparamsasset
//   0x822C0FA0  lwz r11, 0x64(sp)   # the constructed instance's layout block
//                                   # (Attrib::Instance::mpAttributeData)
//   0x822C0FA4..0x822C10F0  34 record loads -> 34 base-member stores
//   0x822C10E4..0x822C1104  the resets (interleaved with the last stores by the
//                           scheduler; all independent)
//   0x822C1108  bl Attrib::Instance::~Instance   # lBoostParams leaves scope
//   0x822C110C  li r3,1 ; blr                    # return true
//
// RETAIL vs FEB-2007 -- the asm wins on all of these:
//  * Feb-2007 (BrnBoostBurnout2.cpp:53-63) calls BoostStrategy::Prepare() and
//    IGNORES its result. Retail EARLY-OUTS on it (the byte-narrowing
//    `clrlwi r11,r3,24` + `bne` at 0x822C0F50-0x822C0F58, with the `li r3,0`
//    return in the not-taken path).
//  * Feb-2007 loads exactly ONE attribute here (Burnout2CrashDecrease) because
//    its BASE Prepare loaded the shared ones. Retail's base loads NOTHING, so
//    all 34 tuning params are loaded HERE.
//  * Feb-2007 assigns `mfCrashDecrease = lBoostParams.Burnout2CrashDecrease();`.
//    RETAIL DOES NOT: there is no store to +0x130 anywhere in the image --
//    OnCrash @0x822A6378 only READS it (BrnBoostBurnout2.h records the same).
//    Do not re-add the assignment.
//  * Feb-2007 clears mbBoostInterrupted BEFORE the asset load; retail clears it
//    after (0x822C10E4). Semantically identical; asm order kept.
//
// The key handed to the boostparamsasset ctor is LIVE, not dead: the ctor
// @0x822B8C88 never WRITES r4, so it reaches Attrib::FindCollection whole as the
// COLLECTION key (full derivation in boostparamsasset.h). Attrib::StringToKey
// returns u64 and the asm's `mr r4,r3` is a full 64-bit move with no clrldi --
// so there is deliberately NO narrowing cast on it here.
//
// SpeedForMin/MaxEarning are the ONLY two attributes converted int->float
// (lwz + extsw + std/lfd + fcfid + frsp at 0x822C0FBC and 0x822C0FD8); they are
// attrib Int32 and land in the f32 members at +0x1C/+0x20, the two values
// AddBoost @0x822C0E10 lerps between.
// ---------------------------------------------------------------------------
bool
BoostBurnout2::Prepare()
{
    if (!BoostStrategy::Prepare())
    {
        return false;
    }

    char lacBoostKey[KU_BOOST_KEY_TEXT_SIZE];
    rw::core::stdc::ConvertI64ToA(KI_DEFAULT_DANGER_BOOST_PARAMS_GUID, lacBoostKey, 10);

    Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacBoostKey));

    // ---- the 34 tuning params, in member order (== asm store order) ---------
    mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();     // +0x10 <- rec+0x3C
    mfDriftEarning            = lBoostParams.DriftEarning();             // +0x14 <- rec+0x54
    mfAirEarning              = lBoostParams.AirEarning();               // +0x18 <- rec+0x84
    mfSpeedForMinEarning      = static_cast<f32>(lBoostParams.SpeedForMinEarning()); // +0x1C <- rec+0x1C (Int32, fcfid)
    mfSpeedForMaxEarning      = static_cast<f32>(lBoostParams.SpeedForMaxEarning()); // +0x20 <- rec+0x20 (Int32, fcfid)
    mfMaxSpeedBoostModifier   = lBoostParams.MaxSpeedBoostModifier();    // +0x24 <- rec+0x40
    mfTakedownEarning         = lBoostParams.TakedownEarning();          // +0x28 <- rec+0x08
    mfShuntEarning            = lBoostParams.ShuntEarning();             // +0x2C <- rec+0x28
    mfSlamEarning             = lBoostParams.SlamEarning();              // +0x30 <- rec+0x24
    mfNudgeEarning            = lBoostParams.NudgeEarning();             // +0x34 <- rec+0x38
    mfTradingPaintEarning     = lBoostParams.TradingPaintEarning();      // +0x38 <- rec+0x04
    mfGrindingEarning         = lBoostParams.GrindingEarning();          // +0x3C <- rec+0x4C
    mfRubbingEarning          = lBoostParams.RubbingEarning();           // +0x40 <- rec+0x2C
    mfTailgatingEarning       = lBoostParams.TailgatingEarning();        // +0x44 <- rec+0x0C
    mfTrafficCheck            = lBoostParams.TrafficCheck();             // +0x48 <- rec+0x00
    mfBoostSlamStrength       = lBoostParams.BoostSlamStrength();        // +0x4C <- rec+0x6C
    mfHandbrake180Earning     = lBoostParams.Handbrake180Earning();      // +0x50 <- rec+0x48
    mfHandbrake360Earning     = lBoostParams.Handbrake360Earning();      // +0x54 <- rec+0x44
    mfAirSpinEarning          = lBoostParams.AirSpinEarning();           // +0x58 <- rec+0x80
    mfBarrelRollEarning       = lBoostParams.BarrelRollEarning();        // +0x5C <- rec+0x7C
    mfCleanLanding            = lBoostParams.CleanLanding();             // +0x60 <- rec+0x60
    mfFakieLanding            = lBoostParams.FakieLanding();             // +0x64 <- rec+0x50
    mfBoostSpinIncrease       = lBoostParams.BoostSpinIncrease();        // +0x68 <- rec+0x68
    mfComboModifier           = lBoostParams.ComboModifier();            // +0x6C <- rec+0x5C
    mfBurnRateBoost           = lBoostParams.BurnRateBoost();            // +0x70 <- rec+0x64
    mfBoostChainMin           = lBoostParams.BoostChainMin();            // +0x74 <- rec+0x70
    mfBoostOnComing           = lBoostParams.OnComing();                 // +0x78 <- rec+0x34
    mfBeingSlammed            = lBoostParams.BeingSlammed();             // +0x7C <- rec+0x78
    mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();         // +0x80 <- rec+0x14
    mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();        // +0x84 <- rec+0x10
    mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();    // +0x88 <- rec+0x18
    mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning();  // +0x8C <- rec+0x58
    mfBoostChainBonus         = lBoostParams.BoostChainBonus();          // +0x90 <- rec+0x74
    mfOnWrecked               = lBoostParams.OnWrecked();                // +0x94 <- rec+0x30

    // ---- the resets --------------------------------------------------------
    // asm 0x822C10E4..0x822C1104, in emitted order:
    //   stb  r10, 0x138(r31)   stw  r10, 0x13C(r31)   stb  r10, 0xCB(r31)
    //   stfs f0,  0x140(r31)   stb  r10, 0xC3(r31)    stfs f0, 0x144(r31)
    //   stfs f0,  0x148(r31)                          (f0 == flt_82001CC0 == 0.0f)
    // Note that +0x130 mfCrashDecrease and +0x134 mfHiddenBoost are NOT touched.
    mbBoostInterrupted          = false;   // +0x138
    miChainSize                 = 0;       // +0x13C
    mbChainNotifyPending        = false;   // +0x0CB (base)
    mfTimeBasedBoostingBonus    = 0.0f;    // +0x140
    mbIsBoostFull               = false;   // +0x0C3 (base)
    mfContinuousBoostingTimeAdd = 0.0f;    // +0x144
    mfTimeBoosting              = 0.0f;    // +0x148

    // lBoostParams leaves scope here -- the `bl Attrib::Instance::~Instance`
    // at 0x822C1108, immediately before `li r3,1`.
    return true;
}

// ---------------------------------------------------------------------------
// UpdateStuntBoost @ 0x822A6478 -- base vtable slot 48.
//
// Four independent stunt awards, each gated on its own bit of
// lpCompletedStuntAction->muStuntActionComplete and each paid through the
// VIRTUAL AddBoost (base slot 49, vtable +0xC4 -- every one of the four sites is
// `lwz r11,0(r31) ; lwz r11,0xC4(r11) ; mtctr ; bctrl`, never a direct call), so
// BoostBurnout2's own AddBoost @0x822C0E10 speed-lerp + clamp applies to every
// award. An unqualified call on `this` is that virtual dispatch.
//
//   bit 0 (clrlwi  r11,r11,31)      @0x822A64C0
//        -> AddBoost((f32)miCompletedBarrelRolls * mfBarrelRollEarning)
//           the count is an INT: `lwz r11,0x18(r30)` + extsw + fcfid + frsp
//           (0x822A64CC-0x822A64F0) before `fmuls f1,f13,f0` -- f13 is the
//           converted count, f0 is `lfs f0,0x5C(r31)` == mfBarrelRollEarning.
//   bit 1 (rlwinm r11,r11,0,30,30)  @0x822A6504
//        -> AddBoost(mfAirSpinEarning * mfCompletedAirSpinAngle)
//           `lfs f0,0x58(r31)` * `lfs f13,8(r30)`, fmuls f1,f0,f13.
//   bit 2 (rlwinm r11,r11,0,29,29)  @0x822A6534
//        -> AddBoost(mfHandbrake180Earning * mfCompletedHandbreakTurnAngle)
//           `lfs f0,0x50(r31)` * `lfs f13,0xC(r30)`, fmuls f1,f0,f13.
//   bit 3 (rlwinm r11,r11,0,28,28)  @0x822A6564
//        -> AddBoost(mfCleanLanding)   flat, no payload: `lfs f1,0x60(r31)`.
//
// Operand order in each fmuls is preserved exactly as encoded. The bits are NOT
// mutually exclusive -- each `beq` skips only its own block and falls through
// into the next test, so one action can pay all four.
//
// The four masks 1/2/4/8 are BrnPhysics::EStuntActionComplete's BARREL_ROLL /
// AIR_SPIN / HANDBREAK_TURN / CLEANLANDING (DWARF-verbatim enum, already
// recovered in BrnStuntOffencesManagerShared.h) -- the same enum
// CompletedStuntAction::muStuntActionComplete is documented to carry. No names
// are invented here.
//
// The assert is the retail one: expression "lpCompletedStuntAction != NULL",
// file "d:\p4\b5_main\burnout\main\code\gamesource\unity\../World/EntityModules/
// RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp", line 0x304 == 772. It is NOT
// an early-out -- 0x822A64B8 falls straight through into `lwz r11,0(r30)`, so no
// `return` is added after it.
//
// No Feb-2007 counterpart: UpdateStuntBoost did not exist in that drop. This
// body is read entirely from the retail asm.
// ---------------------------------------------------------------------------
void
BoostBurnout2::UpdateStuntBoost(const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpCompletedStuntAction)
{
    CGS_ASSERT(lpCompletedStuntAction != NULL, "lpCompletedStuntAction != NULL");

    if (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_BARREL_ROLL)
    {
        AddBoost(static_cast<f32>(lpCompletedStuntAction->miCompletedBarrelRolls) * mfBarrelRollEarning);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_AIR_SPIN)
    {
        AddBoost(mfAirSpinEarning * lpCompletedStuntAction->mfCompletedAirSpinAngle);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_HANDBREAK_TURN)
    {
        AddBoost(mfHandbrake180Earning * lpCompletedStuntAction->mfCompletedHandbreakTurnAngle);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_CLEANLANDING)
    {
        AddBoost(mfCleanLanding);
    }
}

}   // namespace BrnWorld
