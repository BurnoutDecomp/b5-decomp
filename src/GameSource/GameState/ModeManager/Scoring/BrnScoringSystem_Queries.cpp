// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_Queries.cpp
// ============================================================================
// Out-of-line bodies for the ScoringSystem "per-car race query" group
// (BrnScoringSystem.h lines 366-401).  Reconstructed from the X360 pseudocode
// (authoritative BODY) over the keystone-defined member layout, accessing every
// member BY NAME.
//
// Each query is a thin read off the per-car record returned by GetCarData():
// the X360 form is `p = GetCarData(idx); return p ? *(p + OFFSET) : default;`
// where OFFSET indexes into the per-car GameStateModuleIO::CarScoreData embedded
// at offset 0 of CarData (GetScoreData() yields that same address).
//
// SCOPE NOTE -- only the queries whose target field is reachable through a
// PUBLIC CarScoreData accessor are reconstructed here.  Most of this group reads
// CarScoreData fields (race position / completed-laps / finish-position /
// eliminator-index / eliminations / takedowns-against / per-lap Time array) that
// the committed CarScoreData slice does NOT expose -- it carries an
// online-scorer-derived packed layout with no getters for those offsets, and at
// one slot (+0x48) a semantic-label conflict vs the DWARF.  Growing CarScoreData
// is a separate keystone reconciliation, so those bodies are FLAGGED BLOCKED and
// omitted (left declare-only) rather than reaching into that shared type here.

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // GetFinishTime  --  X360 0x82326CB8  (DWARF :700)
    // Returns the car's recorded finish time (CarScoreData::mTotalTime, +0x00),
    // or Time(0) when no record matches the index.
    // ------------------------------------------------------------------------
    CgsSystem::Time ScoringSystem::GetFinishTime(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT((leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && "
                   "(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData = GetCarData(leActiveRaceCarIndex);
        if (lpCarData != NULL)
        {
            return lpCarData->GetScoreData()->GetFinishTime();
        }
        return CgsSystem::Time(0.0f);
    }

    // ------------------------------------------------------------------------
    // GetRaceCarDistanceToFinishAtRoundEnd  --  X360 0x82326B18  (DWARF :655)
    // X360 reads CarScoreData +0x48 (the committed slice's mfDistanceToFinish,
    // surfaced by GetDistanceToFinish()); returns 0 when no record matches.
    // ------------------------------------------------------------------------
    f32 ScoringSystem::GetRaceCarDistanceToFinishAtRoundEnd(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT((leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData = GetCarData(leActiveRaceCarIndex);
        if (lpCarData != NULL)
        {
            return lpCarData->GetScoreData()->GetDistanceToFinish();
        }
        return 0.0f;
    }

    // ------------------------------------------------------------------------
    // GetNumberOfTakedowns  --  X360 0x82326D50  (DWARF :726)
    // Returns the car's takedown count (CarScoreData::miTakedowns, +0x4C),
    // or 0 when no record matches the index.
    // ------------------------------------------------------------------------
    const s32 ScoringSystem::GetNumberOfTakedowns(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT((leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>=E_ACTIVE_RACE_CAR_INDEX_0) && "
                   "(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData = GetCarData(leActiveRaceCarIndex);
        if (lpCarData != NULL)
        {
            return lpCarData->GetScoreData()->GetTakedowns();
        }
        return 0;
    }
}
