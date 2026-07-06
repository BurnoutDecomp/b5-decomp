// =============================================================================
// GameSource/World/AI/BrnAIBuzzBy.cpp  (X360 ARTIST)
//
// BrnAI::BuzzBy -- the AI "buzz-by" director. Reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. Bodies homed here:
//   AICarCanBuzz @0x82767020, BuzzOccured @0x82771BA8, ChooseAheadOrBehind @0x827718B8,
//   IsPlayerBuzzable @0x827719F8, IsPositionInNoBuzzZone @0x82766FC0,
//   ResetActiveList @0x82771C90, StartABuzzBy @0x8278B858, Update @0x8278B8C8.
// (GetBuzzFrequency / Prepare / the remaining API land with sibling waves -- declared only.)
// =============================================================================

#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Dot / MagnitudeSquared / operator-

namespace BrnAI
{
    // --- file-scope tuning constants (DWARF-`extern`; grounded by the asm rodata uses) ------
    // Two are pinned by rodata (200.0 / 0.5); the four marked (runtime) were initialised in the
    // X360 data segment and their concrete values were NOT recovered -- defined as 0.0f
    // placeholders (not invented gameplay values) so the extern definitions link.
    const f32 KF_ON_COMING_RESET_SPEED    = 200.0f;  // flt_820C4318 (rodata-pinned)
    const f32 KF_START_FAR_AHEAD          = 0.0f;    // flt_8300DBEC (runtime; value not recovered)
    const f32 KF_START_FAR_BEHIND         = 0.0f;    // flt_8300D7F4 (runtime; value not recovered)
    const f32 KF_MIN_SPEED_FOR_BUZZING    = 0.0f;    // flt_8300D938 (runtime; value not recovered)
    const f32 KF_START_BEHIND_PROBABLITY  = 0.5f;    // flt_820C4168 (rodata-pinned)
    const f32 KF_SIDE_TURNING_PROBABILITY = 0.0f;    // flt_820C4330 (runtime; value not recovered)

    // DWARF marks BuzzBy::mRandom `extern` -- it is a TU-local file-scope static (mirroring
    // BrnRouteRequestManager's mRandom), NOT a per-instance member.
    static CgsNumeric::Random mRandom;

    // No-buzz sphere geometry (unk_8300DB30 centres / unk_820C4334 radii). The rodata values
    // were not recovered; zero-init placeholders preserve the 7-zone / 16-byte-centre /
    // 4-byte-radius shape the 0x70 loop span attests.
    static const Vector3 KA_NO_BUZZ_ZONE_CENTRES[KI_NUM_NO_BUZZ_ZONES] = {}; // unk_8300DB30
    static const f32     KAF_NO_BUZZ_ZONE_RADII[KI_NUM_NO_BUZZ_ZONES]  = {}; // unk_820C4334

    // ------------------------------------------------------------------------
    // AICarCanBuzz @0x82767020
    // The car can buzz once its buzz distance-to-player reaches 200 units.
    // ------------------------------------------------------------------------
    bool BuzzBy::AICarCanBuzz(const AICar* lpCar)
    {
        CGS_ASSERT(lpCar != NULL, "lpCar != NULL");

        // 0x82767020: lfs f13,0x1508(lpCar); lfs f0,flt_820C4318(=200.0); blt -> 0 else 1.
        return lpCar->mfBuzzDistanceToPlayer >= 200.0f;
    }

    // ------------------------------------------------------------------------
    // BuzzOccured @0x82771BA8
    // ------------------------------------------------------------------------
    bool BuzzBy::BuzzOccured(const AICar* lpPlayerCar, const AICar* lpAICar)
    {
        if (lpAICar == NULL)
        {
            return false;
        }

        // flt_820C3FA8 = 30.0f: too far from the player for the buzz to register.
        if (lpAICar->mfBuzzDistanceToPlayer > 30.0f)
        {
            return false;
        }

        const Vector3 lAIPosition      = lpAICar->GetPosition();
        const Vector3 lPlayerPosition  = lpPlayerCar->GetPosition();
        const Vector3 lPlayerDirection = lpPlayerCar->GetDirection();

        // dot(playerPos - aiPos, playerDir): negative once the AI car is behind the player's
        // facing. The buzz "occured" when that projection drops below flt_8200D5FC (-0.7f).
        const Vector3 lRelativePosition = rw::math::vpu::operator-(lPlayerPosition, lAIPosition);
        const f32 lfProjection = rw::math::vpu::Dot(lRelativePosition, lPlayerDirection);

        return -0.7f > lfProjection;
    }

