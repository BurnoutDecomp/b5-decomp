// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp
// ============================================================================
// Reconstructed BODIES (store-for-store from the X360 dossiers) for the crash-mode
// scorer BrnGameState::CrashModeScoring (home header BrnCrashModeScoringRecentCrash.h).
// This is a PARTIAL TU for the type -- the remaining lifecycle / DealWith* / Update
// methods + the full member layout land later when the rest of the type is recovered;
// grow the home in place, do NOT fork.
//
// Methods bodied here (X360 ARTIST.XEX addresses):
//   GetRecentCrash                0x8232BEF8
//   ClearData                     0x82320D10
//   DealWithComboItem             0x82312918
//   DealWithCrashbreakerRequest   0x82320EB8
//   DealWithHitOverheadSign       0x82312928
//   DealWithHitProp               0x82320DC8
//   DealWithHitTrafficCar         0x82338558
//   DealWithPickup                0x82312970
//   DealWithScoreForVehicleClass  0x82338778
//   DealWithVehicleLeaping        0x82312980
//   GetVehicleScoreData           0x82312AB0
//   HasCrashModeEnded             0x823129A0
//   IsActiveCrash                 0x82312A30
//   Update                        0x82320808
//
// ACCESSOR CLOSURE (2026-08-26). The six live-score getters below join them. NONE has its
// own out-of-line X360 symbol -- the free build inlined every one of them -- so all six are
// recovered from the SAME call site, ScoringSystem::WriteDataToOutput (X360 0x8232AE98),
// whose DecFIGS dwarfdump (BrnScoringSystem.cpp:799) states the source reads the embedded
// crash scorer through exactly these getters. In that body the scorer is embedded at
// ScoringSystem+0x20, so each `*(a1 + N)` resolves to member offset N - 0x20, and every one
// of the six lands on a member this home already carves at that proven offset:
//   *(a1+788)+*(a1+784)+*(a1+780)+*(a1+776) -> +0x2F4/+0x2F0/+0x2EC/+0x2E8 maiNumCarsCrashed[3..0]
//   *(a1+772)                               -> +0x2E4 miScoreMultiplier
//   *(a1+768)                               -> +0x2E0 miCurrentComboCount
//   *(a1+808)                               -> +0x308 mfDistanceTravelled
// GetNumCarsLeapt / GetBestAirTime have no site in that body; they are read by the crash-score
// debug overlay (BrnCrashScoreDebugComponent.cpp, already committed and already calling them
// by name) and map to the two remaining declared members at the offsets the home records --
// +0x2F8 miNumCarsLeaped and +0x31C mfLongestJumpAirTime, which the home's own declaration
// comments already pin ("BrnCrashModeScoring.h:180" / ":183 (reads mfLongestJumpAirTime,
// +0x31C)"). Those two are therefore DERIVED (declaration-to-member, single candidate each),
// not asm-transcribed; the other four are asm-transcribed.
//
// OFFSET -> NAMED-MEMBER MAP (the X360 store offsets across these methods pin every named
// member; the committed home's declared member ORDER is authoritative for naming):
//   this+0x40 mfTimeSincePlayerCarMoved   this+0x44 mfTimeSinceLastEvent
//   this+0x48 mfTimeSinceModeStart        this+0x4C mfDistanceUntilStorePosition
//   this+0x50 mfPlayerBoostPercentage     this+0x54 mbPlayerIsCrashing  this+0x55 mbInfiniteCrashMode
//   this+0x58 mRecentlyHitPropSet (RingBuffer<u16>: mpData@+0,miMaxLength@+4,miReadPos@+8,
//             miWritePos@+0xC,miLength@+0x10 => the +0x16..+0x1A words the asm walks)
//   this+0x7C maRecentCrashes (count word @+0x27C)
//   this+0x2D8 miNumWheelsLastFrame  this+0x2DC miBaseScore  this+0x2E0 miCurrentComboCount
//   this+0x2E4 miScoreMultiplier  this+0x2E8 maiNumCarsCrashed[0..3]  this+0x2F8 miNumCarsLeaped
//   this+0x2FC miNumPropsDestroyed  this+0x300 mbAboutToResetCombo  this+0x304 mfResetComboGracePeriod
//   this+0x308 mfDistanceTravelled  this+0x30C mfTimeSinceLastHitOverheadSign
//   this+0x310 mfTimeContactingWall  this+0x314 mfTotalAirTime  this+0x318 mfCurrentJumpAirTime
//   this+0x31C mfLongestJumpAirTime  this+0x320 mfHighestJump  this+0x324 miStuntsPerformed
//   this+0x328 miCarDestructionBonus
//
// Each body is a clean (de-optimized) translation of its X360 pseudocode/asm. Members
// are accessed BY NAME against the committed layout in the home; semantic parity, NOT
// byte-matching.
//
// NOTE on the OTHER ledger function in this TU (X360 0x8231ADB0): that address is the
// non-const Array<CrashModeScoring::RecentCrash,64>::operator[](u32) -- the bounds-
// checked indexed accessor of the recent-hit-cars set (the truncated ledger name
// "BrnGameState::Cras"). It is NOT a CrashModeScoring method; its body is the generic
// inline Array<T,N>::operator[] in CgsArray.h and it is already emitted via the explicit
// instantiation in Array_RecentCrash_64.cpp (the same instantiation the catch-all noted
// at 0x8231AEB8). GetRecentCrash below drives it directly. No separate body is needed
// here.
// ============================================================================

