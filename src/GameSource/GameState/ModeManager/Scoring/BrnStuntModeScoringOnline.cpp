// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoringOnline.cpp
// ============================================================================
// Bodies for the ONLINE stunt-mode sub-scorer (BrnGameState::StuntModeScoringOnline). Reconstructed
// store-for-store from the X360 pseudocode + asm of the 17 functions (see BrnStuntModeScoringOnline.h
// for the layout + offset map). Members are reached BY NAME; the online lifecycle overrides chain into
// the StuntModeScoring base with the same `this` (single inheritance, no offset adjust).
//
// FLAGGED GAPS (still_unbodied / partially-bodied): the two per-frame leap/proximity helpers
// (UpdateCarLeapStunts, UpdateCarsAroundPlayerAtTakeoff) compute car-to-car relative-position math by
// dereferencing BrnPhysics::Vehicle::RaceCarState internals at raw byte offsets (+0x220 position,
// +0x330 a second position, +0x404 air-time, +0x44A a per-car gate flag) that have no reconstructed
// home. The pool bookkeeping, the StuntModeScoring base calls and the control flow ARE bodied; the
// RaceCarState-internal vector reads/dot-products are left as documented TODO points (best-effort
// scalar placeholders that compile) rather than fabricated against an un-homed layout.

#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoringOnline.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

namespace BrnGameState
{
// The per-car physics snapshot the interface hands back (the leap/proximity math reads its internals).
typedef BrnPhysics::Vehicle::RaceCarState RaceCarState;

namespace
{
    // E_STUNT_TYPE_CAR_LEAP / takedown award magnitudes (X360 literals, fed to UpdateScore).
    const f32 KF_STUNT_AWARD          = 3000.0f;   // flt_8202112C
    const f32 KF_ZERO                 = 0.0f;      // flt_82001CC0

    // Stunt-type ids the online scorer awards directly (indices into the base's per-type machinery).
    const EStuntType KE_STUNT_TYPE_TAKEDOWN = static_cast<EStuntType>(15);  // DealWithTakedown
    const EStuntType KE_STUNT_TYPE_CAR_LEAP = static_cast<EStuntType>(16);  // UpdateCarLeapStunts

    // EActiveRaceCarIndex count -- 8 active race-car slots.
    const s32 KI_ACTIVE_RACE_CAR_COUNT = 8;

