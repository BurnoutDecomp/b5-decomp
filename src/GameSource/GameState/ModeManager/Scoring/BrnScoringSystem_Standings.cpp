// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_Standings.cpp
// ============================================================================
// Out-of-line bodies for the BrnGameState::ScoringSystem "Standings" group:
//   points leader / standings position / player team query / player-disconnect
//   tracking + the non-disconnected-player count.
//
// SHAPE from BrnScoringSystem.h (keystone); BODY reconstructed from the X360 pseudocode
// (addresses noted per method). Members accessed BY NAME -- no offset casts. The per-car
// records are reached through the keystone's GetCarData() accessors (declare-only there;
// their bodies land with the keystone's own TU) and the embedded maCarData[] array, which
// is a private ScoringSystem member visible to these member functions.
//
// Methods in the assigned range whose bodies need a type with no committed definition are
// FLAGGED BLOCKED and intentionally omitted here (see the report), per the dependency rule:
//   - SetPlayerEliminated / IsBlueTeamEliminated  -> CarScoreData eliminator(+0x60),
//       eliminations(+0x64) and eliminated-flag(+0xD9) fields are unnamed blob bytes with
//       no accessors (that record's full layout is grown by its own TU).
//   - SetPlayerTeam                               -> resets the per-car "time in current
//       team" Time field (CarScoreData +0x6C), an unnamed blob byte with no accessor.
//   - GetTimeSpentInFirstPlace/InLastPlace/Boosting -> read CarScoreData Time fields
//       (+0xE0/+0xE8/+0xF0), unnamed blob bytes with no accessors.
//   - StoreCarIds                                 -> dereferences ActiveRaceCarOutputInterface
//       (forward-declared only) to gather per-car model ids.

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // GetPointsLeader -- X360 0x82312648
    // Walk every car slot; the leader is the slot with the strictly-highest
    // cumulative points (ties keep the first/earliest slot, mirroring the strict
    // `>` test). Returns that car's network player id, or the K_INVALID_PLAYER_ID
    // (-1) seed when no slot has scored.
    // ------------------------------------------------------------------------
    BrnNetwork::NetworkPlayerID ScoringSystem::GetPointsLeader() const
    {
        s32 liBestPoints = 0;
        BrnNetwork::NetworkPlayerID lLeaderID = -1;

        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            const CarData& lrCar = maCarData[liSlot];
            if (lrCar.GetCumulativePoints() > liBestPoints)
            {
                lLeaderID    = lrCar.GetNetworkPlayerID();
                liBestPoints = lrCar.GetCumulativePoints();
            }
        }

        return lLeaderID;
    }

    // ------------------------------------------------------------------------
    // GetRaceCarStandingsPosition -- X360 0x82320190
    // 1-based standings position for a car: 1 + (number of cars with strictly
    // more cumulative points). Returns E_ACTIVE_RACE_CAR_INDEX_COUNT (8) as the
    // "not found" sentinel when the car has no record.
    // ------------------------------------------------------------------------
    s32 ScoringSystem::GetRaceCarStandingsPosition(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leActiveRaceCarIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCar = GetCarData(leRaceCarIndex);
        if (!lpCar)
        {
            return E_ACTIVE_RACE_CAR_INDEX_COUNT;
        }

        const s32 liMyPoints = lpCar->GetCumulativePoints();
        s32 liPosition = 1;
        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            const CarData* lpOther = GetCarData(static_cast<EActiveRaceCarIndex>(liSlot));
            if (lpOther && lpOther->GetCumulativePoints() > liMyPoints)
            {
                ++liPosition;
            }
        }

        return liPosition;
    }

    // ------------------------------------------------------------------------
    // GetPlayerTeam -- X360 0x8231FDB8
    // The car's current team, or E_PLAYER_TEAM_NONE (0) when it has no record.
    // ------------------------------------------------------------------------
    GameStateModuleIO::EPlayerTeam ScoringSystem::GetPlayerTeam(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leActiveRaceCarIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCar = GetCarData(leRaceCarIndex);
        if (lpCar)
        {
            return lpCar->GetTeam();
        }
        return GameStateModuleIO::E_PLAYER_TEAM_NONE;
    }

    // ------------------------------------------------------------------------
    // SetPlayerDisconnected -- X360 0x8231D950
    // Find the car holding this network player id and mark its per-car score
    // record disconnected. A miss leaves all records untouched (the X360 search
    // simply falls out of the loop).
    // ------------------------------------------------------------------------
    void ScoringSystem::SetPlayerDisconnected(BrnNetwork::NetworkPlayerID lID)
    {
        CGS_ASSERT(lID != -1, "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            if (maCarData[liSlot].GetNetworkPlayerID() == lID)
            {
                maCarData[liSlot].GetScoreData()->SetDisconnected(true);
                break;
            }
        }
    }

    // ------------------------------------------------------------------------
    // GetPlayerDisconnected -- X360 0x8231DA00
    // Look the car up by network player id and report its disconnected flag.
    // (The X360 body fires an assert and returns true when the id is not found;
    // the lookup-failure path is reproduced as that same `true` fallback.)
    // ------------------------------------------------------------------------
    bool ScoringSystem::GetPlayerDisconnected(BrnNetwork::NetworkPlayerID lID) const
    {
        CGS_ASSERT(lID != -1, "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            if (maCarData[liSlot].GetNetworkPlayerID() == lID)
            {
                return maCarData[liSlot].GetScoreData()->GetDisconnected();
            }
        }

        CGS_ASSERT(false, "BrnGameState::ScoringSystem::GetPlayerDisconnected could not find network id");
        return true;
    }

    // ------------------------------------------------------------------------
    // ClearDisconnectedPlayers -- X360 0x8231DBB0
    // Reset every car's disconnected flag.
    // ------------------------------------------------------------------------
    void ScoringSystem::ClearDisconnectedPlayers()
    {
        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            maCarData[liSlot].GetScoreData()->SetDisconnected(false);
        }
    }

    // ------------------------------------------------------------------------
    // GetNumberOfNonDisconnectedPlayers -- X360 0x8231FF80
    // Count the car slots that have a record and are still connected.
    // ------------------------------------------------------------------------
    s32 ScoringSystem::GetNumberOfNonDisconnectedPlayers()
    {
        s32 liCount = 0;
        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            const CarData* lpCar = GetCarData(static_cast<EActiveRaceCarIndex>(liSlot));
            if (lpCar && !lpCar->GetScoreData()->GetDisconnected())
            {
                ++liCount;
            }
        }
        return liCount;
    }
}