    // ------------------------------------------------------------------------
    // ChooseAheadOrBehind @0x827718B8
    // ------------------------------------------------------------------------
    void BuzzBy::ChooseAheadOrBehind(AIModuleIO::ResetOnTrackRequest* lpRequest,
                                     f32 lfPlayerSpeed,
                                     EGlobalRaceCarIndex leGlobalRaceCarToTeleport)
    {
        // draw in [1,2); (draw - 1.0) is the uniform [0,1) roll vs KF_START_BEHIND_PROBABLITY.
        const f32 lfAheadLikelyHood = mRandom.RandomFloat();

        if ((lfAheadLikelyHood - 1.0f) <= KF_START_BEHIND_PROBABLITY)
        {
            // Start the reset far BEHIND the player, road-rage style. asm: f1=FAR_BEHIND+speed,
            // f2=-60.0, type=3. Construct stores f1->resetSpeed(+4), f2->resetDistance(+8).
            lpRequest->Construct(leGlobalRaceCarToTeleport,
                                 KF_START_FAR_BEHIND + lfPlayerSpeed,
                                 -60.0f,
                                 E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE);
        }
        else
        {
            // Far AHEAD: a second roll picks head-on (type 4) vs from-turnings (type 5).
            const f32 lfSideTurningsLikelyHood = mRandom.RandomFloat();

            EResetType leResetType;
            if ((lfSideTurningsLikelyHood - 1.0f) >= KF_SIDE_TURNING_PROBABILITY)
            {
                leResetType = E_RESET_TYPE_AHEAD_PLAYER_ON_COMING;
            }
            else
            {
                leResetType = E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE;
            }

            // asm: f1=KF_START_FAR_AHEAD, f2=200.0 (KF_ON_COMING_RESET_SPEED).
            lpRequest->Construct(leGlobalRaceCarToTeleport,
                                 KF_START_FAR_AHEAD,
                                 KF_ON_COMING_RESET_SPEED,
                                 leResetType);
        }
    }

    // ------------------------------------------------------------------------
    // IsPlayerBuzzable @0x827719F8
    // ------------------------------------------------------------------------
    bool BuzzBy::IsPlayerBuzzable(AICar* lpPlayerCar)
    {
        // The player must be actively driving, up to speed, not crashing or in a shortcut, and
        // we must not already have a full backlog of cars awaiting collection.
        if (lpPlayerCar->mbIsInShortcut)      // 0x154C
        {
            return false;
        }
        if (lpPlayerCar->mbIsCrashing)        // 0x1542
        {
            return false;
        }
        if (lpPlayerCar->GetSpeed() < KF_MIN_SPEED_FOR_BUZZING)
        {
            return false;
        }
        if (!lpPlayerCar->mbIsDrivenByPlayer) // 0x154A
        {
            return false;
        }
        if (miCarsAwaitingCollection >= KI_MAX_CARS_AWAITING_COLLECTION)
        {
            return false;
        }

        const Vector3 lPlayerPosition = lpPlayerCar->GetPosition();
        if (IsPositionInNoBuzzZone(lPlayerPosition))
        {
            return false;
        }

        // Count idle/active (IN_RANGE / OUT_OF_RANGE) non-player buzz cars within
        // flt_8201C220 (40000 == 200^2) of the player.
        s32 liNearbyBuzzCars = 0;
        for (s32 liActive = 0; liActive < miNumActiveCars; ++liActive)
        {
            AICar* lpCar = &mpGlobalRaceCars[maeActiveList[liActive]];

            const EAICarState leState = lpCar->GetState();
            const bool lbEligible = (leState == E_AI_CAR_STATE_IN_RANGE) ||
                                    (leState == E_AI_CAR_STATE_OUT_OF_RANGE);

            if (lbEligible && !lpCar->mbIsPlayer)
            {
                const Vector3 lCarPosition = lpCar->GetPosition();
                const Vector3 lRelativePosition =
                    rw::math::vpu::operator-(lPlayerPosition, lCarPosition);
                const f32 lfDistanceSq = rw::math::vpu::MagnitudeSquared(lRelativePosition);

                if (40000.0f > lfDistanceSq)
                {
                    ++liNearbyBuzzCars;
                }
            }
        }

        return liNearbyBuzzCars < 1;
    }

    // ------------------------------------------------------------------------
    // IsPositionInNoBuzzZone @0x82766FC0
    // Inside any no-buzz sphere?  distanceSq(lPosition, centre) < radiusSq.
    // ------------------------------------------------------------------------
    bool BuzzBy::IsPositionInNoBuzzZone(Vector3 lPosition) const
    {
        for (s32 liZoneIndex = 0; liZoneIndex < KI_NUM_NO_BUZZ_ZONES; ++liZoneIndex)
        {
            const f32 lfDistanceSq =
                rw::math::vpu::MagnitudeSquared(lPosition - KA_NO_BUZZ_ZONE_CENTRES[liZoneIndex]);
            const f32 lfRadius = KAF_NO_BUZZ_ZONE_RADII[liZoneIndex];
            if (lfDistanceSq < lfRadius * lfRadius)
                return true;
        }
        return false;
    }

