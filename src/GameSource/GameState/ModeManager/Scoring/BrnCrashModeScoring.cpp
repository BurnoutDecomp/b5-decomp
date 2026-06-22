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
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the X360 assert sites in these bodies)
#include "GameShared/GameClasses/Core/CgsID.h"        // CgsIDUnCompress (GetVehicleScoreData diagnostic)

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
        std::memset(maPlayerPosLastFrame, 0, sizeof(maPlayerPosLastFrame));
        std::memset(maPlayerPosLastStored, 0, sizeof(maPlayerPosLastStored));

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
    // (event+0x18, magnitude-masked) against flt_82020B30 gates a car-destruction-bonus
    // bump: when the value is NOT greater than the threshold, ++miCarDestructionBonus.
    //
    // FLAG: TriggerCrashBreakerEvent is forward-declared (pointer-only) here -- its float
    // field @+0x18 cannot be named until that event type is homed; read via a flagged
    // reinterpret (mirrors DealWithVehicleLeaping). flt_82020B30 is unexported rodata,
    // modelled as a 0.0f placeholder threshold (FLAG). Confirm both when they land.
    // ------------------------------------------------------------------------
    void CrashModeScoring::DealWithCrashbreakerRequest(const TriggerCrashBreakerEvent* lpEvent)
    {
        mfTimeSinceLastEvent = 0.0f;
        const f32 KF_CRASHBREAKER_REQUEST_THRESHOLD = 0.0f; // FLAG: flt_82020B30 value unrecovered
        const f32 lfRequestValue =
            *reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(lpEvent) + 0x18); // FLAG: un-homed event field
        if (!(lfRequestValue > KF_CRASHBREAKER_REQUEST_THRESHOLD))
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
        BrnTraffic::VehicleScoreCategory leCategory = BrnTraffic::E_VEHICLESCORECATEGORY_CAR;
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
    // FLAG: K_VEHICLE_SCORE_LOOKUP_TABLE's per-row score / multiplier / category COLUMNS are
    // not recoverable from this dossier -- the table lives in X360 rodata (unk_82020FA8),
    // which is not exported as a dossier. The 24 vehicle-type CgsID keys ARE attested (DWARF
    // BrnCrashModeScoring.cpp:53-76 K_ID_*); the table below is seeded with those keys and
    // ZERO score/mult/category placeholders, clearly flagged. The lookup-loop control flow,
    // the row field offsets, and the full VehicleClass fallback (the only part with attested
    // values: car 0/750/0, van 1/2000/0, bus 3/5000/0, bigrig 4/10000/0) are reconstructed
    // store-for-store. Fill the table columns when the rodata is exported.
    // ------------------------------------------------------------------------
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

        // The 24 attested vehicle-type keys (DWARF K_ID_* constants). FLAG: score/mult/category
        // columns are placeholders pending the rodata table export (see header note above).
        static const VehicleScoreLookup K_VEHICLE_SCORE_LOOKUP_TABLE[24] =
        {
            { 0x0000000070360000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_CAR, 0, 0 }, // K_ID_COMPACT      (FLAG cols)
            { 0x0000000098700000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_CAR, 0, 0 }, // K_ID_ASIAN_SALOON
            { 0x00000000D2D60000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_CAR, 0, 0 }, // K_ID_US_SALOON
            { 0x00000000FE33C000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_CAR, 0, 0 }, // K_ID_MPV
            { 0x000000000A360000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_CAR, 0, 0 }, // K_ID_PICKUP
            { 0x0000000070B60000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_CAR, 0, 0 }, // K_ID_STATION_WAGON
            { 0x00000000B07AD000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_CAR, 0, 0 }, // K_ID_SUBURBAN_SUV
            { 0x00000000B053C000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_CAR, 0, 0 }, // K_ID_RURAL_SUV
            { 0x00000000736BC000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_CAR, 0, 0 }, // K_ID_TAXI
            { 0x00000000D1760000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_CAR, 0, 0 }, // K_ID_LIMO
            { 0x00000000A7700000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_BUS, 0, 0 }, // K_ID_CITY_BUS
            { 0x000000009B940000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_BUS, 0, 0 }, // K_ID_TOUR_BUS
            { 0x0000000070308000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_VAN, 0, 0 }, // K_ID_SMALL_RV
            { 0x000000009B0BC000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_VAN, 0, 0 }, // K_ID_COUNTRY_WAGON
            { 0x000000009B32D000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_VAN, 0, 0 }, // K_ID_CHERRY_PICKER
            { 0x000000009B59E000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_VAN, 0, 0 }, // K_ID_SMALL_BOX_TRUCK
            { 0x000000009B80F000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_VAN, 0, 0 }, // K_ID_SMALL_TOW_TRUCK
            { 0x000000002F940000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_VAN, 0, 0 }, // K_ID_VAN
            { 0x0000000085070000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_VAN, 0, 0 }, // K_ID_24_RESCUE_VAN
            { 0x0000000090E30000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_VAN, 0, 0 }, // K_ID_CHANNEL_5_VAN
            { 0x000000003B700000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_VAN, 0, 0 }, // K_ID_REPAIR_VAN
            { 0x000000004B160000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_VAN, 0, 0 }, // K_ID_MAIL_VAN
            { 0x00000000C6280000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_BIGRIG, 0, 0 }, // K_ID_GILLETTE
            { 0x0000000056400000ULL, BrnTraffic::E_VEHICLESCORECATEGORY_BIGRIG, 0, 0 }, // K_ID_DIESEL
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
            *lpeCategory = BrnTraffic::E_VEHICLESCORECATEGORY_CAR;    *lpiScore = 750;   *lpiMultiplier = 0;
            break;
        case BrnTraffic::E_VEHICLECLASS_VAN:
            *lpeCategory = BrnTraffic::E_VEHICLESCORECATEGORY_VAN;    *lpiScore = 2000;  *lpiMultiplier = 0;
            break;
        case BrnTraffic::E_VEHICLECLASS_BUS:
            *lpeCategory = BrnTraffic::E_VEHICLESCORECATEGORY_BUS;    *lpiScore = 5000;  *lpiMultiplier = 0;
            break;
        case BrnTraffic::E_VEHICLECLASS_BIGRIG:
            *lpeCategory = BrnTraffic::E_VEHICLESCORECATEGORY_BIGRIG; *lpiScore = 10000; *lpiMultiplier = 0;
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

    // ------------------------------------------------------------------------
    // Update  (X360 0x82320808)  -- per-frame driver
    // Advances the crash-mode timers/state once per simulation step (lfSimTimeStep). The
    // scalar/timer bookkeeping is reconstructed BY NAME from the proven store offsets:
    //   - advance the running clocks (mfTimeSinceModeStart, mfTimeSincePlayerCarMoved,
    //     mfTimeSinceLastEvent, mfTimeSinceLastHitOverheadSign) by the step,
    //   - latch mbPlayerIsCrashing / mfPlayerBoostPercentage from the player race-car state,
    //   - drive the wall-contact grace timer (mfTimeContactingWall, mbAboutToResetCombo,
    //     mfResetComboGracePeriod) and break the combo when it lapses,
    //   - drive the air-time accumulators (mfTotalAirTime / mfCurrentJumpAirTime /
    //     mfLongestJumpAirTime / mfHighestJump) and the distance/wheel bookkeeping,
    //   - decay miNumWheelsLastFrame toward the live attached-wheel count, crediting the
    //     car-destruction bonus (miCarDestructionBonus) per wheel lost.
    //
    // FLAG: the bulk of Update reads the player RaceCarState and the traffic-state queue
    // through the RCEntityActiveRaceCarOutputInterface / PhysicalTrafficStateQueue accessors
    // and performs the speed / travel-direction / in-air tests with rw::math::vpu VMX vector
    // intrinsics (Magnitude / Dot / IsValid / vsel masks). Those interface types and the
    // VMX vector ops are NOT homed in this scope, so the interface-driven branches cannot be
    // bodied store-for-store here without dragging their includes into the keystone's
    // by-value embed. This body reconstructs ONLY the interface-independent scalar/timer
    // bookkeeping that the asm proves by raw member offset; the interface reads are gated
    // behind the non-null assert and otherwise left to land when those types are homed.
    // ------------------------------------------------------------------------
    void CrashModeScoring::Update(
            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
            const VehicleOutputInterface::PhysicalTrafficStateQueue* /*lpTrafficStateQueue*/,
            f32 lfSimTimeStep)
    {
        CGS_ASSERT(lpActiveRaceCarInterface != nullptr, "lpActiveRaceCarInterface != NULL");

        // Advance the running clocks (the X360 stores these unconditionally at the top).
        mfTimeSinceModeStart           += lfSimTimeStep;   // *(a1+72)
        mfTimeSincePlayerCarMoved      += lfSimTimeStep;   // *(a1+64)
        mfTimeSinceLastEvent           += lfSimTimeStep;   // *(a1+68)
        mfTimeSinceLastHitOverheadSign += lfSimTimeStep;   // *(a1+780)

        // FLAG: the remaining per-frame work (player crashing/boost latch @+0x54/+0x50, the
        // wall-contact grace timer @+0x310/+0x304/+0x300, the air-time accumulators
        // @+0x314/+0x318/+0x31C/+0x320, the distance @+0x308, and the wheel-loss decay
        // @+0x2D8/+0x328) is driven by the RaceCarState pulled from lpActiveRaceCarInterface
        // and the traffic-state queue via VMX vector math. Those reads/ops require the
        // RCEntityActiveRaceCarOutputInterface / PhysicalTrafficStateQueue accessors and the
        // rw::math::vpu intrinsics, neither homed in this scope; they land when those types
        // are homed. The clock advance above is the interface-independent core.
    }
}
