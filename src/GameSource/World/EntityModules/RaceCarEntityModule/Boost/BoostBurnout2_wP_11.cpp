// ============================================================================
// BrnWorld::BoostBurnout2 -- wave P partfile 11.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Body in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX; the ARTIST
// asm is rung 1 and arbitrates every branch, constant and store below):
//   BoostBurnout2::ApplyUpdate @ 0x822C1128   (base vtable slot 1, pure in base)
//
// Parked earlier in wave P on one header blocker that is now gone:
//   Attrib::Gen::boostparamsasset exposes the 34 named tuning-parameter
//   accessors (GameSource/AttribSys/Generated/classes/boostparamsasset.h), so
//   the per-frame re-read at the tail goes through those accessors. This TU does
//   NOT re-export or poke Attrib::Instance::GetLayoutPointer.
//
// Offset -> member map used below. BoostBurnout2's own members start at +0x130
// because sizeof(BoostStrategy) == 0x130 on BOTH console and host; the 34 tuning
// params and the shared flags live in the base (BrnBoostStrategy.h).
//   +0x014 BoostStrategy::mfDriftEarning          +0x0A4 BoostStrategy::mfMaxBoost
//   +0x018 BoostStrategy::mfAirEarning            +0x0B4 BoostStrategy::mfSpeed
//   +0x044 BoostStrategy::mfTailgatingEarning     +0x0C3 BoostStrategy::mbIsBoostFull
//   +0x070 BoostStrategy::mfBurnRateBoost         +0x0C5 BoostStrategy::mbBoosting
//   +0x078 BoostStrategy::mfBoostOnComing         +0x0C7 BoostStrategy::mbBoostRequested
//   +0x090 BoostStrategy::mfBoostChainBonus       +0x0C9 BoostStrategy::mbInChainMode
//   +0x098 BoostStrategy::mfTimeSpentCheating     +0x0CB BoostStrategy::mbChainNotifyPending
//   +0x0A0 BoostStrategy::mfBoostAmount           +0x0F0 BoostStrategy::mfCurrentCarBoostLossLevel
//   +0x120 BoostStrategy::mfTotalDistanceTraveled
//   +0x134 BoostBurnout2::mfHiddenBoost           +0x144 BoostBurnout2::mfContinuousBoostingTimeAdd
//   +0x138 BoostBurnout2::mbBoostInterrupted      +0x148 BoostBurnout2::mfTimeBoosting
//   +0x13C BoostBurnout2::miChainSize
//   +0x140 BoostBurnout2::mfTimeBasedBoostingBonus
//
// WHAT THE ASM SAYS THAT FEB-2007 DOES NOT
//  * Retail ApplyUpdate takes ONLY (this, f1 = lfTimeStep). The Feb-2007
//    `CarOffenceManager* lpCarOffenceManager` second parameter is gone, and so is
//    the `lpCarOffenceManager->OnBoostChain()` call: the chain notification became
//    the mbChainNotifyPending / miChainSize latch that GetIsChainNotifyPending
//    @0x822A65A0 drains.
//  * Retail RELOADS the whole boostparamsasset record at the END of every
//    ApplyUpdate call (0x822C1450..0x822C15BC), store-for-store identical to the
//    block in Prepare @0x822C0FA0 (landed in partfile 16) -- a live-tuning re-read
//    that survived into the shipping build. All paths converge on it (the
//    "stop boosting" arm `b loc_822C1450` at 0x822C13B0 jumps INTO it), so it is
//    not a cold/debug-only branch. Feb-2007 read a single parameter, in Prepare.
//  * The drift anti-exploit block (mfTimeSpentCheating drop-off), the high-speed
//    bonus, the continuous-boosting bonus, mfTimeBoosting and the chain counter
//    are all post-Feb-2007 additions with no counterpart in that drop.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"

#include "GameSource/AttribSys/Generated/classes/boostparamsasset.h"              // Attrib::Gen::boostparamsasset
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey
#include "rw/core/stdc/stdc.h"                                                    // rw::core::stdc::ConvertI64ToA

