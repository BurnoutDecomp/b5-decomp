// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 11.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::ApplyUpdate  @ 0x822C1880   (base vtable slot 1)
//
// This is the body that was parked as
// scratchpad/waveP/parked/BoostBurnout3_wP_1.parked.cpp (workflow repo). Both
// of its parked blockers are gone:
//   * BrnBoostBurnout3.h landed (base 0x130 + four own members at +0x130..+0x13C).
//   * Attrib::Gen::boostparamsasset now exposes the 34 NAMED attribute accessors
//     (DecFIGS DWARF boostparamsasset.h:73-307), so the tuning record is read
//     through the generated API instead of a raw offset poke at the layout block.
//     The parked draft asked for `using Instance::GetLayoutPointer;` and then
//     indexed the block itself; that was rejected -- it pushes the console-vs-host
//     +0x04/+0x08 mpAttributeData widening into a consumer TU and violates
//     AGENTS.md "NO RAW OFFSET POINTER HACKS". The offset knowledge stays in
//     boostparamsasset.h.
//
// TWO DEFECTS IN THE PARKED DRAFT, FIXED HERE (both re-derived from the asm):
//   1. `static_cast<u32>(Attrib::StringToKey(lacGuidText))` -- a LIVE 64-bit
//      truncation. Attrib::StringToKey returns u64 (AttributeKey.h:43/47;
//      @0x82805828 tail-calls the lookup8 hash and returns r3 whole), the console
//      moves it with a full 64-bit `mr r4, r3` @0x822C1A5C (no clrldi), and the
//      boostparamsasset ctor hands r4 straight to Attrib::FindCollection as the
//      COLLECTION key. Narrowed, every lookup misses and all 34 parameters come
//      back from the ctor's zeroed DefaultDataArea(0x88).
//   2. The draft's banner claimed "the key argument is DEAD -- the ctor resolves
//      its collection from the boostparamsasset CLASS key". FALSE, and it is the
//      claim that justified defect 1. The ctor @0x822B8C88 never WRITES r4 --
//      which is exactly why the caller's key is LIVE: between the entry and
//      `bl Attrib__FindCollection` @0x822B8CB8 it writes only r31 (this), r30
//      (owner) and r3 (the whole-doubleword class key, lis/ori + insrdi), so r4
//      reaches FindCollection holding the caller's key. Hex-Rays rendering a
//      one-argument call is an artifact of the un-written register.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"

