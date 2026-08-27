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
// SCOPE NOTE -- this TU is the home for the implemented per-car query group that
// reads CarScoreData; every body in the group is reconstructed and present here.
// Each query reaches its target field through a PUBLIC accessor on the (since-grown)
// committed CarScoreData slice: completed-laps (+0x10 GetCompletedLaps), race
// position (+0x08 GetRacePosition), finish-position (+0x14 GetFinishPosition), live
// distance-to-finish (+0x18 GetDistanceToFinishLive) and round-end distance-to-finish
// (+0x48 GetDistanceToFinish), eliminator-index (+0x60 GetEliminatorRaceCarIndex),
// eliminations (+0x64 GetNumEliminations), takedowns (+0x4C GetTakedowns) and
// takedowns-against (+0x50 GetTakedownsAgainst), finish time (+0x00 GetFinishTime),
// and the per-lap Time array (GetLapTime).  CarScoreData was subsequently grown to
// expose these inline getters, so the earlier keystone-reconciliation blocker (the
// committed slice carried no getters for those offsets, plus a +0x48 semantic-label
// question vs the DWARF) is RESOLVED: +0x48 is the round-end mfDistanceToFinish
// (GetDistanceToFinish), distinct from the live mfDistanceToFinishLive at +0x18
// (GetDistanceToFinishLive).  Nothing in this group is left declare-only.

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

    // ------------------------------------------------------------------------
    // GetRaceCarNumCompletedLaps  --  X360 0x82326900  (DWARF :615)
    // Returns the car's completed-lap count (CarScoreData::miCompletedLaps,
    // +0x10), or 0 when no record matches the index.
    // ------------------------------------------------------------------------
    u32 ScoringSystem::GetRaceCarNumCompletedLaps(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData = GetCarData(leRaceCarIndex);
        if (lpCarData != NULL)
        {
            return static_cast<u32>(lpCarData->GetScoreData()->GetCompletedLaps());
        }
        return 0;
    }

    // ------------------------------------------------------------------------
    // GetCarRacePosition  --  X360 0x82326980  (DWARF :630)
    // Returns the car's live race position (CarScoreData::miRacePosition,
    // +0x08), or E_ACTIVE_RACE_CAR_INDEX_COUNT (8) when no record matches.
    // ------------------------------------------------------------------------
    u32 ScoringSystem::GetCarRacePosition(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData = GetCarData(leRaceCarIndex);
        if (lpCarData != NULL)
        {
            return static_cast<u32>(lpCarData->GetScoreData()->GetRacePosition());
        }
        return static_cast<u32>(E_ACTIVE_RACE_CAR_INDEX_COUNT);
    }

    // ------------------------------------------------------------------------
    // GetCarRaceFinishPosition  --  X360 0x82326A08  (DWARF :635)
    // Returns the car's recorded finish position (CarScoreData::miFinishPosition,
    // +0x14), or E_ACTIVE_RACE_CAR_INDEX_COUNT (8) when no record matches.
    // ------------------------------------------------------------------------
    s32 ScoringSystem::GetCarRaceFinishPosition(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData = GetCarData(leRaceCarIndex);
        if (lpCarData != NULL)
        {
            return lpCarData->GetScoreData()->GetFinishPosition();
        }
        return static_cast<s32>(E_ACTIVE_RACE_CAR_INDEX_COUNT);
    }

    // ------------------------------------------------------------------------
    // GetRaceCarDistanceToFinish  --  X360 0x82326A90  (DWARF :640)
    // Returns the car's live distance-to-finish (CarScoreData::mfDistanceToFinishLive,
    // +0x18), or 0 when no record matches the index.
    // ------------------------------------------------------------------------
    f32 ScoringSystem::GetRaceCarDistanceToFinish(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData = GetCarData(leRaceCarIndex);
        if (lpCarData != NULL)
        {
            return lpCarData->GetScoreData()->GetDistanceToFinishLive();
        }
        return 0.0f;
    }

    // ------------------------------------------------------------------------
    // GetRaceCarEliminatorIndex  --  X360 0x82326BB0  (DWARF :669)
    // Returns the race-car index that eliminated this car
    // (CarScoreData::meEliminatorRaceCarIndex, +0x60), or
    // E_ACTIVE_RACE_CAR_INDEX_INVALID (-1) when no record matches.
    // ------------------------------------------------------------------------
    EActiveRaceCarIndex ScoringSystem::GetRaceCarEliminatorIndex(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData = GetCarData(leRaceCarIndex);
        if (lpCarData != NULL)
        {
            return lpCarData->GetScoreData()->GetEliminatorRaceCarIndex();
        }
        return E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }

    // ------------------------------------------------------------------------
    // GetNumberOfEliminations  --  X360 0x82326C38  (DWARF :674)
    // Returns the car's elimination tally (CarScoreData::miNumEliminations,
    // +0x64), or 0 when no record matches the index.
    // ------------------------------------------------------------------------
    s32 ScoringSystem::GetNumberOfEliminations(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData = GetCarData(leRaceCarIndex);
        if (lpCarData != NULL)
        {
            return lpCarData->GetScoreData()->GetNumEliminations();
        }
        return 0;
    }

    // ------------------------------------------------------------------------
    // GetNumberOfTakedownsAgainst  --  X360 0x82326DD0  (DWARF :736)
    // Returns the count of takedowns suffered by this car
    // (CarScoreData::miTakedownsAgainst, +0x50), or 0 when no record matches.
    // ------------------------------------------------------------------------
    const s32 ScoringSystem::GetNumberOfTakedownsAgainst(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData = GetCarData(leRaceCarIndex);
        if (lpCarData != NULL)
        {
            return lpCarData->GetScoreData()->GetTakedownsAgainst();
        }
        return 0;
    }

    // ------------------------------------------------------------------------
    // GetRaceCarTotalTime  --  X360 0x8231F480  (DWARF :707)
    // Returns the car's total elapsed time.  When the car has completed at least
    // the full lap count, this is the sum of its per-lap times
    // (CarScoreData::maaLapTimes[0 .. completedLaps)).  Otherwise, if the mode
    // timer was never started (mStartTime seconds < 0) the car's recorded finish
    // time is returned; if it is running, the live elapsed time (lTime - start)
    // is returned.  Returns Time(0) when no record matches the index.
    // ------------------------------------------------------------------------
    const CgsSystem::Time ScoringSystem::GetRaceCarTotalTime(EActiveRaceCarIndex leRaceCarIndex,
                                                             const CgsSystem::Time& lTime) const
    {
        CGS_ASSERT((leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && "
                   "(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        CgsSystem::Time lTotalTime(0.0f);

        const CarData* lpCarData = GetCarData(leRaceCarIndex);
        if (lpCarData == NULL)
        {
            return lTotalTime;
        }

        const GameStateModuleIO::CarScoreData* lpScoreData = lpCarData->GetScoreData();
        const u32 luCompletedLaps = static_cast<u32>(lpScoreData->GetCompletedLaps());

        if (luCompletedLaps >= GetTotalLaps())
        {
            for (u32 luLap = 0; luLap < luCompletedLaps; ++luLap)
            {
                lTotalTime += lpScoreData->GetLapTime(luLap);
            }
            return lTotalTime;
        }

        // Race not yet complete: report the live elapsed time, or the recorded
        // finish time if the mode timer was never started.
        if (mStartTime.GetSeconds() < 0)
        {
            return lpScoreData->GetFinishTime();
        }
        return lTime - mStartTime;
    }

    // ------------------------------------------------------------------------
    // GetRaceCarFastestLapTime  --  X360 0x8231F880  (DWARF :721)
    // Returns the car's fastest completed lap, scanning the per-lap times from
    // lap index 1 up to (but excluding) the completed-lap count
    // (CarScoreData::maaLapTimes[1 .. completedLaps)).  The running best starts
    // at Time(0) and is replaced on the first sample and whenever a strictly
    // smaller lap is found.  Returns Time(0) when no record matches the index or
    // when fewer than two laps have been completed.
    // ------------------------------------------------------------------------
    const CgsSystem::Time ScoringSystem::GetRaceCarFastestLapTime(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "( leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && "
                   "( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");

        CgsSystem::Time lFastestLapTime(0.0f);

        const CarData* lpCarData = GetCarData(leRaceCarIndex);
        if (lpCarData != NULL)
        {
            const GameStateModuleIO::CarScoreData* lpScoreData = lpCarData->GetScoreData();
            const u32 luCompletedLaps = static_cast<u32>(lpScoreData->GetCompletedLaps());

            for (u32 luLap = 1; luLap < luCompletedLaps; ++luLap)
            {
                const CgsSystem::Time lLapTime = lpScoreData->GetLapTime(luLap);
                if ((lFastestLapTime.GetFloatVal() == 0.0f) || (lLapTime < lFastestLapTime))
                {
                    lFastestLapTime = lLapTime;
                }
            }
        }

        return lFastestLapTime;
    }

    // ------------------------------------------------------------------------------------
    // ScoringSystem::GetNewlyWreckedCarCount
    //
    // "Bodies land with the ScoringSystem TU; declare-only suffices for the `cl /c` compile
    // gate" -- that note (BrnScoringSystem.h:458) expires here: the compile gate is not the
    // gate any more, because AchievementManagerBase::OnBodyShop is now a LINK requirement of
    // the mounted drive-thru chain, and this is its only unresolved callee.
    //
    // The offsets the header calls "out of scope" ARE mapped now, and they are two ordinary
    // members of this class: X360 OnBodyShop @0x8235AA18 computes
    // `*(mpScoringSystem + 0x4B58) - *(mpScoringSystem + 0x4B5C)` and tests it == 1, and
    // 0x4B58 / 0x4B5C are miMaximumPlayerCrashedNumber / miCurrentPlayerCrashedNumber
    // (BrnScoringSystem.h:728/:729, DWARF :1213/:1214). So the count is the HIGH-WATER wreck
    // count minus the wrecks still outstanding -- i.e. how many of the player's wrecks have
    // been cleared, which is exactly "newly wrecked-but-not-yet-repaired" read from the other
    // end. No assert: the console has none and neither word is indexed.
    // ------------------------------------------------------------------------------------
    s32 ScoringSystem::GetNewlyWreckedCarCount() const
    {
        return miMaximumPlayerCrashedNumber - miCurrentPlayerCrashedNumber;
    }
}
