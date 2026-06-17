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
// The eliminator/team/time-spent group below is now unblocked: the CarScoreData layout was
// grown by its own TU, so eliminator(+0x60), eliminations(+0x64), eliminated-flag(+0xD9),
// time-in-current-team(+0x6C) and the time-spent Time fields (+0xE0/+0xE8/+0xF0) all carry
// named members + Get/Set accessors now, reached through CarData::GetScoreData().
//
// Methods in the assigned range whose bodies still need a type with no committed definition are
// FLAGGED BLOCKED and intentionally omitted here (see the report), per the dependency rule:
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

    // ------------------------------------------------------------------------
    // SetPlayerEliminated -- X360 0x823267A0
    // Record that car leRaceCarIndex was eliminated by car leEliminator. On the
    // eliminated car's score record: stamp the eliminator's race-car index (+0x60),
    // raise the eliminated flag (+0xD9), and capture the live distance-to-finish
    // (+0x18) into the frozen distance-to-finish (+0x48). On the eliminator's record,
    // bump its elimination tally (+0x64). A missing slot is left untouched (the X360
    // null-checks each GetCarData result independently).
    // ------------------------------------------------------------------------
    void ScoringSystem::SetPlayerEliminated(EActiveRaceCarIndex leRaceCarIndex, EActiveRaceCarIndex leEliminator)
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");
        CGS_ASSERT((leEliminator > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leEliminator < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leEliminatorIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leEliminatorIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        CarData* lpEliminatedCar = GetCarData(leRaceCarIndex);
        if (lpEliminatedCar)
        {
            GameStateModuleIO::CarScoreData* lpScore = lpEliminatedCar->GetScoreData();
            lpScore->SetEliminatorRaceCarIndex(leEliminator);
            lpScore->SetEliminated(true);
            lpScore->SetDistanceToFinish(lpScore->GetDistanceToFinishLive());
        }

        CarData* lpEliminatorCar = GetCarData(leEliminator);
        if (lpEliminatorCar)
        {
            GameStateModuleIO::CarScoreData* lpEliminatorScore = lpEliminatorCar->GetScoreData();
            lpEliminatorScore->SetNumEliminations(lpEliminatorScore->GetNumEliminations() + 1);
        }
    }

    // ------------------------------------------------------------------------
    // IsBlueTeamEliminated -- X360 0x8231FEE8
    // True once every blue-team car is eliminated: the search returns false the moment
    // it finds a blue car (team == E_PLAYER_TEAM_BLUE_TEAM) whose score record is not
    // flagged eliminated; if no such live blue car exists it returns true.
    // ------------------------------------------------------------------------
    bool ScoringSystem::IsBlueTeamEliminated() const
    {
        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            const CarData* lpCar = GetCarData(static_cast<EActiveRaceCarIndex>(liSlot));
            if (lpCar &&
                lpCar->GetTeam() == GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM &&
                !lpCar->GetScoreData()->GetEliminated())
            {
                return false;
            }
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // SetPlayerTeam -- X360 0x8231FE38
    // Move a car to a new team. Only acts on an actual change: when the new team
    // differs from the car's current team, the team is written and the per-car
    // "time in current team" timer (CarScoreData +0x6C) is reset to zero.
    // ------------------------------------------------------------------------
    void ScoringSystem::SetPlayerTeam(EActiveRaceCarIndex leRaceCarIndex, GameStateModuleIO::EPlayerTeam leTeam)
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leActiveRaceCarIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        CarData* lpCar = GetCarData(leRaceCarIndex);
        CGS_ASSERT(lpCar != 0, "lpCarData");
        if (lpCar)
        {
            if (lpCar->GetTeam() != leTeam)
            {
                lpCar->SetTeam(leTeam);
                lpCar->GetScoreData()->SetTimeInCurrentTeam(CgsSystem::Time(0.0f));
            }
        }
    }

    // ------------------------------------------------------------------------
    // GetTimeSpentInFirstPlace -- X360 0x82326F28
    // The car's accumulated time-in-first-place (CarScoreData +0xE0), or a zeroed
    // Time when the car has no record.
    // ------------------------------------------------------------------------
    CgsSystem::Time ScoringSystem::GetTimeSpentInFirstPlace(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCar = GetCarData(leRaceCarIndex);
        if (lpCar)
        {
            return lpCar->GetScoreData()->GetTimeInFirstPlace();
        }
        return CgsSystem::Time(0.0f);
    }

    // ------------------------------------------------------------------------
    // GetTimeSpentInLastPlace -- X360 0x82326FC0
    // The car's accumulated time-in-last-place (CarScoreData +0xE8), or a zeroed
    // Time when the car has no record.
    // ------------------------------------------------------------------------
    CgsSystem::Time ScoringSystem::GetTimeSpentInLastPlace(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCar = GetCarData(leRaceCarIndex);
        if (lpCar)
        {
            return lpCar->GetScoreData()->GetTimeInLastPlace();
        }
        return CgsSystem::Time(0.0f);
    }

    // ------------------------------------------------------------------------
    // GetTimeSpentBoosting -- X360 0x82327058
    // The car's accumulated time-boosting (CarScoreData +0xF0), or a zeroed Time
    // when the car has no record.
    // ------------------------------------------------------------------------
    CgsSystem::Time ScoringSystem::GetTimeSpentBoosting(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCar = GetCarData(leRaceCarIndex);
        if (lpCar)
        {
            return lpCar->GetScoreData()->GetTimeBoosting();
        }
        return CgsSystem::Time(0.0f);
    }
}