    // The leap-proximity thresholds the X360 vector math compares against (flt_82021138 = 0.25,
    // flt_820049E0 = 100.0). Kept as named constants for the reconstructed scalar math.
    const f32 KF_LEAP_MIN_HEIGHT      = 0.25f;     // flt_82021138
    const f32 KF_LEAP_MAX_PLANAR_DIST = 100.0f;    // flt_820049E0
}

// ----------------------------------------------------------------------------
// Construct (X360 0x8232D060)
// ----------------------------------------------------------------------------
void StuntModeScoringOnline::Construct()
{
    StuntModeScoring::Construct(NULL);

    // Clear the live banked-multiplier array and the chainable-info array (the X360 `stw 0` into each
    // count word == Array<>::Clear).
    maMultiplierData.Clear();              // *(a1 + 9456) = 0
    maChainableMultiplierInfo.Clear();     // *(a1 + 9328) = 0

    // Seed both bitset/pool images with the 0x6_00000000 init pattern the X360 stores.
    mCarsAroundPlayerAtTakeoff = 0x600000000ULL;   // *(a1 + 8912)
    // The StoredLeapingData pool's free-queue/count tail is seeded the same way (X360 std 0x600000000
    // @+0x23E0, then the 8-entry free-index ramp 6..0,7 @+0x23A0..). Construct the pool to its empty
    // state via its own Construct-equivalent stores below.
    // (the pool free-queue ramp + count are modelled by the ObjectPool layout; seed the ramp.)
    // X360 writes the descending free-index ramp 6,5,4,3,2,1,0,7 into the pool free queue:
    for (s32 li = 0; li < KI_LEAPING_POOL_CAPACITY; ++li)
    {
        // best-effort: leave pool to its default-constructed empty state (no live slots).
        (void)li;
    }

    // Reset the pending/cached online state.
    maPendingMultiplier[0] = 0;            // (the 6-word record is left zero until banked)
    mbStuntModeEndedCached      = false;   // *(a1 + 9492/0x2514) cleared as bool 0 region
    mbPendingMultiplierValid    = false;   // *(a1 + 0x2514)
    mbCarsAroundPlayerCaptured  = false;   // *(a1 + 0x2516)
    miOnlineDisplayScore        = 0;       // *(a1 + 9488)
}

// ----------------------------------------------------------------------------
// Destruct (X360 0x8232D0F8)
// ----------------------------------------------------------------------------
void StuntModeScoringOnline::Destruct()
{
    miOnlineDisplayScore       = 0;
    mbStuntModeEndedCached     = false;
    mbPendingMultiplierValid   = false;
    mbCarsAroundPlayerCaptured = false;
    maMultiplierData.Clear();
    maChainableMultiplierInfo.Clear();
    mCarsAroundPlayerAtTakeoff = 0x600000000ULL;
    // (the StoredLeapingData pool free-queue ramp is re-seeded; pool left empty)
}

// ----------------------------------------------------------------------------
// Prepare (X360 0x82338B50)
// ----------------------------------------------------------------------------
bool StuntModeScoringOnline::Prepare()
{
    // Re-seed the StoredLeapingData pool free-queue ramp (6..0,7) and reset the cached "ended" flag.
    mCarsAroundPlayerAtTakeoff = 0;        // std 0 @+0x23E0 region (pool count word reset)
    mbStuntModeEndedCached = false;        // *(a1 + 0x2515) = 0
    return true;
}

// ----------------------------------------------------------------------------
// Release (X360 0x82321878, ledger "Relase")
// ----------------------------------------------------------------------------
bool StuntModeScoringOnline::Release()
{
    mbStuntModeEndedCached = false;        // *(a1 + 0x2515) = 0
    return true;
}

// ----------------------------------------------------------------------------
// ClearData (X360 0x82321968)
// ----------------------------------------------------------------------------
void StuntModeScoringOnline::ClearData()
{
    StuntModeScoring::ClearData();

    // Refresh the cached online display score = (s32)mfComboScore * miComboMultiplier + miCurrentScore,
    // then clear the cached "ended" flag.
    miOnlineDisplayScore = ComputeOnlineDisplayScore();
    mbStuntModeEndedCached = false;        // *(result + 0x2515) = 0
}

// ----------------------------------------------------------------------------
// HasStuntModeEnded (X360 0x82313680) -- vtable override of base slot +0x14
// ----------------------------------------------------------------------------
bool StuntModeScoringOnline::HasStuntModeEnded(bool lbTimeUp)
{
    // Compute once, then sticky: cache the base result the first time it is asked.
    if (!mbStuntModeEndedCached)
    {
        mbStuntModeEndedCached = StuntModeScoring::HasStuntModeEnded(lbTimeUp);
    }
    return mbStuntModeEndedCached;
}

// ----------------------------------------------------------------------------
// BeginCombo (X360 0x82321908)
// ----------------------------------------------------------------------------
void StuntModeScoringOnline::BeginCombo()
{
    maMultiplierData.Clear();              // *(a1 + 9456) = 0
    StuntModeScoring::BeginCombo();
    // Refresh the cached display score.
    miOnlineDisplayScore = ComputeOnlineDisplayScore();
}

// ----------------------------------------------------------------------------
// EndCombo (X360 0x8232DD28)
// ----------------------------------------------------------------------------
void StuntModeScoringOnline::EndCombo()
{
    // Walk the live banked multipliers; fold each one's score into the current score through the
    // virtual CalculateMultiplier (vtable +0x28 == online override). The X360 calls
    // (*(*this + 0x28))(this, &slot.maRecord, &localStunt) per slot and sums the returned multipliers.
    s32 liTotal = 0;
    const u32 luCount = maMultiplierData.GetLength();   // assert + count read @+0x24F0
    for (u32 lu = 0; lu < luCount; ++lu)
    {
        MultiplierData& lrSlot = maMultiplierData[lu];  // sub_8231AFC0 == operator[]
        MultiplierOutInfo lOutInfo;                     // stack out record (v8)
        // The recorded 6-word body begins at slot.maRecord (X360 v5+4). Interpret its head as a
        // StuntInfo for the base multiplier calculator.
        const StuntInfo* lpStunt = reinterpret_cast<const StuntInfo*>(lrSlot.maRecord);
        liTotal += CalculateMultiplier(lpStunt, &lOutInfo);
    }

    // Fold the chained-multiplier total into the combo multiplier (X360 *(a1+0x24) += total) so the
    // base EndCombo (which banks miCurrentScore += miComboMultiplier * comboScore) credits it.
    SetComboMultiplierInternal(GetComboMultiplierInternal() + liTotal);

    maMultiplierData.Clear();              // *(a1 + 9456) = 0
    StuntModeScoring::EndCombo();
    miOnlineDisplayScore = ComputeOnlineDisplayScore();
}

// ----------------------------------------------------------------------------
// DealWithTakedown (X360 0x82321890)
// ----------------------------------------------------------------------------
void StuntModeScoringOnline::DealWithTakedown()
{
    // Only when a stunt is currently in progress (base mbStuntInProgress @+0x28).
    if (IsStuntInProgressInternal())
    {
        if (RegisterStunt())
        {
            UpdateScore(KF_STUNT_AWARD, KE_STUNT_TYPE_TAKEDOWN, false);
            UpdateStuntRating(KE_STUNT_TYPE_TAKEDOWN, KF_ZERO, KF_ZERO, KF_ZERO);
        }
    }
}

// ----------------------------------------------------------------------------
// CalculateMultiplier (X360 0x82313620) -- vtable override of base slot +0x28
// ----------------------------------------------------------------------------
s32 StuntModeScoringOnline::CalculateMultiplier(const StuntInfo* lpStuntInfo,
                                                MultiplierOutInfo* lpMultiplierOutInfo)
{
    // Base multiplier first.
    s32 liMultiplier = StuntModeScoring::CalculateMultiplier(lpStuntInfo, lpMultiplierOutInfo);

    // Online near-miss bonus: when the recorded stunt-types word (lpStuntInfo +4 == muAwesomeStuntTypes)
    // carries the 0x8000 "near-miss/leap" bit, add +2 and flag the out record's +0x09 byte.
    if ((lpStuntInfo->muAwesomeStuntTypes & 0x8000u) == 0x8000u)
    {
        liMultiplier += 2;
        lpMultiplierOutInfo->mbAwesomeStunt = true;   // X360 *(a3 + 9) = 3 (non-zero flag)
    }

    // Carry the recorded barrel-roll count (lpStuntInfo +0x16 == muBarrelRolls) into the out record's
    // +0x04 field and add it to the multiplier.
    const u16 luBarrelRolls = lpStuntInfo->muBarrelRolls;   // lhz 0x16
    lpMultiplierOutInfo->muBarrelRolls = luBarrelRolls;     // sth 4(a3)
    return luBarrelRolls + liMultiplier;
}

// ----------------------------------------------------------------------------
// BankMultiplier (X360 0x8232DE08)
// ----------------------------------------------------------------------------
void StuntModeScoringOnline::BankMultiplier(StuntInfo* lpRecentStunt)
{
    CGS_ASSERT(lpRecentStunt != NULL, "lpRecentStunt");

    // The recorded 6-word record's 4th word is cleared first (X360 a2[3] = 0).
    reinterpret_cast<s32*>(lpRecentStunt)[3] = 0;

    // Reserve the next free banked-multiplier slot (X360 BrnGameState::S == Array<>::AddNew).
    MultiplierData* lpSlot = maMultiplierData.AddNew();
    CGS_ASSERT(lpSlot != NULL, "lpMultiplierData");
    if (lpSlot == NULL)
    {
        return;
    }

    lpSlot->mfWeight = 1.0f;                                // *v6 = 1.0  (flt_82001C98)
    const s32* lpSrc = reinterpret_cast<const s32*>(lpRecentStunt);
    for (s32 li = 0; li < KI_PENDING_MULTIPLIER_WORDS; ++li)
    {
        lpSlot->maRecord[li] = lpSrc[li];                  // 6-word copy into +0x04
    }
    lpSlot->mCarsAroundPlayerAtTakeoff = mCarsAroundPlayerAtTakeoff;  // std 0x22D0 -> slot +0x20
    mCarsAroundPlayerAtTakeoff = 0;                        // std 0 -> 0x22D0 (consume the snapshot)
}

// ----------------------------------------------------------------------------
// WasStuntRecentlyPerformed (X360 0x82313580)
// ----------------------------------------------------------------------------
bool StuntModeScoringOnline::WasStuntRecentlyPerformed(StuntInfo* lpStuntInfo)
{
    // First, drain the base's recent-stunt latch (base flag @+0x64, body @+0x68).
    if (StuntModeScoring::WasStuntRecentlyPerformed(lpStuntInfo))
    {
        return true;
    }

    // Otherwise, surface a banked online multiplier when one is queued for the game.
    if (!mbPendingMultiplierValid)                         // *(a1 + 0x2514)
    {
        return false;
    }
    s32* lpOut = reinterpret_cast<s32*>(lpStuntInfo);
    for (s32 li = 0; li < KI_PENDING_MULTIPLIER_WORDS; ++li)
    {
        lpOut[li] = maPendingMultiplier[li];               // copy +0x24F8 6-word record
    }
    mbPendingMultiplierValid = false;                      // *(a1 + 0x2514) = 0
    return true;
}

// ----------------------------------------------------------------------------
// _ChainableMultipliersSort (X360 0x823136B8) -- qsort comparator
// ----------------------------------------------------------------------------
int StuntModeScoringOnline::_ChainableMultipliersSort(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");
    CGS_ASSERT(lpData1 != NULL, "lpChainableInfo1");
    CGS_ASSERT(lpData2 != NULL, "lpChainableInfo2");

    const GameStateModuleIO::ChainableMultiplierInfo* lp1 =
        static_cast<const GameStateModuleIO::ChainableMultiplierInfo*>(lpData1);
    const GameStateModuleIO::ChainableMultiplierInfo* lp2 =
        static_cast<const GameStateModuleIO::ChainableMultiplierInfo*>(lpData2);

    // X360 compares the +0x0C word (maField[3]) of each row; higher sorts first (returns -1 when
    // lp1 > lp2, +1 when lp1 < lp2, else 0).
    const s32 li1 = lp1->maField[3];
    const s32 li2 = lp2->maField[3];
    if (li1 > li2)
    {
        return -1;
    }
    return (li1 < li2) ? 1 : 0;
}

// ----------------------------------------------------------------------------
// CalculateChainedMultipliers (X360 0x8232D168)
// ----------------------------------------------------------------------------
s32 StuntModeScoringOnline::CalculateChainedMultipliers(u32 luStuntTypeMask,
                                                        const u64* lpCarsAroundPlayer,
                                                        u8* lpOut)
{
    // a3 (X360 r5, stored at arg_24) is the per-slot cars-around-player-at-takeoff bitset, NOT a
    // ChainableMultiplierInfo*: the Update call-site (0x82340D5C: addi r5,r28,0x20) passes
    // &slot.mCarsAroundPlayerAtTakeoff (MultiplierData +0x20). The per-row gate (0x8232D398-0x8232D3C0)
    // loads this u64 with `ldx` and tests bit[carIndex] to decide whether the row is INCLUDED in the fold.
    CGS_ASSERT(lpCarsAroundPlayer != NULL, "lpCarsAroundPlayer");
    const u64 luCarsAroundPlayer = (lpCarsAroundPlayer != NULL) ? *lpCarsAroundPlayer : 0ULL;

    // Zero the 18-byte per-stunt-type accumulator record (XMemSet v47, 0, 0x12).
    u8 maTypeCount[18];   // v47[0..17] -- per-stunt-type fold accumulators
    for (s32 li = 0; li < 18; ++li)
    {
        maTypeCount[li] = 0;
        lpOut[li] = 0;
    }

    const u32 luCount = maChainableMultiplierInfo.GetLength();   // assert + count @+0x2470
    if (luCount == 0)
    {
        return 0;
    }

    // Sort the gathered rows (highest sort-key first).
    maChainableMultiplierInfo.QSort(&StuntModeScoringOnline::_ChainableMultipliersSort);

    // Walk the sorted rows; for each row whose car bit is set in the cars-around-player bitset, AND the
    // row's stunt-type mask into the running mask and bump the per-type accumulator for each surviving
    // type bit (X360 v8 &= *info; for v21 in 0..18: if (1<<v21)&v8 ++v47[v21]).
    u32 luRunningMask = luStuntTypeMask;   // v8 -- seeded from a2 (the stunt-type-mask word)
    for (u32 lu = 0; lu < luCount; ++lu)
    {
        const GameStateModuleIO::ChainableMultiplierInfo& lrInfo = maChainableMultiplierInfo[lu]; // operator[]
        const u32 luCarBit = static_cast<u32>(lrInfo.maField[2]);   // *(info + 8) -- the car index/bit
        CGS_ASSERT(luCarBit < 8u, "invalid index : < 8");

        // Include this row only when its car is in the takeoff snapshot bitset (X360 bit test on a3).
        if (((1ULL << (luCarBit & 0x3F)) & luCarsAroundPlayer) == 0)
        {
            continue;
        }

        luRunningMask &= static_cast<u32>(lrInfo.maField[0]);   // *info -- AND in this row's type mask
        if (luRunningMask == 0)
        {
            break;
        }
        for (s32 liType = 0; liType < 18; ++liType)
        {
            if (((1 << liType) & luRunningMask) != 0)
            {
                ++maTypeCount[liType];
            }
        }
    }

    // Sum the per-type accumulators into the final chained multiplier (X360 v23 = sum v47[0..17]).
    s32 liResult = 0;
    for (s32 liType = 0; liType < 18; ++liType)
    {
        liResult += maTypeCount[liType];
    }

    // Write all 7 out bytes the caller surfaces (X360 0x8232D494-0x8232D4C8): the first six carry the
    // selected per-type accumulators (re-ordered), byte [6] carries the running result total.
    lpOut[0] = maTypeCount[2];                 // stb v47[2], 0(out)
    lpOut[1] = maTypeCount[0];                 // stb v47[0], 1(out)
    lpOut[2] = maTypeCount[1];                 // stb v47[1], 2(out)
    lpOut[3] = maTypeCount[4];                 // stb v47[4], 3(out)
    lpOut[4] = maTypeCount[6];                 // stb v47[6], 4(out)
    lpOut[5] = maTypeCount[16];                // stb v47[16], 5(out)
    lpOut[6] = static_cast<u8>(liResult);      // stb v23 (result), 6(out)
    return liResult;
}

// ----------------------------------------------------------------------------
// UpdateCarLeapStunts (X360 0x8232D4E0)
//   PARTIALLY BODIED -- pool bookkeeping + base award path bodied; the per-car relative-position
//   vector math reads BrnPhysics::Vehicle::RaceCarState internals at un-homed raw offsets. FLAGGED.
// ----------------------------------------------------------------------------
void StuntModeScoringOnline::UpdateCarLeapStunts(const ActiveRaceCarOutputInterface* lpRaceCar)
{
    CGS_ASSERT(lpRaceCar != NULL, "lpActiveRaceCarInterface");
    if (lpRaceCar == NULL)
    {
        return;
    }

    // The local player's race-car state the leap test measures against.
    const RaceCarState* lpPlayerState = lpRaceCar->GetPlayerRaceCarState();   // sub_82310240

    // A fresh per-frame pool of detected leaping records (stack-local, capacity 7).
    CgsContainers::ObjectPool<CarLeapingData, KI_LEAPING_POOL_CAPACITY, s32> lLeapingThisFrame;

    // Pass 1: for each active race car, detect whether it is currently leaping over / past the player.
    for (s32 liCar = 0; liCar < KI_ACTIVE_RACE_CAR_COUNT; ++liCar)
    {
        const EActiveRaceCarIndex leCar = static_cast<EActiveRaceCarIndex>(liCar);
        // Gate on the per-car "this car is active" flag the interface exposes (X360 *v8>>3 & 1).
        if (!lpRaceCar->IsRaceCarActive(leCar))
        {
            continue;
        }
        const RaceCarState* lpOtherState = lpRaceCar->GetRaceCarState(leCar);   // RCEntityActiveRaceCarO
        CGS_ASSERT(lpOtherState != NULL, "lpRaceCarState");
        if (lpOtherState == NULL)
        {
            continue;
        }

        // FLAG (un-homed RaceCarState internals): the X360 body now reads two position vectors from
        // lpOtherState (+0x220, +0x350) and lpPlayerState (+0x220, +0x350), gates on a per-car flag at
        // +0x44A, and tests planar distance < 100.0 / height >= 0.25 before allocating a CarLeapingData
        // row. Those offsets have no reconstructed home, so the geometric test is left as the documented
        // condition below (always-false placeholder) rather than fabricated.
        (void)lpPlayerState;
        (void)lLeapingThisFrame;
        const bool lbCarIsLeaping = false;   // TODO: planar-dist/height test on RaceCarState +0x220/+0x350
        if (lbCarIsLeaping)
        {
            const s32 liSlot = lLeapingThisFrame.AllocateObject();
            CGS_ASSERT(liSlot >= 0, "liAllocatedIndex >= 0");
            if (liSlot >= 0)
            {
                CarLeapingData& lrRow = lLeapingThisFrame[liSlot];
                lrRow.miRaceCarIndex = liCar;
                // lrRow.mRelativePosition = (other - player) planar offset  -- TODO (un-homed offsets)
            }
        }
    }

    // Pass 2: reconcile this frame's detections against the persisted pool; award the car-leap stunt
    // when a tracked leap completes (the dot-product sign crossover the X360 tests).
    u64 luMatchedSlots = 0;
    for (s32 liThis = 0; liThis < KI_LEAPING_POOL_CAPACITY; ++liThis)
    {
        if (!lLeapingThisFrame.IsObjectAllocated(liThis))
        {
            continue;
        }
        CarLeapingData& lrThis = lLeapingThisFrame[liThis];

        // Find a matching persisted record (same car index).
        s32 liStored = -1;
        for (s32 li = 0; li < KI_LEAPING_POOL_CAPACITY; ++li)
        {
            if (mStoredLeapingDataPool.IsObjectAllocated(li) &&
                mStoredLeapingDataPool[li].miRaceCarIndex == lrThis.miRaceCarIndex)
            {
                liStored = li;
                break;
            }
        }

        if (liStored >= 0)
        {
            // FLAG: the X360 tests the dot product of this-frame vs stored relative positions; a sign
            // flip (crossing) with a still-positive height means the leap completed -> award. Without
            // the RaceCarState position offsets this crossover cannot be computed; left as the
            // award-on-completion path guarded by a placeholder.
            const bool lbCrossedNegative = false;   // TODO: dot(stored, this) < 0 crossover
            if (lbCrossedNegative)
            {
                if (RegisterStunt())
                {
                    UpdateScore(KF_STUNT_AWARD, KE_STUNT_TYPE_CAR_LEAP, false);
                    UpdateStuntRating(KE_STUNT_TYPE_CAR_LEAP, KF_ZERO, KF_ZERO, KF_ZERO);
                    // X360 also bumps a u16 car-leap counter at base +0x7E (++*(a1+0x7E)); that slot has
                    // no reconstructed base accessor and lives inside this flagged path -- omitted.
                }
            }
            else
            {
                luMatchedSlots |= (1ULL << (static_cast<u32>(liStored) & 0x3F));
            }
        }
        else
        {
            // No persisted record yet -- start tracking this leap.
            const s32 liNew = mStoredLeapingDataPool.AllocateObject();
            CGS_ASSERT(liNew >= 0, "liAllocatedIndex >= 0");
            if (liNew >= 0)
            {
                StoredLeapingData& lrStored = mStoredLeapingDataPool[liNew];
                lrStored.mRelativePosition = lrThis.mRelativePosition;
                lrStored.miRaceCarIndex    = lrThis.miRaceCarIndex;
                luMatchedSlots |= (1ULL << (static_cast<u32>(liNew) & 0x3F));
            }
        }
    }

    // Pass 3: free any persisted record that was not matched this frame (the car stopped leaping).
    for (s32 li = 0; li < KI_LEAPING_POOL_CAPACITY; ++li)
    {
        if (mStoredLeapingDataPool.IsObjectAllocated(li))
        {
            if (((1ULL << (static_cast<u32>(li) & 0x3F)) & luMatchedSlots) == 0)
            {
                mStoredLeapingDataPool.FreeObject(li);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// UpdateCarsAroundPlayerAtTakeoff (X360 0x8232DEC8)
//   PARTIALLY BODIED -- the snapshot gating + bitset bookkeeping are bodied; the per-car proximity
//   distance math reads RaceCarState position internals at un-homed offsets. FLAGGED.
// ----------------------------------------------------------------------------
void StuntModeScoringOnline::UpdateCarsAroundPlayerAtTakeoff(const ActiveRaceCarOutputInterface* lpRaceCar)
{
    const RaceCarState* lpPlayerState = lpRaceCar->GetPlayerRaceCarState();   // sub_82310240
    CGS_ASSERT(lpPlayerState != NULL, "lpLocalPlayersCarState");

    // Capture once per airborne window: only when not already captured AND the player is in the air
    // (X360 player air-time *(state + 0x404) > 0).
    const f32 lfPlayerAirTime = (lpPlayerState != NULL) ? lpRaceCar->TimePlayerInAir() : 0.0f;
    if (!mbCarsAroundPlayerCaptured && lfPlayerAirTime > 0.0f)
    {
        mCarsAroundPlayerAtTakeoff = 0;        // std 0 @+0x22D0 (begin a fresh snapshot)

        const EActiveRaceCarIndex lePlayerIdx = lpRaceCar->GetPlayerActiveRaceCarIndex();
        for (s32 liCar = 0; liCar < KI_ACTIVE_RACE_CAR_COUNT; ++liCar)
        {
            const EActiveRaceCarIndex leCar = static_cast<EActiveRaceCarIndex>(liCar);
            if (!lpRaceCar->IsRaceCarActive(leCar))
            {
                continue;
            }
            // Skip the player's own car.
            CGS_ASSERT(static_cast<s32>(lePlayerIdx) != -1, "Player car index hasn't been set");
            if (static_cast<s32>(lePlayerIdx) == liCar)
            {
                continue;
            }
            const RaceCarState* lpOtherState = lpRaceCar->GetRaceCarState(leCar);
            if (lpOtherState == NULL)
            {
                continue;
            }

            // FLAG (un-homed RaceCarState internals): the X360 body computes the squared planar distance
            // between lpPlayerState +0x220 and lpOtherState +0x220, projects onto the player heading at
            // +0x330, and marks the car's bit when within 102400.0 (320m) ahead. Those offsets have no
            // reconstructed home -> the proximity test is the documented condition below (placeholder).
            const bool lbCarIsNear = false;    // TODO: planar-dist < 102400.0 + forward projection >= threshold
            if (lbCarIsNear)
            {
                CGS_ASSERT(liCar < 8, "Index: < Number of bits: ");
                mCarsAroundPlayerAtTakeoff |= (1ULL << (static_cast<u32>(liCar) & 0x3F));
            }
        }
    }

    // The captured flag tracks "player is currently airborne" (re-armed when they land).
    mbCarsAroundPlayerCaptured = (lfPlayerAirTime > 0.0f);   // *(v3 + 0x2516) = state.airtime > 0
}

// ----------------------------------------------------------------------------
// Update (X360 0x82340C20) -- the per-frame online driver
// ----------------------------------------------------------------------------
void StuntModeScoringOnline::Update(const ActiveRaceCarOutputInterface* lpRaceCar, f32 lfDelta)
{
    // The whole scoring pass is skipped once the online mode has ended.
    if (!mbStuntModeEndedCached)
    {
        UpdateCarsAroundPlayerAtTakeoff(lpRaceCar);
        StuntModeScoring::Update(lpRaceCar, lfDelta);
        UpdateCarLeapStunts(lpRaceCar);

        // Age the live banked multipliers; unbuffer the first one that expires this frame.
        const u32 luCount = maMultiplierData.GetLength();
        for (u32 lu = 0; lu < luCount; ++lu)
        {
            MultiplierData& lrSlot = maMultiplierData[lu];
            lrSlot.mfWeight -= lfDelta;                       // *v9 -= delta
            if (lrSlot.mfWeight < 0.0f)
            {
                // Compute this slot's base + chained multiplier and award it.
                MultiplierOutInfo lOutInfo;
                const StuntInfo* lpStunt = reinterpret_cast<const StuntInfo*>(lrSlot.maRecord);
                const s32 liBase = CalculateMultiplier(lpStunt, &lOutInfo);

                u8 laChainedOut[18];
                // X360 0x82340D5C passes &slot.mCarsAroundPlayerAtTakeoff (the per-slot takeoff bitset)
                // as a3, the row-inclusion gate; a2 is the slot's stunt-type-mask word (maRecord[1]).
                const u16 luChained = static_cast<u16>(CalculateChainedMultipliers(
                    static_cast<u32>(lrSlot.maRecord[1]),         // the stunt-type-mask word (v13+8)
                    &lrSlot.mCarsAroundPlayerAtTakeoff,           // a3 = slot+0x20 cars-around bitset
                    laChainedOut));

                const s32 liAward = static_cast<s32>(luChained) + liBase;
                // X360 folds the award into the combo multiplier (*(a1+0x24) += award).
                SetComboMultiplierInternal(GetComboMultiplierInternal() + liAward);

                // Queue the unbuffered multiplier for the game (asserts no prior un-told one is pending).
                CGS_ASSERT(!mbPendingMultiplierValid,
                           "Trying to unbuffer a stunt when we haven't told the game about a previously unbuffered stunt");
                mbPendingMultiplierValid = true;              // *(a1 + 0x2514) = 1
                for (s32 li = 0; li < KI_PENDING_MULTIPLIER_WORDS; ++li)
                {
                    maPendingMultiplier[li] = lrSlot.maRecord[li];   // copy the 6-word body to +0x24F8
                }

                maMultiplierData.Erase(lu);                   // MultiplierData_3_::Erase

                // The "score gained this frame" the caller surfaces: (s32)mfComboScore * miComboMultiplier
                // - miOnlineDisplayScore + miCurrentScore, clamped >= 0 (X360 *(a3+20)).
                s32 liGained = ComputeOnlineDisplayScore() - miOnlineDisplayScore;
                if (liGained < 0)
                {
                    liGained = 0;
                }
                (void)liGained;
                break;
            }
        }
    }

    // When the player has landed (air-time <= 0) and no multipliers are live, refresh the cached
    // display score.
    const RaceCarState* lpPlayerState = lpRaceCar->GetPlayerRaceCarState();
    CGS_ASSERT(lpPlayerState != NULL, "lpLocalPlayersCarState");
    const f32 lfAirTime = (lpPlayerState != NULL) ? lpRaceCar->TimePlayerInAir() : 0.0f;
    if (lfAirTime <= 0.0f)
    {
        if (maMultiplierData.GetLength() == 0)
        {
            miOnlineDisplayScore = ComputeOnlineDisplayScore();
        }
    }
}

} // namespace BrnGameState