#include "GameSource/AttribSys/Generated/classes/boostparamsasset.h"            // Attrib::Gen::boostparamsasset
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey
#include "rw/core/stdc/stdc.h"                                                  // rw::core::stdc::ConvertI64ToA

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// ApplyUpdate @ 0x822C1880 -- base vtable slot 1, dispatched by
// BoostStrategy::Update @0x822F8160.
//
// SIGNATURE from the asm, not the pseudocode: `(this, f1)` only. r3 = this
// (`mr r31, r3` @0x822C1894), the time step arrives in f1 and is parked in f31
// for the whole body (`fmr f31, f1` @0x822C1898). No GPR past r3 is read on
// entry, so the Feb-2007 second parameter (a CarOffenceManager*) is gone in
// retail. (PPC float-arg ABI: the f32 travels in an FPR and skips its GPR slot,
// which is why there is no r4 use to look for.)
//
// SHAPE -- four earning sources, then the burn step, then an unconditional
// re-read of the whole 34-value boostparamsasset record.
//
// RETAIL vs FEB-2007 (all four asm-driven):
//   (a) a FOURTH earning source, tailgating (slot 27 @0x822C1954, scaled by
//       mfTailgatingEarning +0x44). Feb-2007 has only air/drift/oncoming.
//   (b) the boost-request test gained a MINIMUM-BOOST-TIME latch: once boosting,
//       boost keeps running while mfTimeBoosting is under 1.25 s even after the
//       request drops (0x822C199C-0x822C19B8). Feb-2007 drops boost the instant
//       mbBoostRequested goes false.
//   (c) the drain rate is no longer a flat constant: it is the per-car
//       mfCurrentCarBoostLossLevel (+0xF0, seeded by SetCarStatBoostLevel),
//       falling back to the attrib mfBurnRateBoost (+0x70) when that is exactly
//       zero (0x822C1A14-0x822C1A28).
//   (d) the whole boostparamsasset record is re-read at the END OF EVERY UPDATE
//       (loc_822C1A40, reached from both the boosting and the not-boosting
//       paths). Not in Feb-2007 at all, and the reason this function carries a
//       0x290-byte stack frame.
//
// NaN polarity, taken from the branch senses rather than transliterated:
//   * `fcmpu f13(mfTimeBoosting), 1.25 ; blt` @0x822C19AC -- taken only when
//     ORDERED-less, so a NaN clears the latch: `mfTimeBoosting < 1.25f`.
//   * `fcmpu f12(mfBoostAmount), 0.0 ; bgt` @0x822C19F0 -- ordered-greater only,
//     so a NaN takes the "no boost left" path: `mfBoostAmount > 0.0f`.
//   * `fcmpu f13(mfCurrentCarBoostLossLevel), 0.0 ; bne` @0x822C1A20 -- `bne` is
//     taken when UNORDERED too, so a NaN keeps the per-car rate. C++ `==` is
//     false for NaN, so `if (rate == 0.0f) rate = fallback;` matches.
//   * `fcmpu f13(new mfBoostAmount), 0.0 ; bge` @0x822C1A34 -- `bge` tests LT==0
//     and is taken when unordered, so a NaN is NOT clamped. C++ `<` is false for
//     NaN, so `if (amount < 0.0f) amount = 0.0f;` matches.
// All four match the plain C++ spelling used below.
// ---------------------------------------------------------------------------
void BoostBurnout3::ApplyUpdate(f32 lfTimeStep)
{
    // ---- earning ---------------------------------------------------------
    // Each source is `bctrl` through the base getter's slot, then
    // `lfs <param> ; fmuls f1, f0, f31 ; bctrl` through slot 49 (AddBoost,
    // console vtable +0xC4).
    if (IsInAir())                                          // slot 20, +0x50 @0x822C18A0
    {
        AddBoost(mfAirEarning * lfTimeStep);                // lfs f0, 0x18(r31)
    }

    if (IsDrifting())                                       // slot 21, +0x54 @0x822C18DC
    {
        AddBoost(mfDriftEarning * lfTimeStep);              // lfs f0, 0x14(r31)
    }

    if (IsOncoming())                                       // slot 25, +0x64 @0x822C1918
    {
        AddBoost(mfBoostOnComing * lfTimeStep);             // lfs f0, 0x78(r31)
    }

    if (IsTailgating())                                     // slot 27, +0x6C @0x822C1954
    {
        AddBoost(mfTailgatingEarning * lfTimeStep);         // lfs f0, 0x44(r31)
    }

    // ---- burning ---------------------------------------------------------
    // `lbz r10, 0xC5(r31)` @0x822C1988 -- mbBoosting is sampled ONCE into r10,
    // and that pre-update value is what BOTH the latch test @0x822C1994 and the
    // "boost has just started" test @0x822C1A00 use. (No store to +0xC5 can
    // precede either test: the two stores at 0x822C19DC / 0x822C19F8 both branch
    // straight to loc_822C1A40.)
    const bool lbWasBoosting = mbBoosting;

    // The minimum-boost-time latch (r11 @0x822C199C-0x822C19B8): already
    // boosting, and the current burst is younger than 1.25 s.
    //
    // 1.25f is flt_820147F8, loaded by all three strategies' ApplyUpdate
    // (B2 @0x822C1340, B3 here, B5 @0x822C2170) -- consistent with the shared
    // BrnBoostStrategy.h-level KF_MIN_BOOST_TIME (h:49), whose value the
    // BrnBoostStrategy.cpp wave has not pinned. It is NOT spelled as that name
    // here because the .rdata float region is not a per-header constant array:
    // flt_820147E8, four slots earlier, is a BrnBoostBurnout3.cpp-local constant
    // (0.15f; loaded by OnEndCrashPlay @0x822A6B40/0x822A6B48 -- landed as a
    // literal in BoostBurnout3_wP_02.cpp -- and again by SetCarStatBoostLevel
    // @0x822C1C44, BoostBurnout3_wP_06.cpp), so position in that pool identifies
    // nothing. (RemoveAllBoostAndChunks @0x822A6B68 never touches flt_820147E8;
    // it loads flt_82001CC0 == 0.0f.)
    // The literal the asm actually loads is written instead of asserting an
    // identification this TU cannot prove.
    const bool lbWithinMinBoostTime = lbWasBoosting && (mfTimeBoosting < 1.25f);

    if (mbBoostRequested || lbWithinMinBoostTime)           // lbz 0xC7 ; bne / clrlwi r11
    {
        if (mbInfiniteBoost)                                // lbz 0xC4 @0x822C19D0
        {
            mbBoosting = true;                              // stb r8(=1), 0xC5
        }
        else if (mfBoostAmount > 0.0f)                      // lfs 0xA0 ; fcmpu ; bgt
        {
            // A fresh burst restarts the timer (`cmplwi r10,0 ; bne` @0x822C1A00
            // skips the zero-store when we were already boosting).
            if (!lbWasBoosting)
            {
                mfTimeBoosting = 0.0f;                      // stfs f0(=0.0), 0x13C
            }
            mfTimeBoosting += lfTimeStep;                   // lfs/fadds f31/stfs 0x13C
            mbBoosting = true;                              // stb r8(=1), 0xC5

            // Per-car drain rate, with the attrib default as the fallback.
            f32 lfBoostBurnRate = mfCurrentCarBoostLossLevel;   // lfs f13, 0xF0
            if (lfBoostBurnRate == 0.0f)                        // fcmpu ; bne
            {
                lfBoostBurnRate = mfBurnRateBoost;              // lfs f13, 0x70
            }

            // 0x822C1A2C fnmsubs f13, f13, f31, f12 (== mfBoostAmount - rate*dt)
            // + 0x822C1A30 stfs 0xA0 + 0x822C1A34 fcmpu / `bge` @0x822C1A38 /
            // 0x822C1A3C stfs 0.0. That subtract-then-clamp-at-zero pair IS
            // BoostStrategy::RemoveBoost (no standalone X360 symbol -- inlined at
            // every call site). Reversing the inline restores the call, matching
            // the landed sibling partfiles BoostBurnout2_wP_11 and _wP_05.
            // NaN note unchanged: RemoveBoost's own `<` is false for NaN, exactly
            // as `bge` is taken when the compare is unordered.
            RemoveBoost(lfBoostBurnRate * lfTimeStep);
        }
        else
        {
            mbBoosting = false;                             // stb r9(=0), 0xC5 (loc_822C19F8)
        }
    }
    else
    {
        mbBoosting = false;                                 // stb r9(=0), 0xC5 (loc_822C19F8)
    }

    // ---- re-read the boost tuning parameters (loc_822C1A40) ---------------
    // Unconditional, every update. Print the asset GUID as decimal text, hash
    // the text into a collection key, construct a stack boostparamsasset over
    // it, then copy the 34-slot (0x88-byte) record into the base's 34 tuning
    // members. The same block runs in BoostBurnout3::Prepare @0x822C1680.
    //
    // 576011 == 0x8CA0B, staged as `lis r3,8 ; ori r3,r3,0xCA0B` @0x822C1A40/4C.
    // The DecFIGS DWARF names it BrnWorld::KI_DEFAULT_AGGRESSION_BOOST_PARAMS_GUID
    // (BrnBoostBurnout3.cpp:43, `const int64_t = 576011`). That namespace-scope
    // definition belongs to whichever partfile lands Prepare -- spelling it here
    // too would duplicate it -- so the literal is written out.
    //
    // The 512-byte text buffer is not DWARF-attested for THIS function (the
    // dwarfdump for BrnBoostBurnout3.cpp carries only the GUID constant). The
    // console frame bounds it to 512..544 bytes (buffer at sp+0x70 inside a
    // 0x290 frame, minus the callee linkage/parameter-save area at the top), and
    // 512 is the size the DWARF gives the identical ConvertI64ToA-then-hash site
    // CgsAttribSys::AttribSysCollectionKey::GetHashKey (DWARF cpp:81, already
    // landed as `char lacTemp[512]`).
    char lacGuidText[512];                                  // sp+0x70
    rw::core::stdc::ConvertI64ToA(576011, lacGuidText, 10);

    // `bl Attrib__StringToKey` @0x822C1A58 -> `mr r4, r3` @0x822C1A5C: the FULL
    // 64-bit hash, moved whole (no clrldi), is the ctor's collection key.
    // `li r5, 0` @0x822C1A60 is the owner.
    Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacGuidText), nullptr);

    // The console loads the layout block once (`lwz r11, 4(inst)` @0x822C1A6C --
    // Attrib::Instance::mpAttributeData) and then reads it at fixed offsets.
    // Those offsets live in boostparamsasset.h, one per named accessor; the
    // record-offset -> member mapping below is the store order of
    // 0x822C1A70..0x822C1BA8, member offset +0x10..+0x94 ascending, 34 for 34.
    mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();    // rec+0x3C -> +0x10
    mfDriftEarning            = lBoostParams.DriftEarning();            // rec+0x54 -> +0x14
    mfAirEarning              = lBoostParams.AirEarning();              // rec+0x84 -> +0x18

    // The ONLY two Int32 attributes in the record -- the only two slots the
    // console routes through lwz/extsw/fcfid/frsp instead of lfs
    // (0x822C1A88-0x822C1ABC), i.e. a signed int-to-float conversion.
    mfSpeedForMinEarning      = static_cast<f32>(lBoostParams.SpeedForMinEarning()); // rec+0x1C -> +0x1C
    mfSpeedForMaxEarning      = static_cast<f32>(lBoostParams.SpeedForMaxEarning()); // rec+0x20 -> +0x20

    mfMaxSpeedBoostModifier   = lBoostParams.MaxSpeedBoostModifier();   // rec+0x40 -> +0x24
    mfTakedownEarning         = lBoostParams.TakedownEarning();         // rec+0x08 -> +0x28
    mfShuntEarning            = lBoostParams.ShuntEarning();            // rec+0x28 -> +0x2C
    mfSlamEarning             = lBoostParams.SlamEarning();             // rec+0x24 -> +0x30
    mfNudgeEarning            = lBoostParams.NudgeEarning();            // rec+0x38 -> +0x34
    mfTradingPaintEarning     = lBoostParams.TradingPaintEarning();     // rec+0x04 -> +0x38
    mfGrindingEarning         = lBoostParams.GrindingEarning();         // rec+0x4C -> +0x3C
    mfRubbingEarning          = lBoostParams.RubbingEarning();          // rec+0x2C -> +0x40
    mfTailgatingEarning       = lBoostParams.TailgatingEarning();       // rec+0x0C -> +0x44
    mfTrafficCheck            = lBoostParams.TrafficCheck();            // rec+0x00 -> +0x48
    mfBoostSlamStrength       = lBoostParams.BoostSlamStrength();       // rec+0x6C -> +0x4C
    mfHandbrake180Earning     = lBoostParams.Handbrake180Earning();     // rec+0x48 -> +0x50
    mfHandbrake360Earning     = lBoostParams.Handbrake360Earning();     // rec+0x44 -> +0x54
    mfAirSpinEarning          = lBoostParams.AirSpinEarning();          // rec+0x80 -> +0x58
    mfBarrelRollEarning       = lBoostParams.BarrelRollEarning();       // rec+0x7C -> +0x5C
    mfCleanLanding            = lBoostParams.CleanLanding();            // rec+0x60 -> +0x60
    mfFakieLanding            = lBoostParams.FakieLanding();            // rec+0x50 -> +0x64
    mfBoostSpinIncrease       = lBoostParams.BoostSpinIncrease();       // rec+0x68 -> +0x68
    mfComboModifier           = lBoostParams.ComboModifier();           // rec+0x5C -> +0x6C
    mfBurnRateBoost           = lBoostParams.BurnRateBoost();           // rec+0x64 -> +0x70
    mfBoostChainMin           = lBoostParams.BoostChainMin();           // rec+0x70 -> +0x74
    mfBoostOnComing           = lBoostParams.OnComing();                // rec+0x34 -> +0x78
    mfBeingSlammed            = lBoostParams.BeingSlammed();            // rec+0x78 -> +0x7C
    mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();        // rec+0x14 -> +0x80
    mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();       // rec+0x10 -> +0x84
    mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();   // rec+0x18 -> +0x88
    mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning(); // rec+0x58 -> +0x8C
    mfBoostChainBonus         = lBoostParams.BoostChainBonus();         // rec+0x74 -> +0x90
    mfOnWrecked               = lBoostParams.OnWrecked();               // rec+0x30 -> +0x94

    // `bl Attrib__Instance___Instance` @0x822C1BAC -- lBoostParams leaving scope.
}

}   // namespace BrnWorld
