#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"

#include "GameSource/GameState/BrnGameActions.h"                                // BrnGameState::GameStateModuleIO::CompletedStuntAction
#include "GameSource/AttribSys/Generated/classes/boostparamsasset.h"            // Attrib::Gen::boostparamsasset (34 named accessors)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey (u64)
#include "rw/core/stdc/stdc.h"                                                  // rw::core::stdc::ConvertI64ToA
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (the Begin/Fire/EndAssert triple)

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 16.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom only).
//
// Functions in this partfile:
//   BoostBurnout5::Prepare           @ 0x822C1D58   (vtable slot 0)
//   BoostBurnout5::UpdateStuntBoost  @ 0x822A6F98   (vtable slot 48)
//
// Both bodies were re-walked instruction-by-instruction against the ARTIST
// listing for this landing pass (0x822C1D58..0x822C1FB8 and
// 0x822A6F98..0x822A70BC, both dumped end to end).
// ============================================================================

namespace BrnWorld
{

// The boostparamsasset collection GUID BoostBurnout5 tunes itself from. X360
// @0x822C1E1C-0x822C1E28: `lis r3,8 ; ori r3,r3,0xCA0D` == 0x0008CA0D == 576013,
// handed to ConvertI64ToA with radix 10 (`li r5,0xA` @0x822C1E20), so the key
// text is "576013". Sibling GUID: BoostBurnout2 = 576005 (0x0008CA05,
// BoostBurnout2::Prepare @0x822C0F74).
//
// Feb-2007 had no per-subclass GUID at all: the BASE Prepare built the key from
// KI_DEFAULT_BOOST_PARAMS_GUID and cached it in the base member mBoostVaultKey
// (BrnBoostStrategy.cpp:48-51), and the subclass constructed its own instance
// from that member (BrnBoostBurnout5.cpp:200). Retail removed it (see the
// maPad0 note in BrnBoostStrategy.h), moved the whole load down into each
// subclass, and gave each subclass its own GUID.
static const s64 KI_BURNOUT5_BOOST_PARAMS_GUID = 576013;

// ---------------------------------------------------------------------------
// Prepare @ 0x822C1D58 -- vtable slot 0.
//
// Shape (asm walked end to end):
//   0x822C1D74  bl BoostStrategy::Prepare ; clrlwi r11,r3,24 ; cmplwi cr6,r11,0 ;
//               bne -> continue, else `li r3,0 ; b <epilogue>`     # early-out
//   0x822C1DB0..0x822C1E14   the state reset -- 23 stores. f31 is flt_82001CC0
//               == 0.0f (loaded once @0x822C1DA4) and f0 is flt_820138DC ==
//               50.0f (@0x822C1DCC, Hex-Rays: `*(_R31 + 328) = 50.0`); v0 is
//               `vspltisw v0,0` @0x822C1D90, stored with stvx128 over the four
//               Vector3 members at +0x150/+0x160/+0x170/+0x180.
//   0x822C1E18  bl BoostStrategy::UpdateMaxBoost   # r4 == 0 (li r4,0 @0x822C1DAC)
//   0x822C1E1C  r3 = 0x0008CA0D (576013) ; li r5,0xA ; r4 = the 512-byte stack
//               text buffer ; bl rw::core::stdc::ConvertI64ToA
//   0x822C1E34  bl Attrib::StringToKey                # r3 = the FULL u64 key
//   0x822C1E38  mr r4,r3 ; li r5,0 ; r3 = the stack instance ;
//               bl Attrib::Gen::boostparamsasset::boostparamsasset
//   0x822C1E48  lwz r11, <instance>+4                 # Instance::mpAttributeData
//   0x822C1E50..0x822C1F88   34 record loads -> 34 member stores
//   0x822C1F84  stb r30(0), 0xC3(r31)                 # mbIsBoostFull = false
//   0x822C1F8C..0x822C1F94   the three stunt earnings RE-ZEROED (stfs f31)
//   0x822C1F98  bl Attrib::Instance::~Instance        # lBoostParams scope exit
//   0x822C1F9C  li r3,1                               # return true
//
// THE CONSTRUCTOR KEY IS LIVE, NOT DEAD. `mr r4,r3` @0x822C1E38 is a full 64-bit
// move with no clrldi, and the ctor @0x822B8C88 never WRITES r4 -- which is
// exactly why the caller's key reaches Attrib::FindCollection whole and is
// consumed there as the COLLECTION key. Attrib::StringToKey returns u64
// (AttributeKey.h:47) and the boostparamsasset ctor parameter is u64; there is
// no narrowing cast here and there must never be one (a static_cast<u32> makes
// every collection lookup miss and silently serves the zeroed default record).
// Full derivation: boostparamsasset.h and BrnBoostStrategy.h.
//
// RETAIL vs FEB-2007 (BrnBoostBurnout5.cpp:186-210) -- the asm wins on all of
// these:
//  * Feb-2007 calls BoostStrategy::Prepare() and IGNORES its result. Retail
//    EARLY-OUTS on it: `if (!BoostStrategy::Prepare()) return false;` (the
//    byte-narrowing clrlwi + cmplwi + bne at 0x822C1D78-0x822C1D80).
//  * Feb-2007 sets `miBoostLevel = 0`. RETAIL DOES NOT -- there is no store to
//    +0x104 anywhere in this function. (RemoveAllBoostAndChunks @0x822C2410
//    still zeroes it; Prepare leaves whatever SetBoostSegments /
//    SetCarStatBoostLevel put there.) Do not re-add the assignment.
//  * Feb-2007 loads NO tuning attributes here -- its BASE Prepare
//    (BrnBoostStrategy.cpp:48-63) loaded an eleven-attribute shared set.
//    Retail's base Prepare @0x822A5C48 touches only the runtime state block, so
//    all 34 tuning params are loaded HERE, exactly as in BoostBurnout2::Prepare
//    (its own layout-pointer load is @0x822C0FA0).
//  * Feb-2007: `mfBlueCrashDecrease = 0.5f * mfMaxMaxBoost;`. Retail stores the
//    literal 50.0f. It is a LITERAL in the binary, not a fold of anything this
//    build still has: mfMaxMaxBoost was a Feb-2007 *BoostStrategy member*
//    (Feb-2007 BrnBoostStrategy.h:237, filled from the asset's Max() at
//    Feb-2007 BrnBoostStrategy.cpp:53) that retail removed, so nothing here
//    pins a named constant -- do not attribute the 50.0f to KF_MAX_MAX_BOOST or
//    to any other declared symbol. (It is numerically half the 100.0f retail's
//    base Prepare @0x822A5C48 seeds into mfMaxBoost -- `*(a1 + 164) = 100.0`,
//    164 == 0xA4 -- which is consistent, but consistency is not attestation.)
//  * Feb-2007 passes the base member mBoostVaultKey to the boostparamsasset
//    ctor. Retail removed that member and builds the key inline.
//  * Retail zeroes a whole stunt-tracking block Feb-2007 did not have at all
//    (+0x150..+0x1A2: the four Vector3s, the four stunt floats, the three stunt
//    bools) and three extra mode flags (+0x136/+0x137/+0x138).
//
// THE RE-ZERO AT THE TAIL IS NOT A TRANSCRIPTION SLIP -- RE-CONFIRMED against
// the asm for this landing. mfStuntJumpEarning / mfStuntSmashEarning /
// mfStuntBillBoardEarning are loaded from the record at 0x822C1F58 (rec+0x14 ->
// +0x80), 0x822C1F60 (rec+0x10 -> +0x84) and 0x822C1F68 (rec+0x18 -> +0x88), and
// then immediately overwritten with f31 == 0.0f at 0x822C1F8C / 0x822C1F90 /
// 0x822C1F94. BoostBurnout5 (blue-bar "Burnout 5" rules) pays no stunt-element
// boost through the shared earnings; the shared asset still carries the values
// for the other two strategies. Keep BOTH the loads and the re-zero -- the loads
// are what the binary does, and dropping them would be a silent divergence.
//
// SpeedForMin/MaxEarning are the ONLY two attributes converted int->float (lwz +
// extsw + std/lfd + fcfid + frsp at 0x822C1E68-0x822C1E80 and
// 0x822C1E84-0x822C1E9C); they are attrib Int32 and land at member +0x1C/+0x20,
// the offsets AddBoost @0x822C0E10 lerps between.
// ---------------------------------------------------------------------------
bool
BoostBurnout5::Prepare()
{
    if (!BoostStrategy::Prepare())
    {
        return false;
    }

    // ---- BoostBurnout5's own state, plus the two base fields this owns ------
    // (asm 0x822C1DB0-0x822C1E14; the stores are independent, so they are
    //  grouped by member here rather than kept in the compiler's interleaving.)
    mfBoostAmount = 0.0f;                       // +0x0A0 (base), stfs @0x822C1DB0
    mbChainNotifyPending = false;               // +0x0CB (base), stb  @0x822C1DEC

    meBoostMode = E_BOOSTMODE_B3_RED;           // +0x130 stw @0x822C1DC0
    mbLeaveBlueDueToCrash = false;              // +0x134 stb @0x822C1DC8
    mbLeaveBlueDueToInsufficientHidden = false; // +0x135 stb @0x822C1DD8
    mbSwicthBlueToRed = false;                  // +0x136 stb @0x822C1DF4
    mbAddRemoveChunkMode = false;               // +0x137 stb @0x822C1DFC
    mbAllowBoostEarning = false;                // +0x138 stb @0x822C1E04
    mfHiddenBoost = 0.0f;                       // +0x13C stfs @0x822C1DB8
    mbBoostInterrupted = false;                 // +0x140 stb @0x822C1DE0
    miChainSize = 0;                            // +0x144 stw @0x822C1DE8

    // The literal flt_820138DC (see the RETAIL vs FEB-2007 note above).
    mfBlueCrashDecrease = 50.0f;                // +0x148 stfs @0x822C1DD4

    // The four stvx128 stores of a `vspltisw v0,0` register: whole-vector zero.
    mStuntRollInProgress.SetZero();             // +0x150 stvx128 r11=0x150 @0x822C1DF8
    mvPositionLastFrame.SetZero();              // +0x160 stvx128 r10=0x160 @0x822C1DF0
    mvTakeOffAtVec.SetZero();                   // +0x170 stvx128 r9 =0x170 @0x822C1E00
    mvLandAtVec.SetZero();                      // +0x180 stvx128 r8 =0x180 @0x822C1E08

    mfBearingLastFrame = 0.0f;                  // +0x190 stfs @0x822C1DBC
    mfAngleSoFar = 0.0f;                        // +0x194 stfs @0x822C1DC4
    mfTimeElapsed = 0.0f;                       // +0x198 stfs @0x822C1DDC
    mfTimeBoosting = 0.0f;                      // +0x19C stfs @0x822C1DE4
    mbHandbreakTurnAttempting = false;          // +0x1A0 stb @0x822C1E0C
    mbWasJustInTheAir = false;                  // +0x1A1 stb @0x822C1E10
    mbTestForCleanLanding = false;              // +0x1A2 stb @0x822C1E14

    UpdateMaxBoost(false);

    // The stack text buffer the GUID is printed into. 512 bytes is measured, not
    // assumed: the frame is 0x290 and the buffer starts at r1+0x70 (var_220),
    // with the saved-register area beginning at r1+0x270 (var_20) -- exactly
    // 0x200 bytes of room. Same idiom as CgsAttribSys::AttribSysCollectionKey::
    // GetHashKey @0x82805C20 (`char lacTemp[512]`, DWARF cpp:81).
    char lacBoostKey[512];
    rw::core::stdc::ConvertI64ToA(KI_BURNOUT5_BOOST_PARAMS_GUID, lacBoostKey, 10);

    Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacBoostKey));

    // ---- the 34 tuning params, in member order (= asm store order) ----------
    mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();     // +0x10 <- rec+0x3C @0x822C1E50
    mfDriftEarning            = lBoostParams.DriftEarning();             // +0x14 <- rec+0x54 @0x822C1E58
    mfAirEarning              = lBoostParams.AirEarning();               // +0x18 <- rec+0x84 @0x822C1E60
    mfSpeedForMinEarning      = static_cast<f32>(lBoostParams.SpeedForMinEarning()); // +0x1C <- rec+0x1C (Int32, fcfid @0x822C1E78)
    mfSpeedForMaxEarning      = static_cast<f32>(lBoostParams.SpeedForMaxEarning()); // +0x20 <- rec+0x20 (Int32, fcfid @0x822C1E94)
    mfMaxSpeedBoostModifier   = lBoostParams.MaxSpeedBoostModifier();    // +0x24 <- rec+0x40 @0x822C1EA0
    mfTakedownEarning         = lBoostParams.TakedownEarning();          // +0x28 <- rec+0x08 @0x822C1EA8
    mfShuntEarning            = lBoostParams.ShuntEarning();             // +0x2C <- rec+0x28 @0x822C1EB0
    mfSlamEarning             = lBoostParams.SlamEarning();              // +0x30 <- rec+0x24 @0x822C1EB8
    mfNudgeEarning            = lBoostParams.NudgeEarning();             // +0x34 <- rec+0x38 @0x822C1EC0
    mfTradingPaintEarning     = lBoostParams.TradingPaintEarning();      // +0x38 <- rec+0x04 @0x822C1EC8
    mfGrindingEarning         = lBoostParams.GrindingEarning();          // +0x3C <- rec+0x4C @0x822C1ED0
    mfRubbingEarning          = lBoostParams.RubbingEarning();           // +0x40 <- rec+0x2C @0x822C1ED8
    mfTailgatingEarning       = lBoostParams.TailgatingEarning();        // +0x44 <- rec+0x0C @0x822C1EE0
    mfTrafficCheck            = lBoostParams.TrafficCheck();             // +0x48 <- rec+0x00 @0x822C1EE8
    mfBoostSlamStrength       = lBoostParams.BoostSlamStrength();        // +0x4C <- rec+0x6C @0x822C1EF0
    mfHandbrake180Earning     = lBoostParams.Handbrake180Earning();      // +0x50 <- rec+0x48 @0x822C1EF8
    mfHandbrake360Earning     = lBoostParams.Handbrake360Earning();      // +0x54 <- rec+0x44 @0x822C1F00
    mfAirSpinEarning          = lBoostParams.AirSpinEarning();           // +0x58 <- rec+0x80 @0x822C1F08
    mfBarrelRollEarning       = lBoostParams.BarrelRollEarning();        // +0x5C <- rec+0x7C @0x822C1F10
    mfCleanLanding            = lBoostParams.CleanLanding();             // +0x60 <- rec+0x60 @0x822C1F18
    mfFakieLanding            = lBoostParams.FakieLanding();             // +0x64 <- rec+0x50 @0x822C1F20
    mfBoostSpinIncrease       = lBoostParams.BoostSpinIncrease();        // +0x68 <- rec+0x68 @0x822C1F28
    mfComboModifier           = lBoostParams.ComboModifier();            // +0x6C <- rec+0x5C @0x822C1F30
    mfBurnRateBoost           = lBoostParams.BurnRateBoost();            // +0x70 <- rec+0x64 @0x822C1F38
    mfBoostChainMin           = lBoostParams.BoostChainMin();            // +0x74 <- rec+0x70 @0x822C1F40
    mfBoostOnComing           = lBoostParams.OnComing();                 // +0x78 <- rec+0x34 @0x822C1F48
    mfBeingSlammed            = lBoostParams.BeingSlammed();             // +0x7C <- rec+0x78 @0x822C1F50
    mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();         // +0x80 <- rec+0x14 @0x822C1F58
    mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();        // +0x84 <- rec+0x10 @0x822C1F60
    mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();    // +0x88 <- rec+0x18 @0x822C1F68
    mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning();  // +0x8C <- rec+0x58 @0x822C1F70
    mfBoostChainBonus         = lBoostParams.BoostChainBonus();          // +0x90 <- rec+0x74 @0x822C1F78
    mfOnWrecked               = lBoostParams.OnWrecked();                // +0x94 <- rec+0x30 @0x822C1F80

    mbIsBoostFull = false;                      // +0x0C3 (base), stb @0x822C1F84

    // Burnout 5 rules pay nothing for stunt elements through the shared
    // earnings: the three values are read from the asset above and then
    // discarded (asm 0x822C1F8C-0x822C1F94, three stfs of the same f31 == 0.0f).
    mfStuntJumpEarning      = 0.0f;             // +0x80
    mfStuntSmashEarning     = 0.0f;             // +0x84
    mfStuntBillBoardEarning = 0.0f;             // +0x88

    return true;
}