namespace BrnWorld
{

// ----------------------------------------------------------------------------
// DecFIGS DWARF BrnBoostBurnout2.cpp:43/44 declares these two at .cpp scope
// (BrnBoostBurnout2.h:83-87 records that they are NOT header declarations).
// Both are DOUBLY attested:
//   VALUE from the DWARF's own initialised bytes --
//     KF_MIN_TIME_TILL_DROP_OFF     = [65,160,0,0] = 0x41A00000 = 20.0f
//     KF_MIN_DISTANCE_TILL_DROP_OFF = [66,200,0,0] = 0x42C80000 = 200.0f
//   VALUE from the X360 image constants this body loads --
//     flt_820149CC (0x822C11E8) == 20.0f, flt_8201A1F0 (0x822C11C0) == 200.0f
// The drift anti-exploit block below is the only place in the whole TU that
// compares a time or a distance against a threshold, which is what binds each
// name to its site.
// ----------------------------------------------------------------------------
const f32 KF_MIN_TIME_TILL_DROP_OFF     = 20.0f;    // flt_820149CC
const f32 KF_MIN_DISTANCE_TILL_DROP_OFF = 200.0f;   // flt_8201A1F0

namespace
{
    // MERGE NOTE: BoostBurnout2_wP_16.cpp:56/61 carries this identical pair for
    // Prepare @0x822C0F38, which builds the same key with the same GUID. Both are
    // anonymous-namespace (internal linkage), so the two partfiles do NOT collide
    // as separate TUs -- but when the conductor merges the partfiles into one
    // BrnBoostBurnout2.cpp, keep exactly ONE copy of each.

    // The boostparamsasset collection GUID BoostBurnout2 tunes itself from.
    // X360-attested HERE too, not just in Prepare: ApplyUpdate @0x822C1450
    // `lis r3,8` + @0x822C145C `ori r3,r3,0xCA05` == 0x0008CA05 == 576005, handed
    // to ConvertI64ToA with r5 == 0xA so the key text is "576005".
    //
    // NAME AND TYPE are the DecFIGS DWARF's, verbatim -- BrnBoostBurnout2.cpp:4
    // of the dwarfdump declares `const int64_t KI_DEFAULT_DANGER_BOOST_PARAMS_
    // GUID = 576005;` at .cpp scope.
    const s64 KI_DEFAULT_DANGER_BOOST_PARAMS_GUID = 576005;