    // ------------------------------------------------------------------------
    // ResetActiveList @0x82771C90
    // Rebuild the buzz-by active list from the global race-car array.
    // ------------------------------------------------------------------------
    void BuzzBy::ResetActiveList()
    {
        miNumActiveCars = 0;

        EGlobalRaceCarIndex leGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
        for (s32 liCar = 0; liCar < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++liCar)
        {
            const AICar* lpAICar = reinterpret_cast<const AICar*>(
                reinterpret_cast<const u8*>(mpGlobalRaceCars) + liCar * 0x1560);

            // Active == IN_RANGE (0) or OUT_OF_RANGE (1); and not the player car.
            const EAICarState leState = lpAICar->GetState();
            const bool lbActive = (leState == E_AI_CAR_STATE_IN_RANGE ||
                                   leState == E_AI_CAR_STATE_OUT_OF_RANGE);
            if (lbActive && !lpAICar->IsPlayerCar())
            {
                CGS_ASSERT(miNumActiveCars < 35, "Too many AI cars in buzz-by active list\n");

                maeActiveList[miNumActiveCars] = leGlobalRaceCarIndex;
                mafBuzzTimes[miNumActiveCars]  = GetBuzzFrequency(lpAICar);
                ++miNumActiveCars;
            }

            leGlobalRaceCarIndex = static_cast<EGlobalRaceCarIndex>(leGlobalRaceCarIndex + 1);
            CGS_ASSERT(leGlobalRaceCarIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT");
        }
    }

    // ------------------------------------------------------------------------
    // StartABuzzBy @0x8278B858
    // Teleport (buzz) leGlobalRaceCarToTeleport past the player.
    // ------------------------------------------------------------------------
    void BuzzBy::StartABuzzBy(const AICar* lpPlayerCar, EGlobalRaceCarIndex leGlobalRaceCarToTeleport)
    {
        CGS_ASSERT(leGlobalRaceCarToTeleport != E_GLOBAL_RACE_CAR_INDEX_INVALID,
                   "leGlobalRaceCarToTeleport != E_GLOBAL_RACE_CAR_INDEX_INVALID");

        const f32 lfPlayerSpeed = lpPlayerCar->GetSpeed();

        AIModuleIO::ResetOnTrackRequest lRequest;
        ChooseAheadOrBehind(&lRequest, lfPlayerSpeed, leGlobalRaceCarToTeleport);

        // X360 `lwz r3, 0x128(this)` (mpResetOnTrackManager) then
        // ResetOnTrackManager::PushResetOnTrackRequest(mgr, &request) @0x82769E88.
        mpResetOnTrackManager->PushResetOnTrackRequest(&lRequest);
    }

    // ------------------------------------------------------------------------
    // Update @0x8278B8C8
    // Per-frame buzz-by tick.
    // ------------------------------------------------------------------------
    void BuzzBy::Update(f32 lfTimeStep, AICar* lpPlayerCar, AICar* lpBuzzCar, bool* lpbBuzzOccured)
    {
        *lpbBuzzOccured = false;

        // While in-game / in a junkyard, freeze free-roam buzzing: reset the timer and re-arm.
        if (mbIsInGameMode || mbIsInJunkyard)
        {
            mfTimeInFreeRoam  = 0.0f;
            mbResetBuzzTimers = true;
            return;
        }

        if (mbResetBuzzTimers)
        {
            mfTimeInFreeRoam = 0.0f;
            ResetActiveList();
            mbResetBuzzTimers = false;
        }

        *lpbBuzzOccured = BuzzOccured(lpPlayerCar, lpBuzzCar);

        if (!IsPlayerBuzzable(lpPlayerCar))
            return;

        mfTimeInFreeRoam += lfTimeStep;

        EGlobalRaceCarIndex leCarToTeleport = E_GLOBAL_RACE_CAR_INDEX_COUNT;   // 35 sentinel
        const AICar*        lpChosenCar     = nullptr;
        s32                 liChosenSlot    = -1;
        bool                lbFound         = false;

        for (s32 liEntry = 0; liEntry < miNumActiveCars; ++liEntry)
        {
            // Timer elapsed (buzz overdue) for this entry?
            if (mfTimeInFreeRoam > mafBuzzTimes[liEntry] &&
                (mfTimeInFreeRoam - mafBuzzTimes[liEntry]) > 0.0f)
            {
                const EGlobalRaceCarIndex leIndex = maeActiveList[liEntry];
                const AICar* lpAICar = reinterpret_cast<const AICar*>(
                    reinterpret_cast<const u8*>(mpGlobalRaceCars) + leIndex * 0x1560);

                const EAICarState leState = lpAICar->GetState();
                const bool lbActive = (leState == E_AI_CAR_STATE_IN_RANGE ||
                                       leState == E_AI_CAR_STATE_OUT_OF_RANGE);
                if (lbActive && AICarCanBuzz(lpAICar))
                {
                    leCarToTeleport = leIndex;
                    lpChosenCar     = lpAICar;
                    liChosenSlot    = liEntry;
                    lbFound         = true;
                }
            }
        }

        if (lbFound)
        {
            StartABuzzBy(lpPlayerCar, leCarToTeleport);

            // Re-seed the chosen entry's buzz timer from the buzzed car's frequency.
            mafBuzzTimes[liChosenSlot] = mfTimeInFreeRoam + GetBuzzFrequency(lpChosenCar);
        }
    }
}