// ---------------------------------------------------------------------------
// UpdateStuntBoost @ 0x822A6F98 -- vtable slot 48 (the base body is overridden
// by all three strategies).
//
// Four independent stunt awards, each gated on its own bit of
// lpCompletedStuntAction->muStuntActionComplete and each paid through the
// VIRTUAL AddBoost (base slot 49, vtable +0xC4 -- `lwz r10,0(r31) ;
// lwz r10,0xC4(r10) ; mtctr ; bctrl`, never a direct call), so BoostBurnout5
// inherits the base AddBoost @0x822C0E10 speed multiplier and [0, mfMaxBoost]
// clamp on every award:
//
//   bit 0 (clrlwi r11,r11,31 @0x822A6FE0)
//        -> AddBoost((f32)miCompletedBarrelRolls * mfBarrelRollEarning)
//           the count is an INT32: `lwz r11,0x18(r30)` @0x822A6FEC then extsw /
//           std / lfd / fcfid / frsp before the fmuls @0x822A7014.
//   bit 1 (rlwinm r11,r11,0,30,30 @0x822A7024)
//        -> AddBoost(mfAirSpinEarning * mfCompletedAirSpinAngle)
//           lfs f0,0x58(r31) / lfs f13,8(r30) / fmuls f1,f0,f13.
//   bit 2 (rlwinm r11,r11,0,29,29 @0x822A7054)
//        -> AddBoost(mfHandbrake180Earning * mfCompletedHandbreakTurnAngle)
//           lfs f0,0x50(r31) / lfs f13,0xC(r30) / fmuls f1,f0,f13.
//   bit 3 (rlwinm r11,r11,0,28,28 @0x822A7084)
//        -> AddBoost(mfCleanLanding)      # flat, lfs f1,0x60(r31), no payload
//
// The bits are NOT mutually exclusive -- each `beq` skips only its own block and
// falls through into the next test, so one completed stunt can pay all four. The
// bit-3 block is the function's tail (`bctrl` @0x822A70A4 then straight into the
// epilogue), which is why Hex-Rays renders it as a separate `return`; it is the
// same fall-through shape as the other three.
//
// Operand order in each fmuls is preserved exactly as encoded (bit 0:
// count * earning; bits 1 and 2: earning * angle). mfBarrelRollEarning /
// mfAirSpinEarning / mfHandbrake180Earning / mfCleanLanding are the base tuning
// params at +0x5C / +0x58 / +0x50 / +0x60 (BrnBoostStrategy.h), which is exactly
// where Prepare above stores rec+0x7C / rec+0x80 / rec+0x48 / rec+0x60.
//
// The assert is the retail one, verbatim: text "lpCompletedStuntAction != NULL"
// (aLpcompletedstu_0), file aDP4B5MainBurno_532, line immediate `li r5, 0x33E`
// == 830. It is NOT an early-out -- `bne cr6, loc_822A6FDC` @0x822A6FB8 skips
// only the assert triple, and the assert path falls straight through EndAssert
// into the same dereferences (a null pointer would fault), so no `return` is
// added here. Identical shape and constants to BoostBurnout2::UpdateStuntBoost
// @0x822A6478 (assert line 772) and BoostBurnout3::UpdateStuntBoost @0x822A6708.
//
// No Feb-2007 counterpart: neither UpdateStuntBoost nor CompletedStuntAction
// exists anywhere in that drop, so this body is read entirely from the retail
// asm.
//
// BIT NAMES DELIBERATELY NOT INVENTED HERE. The four mask VALUES are
// X360-attested (bits 0/1/2/3, from the clrlwi/rlwinm encodings) and each ROLE
// is attested by which tuning parameter it pays. BrnGameActions.h:760-765
// records that muStuntActionComplete is the physics detector's bitfield and
// points at BrnPhysics::EStuntActionComplete (BrnStuntOffencesManagerShared.h)
// as its enumerator home; that header is not included by this TU, so the masks
// are spelled as literals with their role in a comment, exactly as that note
// permits. Tempting near-miss to reject: BrnGameStateTypes.h's EStuntType IS
// used as a bit position elsewhere, but mapping it here would make bit 0
// (E_STUNT_TYPE_SPIN) pay out barrel rolls, contradicting the tuning parameter
// each bit selects -- it is a DIFFERENT mask.
// ---------------------------------------------------------------------------
void
BoostBurnout5::UpdateStuntBoost(const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpCompletedStuntAction)
{
    // BrnBoostBurnout5.cpp:830 on the console (the assert's line immediate is
    // 0x33E @0x822A6FC4); reported, then execution continues into the
    // dereferences below.
    CGS_ASSERT(lpCompletedStuntAction != nullptr, "lpCompletedStuntAction != NULL");

    if (lpCompletedStuntAction->muStuntActionComplete & 0x1u)        // barrel roll(s)
    {
        AddBoost(static_cast<f32>(lpCompletedStuntAction->miCompletedBarrelRolls) * mfBarrelRollEarning);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & 0x2u)        // air spin
    {
        AddBoost(mfAirSpinEarning * lpCompletedStuntAction->mfCompletedAirSpinAngle);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & 0x4u)        // handbrake turn
    {
        AddBoost(mfHandbrake180Earning * lpCompletedStuntAction->mfCompletedHandbreakTurnAngle);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & 0x8u)        // clean landing
    {
        AddBoost(mfCleanLanding);
    }
}

} // namespace BrnWorld