    // The stack text buffer ConvertI64ToA writes the decimal GUID into. Hex-Rays
    // renders it as `_BYTE v34[512]` at sp+0x70 of this function's 0x2B0-byte
    // frame. A char count is width-invariant, so this is not a console-size
    // hazard (nothing here feeds a stride, an advance or an allocation).
    const u32 KU_BOOST_KEY_TEXT_SIZE = 512;
}

// ---------------------------------------------------------------------------
// ApplyUpdate @ 0x822C1128 -- base vtable slot 1 (pure in the base).
//
// SIGNATURE. The prologue takes `this` in r3 and the time step in f1 only
// (`mr r31,r3` @0x822C1140, `fmr f31,f1` @0x822C1144; nothing in the body ever
// reads r4). PPC float-arg ABI: a float travels in an FPR and SKIPS its GPR
// slot, so the absent r4 is NOT a dropped pointer argument -- it confirms there
// is no second parameter, matching the DWARF `virtual void ApplyUpdate(float32_t)`.
//
// REGISTER MAP for the walk below: r31 = this, f31 = lfTimeStep,
// f29 = flt_82001CC0 = 0.0f, f30 = lfBoostIncrease, r30 = 0, r28 = 1,
// r29 = &flt_8201497C (the constant-pool anchor the `lfs (flt_XXXX -
// 0x8201497C)(r29)` loads use).
//
// VIRTUAL DISPATCHES. Every one is `lwz r11,0(r31) ; lwz r11,<off>(r11) ;
// mtctr ; bctrl`; the X360 vtable has 4-byte slots, so slot == off/4, and the
// slot numbers below are the base's 50-slot order (BrnBoostStrategy.h):
//   +0x50 -> slot 20 IsInAir      +0x54 -> slot 21 IsDrifting
//   +0x64 -> slot 25 IsOncoming   +0x6C -> slot 27 IsTailgating
//   +0xC4 -> slot 49 AddBoost
// BoostBurnout2 overrides none of these, so the unqualified calls below are that
// same virtual dispatch.
//
// NaN POLARITY. PPC `fcmpu` leaves LT/GT/EQ all clear when a compare is
// unordered, so a `ble`/`bge` guard is TAKEN on NaN while the C++ `<=`/`>=` it
// looks like is FALSE on NaN. Every guard below is written from the branch
// SENSE, using the negated ordered predicate where the asm's FALL-THROUGH (not
// its taken edge) is the C++ condition. Each such site is flagged inline.
// Likewise `fsel frD,frA,frB,frC` yields frC when frA is NaN (NaN is not >= 0),
// and treats -0.0 as >= 0; the two clamps below are written in the fsel's own
// ternary sense so both behaviours survive -- the same idiom the sibling
// BoostBurnout2_wP_05.cpp::OnWrecked already lands.
// ---------------------------------------------------------------------------
void
BoostBurnout2::ApplyUpdate(f32 lfTimeStep)
{
    f32 lfBoostIncrease = 0.0f;                        // 0x822C1154 fmr f30, f29

    // -- air ---------------------------------------------------------------
    // 0x822C1160 bctrl slot 20; 0x822C1170 lfs 0x18 / fmuls f30, f0, f31. The
    // asm multiplies INTO f30 rather than accumulating because it knows f30 is
    // still 0.0 here; the source accumulation is restored.
    if (IsInAir())
    {
        lfBoostIncrease += mfAirEarning * lfTimeStep;
    }

    // -- drift, with the chain anti-exploit drop-off ------------------------
    // 0x822C1188 bctrl slot 21. The plain earning is the `else` arm at
    // loc_822C122C; the attenuated arm only runs while a chain is actually being
    // milked (boosting AND more than one chain link already banked).
    if (IsDrifting())
    {
        // 0x822C11A0 lbz 0xC5 (mbBoosting) / 0x822C11AC lwz 0x13C (miChainSize).
        // `cmpwi cr6,r11,1` + `ble` is a SIGNED compare, so the attenuated arm
        // needs strictly `miChainSize > 1`.
        if (mbBoosting && miChainSize > 1)
        {
            // 0x822C11C4 fcmpu / bge loc_822C1224: `bge` is taken for GT, EQ
            // *and unordered*, and it leads to the reset arm -- so the earning
            // arm is the strictly-ordered-less case and plain `<` is exact.
            if (mfTotalDistanceTraveled < KF_MIN_DISTANCE_TILL_DROP_OFF)
            {
                // 0x822C11CC..0x822C11F8: accumulate, then clamp into
                // [0, KF_MIN_TIME_TILL_DROP_OFF] with two fsels. (The asm stores
                // to +0x98 twice, unclamped then clamped; nothing reads it in
                // between, so one store of the final value is equivalent.)
                f32 lfCheatTime = mfTimeSpentCheating + lfTimeStep;   // 0x822C11D4 fadds
                lfCheatTime = (-lfCheatTime >= 0.0f)                  // 0x822C11E4 fsel f13,f13,f29,f0
                                  ? 0.0f
                                  : lfCheatTime;
                lfCheatTime = ((KF_MIN_TIME_TILL_DROP_OFF - lfCheatTime) >= 0.0f) // 0x822C11F4 fsel f13,f11,f13,f0
                                  ? lfCheatTime
                                  : KF_MIN_TIME_TILL_DROP_OFF;
                mfTimeSpentCheating = lfCheatTime;                   // 0x822C11F8 stfs 0x98

                // 0x822C11FC..0x822C1218: (20 - t) * 0.05, clamped to [0, 1].
                // flt_8201497C is a SEPARATE image constant (0.050000001f), not
                // a compiler-folded reciprocal of the 20.0f above -- the asm
                // loads both -- so it is kept as its own constant, while being
                // numerically exactly 1 / KF_MIN_TIME_TILL_DROP_OFF.
                // Name not recovered (the DWARF attests only the three .cpp
                // constants above); the VALUE is the asm's.
                const f32 KF_DROP_OFF_RATE = 0.05f;                  // flt_8201497C

                f32 lfDropOff = (KF_MIN_TIME_TILL_DROP_OFF - lfCheatTime) * KF_DROP_OFF_RATE;
                lfDropOff = (-lfDropOff >= 0.0f)                     // 0x822C120C fsel f0,f13,f29,f0
                                ? 0.0f
                                : lfDropOff;
                lfDropOff = ((1.0f - lfDropOff) >= 0.0f)             // 0x822C1218 fsel f0,f11,f0,f13
                                ? lfDropOff
                                : 1.0f;                              // flt_82001C98

                // 0x822C121C fmuls f0, f0, f12 (mfDriftEarning), then the shared
                // 0x822C1230 fmadds f30, f0, f31, f30. Grouping preserved.
                lfBoostIncrease += lfDropOff * mfDriftEarning * lfTimeStep;
            }
            else
            {
                // 0x822C1224 stfs f29, 0x98 then `b loc_822C1234` -- the drift
                // earning is SKIPPED entirely on this arm, not merely
                // un-attenuated. Deliberate: the jump target is PAST the shared
                // fmadds, and this is the only path that leaves f30 untouched.
                mfTimeSpentCheating = 0.0f;
            }
        }
        else
        {
            // loc_822C122C: full, unattenuated drift earning.
            lfBoostIncrease += mfDriftEarning * lfTimeStep;
        }
    }

    // -- oncoming ----------------------------------------------------------
    // 0x822C1244 bctrl slot 25; 0x822C1254 lfs 0x78 / fmadds f30, f0, f31, f30.
    if (IsOncoming())
    {
        lfBoostIncrease += mfBoostOnComing * lfTimeStep;
    }

    // -- tailgating --------------------------------------------------------
    // 0x822C126C bctrl slot 27; 0x822C1280 lfs 0x44 / 0x822C1288 fmuls f1,f0,f31
    // / 0x822C1294 bctrl slot 49. Tailgating is awarded DIRECTLY through AddBoost
    // (so it is subject to AddBoost's speed multiplier and earning gate) instead
    // of joining lfBoostIncrease -- keep it out of the accumulator.
    if (IsTailgating())
    {
        AddBoost(mfTailgatingEarning * lfTimeStep);
    }

    // -- high-speed bonus --------------------------------------------------
    // 0x822C1298 lfs 0xB4 / 0x822C12A0 fcmpu flt_82014808 / 0x822C12A4 ble skip.
    // `ble` is taken for LT, EQ and unordered, so the executed arm is the
    // strictly-ordered-greater case and plain `>` is exact.
    // Neither constant is named in the DWARF (the six KF_DANGER_BOOST_* statics
    // it attests are header-scope and belong to the near-miss / crash-escape
    // paths, not here), so these are local names over the asm's VALUES, which
    // Hex-Rays renders directly as `if ( *(a1 + 180) > 100.0 ) ... (a2 * 1.1)`.
    {
        const f32 KF_HIGH_SPEED_THRESHOLD = 100.0f;   // flt_82014808
        const f32 KF_HIGH_SPEED_EARNING   = 1.1f;     // flt_82004A1C

        if (mfSpeed > KF_HIGH_SPEED_THRESHOLD)
        {
            // 0x822C12B0 fmadds f30, f31, f0, f30 -- operand order preserved.
            lfBoostIncrease += lfTimeStep * KF_HIGH_SPEED_EARNING;
        }
    }

    // -- continuous-boosting bonus -----------------------------------------
    // 0x822C12B4 lbz r10, 0xC5. That single load is reused for the chain-mode
    // clear and the hidden-boost branch below -- nothing between them writes
    // mbBoosting. (The compiler tests it two ways off that one byte:
    // `cmplwi r10,1`+`bne` here and `cmplwi r10,0`+`beq` at 0x822C12F4. For the
    // `bool` member the DWARF declares, those are the same test.)
    {
        const f32 KF_CONTINUOUS_BOOST_REFERENCE_TIME = 60.0f;   // flt_82004C6C

        if (mbBoosting)
        {
            mfContinuousBoostingTimeAdd += lfTimeStep;          // 0x822C12C8/0x822C12CC

            // 0x822C12D4 fcmpu / 0x822C12D8 bgt loc_822C12F0. The divide is on
            // the FALL-THROUGH edge, which is taken for LT, EQ *and unordered*.
            // Spelled `<=` this would skip the divide on NaN; the negated
            // ordered form reproduces the hardware.
            if (!(mfContinuousBoostingTimeAdd > KF_CONTINUOUS_BOOST_REFERENCE_TIME))
            {
                // 0x822C12DC fdivs f0, f13, f0 -- reference / elapsed.
                mfTimeBasedBoostingBonus =
                    KF_CONTINUOUS_BOOST_REFERENCE_TIME / mfContinuousBoostingTimeAdd;
            }
        }
        else
        {
            // loc_822C12E8, in emitted order.
            mfContinuousBoostingTimeAdd = 0.0f;                 // 0x822C12E8 stfs 0x144
            mfTimeBasedBoostingBonus    = 0.0f;                 // 0x822C12EC stfs 0x140
        }
    }

    // 0x822C12F8 stb r30, 0xC9 -- cleared unconditionally here, and re-armed
    // only by the run gate below.
    mbInChainMode = false;

    // 0x822C12FC beq: while boosting the earnings are stashed instead of being
    // credited, and are paid out as the chain bonus when the run ends.
    if (mbBoosting)
    {
        mfHiddenBoost += lfBoostIncrease;                       // 0x822C1300..0x822C1308
    }
    else
    {
        AddBoost(lfBoostIncrease);                              // 0x822C1324, slot 49
    }

    // -- the boost run gate ------------------------------------------------
    // 0x822C1328 reloads mbBoosting into r9 (the AddBoost call above forced the
    // reload; AddBoost does not write it, so the value is unchanged).
    //
    // lbBoostRunYoung: 0x822C1338 lfs 0x148 / 0x822C1340 lfs flt_820147F8 /
    // fcmpu / `blt` KEEPS r11 = 1, otherwise loc_822C134C sets r11 = 0. `blt` is
    // taken only on an ordered LT, so plain `<` is exact (an unordered compare
    // falls to the r11 = 0 arm, exactly as C++ `<` yields false on NaN).
    // Name not recovered; the value is Hex-Rays' rendering of flt_820147F8.
    const f32 KF_BOOST_RUN_GRACE_TIME = 1.25f;                  // flt_820147F8
    const bool lbBoostRunYoung = mbBoosting && (mfTimeBoosting < KF_BOOST_RUN_GRACE_TIME);

    // 0x822C1350 lfs 0xA0 / 0x822C1358 lfs 0xA4 / fcmpu / `beq` keeps r11 = 1 --
    // an exact ordered equality against the maximum is what marks "a fresh full
    // bar", and it is published to mbIsBoostFull (0x822C1378 stb 0xC3) BEFORE
    // the gate runs.
    const bool lbBoostFull = (mfBoostAmount == mfMaxBoost);
    mbIsBoostFull = lbBoostFull;

    // 0x822C1370..0x822C13A4, three conjuncts:
    //   (mbBoostRequested || lbBoostRunYoung)  -- 0x822C1370 lbz 0xC7 / bne, then
    //       the saved lbBoostRunYoung byte / beq. The grace term is what lets a
    //       chain re-ignite for 1.25 s after the player releases the button;
    //   mfBoostAmount > 0.0f                   -- 0x822C138C fcmpu f13,f29 /
    //       `ble` skips to the else arm, so this is an ordered GT;
    //   (mbBoosting || lbBoostFull)            -- 0x822C1394 / 0x822C139C. A NEW
    //       run may only start off a completely full bar (the Burnout-2 rule);
    //       an existing run continues regardless.
    if ((mbBoostRequested || lbBoostRunYoung)
        && mfBoostAmount > 0.0f
        && (mbBoosting || lbBoostFull))
    {
        mbInChainMode = true;                                   // 0x822C13B8 stb 0xC9

        if (lbBoostFull)
        {
            // 0x822C13C4..0x822C13D0 -- start of a fresh run off a full bar,
            // in emitted order.
            mfHiddenBoost      = 0.0f;                          // 0x822C13C4
            mbBoosting         = true;                          // 0x822C13C8
            mfTimeBoosting     = 0.0f;                          // 0x822C13CC
            mbBoostInterrupted = false;                         // 0x822C13D0
        }

        mfTimeBoosting += lfTimeStep;                           // 0x822C13DC/0x822C13E0

        // 0x822C13D8 lfs 0xF0 / 0x822C13E4 fcmpu f29 / `bne` skips the fallback
        // load at 0x822C13EC: the per-car boost loss level overrides the tuned
        // burn rate whenever it is non-zero. `== 0.0f` is exact -- `bne` is taken
        // for an unordered compare too, and C++ `==` is likewise false on NaN.
        f32 lfBurnRate = mfCurrentCarBoostLossLevel;
        if (lfBurnRate == 0.0f)
        {
            lfBurnRate = mfBurnRateBoost;
        }

        // 0x822C13F0 fnmsubs f0, f0, f31, f13 (== mfBoostAmount - rate*dt; f13
        // still holds the +0xA0 value loaded at 0x822C1350, and nothing has
        // written the member since) + 0x822C13F4 stfs 0xA0 + 0x822C13F8 fcmpu /
        // `bge` / 0x822C1400 stfs 0.0. That subtract-then-clamp-at-zero pair IS
        // BoostStrategy::RemoveBoost (DecFIGS BrnBoostStrategy.cpp:168; no
        // standalone X360 symbol -- inlined at every call site, exactly as in
        // OnCrash @0x822A6370 and OnWrecked @0x822C1608). Reversing the inline
        // restores the call, matching the landed sibling partfile 05.
        RemoveBoost(lfBurnRate * lfTimeStep);

        // 0x822C1404 reloads 0xA0 / 0x822C1408 fcmpu f29 / 0x822C140C `bgt`
        // jumps past the run-ended arm. The arm is therefore the FALL-THROUGH,
        // taken for LT, EQ *and unordered* -- so the negated ordered predicate,
        // not `<= 0.0f`.
        if (!(mfBoostAmount > 0.0f))
        {
            mbBoosting = false;                                 // 0x822C1414 stb 0xC5

            // 0x822C1410 lbz 0x138 / 0x822C141C bne skips the payout: a run that
            // was interrupted pays NO chain bonus and does NOT advance the chain
            // counter. OnCrash @0x822A639C sets the flag to 1 precisely to
            // suppress this path.
            if (!mbBoostInterrupted)
            {
                // 0x822C1430 fadds f1, 0x134, 0x90 then 0x822C143C bctrl slot 49.
                AddBoost(mfHiddenBoost + mfBoostChainBonus);
                mbChainNotifyPending = true;                    // 0x822C1444 stb 0xCB
                ++miChainSize;                                  // 0x822C1448/0x822C144C
            }
        }
    }
    else
    {
        // loc_822C13A8 -- the run is over and the chain is broken.
        mbBoosting  = false;                                    // 0x822C13A8 stb 0xC5
        miChainSize = 0;                                        // 0x822C13AC stw 0x13C
    }

    // -- the per-frame tuning re-read (0x822C1450..0x822C15BC) --------------
    // Store-for-store identical to the block in Prepare @0x822C0FA0 (partfile
    // 16). Every path reaches it, including the "run is over" arm above, which
    // branches straight into it at 0x822C13B0.
    //
    // The key handed to the boostparamsasset ctor is LIVE, not dead: the ctor
    // @0x822B8C88 never WRITES r4, so the caller's key reaches
    // Attrib::FindCollection whole as the COLLECTION key (full derivation in
    // boostparamsasset.h). Attrib::StringToKey returns u64 and 0x822C146C is a
    // full 64-bit `mr r4,r3` with no clrldi -- so there is deliberately NO
    // narrowing cast on it here. The owner argument is `li r5,0` @0x822C1470,
    // which is the accessor's default.
    //
    // SpeedForMin/MaxEarning are the ONLY two attributes converted int->float
    // (lwz + extsw + std/lfd + fcfid + frsp at 0x822C1498 and 0x822C14B4); they
    // are attrib Int32 and land in the f32 members at +0x1C/+0x20.
    {
        char lacBoostKey[KU_BOOST_KEY_TEXT_SIZE];
        rw::core::stdc::ConvertI64ToA(KI_DEFAULT_DANGER_BOOST_PARAMS_GUID, lacBoostKey, 10);

        Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacBoostKey));

        // ---- the 34 tuning params, in member order (== asm store order) -----
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

        // lBoostParams leaves scope here -- the `bl Attrib::Instance::~Instance`
        // at 0x822C15BC. Nothing is returned: the asm's r3 at that point is the
        // destructor's own dead value, and the DWARF declares `void`.
    }
}

}   // namespace BrnWorld