#include <cstring>                                    // std::memset (ClearData zeroes the Vector3 members)
#include <cmath>                                      // std::fabs (Update's IsVectorSet epsilon test)
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the X360 assert sites in these bodies)
#include "GameShared/GameClasses/Core/CgsID.h"        // CgsIDUnCompress (GetVehicleScoreData diagnostic)
// [showtime score wave 2026-08-29] Update's real body needs the two console types its own
// signature already names by pointer. These are .cpp includes, NOT header includes -- the
// keystone's by-value embed is unaffected, which is what the previous FLAG was protecting.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface + BoostOutputInfo + RaceCarState
#include "GameSource/Math/BrnMathUtils.h"             // BrnMath::Magnitude2D @0x822B1DD8 (the XZ length)
#include "rw/math/vpu/vector3_operation.h"            // rw::math::vpu::Magnitude / Dot / operator-

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // GetRecentCrash  (X360 0x8232BEF8)
    // Linear search of the live recent-hit-cars set (maRecentCrashes, reached in the X360
    // at this+0x7C; its count word at +0x200 within the Array). Walk indices 0..count-1,
    // comparing each element's muTrafficCarIndex (u16 @ element+0) against the requested
    // index; return a mutable pointer to the first match, or null if none (the X360 returns
    // 0 both when the set is empty/count<=0 and when the scan falls off the end).
    //
    // The X360 body indexes through Array<>::operator[] (the bounds-checked accessor); the
    // 16-bit compare (`clrlwi r29,r29,16` then `lhz` of the element) is the u16 vs u16 match.
    // ------------------------------------------------------------------------
    CrashModeScoring::RecentCrash* CrashModeScoring::GetRecentCrash(u16 luTrafficCarIndex)
    {
        const s32 liCount = maRecentCrashes.GetCount();   // *(&set + 0x200)
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            RecentCrash& lrCrash = maRecentCrashes[static_cast<u32>(liIndex)];
            if (lrCrash.muTrafficCarIndex == luTrafficCarIndex)
            {
                return &lrCrash;
            }
        }
        return nullptr;
    }

    // ========================================================================
    // File-scope scoring constants (DWARF BrnCrashModeScoring.cpp:28-50). The values
    // attested by the dossier are reproduced; the few the dwarfdump left blank are not
    // referenced by any of the bodies below (the X360 inlines the literals it uses).
    // ========================================================================
    static const s32 KI_SCORE_BONUS_PER_OVERHEAD_SIGN = 10000;   // BrnCrashModeScoring.h:46
    static const s32 KI_SCORE_BONUS_FOR_COMBO_CRASH   = 1000;    // BrnCrashModeScoring.h:47 (per extra chain link)
    static const s32 KI_SCORE_BONUS_PER_PROP_HIT      = 100;     // BrnCrashModeScoring.cpp:34
    static const s32 KI_SCORE_BONUS_PER_BILLBOARD_HIT = 10000;   // BrnCrashModeScoring.cpp:35 (prop flag bit 1)

    // ------------------------------------------------------------------------
    // ClearData  (X360 0x82320D10)
    // Reset all live-scoring state to its start-of-mode defaults. The X360 stores into
    // every scalar/timer/counter member by offset; reproduced BY NAME here. The two
    // `stvx128 v0` to this+0x20 / this+0x30 zero the two Vector3 player-position members
    // (kept opaque in the home -- zeroed via memset to preserve the store-for-store reset).
    // ------------------------------------------------------------------------
    void CrashModeScoring::ClearData()
    {
        // Vector3 mPlayerPosLastFrame (+0x20) and mPlayerPosLastStored (+0x30): the X360
        // splat-zeros both 16-byte lanes.
        // ⭐ THE ZERO IS LOAD-BEARING, not just tidy. Update's distance block runs only when
        // mPlayerPosLastFrame is non-zero (`vcmpgtfp.` against FLT_EPSILON, @0x82320AE0), so a
        // zeroed anchor is what makes the FIRST frame of a crash contribute no distance -- the
        // car has no previous position to have travelled from. Same test seeds
        // mPlayerPosLastStored from mPlayerPosLastFrame the frame after.
        std::memset(&mPlayerPosLastFrame,  0, sizeof(mPlayerPosLastFrame));
        std::memset(&mPlayerPosLastStored, 0, sizeof(mPlayerPosLastStored));

        mfTimeSincePlayerCarMoved      = 0.0f;   // +0x40
        mfTimeSinceLastEvent           = 0.0f;   // +0x44
        mfTimeSinceModeStart           = 0.0f;   // +0x48
        mfDistanceUntilStorePosition   = 0.0f;   // +0x4C  (stfs flt_82001C98 == 1.0f below? no -- see +0x50)
        mfPlayerBoostPercentage        = 1.0f;   // +0x50  (the X360 stores flt_82001C98 == 1.0f here)
        mbPlayerIsCrashing             = true;   // +0x54  (stb r9==1)

        miNumWheelsLastFrame           = 4;      // +0x2D8 (li r8,4)
        miBaseScore                    = 0;      // +0x2DC
        miCurrentComboCount            = 0;      // +0x2E0
        miScoreMultiplier              = 1;      // +0x2E4 (li r9,1)
        maiNumCarsCrashed[0]           = 0;      // +0x2E8
        maiNumCarsCrashed[1]           = 0;      // +0x2EC
        maiNumCarsCrashed[2]           = 0;      // +0x2F0
        maiNumCarsCrashed[3]           = 0;      // +0x2F4
        miNumCarsLeaped                = 0;      // +0x2F8
        miNumPropsDestroyed            = 0;      // +0x2FC
        mbAboutToResetCombo            = false;  // +0x300 (stb 0)
        mfResetComboGracePeriod        = 0.0f;   // +0x304
        mfDistanceTravelled            = 0.0f;   // +0x308
        mfTimeSinceLastHitOverheadSign = 1.0f;   // +0x30C (stfs flt_82001C98 == 1.0f)
        mfTimeContactingWall           = 0.0f;   // +0x310
        mfTotalAirTime                 = 0.0f;   // +0x314
        mfCurrentJumpAirTime           = 0.0f;   // +0x318
        mfLongestJumpAirTime           = 0.0f;   // +0x31C
        mfHighestJump                  = 0.0f;   // +0x320
        miStuntsPerformed              = 0;      // +0x324
        miCarDestructionBonus          = 0;      // +0x328

        // The three container sets are reset to empty. (The X360 ClearData zeros the
        // recent-hit-cars count word @+0x27C directly; Clear() does the same here.)
        mRecentlyHitPropSet.Clear();
        maRecentCrashes.Clear();
    }

    // ------------------------------------------------------------------------
    // DealWithHitOverheadSign  (X360 0x82312928)
    // Award the overhead-sign bonus, but only once per cooldown window: the timer
    // mfTimeSinceLastHitOverheadSign is driven up by Update and must have reached >= 1.0
    // (one second since the last sign) before another sign scores. Either way the
    // event-idle timer mfTimeSinceLastEvent is reset to 0 (an overhead sign counts as
    // activity).
    // ------------------------------------------------------------------------
    void CrashModeScoring::DealWithHitOverheadSign()
    {
        if (mfTimeSinceLastHitOverheadSign < 1.0f)
        {
            mfTimeSinceLastEvent = 0.0f;
        }
        else
        {
            miBaseScore += KI_SCORE_BONUS_PER_OVERHEAD_SIGN;
            mfTimeSinceLastHitOverheadSign = 0.0f;
            mfTimeSinceLastEvent           = 0.0f;
        }
    }

    // ------------------------------------------------------------------------
    // DealWithHitProp  (X360 0x82320DC8)
    // Record a destroyed prop. The recent-prop ring is scanned for luPropIndex; if it is
    // already present (a prop being hit twice) nothing is scored. Otherwise the index is
    // pushed onto the ring, the prop counter bumped, and the score awarded -- the billboard
    // bonus (10000) when the "billboard" flag bit (0x2) is set, else the plain prop bonus
    // (100). The event-idle timer is reset (a prop hit is activity).
    //
    // The X360 walks the ring with the same (miReadPos + i) % miMaxLength addressing the
    // RingBuffer<u16>::operator[] uses and asserts the bounds inside that walk; here the
    // scan is expressed through the live window so the operator[] carries the assert.
    // ------------------------------------------------------------------------
    void CrashModeScoring::DealWithHitProp(u16 luPropIndex, u8 luPropFlags)
    {
        const s32 liLength = mRecentlyHitPropSet.GetLength();
        for (s32 liPropSetIndex = 0; liPropSetIndex < liLength; ++liPropSetIndex)
        {
            if (mRecentlyHitPropSet[static_cast<u32>(liPropSetIndex)] == luPropIndex)
            {
                // Already counted this prop -- no further scoring.
                return;
            }
        }

        u16 luEntry = luPropIndex;
        mRecentlyHitPropSet.Push(&luEntry);
        ++miNumPropsDestroyed;

        if ((luPropFlags & 0x2) != 0)
        {
            miBaseScore += KI_SCORE_BONUS_PER_BILLBOARD_HIT;
        }
        else
        {
            miBaseScore += KI_SCORE_BONUS_PER_PROP_HIT;
        }
        mfTimeSinceLastEvent = 0.0f;
    }

    // ------------------------------------------------------------------------
    // DealWithComboItem  (X360 0x82312918)
    // A combo-item pickup simply bumps the stunt counter (the X360 increments the +0x324
    // member miStuntsPerformed and does nothing else). The event payload pointer is not
    // dereferenced by the X360 body, so no field of CrashComboItemEvent is read here.
    // ------------------------------------------------------------------------
    void CrashModeScoring::DealWithComboItem(const CrashComboItemEvent* /*lpComboItemEvent*/)
    {
        ++miStuntsPerformed;
    }

    // ------------------------------------------------------------------------
    // DealWithPickup  (X360 0x82312970)
    // A pickup counts as activity: the only effect in the X360 body is resetting the
    // event-idle timer. The PickupEvent payload is not dereferenced.
    // ------------------------------------------------------------------------
    void CrashModeScoring::DealWithPickup(const PickupEvent* /*lpPickupEvent*/)
    {
        mfTimeSinceLastEvent = 0.0f;
    }

    // ------------------------------------------------------------------------
    // DealWithCrashbreakerRequest  (X360 0x82320EB8)
    // Reset the event-idle timer; then a VMX compare of the request event's float field
    // (event+0x18) against the near-zero threshold gates a car-destruction-bonus bump: the
    // X360 magnitude-masks the field (vandc clears the sign bit) and increments
    // miCarDestructionBonus when |value| is NOT greater than the threshold (i.e. when the
    // value is effectively zero).
    //
    // The threshold flt_82020B30 is rw::math::fpu::EPSILON: the PS3 unit (0x1CFF24) loads
    // &rw::math::fpu::EPSILON for the identical |value| compare (vcmpgefp EPSILON >= |value|
    // == the X360's !(|value| > EPSILON)). The compared field is the event's mfTimeUntilStart
    // (PS3 reads &lpCrashbreakerEvent->mfTimeUntilStart at the same +0x18 the X360 uses).
    //
    // FLAG: TriggerCrashBreakerEvent is forward-declared (pointer-only) here -- its float
    // field mfTimeUntilStart @+0x18 cannot be named until that event type is homed; read via
    // a flagged reinterpret (mirrors DealWithVehicleLeaping). Replace with the named accessor
    // once TriggerCrashBreakerEvent lands.
    // ------------------------------------------------------------------------
    void CrashModeScoring::DealWithCrashbreakerRequest(const TriggerCrashBreakerEvent* lpEvent)
    {
        mfTimeSinceLastEvent = 0.0f;
        const f32 KF_CRASHBREAKER_REQUEST_EPSILON = 1.1920929e-7f; // rw::math::fpu::EPSILON (PS3 0x1CFF24)
        const f32 lfRequestValue =
            *reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(lpEvent) + 0x18); // FLAG: un-homed mfTimeUntilStart
        const f32 lfRequestMagnitude = (lfRequestValue < 0.0f) ? -lfRequestValue : lfRequestValue; // vandc sign-mask
        if (!(lfRequestMagnitude > KF_CRASHBREAKER_REQUEST_EPSILON))
        {
            ++miCarDestructionBonus;   // *(this+0x328)
        }
    }

    // ------------------------------------------------------------------------
    // DealWithVehicleLeaping  (X360 0x82312980)
    // Add the number of cars leapt this event to the running total and reset the
    // event-idle timer. The X360 reads the first int of the VehicleLeaptEvent payload
    // (*a2) as the per-event leap count.
    //
    // FLAG: VehicleLeaptEvent is forward-declared (pointer-only) in this scope -- its
    // first-int field that the X360 reads (the per-event leap count) cannot be named until
    // that event type is homed. The store-for-store effect is `miNumCarsLeaped += *(int*)a2`;
    // bodied here against the raw first word with a clearly-flagged reinterpret so the count
    // arithmetic is preserved. Replace with the named accessor once VehicleLeaptEvent lands.
    // ------------------------------------------------------------------------
    void CrashModeScoring::DealWithVehicleLeaping(const VehicleLeaptEvent* lpLeapEvent)
    {
        const s32 liNumLeaptThisEvent = *reinterpret_cast<const s32*>(lpLeapEvent); // FLAG: un-homed field
        miNumCarsLeaped += liNumLeaptThisEvent;
        mfTimeSinceLastEvent = 0.0f;
    }

    // ------------------------------------------------------------------------
    // IsActiveCrash  (X360 0x82312A30)
    // A recorded crash is "active" while it is recent: the elapsed time since it happened
    // (the running mode clock mfTimeSinceModeStart minus the crash's own mfTimeOfCrash)
    // must be in [0, 5) seconds. The X360 asserts the elapsed time is non-negative.
    // ------------------------------------------------------------------------
    bool CrashModeScoring::IsActiveCrash(const RecentCrash* lpCrash) const
    {
        const f32 lfTimeSinceCrash = mfTimeSinceModeStart - lpCrash->mfTimeOfCrash;
        CGS_ASSERT(lfTimeSinceCrash >= 0.0f, "lfTimeSinceCrash >= 0.0f");
        return lfTimeSinceCrash < 5.0f;
    }

    // ------------------------------------------------------------------------
    // HasCrashModeEnded  (X360 0x823129A0)
    // The crash run ends when the player has gone idle: not in infinite-crash mode, the
    // player car is not currently crashing, the boost percentage has settled to ~0, and
    // both the player-inactive timer (mfTimeSincePlayerCarMoved) and the event-idle timer
    // (mfTimeSinceLastEvent), each advanced by lfPadding, have exceeded the 3-second
    // threshold. The ~0 test uses the X360's epsilon (FLT_EPSILON ~= 1.1920929e-7).
    // ------------------------------------------------------------------------
    bool CrashModeScoring::HasCrashModeEnded(f32 lfPadding) const
    {
        if (mbInfiniteCrashMode)
        {
            return false;
        }
        if (!mbPlayerIsCrashing)
        {
            return true;
        }

        const f32 lfEpsilon = 1.1920929e-7f;
        const f32 lfBoost   = mfPlayerBoostPercentage;
        const bool lbBoostSettled = !(lfBoost > lfEpsilon) && !(lfBoost < -lfEpsilon);
        if (!lbBoostSettled)
        {
            return false;
        }

        if ((mfTimeSincePlayerCarMoved + lfPadding) <= 3.0f)
        {
            return false;
        }
        if ((mfTimeSinceLastEvent + lfPadding) <= 3.0f)
        {
            return false;
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // DealWithScoreForVehicleClass  (X360 0x82338778)
    // Resolve the base score / multiplier / category for the crashed vehicle (via
    // GetVehicleScoreData), apply any chain bonus (1000 per extra link once the recent
    // crash's chain count reaches 2), then fold the awards into the live totals:
    //   miBaseScore       += chainBonus + baseScore
    //   miScoreMultiplier += multiplier
    //   ++maiNumCarsCrashed[vehicleClass]
    // and write the per-call breakdown back through the out-params. The four leading
    // out-pointers are asserted non-null first (X360 lines 564-568).
    // ------------------------------------------------------------------------
    void CrashModeScoring::DealWithScoreForVehicleClass(
            u16 luTrafficEntityIndex,
            BrnTraffic::VehicleClass leVehicleClass,
            CgsID lVehicleTypeID,
            s32* lpiVehicleTypeCrashed,
            s32* lpiVehicleBaseScore,
            BrnTraffic::VehicleScoreCategory* lpeVehicleScoreCategory,
            s32* lpiScoreMultiplierEarned,
            s32* lpiComboBonusEarned)
    {
        CGS_ASSERT(lpiVehicleTypeCrashed,    "lpiVehicleTypeCrashed");
        CGS_ASSERT(lpiVehicleBaseScore,      "lpiVehicleBaseScore");
        CGS_ASSERT(lpeVehicleScoreCategory,  "lpeVehicleScoreCategory");
        CGS_ASSERT(lpiScoreMultiplierEarned, "lpiScoreMultiplierEarned");
        CGS_ASSERT(lpiComboBonusEarned,      "lpiComboBonusEarned");

        s32 liChainBonus = 0;
        s32 liBaseValue  = 0;
        s32 liMultiplier = 0;
        BrnTraffic::VehicleScoreCategory leCategory = BrnTraffic::E_VEHICLESCORE_CAR;
        GetVehicleScoreData(leVehicleClass, lVehicleTypeID, &liBaseValue, &liMultiplier, &leCategory);

        const RecentCrash* lpRecentCrash = GetRecentCrash(luTrafficEntityIndex);
        if (lpRecentCrash != nullptr)
        {
            const u32 luChainCount = lpRecentCrash->muCrashChainCount;
            if (luChainCount >= 2)
            {
                liChainBonus = KI_SCORE_BONUS_FOR_COMBO_CRASH * static_cast<s32>(luChainCount);
            }
        }

        miBaseScore       += liChainBonus + liBaseValue;
        miScoreMultiplier += liMultiplier;
        const s32 liCrashedOfClass = maiNumCarsCrashed[leVehicleClass] + 1;
        maiNumCarsCrashed[leVehicleClass] = liCrashedOfClass;

        *lpiVehicleTypeCrashed    = liCrashedOfClass;
        *lpiVehicleBaseScore      = liBaseValue;
        *lpeVehicleScoreCategory  = leCategory;
        *lpiScoreMultiplierEarned = liMultiplier;
        *lpiComboBonusEarned      = liChainBonus;
    }

    // ------------------------------------------------------------------------
    // GetVehicleScoreData  (X360 0x82312AB0)
    // Look up the per-vehicle-type base score / multiplier / score-category. The X360 first
    // linearly scans the 24-row K_VEHICLE_SCORE_LOOKUP_TABLE for the row whose midType ==
    // lVehicleTypeID; on a hit it writes that row's category(+8) / score(+0xC, s16) /
    // multiplier(+0xE, s16) and returns. If no row matches (or after writing the matched
    // row when the index is past the table -- the X360's `if (idx < 24) goto done` only
    // skips the fallback on a real hit) it falls back to a coarse per-VehicleClass default
    // and emits a debug "Unknown traffic vehicle in Showtime scoring" line. The three
    // out-pointers are asserted non-null first.
    //
    // [boost-msg wave 2026-08-26] The former "columns not recoverable" FLAG is RETIRED --
    // the full 24-row table was read out of X360 rodata (@unk_82020FA8) and fills the
    // K_VEHICLE_SCORE_LOOKUP_TABLE below verbatim. The lookup-loop control flow,
    // the row field offsets, and the VehicleClass fallback (car 0/750/0, van 1/2000/0,
    // bus 3/5000/0, bigrig 4/10000/0) remain reconstructed store-for-store.
    void CrashModeScoring::GetVehicleScoreData(
            BrnTraffic::VehicleClass leVehicleClass,
            CgsID lVehicleTypeID,
            s32* lpiScore, s32* lpiMultiplier,
            BrnTraffic::VehicleScoreCategory* lpeCategory)
    {
        CGS_ASSERT(lpiScore,    "lpiScore");
        CGS_ASSERT(lpiMultiplier, "lpiMultiplier");
        CGS_ASSERT(lpeCategory, "lpeCategory");

        // VehicleScoreLookup row (DWARF BrnCrashModeScoring.cpp:79-84): midType@+0 (CgsID, 8B),
        // meCategory@+8, miScore@+0xC (s16), miMultiplier@+0xE (s16). 16-byte stride.
        struct VehicleScoreLookup
        {
            CgsID                            midType;
            BrnTraffic::VehicleScoreCategory meCategory;
            s16                              miScore;
            s16                              miMultiplier;
        };

        // [boost-msg wave 2026-08-26] The FLAG above is RETIRED: the real table was read
        // straight out of X360 rodata (@unk_82020FA8, 24 rows x 16 bytes: CgsID midType,
        // s32 category, s16 score, s16 multiplier -- big-endian bytes, host-swapped here).
        // The previous placeholder rows carried HALF of each key's bits and guessed the
        // category column (the taxi/limo rows were E_VEHICLESCORE_CAR); both are gone.
        // The bus/bigrig classes stay on the VehicleClass fallback below -- they have no
        // rows in the shipped table.
        static const VehicleScoreLookup K_VEHICLE_SCORE_LOOKUP_TABLE[24] =
        {
            { 0xBC45991F98700000ULL, BrnTraffic::E_VEHICLESCORE_CAR,           1150, 0 },
            { 0xBF2E4A6770360000ULL, BrnTraffic::E_VEHICLESCORE_CAR,           1000, 0 },
            { 0xBF2EAB1CD2D60000ULL, BrnTraffic::E_VEHICLESCORE_CAR,           1200, 0 },
            { 0xBCDC5901FE33C000ULL, BrnTraffic::E_VEHICLESCORE_CAR,           1300, 0 },
            { 0xBF2E99150A360000ULL, BrnTraffic::E_VEHICLESCORE_CAR,           1400, 0 },
            { 0xBF2EACC070B60000ULL, BrnTraffic::E_VEHICLESCORE_CAR,           1250, 0 },
            { 0xBF2EAC91B07AD000ULL, BrnTraffic::E_VEHICLESCORE_CAR,           1450, 0 },
            { 0xBF2EAC91B053C000ULL, BrnTraffic::E_VEHICLESCORE_CAR,           1500, 0 },
            { 0xBF2EB9DF2F940000ULL, BrnTraffic::E_VEHICLESCORE_VAN,           1800, 0 },
            { 0xBF2EB9DF85070000ULL, BrnTraffic::E_VEHICLESCORE_VAN,           2200, 0 },
            { 0xBF2EB9DE90E30000ULL, BrnTraffic::E_VEHICLESCORE_VAN,           2600, 0 },
            { 0xBF2E64BEC6280000ULL, BrnTraffic::E_VEHICLESCORE_VAN,           2650, 0 },
            { 0xBF2EAC9756400000ULL, BrnTraffic::E_VEHICLESCORE_VAN,           2700, 0 },
            { 0xBF2EB9DE3B700000ULL, BrnTraffic::E_VEHICLESCORE_VAN,           2450, 0 },
            { 0xBF2EAC9A4B160000ULL, BrnTraffic::E_VEHICLESCORE_VAN,           3350, 0 },
            { 0xBF2EA6A470308000ULL, BrnTraffic::E_VEHICLESCORE_VAN,           1850, 0 },
            { 0xBF2EAB5E9B59E000ULL, BrnTraffic::E_VEHICLESCORE_TRUCK,         3400, 0 },
            { 0xBF2EAB5E9B80F000ULL, BrnTraffic::E_VEHICLESCORE_TRUCK,         3150, 0 },
            { 0xBF2EAB5E9B0BC000ULL, BrnTraffic::E_VEHICLESCORE_TRUCK,         3000, 0 },
            { 0xBF2EAB5E9B32D000ULL, BrnTraffic::E_VEHICLESCORE_TRUCK,         3200, 0 },
            { 0xBF2E5301736BC000ULL, BrnTraffic::E_VEHICLESCORE_TAXI,          1250, 0 },
            { 0xBF2E8189D1760000ULL, BrnTraffic::E_VEHICLESCORE_LIMO,          5000, 0 },
            { 0xBF2E42A8A7700000ULL, BrnTraffic::E_VEHICLESCORE_TARGETVEHICLE, 6000, 1 },
            { 0xBF2E42A99B940000ULL, BrnTraffic::E_VEHICLESCORE_TARGETVEHICLE, 6500, 1 },
        };
        const s32 ciNumVehicleScores = 24;

        s32 liIndex = 0;
        const VehicleScoreLookup* lpLookup = K_VEHICLE_SCORE_LOOKUP_TABLE;
        while (lpLookup->midType != lVehicleTypeID)
        {
            ++lpLookup;
            ++liIndex;
            if (liIndex >= ciNumVehicleScores)
            {
                goto LFallback;
            }
        }
        *lpeCategory   = lpLookup->meCategory;
        *lpiScore      = lpLookup->miScore;
        *lpiMultiplier = lpLookup->miMultiplier;
        if (liIndex < ciNumVehicleScores)
        {
            return;
        }

    LFallback:
        // Coarse per-VehicleClass default (the only fully-attested values in this method).
        switch (leVehicleClass)
        {
        case BrnTraffic::E_VEHICLECLASS_CAR:
            *lpeCategory = BrnTraffic::E_VEHICLESCORE_CAR;    *lpiScore = 750;   *lpiMultiplier = 0;
            break;
        case BrnTraffic::E_VEHICLECLASS_VAN:
            *lpeCategory = BrnTraffic::E_VEHICLESCORE_VAN;    *lpiScore = 2000;  *lpiMultiplier = 0;
            break;
        case BrnTraffic::E_VEHICLECLASS_BUS:
            *lpeCategory = BrnTraffic::E_VEHICLESCORE_BUS;    *lpiScore = 5000;  *lpiMultiplier = 0;
            break;
        case BrnTraffic::E_VEHICLECLASS_BIGRIG:
            *lpeCategory = BrnTraffic::E_VEHICLESCORE_BIGRIG; *lpiScore = 10000; *lpiMultiplier = 0;
            break;
        default:
            CGS_ASSERT(false, "Unknown vehicle class in CrashModeScoring.");
            break;
        }

        // Diagnostic trail for the unrecognised type (the X360 streams the un-compressed id
        // through the debug print when the message filter is enabled). The un-compress call
        // is preserved; the log emission itself is left to the (un-homed) debug-print path.
        char lacBuffer[KI_CGSID_STRING_LEN];
        CgsIDUnCompress(lVehicleTypeID, lacBuffer);
        // FLAG: the CgsDev::Log::gpDebugPrint emission ("Unknown traffic vehicle in Showtime
        // scoring: <id>") is gated on CgsDev::Message::gxMessageFilterFlags & 1 in the X360;
        // that debug-print sink is not homed in this scope, so only the un-compress (its input)
        // is reconstructed here.
        (void)lacBuffer;
    }

    // ------------------------------------------------------------------------
    // DealWithHitTrafficCar  (X360 0x82338558)
    // Resolve which traffic car was the victim of a player-vs-traffic or traffic-vs-traffic
    // impact, compute its crash-chain count, record it in the recent-hit-cars set, and
    // report the victim index. EntityId packs an owner-type in its top byte and a 14-bit
    // entity index in bits 10..23 (the X360 reads HIBYTE / (id>>10)&0x3FFF). Owner type 1 is
    // the player race car, owner type 2 is a traffic vehicle. Returns true when a new victim
    // was recorded.
    //
    // FLAG: EntityId's owner-type / index bit layout is reproduced from the X360 word
    // arithmetic (HIBYTE == owner, (value>>10)&0x3FFF == index) against the raw packed word,
    // because EntityId is homed in this scope only as the bare storage word {u32 muValue}
    // (no GetOwner()/GetIndex() accessors) and the BrnWorld::E_ENTITYTYPE_* enum is not homed
    // here. Owner constants 1 (race car) and 2 (traffic vehicle) are the X360 literals.
    // ------------------------------------------------------------------------
    bool CrashModeScoring::DealWithHitTrafficCar(
            EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex,
            EntityId lEntityIdA, EntityId lEntityIdB,
            u16* lpOutVictimIndex)
    {
        const u32 K_OWNER_RACE_CAR        = 1; // BrnWorld::E_ENTITYTYPE_RACE_CAR        (FLAG: enum un-homed)
        const u32 K_OWNER_TRAFFIC_VEHICLE = 2; // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE (FLAG: enum un-homed)

        const u32 luOwnerA = (lEntityIdA.muValue >> 24) & 0xFF;
        const u32 luOwnerB = (lEntityIdB.muValue >> 24) & 0xFF;
        const u32 luIndexA = (lEntityIdA.muValue >> 10) & 0x3FFF;
        const u32 luIndexB = (lEntityIdB.muValue >> 10) & 0x3FFF;
        const u32 luLocalPlayerIndex = static_cast<u32>(leLocalPlayerActiveRaceCarIndex);

        u16 luTrafficCarIndex = 0;
        u16 luCrashChainCount = 0;

        if (luOwnerA == K_OWNER_RACE_CAR)
        {
            // Player (A) hit traffic (B).
            CGS_ASSERT(luOwnerB == K_OWNER_TRAFFIC_VEHICLE,
                       "lEntityIdB.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
            if (luIndexA != luLocalPlayerIndex || GetRecentCrash(static_cast<u16>(luIndexB)) != nullptr)
            {
                return false;
            }
            luTrafficCarIndex = static_cast<u16>(luIndexB);
            luCrashChainCount = 1;
        }
        else if (luOwnerB == K_OWNER_RACE_CAR)
        {
            // Player (B) hit traffic (A).
            CGS_ASSERT(luOwnerA == K_OWNER_TRAFFIC_VEHICLE,
                       "lEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
            if (luIndexB != luLocalPlayerIndex || GetRecentCrash(static_cast<u16>(luIndexA)) != nullptr)
            {
                return false;
            }
            luTrafficCarIndex = static_cast<u16>(luIndexA);
            luCrashChainCount = 1;
        }
        else if (luOwnerA == K_OWNER_TRAFFIC_VEHICLE && luOwnerB == K_OWNER_TRAFFIC_VEHICLE)
        {
            // Traffic-vs-traffic: chain the new victim off whichever participant is an
            // already-active recorded crash; the OTHER participant becomes the new victim.
            RecentCrash* lpCrashA = GetRecentCrash(static_cast<u16>(luIndexA));
            RecentCrash* lpCrashB = GetRecentCrash(static_cast<u16>(luIndexB));
            if (lpCrashA != nullptr)
            {
                if (lpCrashB != nullptr || !IsActiveCrash(lpCrashA))
                {
                    return false;
                }
                luCrashChainCount = static_cast<u16>(lpCrashA->muCrashChainCount + 1);
                luTrafficCarIndex = static_cast<u16>(luIndexB);
            }
            else
            {
                if (lpCrashB == nullptr || !IsActiveCrash(lpCrashB))
                {
                    return false;
                }
                luCrashChainCount = static_cast<u16>(lpCrashB->muCrashChainCount + 1);
                luTrafficCarIndex = static_cast<u16>(luIndexA);
            }
        }
        else
        {
            return false;
        }

        *lpOutVictimIndex = luTrafficCarIndex;

        RecentCrash lCrash;
        lCrash.muTrafficCarIndex = luTrafficCarIndex;
        lCrash.muCrashChainCount = luCrashChainCount;
        lCrash.mfTimeOfCrash     = mfTimeSinceModeStart;   // *(a1+72)

        // Keep the recent-hit-cars set within capacity (drop the oldest when full).
        if (maRecentCrashes.IsFull())
        {
            maRecentCrashes.Erase(0);
        }
        CGS_ASSERT(GetRecentCrash(luTrafficCarIndex) == nullptr, "GetRecentCrash(luTrafficCarIndex) == NULL");
        maRecentCrashes.Append(lCrash);

        ++miCurrentComboCount;        // *(a1+0x2E0) = *(a1+0x2E0)+1  (chain link recorded)
        mbAboutToResetCombo = false;  // *(a1+0x300) = 0
        return true;
    }

    // ========================================================================
    // Update  (X360 0x82320808, 321 instructions)  -- the per-frame driver
    //
    // ⭐⭐⭐ [showtime score wave 2026-08-29] FULLY BODIED. It was the interface-independent
    // clock advance only; everything the showtime readout renders lives in the half that was
    // FLAG'd off, so "the score is always 0" was this function, not the GUI chain.
    //
    // ⚠️ IT IS NOT REACHED YET, AND THAT IS A DIFFERENT DEFECT. Its only caller,
    // ModeManager::PostWorldUpdate, itself has NO CALL SITE on this build (that file's own ARMING
    // STATE banner says so: GameStateModule drives PreWorldUpdateClocksBringUp instead). So the
    // showtime HUD still renders Distance 0m today -- MEASURED 2026-08-29 -- and this body is what
    // makes it move the moment that seam is wired. Landing it now is what turns "the score is
    // always zero" from three unknowns into one named one.
    //
    // ⛔⛔ THE PARK NOTE ON THE CALL SITE IS WRONG, AND THIS IS WHERE IT IS PROVED.
    // BrnModeManager_WorldTick.cpp parked the console's showtime arm with the reason
    // "GameStateModuleIO::VehicleOutputInterface is a different, still-incomplete forward
    // declaration over opaque storage, so the [traffic-state] queue cannot be reached by name".
    // The queue argument arrives in r5 -- and r5 IS NEVER READ IN THIS FUNCTION. The whole
    // 321-instruction body contains exactly two references to r5 and both are `li r5, <line>`
    // writes feeding CgsDev::Assert::FireAssert (@0x82320858 :189, @0x82320888 :193). The f32
    // rides f1, so r4 is the interface and r5 is the dead queue pointer.
    // ⇒ The blocker was real for the ARGUMENT and irrelevant to the CALL. Passing a null queue
    // is provably inert here; NOT calling Update at all cost the entire crash scorer.
    // [[gates-are-stale-not-dead]] -- ask what the block actually blocks.
    //
    // ASM SPINE, in the console's order:
    //   0x82320834  r30 = lpActiveRaceCarInterface, f30 = lfSimTimeStep
    //   0x82320844  assert lpActiveRaceCarInterface != NULL                 (:189)
    //   0x82320870  r27 = sub_82310240(r30) == GetPlayerRaceCarState()
    //   0x82320878  assert lpPlayerState != NULL                            (:193)
    //   0x823208C4  |mLinearVelocity| (+0x330) and |mAngularVelocity| (+0x340), VMX
    //               rsqrt-refined with a vsel that returns EXACTLY 0 for a zero square
    //   0x82320950  v127 = *(playerState + 0x220) == mTransform.wAxis, the world position
    //   0x82320954  the four clocks store back
    //   0x82320964  mbPlayerIsCrashing  <- IsPlayerCarCrashing() (inlined, -1 sentinel and all)
    //   0x823209A4  mfPlayerBoostPercentage <- boost.mfBoostAmount / boost.mfMaxBoost
    //   0x823209CC  the wall-contact grace timer
    //   0x82320A8C  the "player has moved" reset (|linear| > 4 or |angular| > 6)
    //   0x82320AB8  THE DISTANCE BLOCK -- mfDistanceTravelled
    //   0x82320BDC  the air-time accumulators + mfHighestJump
    //   0x82320C68  the wheel-loss decay into miCarDestructionBonus
    //   0x82320CDC  mPlayerPosLastFrame = the world position
    //
    // ⛔ INIT-ORDER CHECKED, BOTH EDGES (2026-08-29). Every literal below is a plain .rdata
    // constant: scanning the ASSEMBLY of all 30,084 exported ARTIST functions finds 2,952 that
    // REFERENCE flt_82001CC0 / flt_82020B30 / flt_82020F9C / flt_82001DA0 / flt_820211D4 /
    // flt_820211C8 / flt_82022E34 / flt_82002514 and ZERO store instructions targeting any of
    // them. No CRT thunk writes them, so no thunk can observe a half-built dependency and there
    // is no init-order question -- the image value IS the shipped value.
    // Image reads (VA -> bytes -> value), big-endian at file offset VA-0x82000000:
    //   flt_82001CC0 00000000  0.0f            flt_82020B30 34000000  1.1920929e-07 (FLT_EPSILON)
    //   flt_82020F9C 3E99999A  0.3f            flt_82001DA0 3F000000  0.5f
    //   flt_820211D4 40800000  4.0f            flt_820211C8 40C00000  6.0f
    //   flt_82022E34 41A00000 20.0f            flt_82002514 B4000000 -1.1920929e-07
    // ========================================================================
    namespace
    {
        // flt_82020B30 / flt_82002514 -- the +/- epsilon pair. The console splats the POSITIVE
        // one into a vector for the "has this position ever been written" test at 0x82320AE0
        // (`vandc` clears the sign bit == fabs, then vcmpgtfp. against the splat) and uses both
        // scalars for the air-time "is it already zero" test at 0x82320C38/0x82320C4C.
        const f32 KF_VECTOR_SET_EPSILON = 1.1920929e-07f;   // flt_82020B30

        // The four tuning literals the body reads, all image-verified (see the banner).
        const f32 KF_WALL_CONTACT_COMBO_BREAK_SECONDS = 0.30000001f;  // flt_82020F9C
        const f32 KF_COMBO_BREAK_GRACE_SECONDS        = 0.5f;         // flt_82001DA0
        const f32 KF_MOVED_LINEAR_SPEED               = 4.0f;         // flt_820211D4
        const f32 KF_MOVED_ANGULAR_SPEED              = 6.0f;         // flt_820211C8
        const f32 KF_DISTANCE_UNTIL_STORE_POSITION    = 20.0f;        // flt_82022E34

        // 0x82320AB8..0x82320AF0 and 0x82320B10..0x82320B44, both times on a Vector3 member.
        // The console tests lanes (x, y, z, x) -- the `vrlimi128 v11, v13, 1, 1` duplicates x
        // into w -- so the fourth lane adds nothing and this is a plain Vector3 test.
        // Semantically: "has this anchor been written since ClearData zeroed it".
        bool IsVectorSet(const Vector3& lrVector)
        {
            return (std::fabs(lrVector.x) > KF_VECTOR_SET_EPSILON) ||
                   (std::fabs(lrVector.y) > KF_VECTOR_SET_EPSILON) ||
                   (std::fabs(lrVector.z) > KF_VECTOR_SET_EPSILON);
        }
    }

    void CrashModeScoring::Update(
            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
            const VehicleOutputInterface::PhysicalTrafficStateQueue* /*lpTrafficStateQueue*/,
            f32 lfSimTimeStep)
    {
        // The console's own two asserts, with their baked line numbers. Neither gates: it fires
        // and dereferences anyway.
        CGS_ASSERT(lpActiveRaceCarInterface != nullptr, "lpActiveRaceCarInterface != NULL");  // :189
        if (lpActiveRaceCarInterface == nullptr)
        {
            return;   // [PC GUARD] not X360 -- the console's precondition is the assert above.
        }

        const BrnPhysics::Vehicle::RaceCarState* const lpPlayerState =
            lpActiveRaceCarInterface->GetPlayerRaceCarState();
        CGS_ASSERT(lpPlayerState != nullptr, "lpPlayerState != NULL");                        // :193
        if (lpPlayerState == nullptr)
        {
            return;   // [PC GUARD] same shape.
        }

        // ---- 0x823208C4..0x82320948: the two speeds, computed BEFORE the clocks store ------
        // The console's VMX form is `vmsum3fp128` + two Newton-Raphson rsqrt refinements + a
        // `vsel` that substitutes an exact 0.0f when the squared length is exactly zero. That is
        // rw::math::vpu::Magnitude's own shape; called by name rather than re-open-coded.
        const f32 lfLinearSpeed  = rw::math::vpu::Magnitude(lpPlayerState->mLinearVelocity);   // +0x330
        const f32 lfAngularSpeed = rw::math::vpu::Magnitude(lpPlayerState->mAngularVelocity);  // +0x340

        // 0x82320950 `li r11, 0x220 ; lvx128 v127, r27, r11` -- 0x220 == 544 == mTransform
        // (+496) plus 48, i.e. the FOURTH row of the Matrix44Affine, which is the translation.
        const Vector3 lPlayerPosition = lpPlayerState->mTransform.Pos();

        // ---- the running clocks (0x823208AC/B4/C0/E4 read, 0x82320954..0960 store) ---------
        mfTimeSinceModeStart           += lfSimTimeStep;   // +0x48
        mfTimeSincePlayerCarMoved      += lfSimTimeStep;   // +0x40
        mfTimeSinceLastEvent           += lfSimTimeStep;   // +0x44
        mfTimeSinceLastHitOverheadSign += lfSimTimeStep;   // +0x30C

        // ---- 0x82320964..0x8232099C: the crashing latch -----------------------------------
        // The console inlines the -1-sentinel form verbatim (`lwz 0x2858 ; cmpwi -1 ; else
        // lbz 1120*idx + 0x77A`), which IS IsPlayerCarCrashing(): 1914 - 816 == RaceCarState
        // +1098 == mbCrashing. Reached by name.
        mbPlayerIsCrashing = lpActiveRaceCarInterface->IsPlayerCarCrashing();   // +0x54

        // ---- 0x823209A0..0x823209C8: the boost gauge --------------------------------------
        // `bl RCEntit` twice with the same argument (the console does NOT CSE it) is
        // GetBoostOutputInfoN @0x823101C0 -- `&this[0x210 + 36*idx]`, with this header's own
        // :1157/:1158 range asserts. +0x10 / +0x14 within the 36-byte record are mfBoostAmount
        // and mfMaxBoost. Called ONCE here: both calls return the same pointer and neither has
        // a side effect.
        const EActiveRaceCarIndex lePlayerIndex =
            lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
        // [PC GUARD] not X360. The console indexes with the raw -1 sentinel and reads off the
        // front of the array (its two asserts fire and do not gate). Bounded here; on every
        // input the asserts accept, the behaviour is identical.
        if (lePlayerIndex >= E_ACTIVE_RACE_CAR_INDEX_0 &&
            lePlayerIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT)
        {
            const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo* const lpBoost =
                lpActiveRaceCarInterface->GetBoostOutputInfoN(lePlayerIndex);
            mfPlayerBoostPercentage = lpBoost->mfBoostAmount / lpBoost->mfMaxBoost;   // +0x50
        }

        // ---- 0x823209CC..0x82320A8C: the wall-contact combo-break timer --------------------
        // Accumulate while the car is scraping a wall OR is on the ground; zero it the moment
        // the car is airborne and not touching anything. Past 0.3 s, arm a 0.5 s grace period;
        // when that lapses the crash chain is broken (miCurrentComboCount = 0).
        // ⚠️ The in-air term is the INLINED IsPlayerInAir() -- the console's own
        // `maRaceCarStates[idx].mfTimeInAir > 0.0f` with the same -1 sentinel (1844 - 816 ==
        // RaceCarState +1028 == mfTimeInAir). Reached by name.
        if (lpPlayerState->mbContactingWall || !lpActiveRaceCarInterface->IsPlayerInAir())
        {
            mfTimeContactingWall += lfSimTimeStep;                          // +0x310
            if (mfTimeContactingWall > KF_WALL_CONTACT_COMBO_BREAK_SECONDS && !mbAboutToResetCombo)
            {
                mfResetComboGracePeriod = KF_COMBO_BREAK_GRACE_SECONDS;     // +0x304
                mbAboutToResetCombo     = true;                             // +0x300
            }
        }
        else
        {
            mfTimeContactingWall = 0.0f;
        }

        if (mbAboutToResetCombo)
        {
            mfResetComboGracePeriod -= lfSimTimeStep;
            if (mfResetComboGracePeriod <= 0.0f)
            {
                mbAboutToResetCombo = false;
                miCurrentComboCount = 0;                                    // +0x2E0
            }
        }

        // ---- 0x82320A8C..0x82320AB4: "the player car has moved" ---------------------------
        // Either speed over its threshold restarts the idle clock. Two DIFFERENT constants and
        // two different quantities: 4.0 m/s of linear speed, 6.0 rad/s of spin -- a car being
        // spun on the spot counts as moving.
        if (lfLinearSpeed > KF_MOVED_LINEAR_SPEED || lfAngularSpeed > KF_MOVED_ANGULAR_SPEED)
        {
            mfTimeSincePlayerCarMoved = 0.0f;                               // +0x40
        }

        // ---- 0x82320AB8..0x82320BD8: THE DISTANCE TRAVELLED -------------------------------
        // ⭐ This is the number the showtime HUD's "Distance" field renders.
        // Gated on mPlayerPosLastFrame having been written at least once (ClearData zeroes it),
        // so the first frame of a crash contributes nothing.
        if (IsVectorSet(mPlayerPosLastFrame))
        {
            const Vector3 lFrameDelta = mPlayerPosLastFrame - lPlayerPosition;

            // BrnMath::Magnitude2D @0x822B1DD8 -- the XZ-plane length, so a drop contributes no
            // distance. ⚠️ A teleport/reset must not read as travel: the console zeroes the
            // measured step when the car's transform was reset this frame.
            f32 lfDistanceMovedThisFrame = BrnMath::Magnitude2D(lFrameDelta);
            if (lpPlayerState->mbResetCarTransform)                          // playerState +1102
            {
                lfDistanceMovedThisFrame = 0.0f;
            }

            // 0x82320B10..0x82320B4C -- seed the direction anchor the first time round.
            if (!IsVectorSet(mPlayerPosLastStored))
            {
                mPlayerPosLastStored = mPlayerPosLastFrame;
            }

            // 0x82320B50..0x82320B90. The sign of dot(thisFrameStep, stepFromTheAnchor) says
            // whether the car is still travelling AWAY from the anchor. Driving back over your
            // own path UNWINDS the distance rather than adding to it -- that is what stops a car
            // bouncing on the spot from inflating the score.
            const Vector3 lStoredDelta = mPlayerPosLastStored - lPlayerPosition;
            if (rw::math::vpu::Dot(lFrameDelta, lStoredDelta) >= 0.0f)
            {
                mfDistanceTravelled += lfDistanceMovedThisFrame;             // +0x308

                if (lfDistanceMovedThisFrame >= mfDistanceUntilStorePosition)
                {
                    // Re-anchor and re-arm the 20 m window.
                    mPlayerPosLastStored         = lPlayerPosition;
                    mfDistanceUntilStorePosition = KF_DISTANCE_UNTIL_STORE_POSITION;   // +0x4C
                }
                else
                {
                    mfDistanceUntilStorePosition -= lfSimTimeStep;
                }
            }
            else
            {
                // `fsel f0, f0, f0, f31` -- (x >= 0) ? x : 0.0f, i.e. clamped at zero.
                const f32 lfUnwound = mfDistanceTravelled - lfDistanceMovedThisFrame;
                mfDistanceTravelled = (lfUnwound >= 0.0f) ? lfUnwound : 0.0f;
            }
        }

        // ---- 0x82320BDC..0x82320C68: the air-time accumulators ----------------------------
        if (lpPlayerState->mfTimeInAir > 0.0f)                               // playerState +1028
        {
            mfCurrentJumpAirTime += lfSimTimeStep;                           // +0x318
            mfTotalAirTime       += lfSimTimeStep;                           // +0x314
            if (mfCurrentJumpAirTime >= mfLongestJumpAirTime)
            {
                mfLongestJumpAirTime = mfCurrentJumpAirTime;                 // +0x31C
            }

            // 0x82320C10..0x82320C2C. The down-ray's vertical distance is the height of the
            // jump; keep the largest. `fsel f0, f12, f13, f0` with f12 == (highest - vertical)
            // is max(highest, vertical).
            if (lpPlayerState->mAboveGroundTestResult.mbValid)                // playerState +488
            {
                const f32 lfVerticalDistance =
                    lpPlayerState->mAboveGroundTestResult.mfVerticalDistance; // playerState +480
                if (lfVerticalDistance > mfHighestJump)
                {
                    mfHighestJump = lfVerticalDistance;                       // +0x320
                }
            }
        }
        else
        {
            // 0x82320C34..0x82320C64. On the ground: end the jump -- but only STORE when the
            // value is not already ~zero, which is the console's own redundant-store elision
            // (`> +eps` / `>= -eps` against flt_82020B30 / flt_82002514), not a semantic test.
            if (mfCurrentJumpAirTime >  KF_VECTOR_SET_EPSILON ||
                mfCurrentJumpAirTime < -KF_VECTOR_SET_EPSILON)
            {
                mfCurrentJumpAirTime = 0.0f;
            }
        }

        // ---- 0x82320C68..0x82320CE0: the wheel-loss decay ---------------------------------
        // Count the wheels still attached, then walk the cached count DOWN one at a time,
        // crediting one car-destruction bonus per wheel lost. The console spells it as a
        // do-while because it can lose more than one wheel in a frame.
        // (WheelLite stride 112; +96 within it is mbAttached -- playerState +96/+208/+320/+432.)
        s32 liWheelsAttached = 0;
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            if (lpPlayerState->maWheels[liWheel].mbAttached)
            {
                ++liWheelsAttached;
            }
        }
        while (liWheelsAttached < miNumWheelsLastFrame)                      // +0x2D8
        {
            --miNumWheelsLastFrame;
            ++miCarDestructionBonus;                                         // +0x328
        }
        miNumWheelsLastFrame = liWheelsAttached;

        // 0x82320CDC -- the tail store the whole distance block depends on next frame.
        mPlayerPosLastFrame = lPlayerPosition;                               // +0x20
    }

    // ========================================================================
    // Live-score getters (see the ACCESSOR CLOSURE note in the banner for how each
    // was recovered). All are plain member reads -- the X360 has no assert in any of
    // them (the inlined sites are bare loads).
    // ========================================================================

    // GetNumCarsCrashed -- the TOTAL cars wrecked this crash, summed across the four
    // BrnTraffic::VehicleClass buckets. X360 (inlined @0x8232AE98):
    //   *(a2+2644) = *(a1+788) + *(a1+784) + *(a1+780) + *(a1+776)
    // i.e. maiNumCarsCrashed[3] + [2] + [1] + [0]. The asm adds them highest-index-first;
    // integer addition is associative so the ascending loop below is equivalent.
    s32 CrashModeScoring::GetNumCarsCrashed() const
    {
        s32 liTotal = 0;
        for (s32 liClass = 0; liClass < 4; ++liClass)   // +0x2E8 .. +0x2F4
        {
            liTotal += maiNumCarsCrashed[liClass];
        }
        return liTotal;
    }

    // GetScoreMultiplier -- the accumulated crash multiplier (ClearData seeds it to 1).
    // X360 (inlined): *(a2+2648) = *(a1+772) == this+0x2E4.
    s32 CrashModeScoring::GetScoreMultiplier() const
    {
        return miScoreMultiplier;        // +0x2E4
    }

    // GetCurrentComboCount -- cars in the chain currently running.
    // X360 (inlined): *(a2+2652) = *(a1+768) == this+0x2E0.
    s32 CrashModeScoring::GetCurrentComboCount() const
    {
        return miCurrentComboCount;      // +0x2E0
    }

    // GetDistanceTravelled -- metres covered since the crash started.
    // X360 (inlined): *(a2+2656) = *(a1+808) == this+0x308.
    f32 CrashModeScoring::GetDistanceTravelled() const
    {
        return mfDistanceTravelled;      // +0x308
    }

    // GetNumCarsLeapt -- how many traffic cars the player jumped over this crash.
    // DERIVED (see the banner note): the declaration at BrnCrashModeScoring.h:180 has exactly
    // one candidate member in the carved layout, miNumCarsLeaped @+0x2F8 -- which is also the
    // member DealWithVehicleLeaping increments. Consumed by the crash-score debug overlay.
    s32 CrashModeScoring::GetNumCarsLeapt() const
    {
        return miNumCarsLeaped;          // +0x2F8
    }

    // GetBestAirTime -- the longest single jump of this crash, in seconds.
    // DERIVED (see the banner note): the home's own declaration comment already pins it to
    // mfLongestJumpAirTime @+0x31C (BrnCrashModeScoring.h:183). Note this is the LONGEST
    // single jump, NOT mfTotalAirTime @+0x314 and NOT the in-progress mfCurrentJumpAirTime
    // @+0x318 -- the three are adjacent and easy to confuse.
    f32 CrashModeScoring::GetBestAirTime() const
    {
        return mfLongestJumpAirTime;     // +0x31C
    }
}
