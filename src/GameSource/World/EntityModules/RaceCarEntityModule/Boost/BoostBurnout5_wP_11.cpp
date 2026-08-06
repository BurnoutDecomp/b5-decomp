#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"

#include "GameSource/AttribSys/Generated/classes/boostparamsasset.h"                 // Attrib::Gen::boostparamsasset
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"   // Attrib::StringToKey
#include "rw/core/stdc/stdc.h"                                                       // rw::core::stdc::ConvertI64ToA
#include "types.hpp"                                                                 // f32 / s64

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 11.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Functions in this partfile:
//   BoostBurnout5::ApplyUpdate  @ 0x822C1FC8   (vtable slot 1)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape and the GUID constant; Feb-2007 is idiom only --
// and it has drifted badly here, see the DIVERGENCES list on the function).
//
// Every offset quoted below was re-derived for this file from the 0x822C1FC8
// listing and cross-checked against the member map in BrnBoostStrategy.h /
// BrnBoostBurnout5.h. Console vtable slots are 4 bytes, so the `lwz 0xNN(vptr)`
// displacements divide by 4 to give the slot numbers the two headers enumerate.
// ============================================================================

namespace BrnWorld
{

// The boostparams asset GUID this strategy resolves its tuning collection from.
// NAME, TYPE and VALUE are all DecFIGS-attested verbatim -- the DWARF dump of
// BrnBoostBurnout5.cpp:46 declares
//   const int64_t KI_DEFAULT_STUNT_BOOST_PARAMS_GUID = 576013;
// and the X360 stages exactly that value inline at 0x822C225C/0x822C2268
// (`lis r3,8` + `ori r3,r3,0xCA0D` == 0x0008CA0D == 576013). int64_t is also
// what ConvertI64ToA takes. BoostBurnout2 uses 576005 and BoostBurnout3 576011;
// each strategy has its own collection. Same spelling and placement as the
// sibling BoostBurnout3_wP_16.cpp:48.
const s64 KI_DEFAULT_STUNT_BOOST_PARAMS_GUID = 576013;

// ---------------------------------------------------------------------------
// ApplyUpdate @ 0x822C1FC8  (vtable slot 1; PURE in the base. Its caller is the
// non-virtual BoostStrategy::Update @0x822F8130, which passes ONLY the time step
// -- the retail signature has no second parameter.)
//
// -- earning (0x822C1FE4-0x822C2108) ---------------------------------------
// Five identical `lwz vptr / lwz 0xNN(vptr) / bctrl` state queries, each gating
// `lfs <param> / fmuls f1,f0,f30 / lwz 0xC4(vptr) / bctrl`, i.e.
// AddBoost(param * lfTimeStep). Slot displacement -> base-header slot, and the
// member offset each one scales:
//     +0x50 -> slot 20 IsInAir      * mfAirEarning        (+0x18)
//     +0x54 -> slot 21 IsDrifting   * mfDriftEarning       (+0x14)
//     +0x58 -> slot 22 IsSpinning   * mfAirSpinEarning     (+0x58)
//     +0x64 -> slot 25 IsOncoming   * mfBoostOnComing      (+0x78)
//     +0x6C -> slot 27 IsTailgating * mfTailgatingEarning  (+0x44)
// AddBoost is +0xC4 == slot 49, the base's protected earn helper; BoostBurnout5
// does not override it, so a plain call here is the same virtual dispatch.
// (The four Feb-2007 pairs -- air/drift/oncoming/tailgating -- pair with exactly
// the tuning params their names predict, which independently corroborates BOTH
// the 50-slot vtable order AND the 34-param member order. The spin pair is new.)
//
// -- burn (0x822C2110-0x822C215C) -------------------------------------------
//   0x822C2110  lbz    r10, 0xC5(r31)       # mbBoosting -- kept in r10 and
//                                           #   re-used at 0x822C2168/0x822C21DC
//   0x822C2118  lfs    f31, flt_82001CC0    # 0.0f
//   0x822C211C  beq    cr6, loc_822C2160    # not boosting -> no burn
//   0x822C2120  lbz    r11, 0xC4(r31)       # mbInfiniteBoost
//   0x822C2128  bne    cr6, loc_822C2160    # infinite -> no burn
//   0x822C212C  lfs    f0,  0xF0(r31)       # mfCurrentCarBoostLossLevel
//   0x822C2134  bne    cr6, loc_822C2148    # != 0.0f -> the loss level wins
//   0x822C2138  lfs    f0,  0xA0(r31)       # mfBoostAmount
//   0x822C213C  lfs    f13, 0x70(r31)       # mfBurnRateBoost
//   0x822C2140  fnmsubs f0, f13, f30, f0    # = mfBoostAmount - mfBurnRateBoost*dt
//   0x822C2148  lfs    f13, 0xA0(r31)
//   0x822C214C  fnmsubs f0, f0, f30, f13    # = mfBoostAmount - lossLevel*dt
//   0x822C2150  stfs   f0,  0xA0(r31)
//   0x822C2158  bge    cr6, loc_822C2160
//   0x822C215C  stfs   f31, 0xA0(r31)       # clamp at 0
//
// -- the request gate (0x822C2168-0x822C2258) --------------------------------
//   0x822C2168  cmplwi cr6, r10, 0          # mbBoosting (the pre-burn read; the
//                                           #   burn writes only mfBoostAmount,
//                                           #   so a plain member read is equal)
//   0x822C2174  lfs    f13, 0x19C(r31)      # mfTimeBoosting
//   0x822C2178  lfs    f0,  flt_820147F8    # 1.25f
//   0x822C2184  blt    cr6, loc_822C218C    # r11 = mbBoosting && (time < 1.25f)
//   0x822C218C  lbz    r9,  0xC7(r31)       # mbBoostRequested
//   0x822C2194  bne    cr6, loc_822C21C4    # requested -> serve
//   0x822C21A0  bne    cr6, loc_822C21C4    # or still inside the minimum run
//   0x822C21A4  lfs    f0,  0xA0(r31)       # -- NOT-SERVED arm --
//   0x822C21A8  stb    r30(1), 0x140(r31)   # mbBoostInterrupted = true
//   0x822C21AC  lfs    f13, 0xA4(r31)       # mfMaxBoost
//   0x822C21B0  stb    r29(0), 0xC5(r31)    # mbBoosting = false
//   0x822C21B8  beq    cr6, loc_822C225C    # bar full -> keep the mode
//   0x822C21BC  stb    r30(1), 0x136(r31)   # mbSwicthBlueToRed = true
//   0x822C21C4  lwz    r11, 0x130(r31)      # -- SERVED arm -- meBoostMode
//   0x822C21C8  cmpwi  cr6, r11, 0          # tested against 0 == E_BOOSTMODE_B3_RED
//   0x822C21CC  bne    cr6, loc_822C220C    #   -> anything else takes the blue arm
//   0x822C21D0  lfs    f0,  0xA0(r31)       # -- RED --
//   0x822C21D8  ble    cr6, loc_822C2204    # no boost left -> stop
//   0x822C21DC  cmplwi cr6, r10, 0          # already boosting?
//   0x822C21E0  bne    cr6, loc_822C21FC
//   0x822C21E4  lfs    f13, 0x100(r31)      # mfMinBoostAllowedAmount
//   0x822C21EC  ble    cr6, loc_822C21F4
//   0x822C21F0  stb    r30(1), 0xC5(r31)    # mbBoosting = true
//   0x822C21F4  stfs   f31, 0x19C(r31)      # mfTimeBoosting = 0.0f
//   0x822C21F8  b      loc_822C2250
//   0x822C21FC  stb    r30(1), 0xC5(r31)    # (already boosting) mbBoosting = true
//   0x822C2200  b      loc_822C2250
//   0x822C2204  stb    r29(0), 0xC5(r31)    # mbBoosting = false; NO time tick
//   0x822C2208  b      loc_822C225C
//   0x822C220C  mr     r3, r31              # -- BLUE --
//   0x822C2210  bl     BoostBurnout5::BlueModeRequestBoost
//   0x822C2214  lbz    r11, 0xC5(r31)       # mbBoosting (RE-read after the call)
//   0x822C221C  bne    cr6, loc_822C2224
//   0x822C2220  stfs   f31, 0x19C(r31)      # mfTimeBoosting = 0.0f
//   0x822C2228  cmplwi cr6, r11, 0          # the returned bool
//   0x822C222C  beq    cr6, loc_822C224C
//   0x822C2230  lfs    f0,  0xA0(r31)
//   0x822C223C  bgt    cr6, loc_822C2244    # mbBoosting = (mfBoostAmount > 0.0f)
//   0x822C2244  stb    r11, 0xC5(r31)
//   0x822C224C  stb    r30(1), 0x136(r31)   # mbSwicthBlueToRed = true
//   0x822C2250  lfs    f0,  0x19C(r31)      # loc_822C2250: mfTimeBoosting += dt
//   0x822C2258  stfs   f0,  0x19C(r31)
//   0x822C225C  ...                         # loc_822C225C: the param reload
//
// DIVERGENCES FROM Feb-2007 (BrnBoostBurnout5.cpp:367-423) -- the asm wins,
// every one of them walked against the listing above:
//   * the retail signature is (f32) only -- Feb-2007's `CarOffenceManager*`
//     second parameter is gone (the base's slot-1 call @0x822F8160 passes only
//     f1, and this prologue keeps only r3 and f1).
//   * Feb-2007 earns through the helper `AddBoostB5`, which diverted blue-mode
//     gains into mfHiddenBoost. That helper does NOT exist in retail (absent
//     from the DecFIGS method list and from the X360 ledger); all five earning
//     sites dispatch slot 49, i.e. the base AddBoost @0x822C0E10, directly.
//   * retail adds a fifth earning source, IsSpinning * mfAirSpinEarning.
//   * the burn is no longer a flat `RemoveBoost(dt * mfUsageDecrease)`: a
//     non-zero mfCurrentCarBoostLossLevel (the per-car boost stat written by
//     SetCarStatBoostLevel @0x822D52D0) OVERRIDES the tuned mfBurnRateBoost.
//   * the request gate is no longer `if (mbBoostRequested)`: a run that has
//     lasted less than 1.25 s keeps being served after the request is released.
//   * Feb-2007's `if (mbInfiniteBoost) mbBoosting = true;` inside the served arm
//     is gone -- infinite boost now only suppresses the burn.
//   * the red arm gained the mfMinBoostAllowedAmount floor and the mfTimeBoosting
//     restart; the blue arm gained the mbSwicthBlueToRed flag on refusal, and the
//     not-served arm sets that flag too whenever the bar is not full.
//   * the trailing `RecalculateBoostMode()` call is NOT in the retail body (no
//     `bl`, and no meBoostMode store anywhere in the function -- which is why
//     BrnBoostBurnout5.h deliberately does not declare it). What retail does in
//     its place is re-read the whole boostparamsasset every update; see the tail.
//
// NaN polarity, checked at all six float compares. The burn clamp is
// `fcmpu + bge` around the zero store, so the store happens ONLY on
// ordered-less -- exactly C++ `< 0.0f` (which is what RemoveBoost's own clamp
// spells). The `blt` on mfTimeBoosting and the `ble`/`bgt` on mfBoostAmount all
// take their true-exit on the ordered case, matching C++ `<` / `>` as written,
// with the `ble` pairs falling through to the not-greater arm exactly as C++
// `>` does for an unordered operand. The one `!=` (mfBoostAmount vs mfMaxBoost,
// `fcmpu + beq` skipping the store) is C++ `!=`, true for unordered -- same as
// the taken branch. No predicate needed negating.
// ---------------------------------------------------------------------------
void
BoostBurnout5::ApplyUpdate(f32 lfTimeStep)
{
    if (IsInAir())
    {
        AddBoost(mfAirEarning * lfTimeStep);
    }
    if (IsDrifting())
    {
        AddBoost(mfDriftEarning * lfTimeStep);
    }
    if (IsSpinning())
    {
        AddBoost(mfAirSpinEarning * lfTimeStep);
    }
    if (IsOncoming())
    {
        AddBoost(mfBoostOnComing * lfTimeStep);
    }
    if (IsTailgating())
    {
        AddBoost(mfTailgatingEarning * lfTimeStep);
    }

    if (mbBoosting && !mbInfiniteBoost)
    {
        // The per-car boost-loss stat overrides the tuned burn rate when set.
        const f32 lfBurnRate = (mfCurrentCarBoostLossLevel == 0.0f)
                             ? mfBurnRateBoost
                             : mfCurrentCarBoostLossLevel;

        // 0x822C2150-0x822C215C is BoostStrategy::RemoveBoost inlined: subtract,
        // store, and clamp mfBoostAmount at zero. De-inlined per the AGENTS.md
        // inlining-reversal rule -- RemoveBoost has no standalone X360 symbol
        // (BrnBoostStrategy.h:331-334) and this site is one of its recovery
        // witnesses; the base .cpp wave owns the body.
        RemoveBoost(lfBurnRate * lfTimeStep);
    }

    // 1.25f == flt_820147F8, written as the literal the image carries. The
    // obvious candidate name is BrnWorld::KF_MIN_BOOST_TIME (DWARF
    // BrnBoostStrategy.h:49, declared extern-without-value at
    // BrnBoostStrategy.h:127 for the base .cpp wave to pin) -- but that
    // identification is NOT proven from this body, so the name is not used here.
    const bool lbInsideMinimumRun = mbBoosting && (mfTimeBoosting < 1.25f);

    if (mbBoostRequested || lbInsideMinimumRun)
    {
        // 0x822C21C8 compares meBoostMode against 0 == E_BOOSTMODE_B3_RED and
        // branches away on "not equal", so RED is the tested arm and everything
        // else falls into the blue path. (Contrast AreWeAllowedToBoost
        // @0x822A6BD8, which really does test both enumerators separately.)
        if (meBoostMode == E_BOOSTMODE_B3_RED)
        {
            if (mfBoostAmount > 0.0f)
            {
                if (mbBoosting)
                {
                    // Already inside a run: the console re-stores the flag
                    // (@0x822C21FC) and does NOT restart the run timer.
                    mbBoosting = true;
                }
                else
                {
                    // Starting a fresh run: the one-chunk floor applies and the
                    // run timer restarts.
                    if (mfBoostAmount > mfMinBoostAllowedAmount)
                    {
                        mbBoosting = true;
                    }

                    mfTimeBoosting = 0.0f;
                }

                mfTimeBoosting += lfTimeStep;
            }
            else
            {
                // Bar empty: stop, and do NOT tick the run timer (@0x822C2208
                // jumps past loc_822C2250).
                mbBoosting = false;
            }
        }
        else
        {
            const bool lbBoostGranted = BlueModeRequestBoost();

            // mbBoosting is re-read AFTER the call (@0x822C2214), which is what
            // the source spells. Nothing in the call chain actually changes it:
            // BlueModeRequestBoost @0x822A6CA8 stores only to +0x13C/+0x140/
            // +0x144/+0x135/+0xCB, and the AddBoost @0x822C0E10 it dispatches
            // has exactly one store, `stfs f0,0xA0(r3)` @0x822C0EA4, and makes
            // no calls of its own. So the reload is the compiler reloading
            // across a call barrier, not a value change -- reading the member
            // here is equivalent either way.
            if (!mbBoosting)
            {
                mfTimeBoosting = 0.0f;
            }

            if (lbBoostGranted)
            {
                mbBoosting = (mfBoostAmount > 0.0f);
            }
            else
            {
                mbSwicthBlueToRed = true;
            }

            mfTimeBoosting += lfTimeStep;
        }
    }
    else
    {
        mbBoostInterrupted = true;
        mbBoosting = false;

        if (mfBoostAmount != mfMaxBoost)
        {
            mbSwicthBlueToRed = true;
        }
    }

    // -- the tuning-param reload (0x822C225C-0x822C23C8) ---------------------
    // Retail re-reads the whole 34-parameter boostparamsasset EVERY update. The
    // block is the one BoostBurnout5::Prepare @0x822C1E90-0x822C1F88 carries
    // (same GUID, same 34 record->member stores, same order) MINUS Prepare's
    // re-zeroing of the three stunt earnings afterwards -- i.e. a live-tuning
    // reload, and the only thing standing where Feb-2007 called
    // RecalculateBoostMode(). It is written inline because the block has no
    // symbol and no declaration of its own anywhere (not in the DecFIGS method
    // list, not in the X360 ledger); BoostBurnout2::ApplyUpdate @0x822C1128
    // carries the same tail, so the original very likely had a shared helper,
    // but naming one here would be an invention.
    //
    // Buffer size: `_BYTE v17[512]` in the DWARF-backed local list for this
    // function, and the same 512 the sibling ConvertI64ToA scratch buffer in
    // CgsAttribSys::AttribSysCollectionKey::GetHashKey carries (DWARF cpp:81).
    char lacGuidText[512];
    rw::core::stdc::ConvertI64ToA(KI_DEFAULT_STUNT_BOOST_PARAMS_GUID, lacGuidText, 10);

    // 0x822C2274-0x822C2284: `bl Attrib__StringToKey / mr r4,r3 / li r5,0`.
    //
    // The key is LIVE, not dead: boostparamsasset's ctor @0x822B8C88 never
    // WRITES r4, so the value handed in here arrives whole at FindCollection and
    // is consumed there as the COLLECTION key (full derivation in
    // boostparamsasset.h). `mr r4,r3` is a 64-bit move with no clrldi,
    // Attrib::StringToKey returns the full 64-bit hash, and the ctor parameter
    // is u64 -- so nothing is narrowed. A static_cast<u32> here would silently
    // drop the high word and make every collection lookup miss, which is exactly
    // the defect that was fixed on this class's declaration.
    Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacGuidText), nullptr);

    // 0x822C2288-0x822C23C4 -- the 34 record -> member stores, in ascending
    // member order (which is the order the X360 emits them: +0x10 up to +0x94,
    // one store per 4-byte member, no gaps). Each one goes through the generated
    // accessor; the record offsets live in boostparamsasset.h, and the console
    // `lwz r11,4(asset)` layout-block fetch is what that accessor performs.
    mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();
    mfDriftEarning            = lBoostParams.DriftEarning();
    mfAirEarning              = lBoostParams.AirEarning();
    // The only two Int32 attributes in the record, and the only two stores the
    // X360 converts (lwz + extsw + std/lfd + fcfid + frsp, 0x822C22A4-0x822C22D8).
    mfSpeedForMinEarning      = static_cast<f32>(lBoostParams.SpeedForMinEarning());
    mfSpeedForMaxEarning      = static_cast<f32>(lBoostParams.SpeedForMaxEarning());
    mfMaxSpeedBoostModifier   = lBoostParams.MaxSpeedBoostModifier();
    mfTakedownEarning         = lBoostParams.TakedownEarning();
    mfShuntEarning            = lBoostParams.ShuntEarning();
    mfSlamEarning             = lBoostParams.SlamEarning();
    mfNudgeEarning            = lBoostParams.NudgeEarning();
    mfTradingPaintEarning     = lBoostParams.TradingPaintEarning();
    mfGrindingEarning         = lBoostParams.GrindingEarning();
    mfRubbingEarning          = lBoostParams.RubbingEarning();
    mfTailgatingEarning       = lBoostParams.TailgatingEarning();
    mfTrafficCheck            = lBoostParams.TrafficCheck();
    mfBoostSlamStrength       = lBoostParams.BoostSlamStrength();
    mfHandbrake180Earning     = lBoostParams.Handbrake180Earning();
    mfHandbrake360Earning     = lBoostParams.Handbrake360Earning();
    mfAirSpinEarning          = lBoostParams.AirSpinEarning();
    mfBarrelRollEarning       = lBoostParams.BarrelRollEarning();
    mfCleanLanding            = lBoostParams.CleanLanding();
    mfFakieLanding            = lBoostParams.FakieLanding();
    mfBoostSpinIncrease       = lBoostParams.BoostSpinIncrease();
    mfComboModifier           = lBoostParams.ComboModifier();
    mfBurnRateBoost           = lBoostParams.BurnRateBoost();
    mfBoostChainMin           = lBoostParams.BoostChainMin();
    mfBoostOnComing           = lBoostParams.OnComing();
    mfBeingSlammed            = lBoostParams.BeingSlammed();
    // NOTE: unlike Prepare @0x822C1F8C-0x822C1F94, this reload does NOT re-zero
    // the three stunt earnings afterwards -- it stores exactly what the asset
    // ships. (Prepare's three extra `stfs f31` stores have no counterpart in the
    // 0x822C2288-0x822C23C4 run; the tail goes straight to the dtor.)
    mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();
    mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();
    mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();
    mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning();
    mfBoostChainBonus         = lBoostParams.BoostChainBonus();
    mfOnWrecked               = lBoostParams.OnWrecked();

    // 0x822C23C8: `bl Attrib::Instance::~Instance` -- lBoostParams leaving scope.
}

} // namespace BrnWorld
