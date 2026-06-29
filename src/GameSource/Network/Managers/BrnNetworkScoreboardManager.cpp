// ---- GameSource/Network/Managers/BrnNetworkScoreboardManager.cpp ----
// BrnNetwork::ScoreboardManager -- reconstructed from the BURNOUT_X360_ARTIST.XEX ARTIST exports
// (the per-function X360 addresses are cited inline). The bodies recovered in this pass are the 13
// ARTIST-attested functions; the remaining declared methods (ProcessBeforeSimulation /
// HandleScoreboardEvent / ProcessEventQueue / Copy* / Page* / Destruct / DirtySockColumnTypeToEDataType
// / AddNumberBeforeAndAfter) have only DWARF variable hints in this slice -- no recovered pseudocode --
// so they are intentionally left declared-only (their bodies land in a follow-up pass). Their call
// sites here are satisfied by the declarations under the cl /c gate.
//
// Source-of-truth: the X360 pseudocode/asm is the spine; the DecFIGS DWARF supplied the declaration
// shapes; member access is BY NAME against the reconstructed header.

#include "GameSource/Network/Managers/BrnNetworkScoreboardManager.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceRankings.h"

#include <cstdlib>   // atoi, qsort
#include <cstdio>    // snprintf (the rank-column "%d" formatter)

namespace BrnNetwork
{
    // ---- file-scope rankings column-type codes the scoreboard recognises -------------------
    // The "points/score" and "rank" column-type tags the scoreboard sorts on (the two literal
    // DirtySock four-CC type codes compared in the X360 bodies of AddColumnInfoToScoreboard /
    // AddSortedRowDataToScoreboard / GetColumnIndexOfType).
    static const s32 KI_COLUMN_TYPE_POINTS = 2121299059;   // sort key (AddColumnInfo skips this column)
    static const s32 KI_COLUMN_TYPE_RANK   = 2121428587;   // synthesised rank column

    // The DirtySock "ScoreboardHasParam" parameter slots the bodies probe (the raw small ints
    // passed to ScoreboardHasParam in the X360 asm). Named from their scoreboard-build use.
    static const s32 KI_SCOREBOARD_PARAM_FRIENDS_ONLY = 0;
    static const s32 KI_SCOREBOARD_PARAM_PERCENT      = 1;
    static const s32 KI_SCOREBOARD_PARAM_SORTED       = 2;   // route to AddSortedRowDataToScoreboard
    static const s32 KI_SCOREBOARD_PARAM_GROUP        = 6;

    // The X360 cell-buffer width every Get*Cell read uses (char[31], the ScoreboardRow cell width).
    static const s32 KI_CELL_BUFFER_SIZE = 31;

    // -------------------------------------------------------------------------------------------
    // Construct  @ 0x8255A408  [EXECUTED in goal trace]
    // Bring the manager to its empty, pre-Prepare state and construct the embedded sub-objects.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::Construct()
    {
        mpNetworkModule     = 0;
        mpRankings          = 0;
        mpServerInterface   = 0;
        miCurrentViewOffset = 0;

        mScoreboardEventQueue.Construct();
        mDebugComponent.Construct(this);
    }

    // -------------------------------------------------------------------------------------------
    // Prepare  @ 0x825471B0
    // Wire up the module + server-interface back-pointers, cache the rankings component, reset the
    // current Category/Index/Variation selection to "none", and prepare the debug component.
    // -------------------------------------------------------------------------------------------
    bool ScoreboardManager::Prepare(CgsNetwork::ServerInterface* lpServerInterface,
                                    BrnNetwork::BrnNetworkModule* lpModule)
    {
        CGS_ASSERT(lpModule != 0, "lpModule");
        mpNetworkModule   = lpModule;
        mpServerInterface = lpServerInterface;
        CGS_ASSERT(lpServerInterface != 0, "mpServerInterface");

        // X360: mpRankings = mpServerInterface->maComponents[E_COMPONENTS_RANKINGS].mpComponent.
        mpRankings = reinterpret_cast<CgsNetwork::ServerInterfaceRankings*>(
            lpServerInterface->GetRankingsComponent());
        CGS_ASSERT(mpRankings != 0, "mpRankings");

        miCurrentView      = KI_INVALID_HEADING;
        miCurrentVariation = KI_INVALID_HEADING;
        miCurrentCategory  = KI_INVALID_HEADING;
        miCurrentIndex     = KI_INVALID_HEADING;
        meCurrentState     = E_STATE_IDLE;
        miCurrentViewOffset = 0;

        mDebugComponent.Prepare();
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // Release  @ 0x82552EF0
    // Tear back down to the unprepared state (drop back-pointers, reset selection + offset/state).
    // -------------------------------------------------------------------------------------------
    bool ScoreboardManager::Release()
    {
        mpNetworkModule     = 0;
        mpRankings          = 0;
        mpServerInterface   = 0;
        miCurrentViewOffset = 0;

        miCurrentView      = KI_INVALID_HEADING;
        miCurrentVariation = KI_INVALID_HEADING;
        miCurrentCategory  = KI_INVALID_HEADING;
        miCurrentIndex     = KI_INVALID_HEADING;
        meCurrentState     = E_STATE_IDLE;

        // X360 zeroes a1[9..11] -- the embedded select-event queue cursors (read/write/length).
        mScoreboardEventQueue.Clear();

        mDebugComponent.Release();
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // Disconnected  @ 0x82553270
    // On losing the connection: cancel any in-flight rankings request, invalidate the scoreboard,
    // reset the download state and drop the queue cursors.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::Disconnected()
    {
        mpRankings->CancelCurrentActionAndInvalidateScoreboard();
        meCurrentState = E_STATE_IDLE;   // a1[14] = 0

        // a1[9..11] = 0 -- the select-event queue cursors (read/write/length): empty the queue.
        mScoreboardEventQueue.Clear();
    }

    // -------------------------------------------------------------------------------------------
    // SetViewAndDownloadHeaders  @ 0x82547280
    // Kick off a headings download and advance into the GETTING_SCOREBOARD state.
    //
    // FLAGGED rodata gap: the X360 body selects between two heading-type descriptor pointers
    // (off_82F29900 / off_82F298FC) based on a global capability-flag test
    // ((dword_82FFA7F4 & dword_82FFA7F8[dword_82FFA864]) ...). Those two descriptor objects and the
    // flag table are file-scope rodata that is NOT recovered in this slice, so the selected heading
    // type cannot be reconstructed without fabricating data. The recoverable structure -- the
    // not-busy assert, the DownloadHeadings call and the state transition -- is reconstructed; the
    // descriptor argument is left as a documented null placeholder to be filled when that rodata is
    // homed. (Per project rule: never fabricate un-recovered rodata.)
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::SetViewAndDownloadHeaders()
    {
        CGS_ASSERT(mpRankings->IsBusy() == false, "mpRankings->IsBusy() == false");

        // FLAGGED: heading-type descriptor selection (off_82F29900 vs off_82F298FC) depends on
        // un-recovered file-scope rodata; placeholder until that data is homed.
        void* lpHeadingType = 0;   // FLAGGED-0 placeholder (un-recovered rodata)

        mpRankings->DownloadHeadings(lpHeadingType);
        meCurrentState = E_STATE_GETTING_SCOREBOARD;   // *(this + 56) = 2
    }

    // -------------------------------------------------------------------------------------------
    // OffsetScoreboard  @ 0x82547348
    // Scroll the visible scoreboard window by liNumberOfRows, clamped to [0, rows - 9], and flag the
    // view as needing a re-render (E_STATE_GETTING_SCOREBOARD).
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::OffsetScoreboard(s32 liNumberOfRows)
    {
        s32 liLastRow = mpRankings->GetNumberOfRows() - 9;
        if (liLastRow < 0)
        {
            liLastRow = 0;
        }

        s32 liNewOffset = miCurrentViewOffset + liNumberOfRows;
        if (liNewOffset > liLastRow)
        {
            liNewOffset = liLastRow;
        }
        if (liNewOffset < 0)
        {
            liNewOffset = 0;
        }

        miCurrentViewOffset = liNewOffset;
        meCurrentState      = E_STATE_COUNT;   // a1[14] = 3 (re-render pending)
    }

    // -------------------------------------------------------------------------------------------
    // GetColumnIndexOfType  @ 0x82547490
    // Linear-scan the rankings columns for the first column whose type matches liType; -1 if none.
    // -------------------------------------------------------------------------------------------
    s32 ScoreboardManager::GetColumnIndexOfType(s32 liType)
    {
        s32 liColumnCount = mpRankings->GetNumberOfColumns();
        for (s32 liColumnIndex = 0; liColumnIndex < liColumnCount; ++liColumnIndex)
        {
            if (mpRankings->GetColumnType(liColumnIndex) == liType)
            {
                return liColumnIndex;
            }
        }
        return -1;
    }

    // -------------------------------------------------------------------------------------------
    // PostProcessColumnData  @ 0x825531F8
    // After a cell is read, fix up its text: for car-CgsID columns the server uses '?' (63) as a
    // padding/placeholder char which is rewritten to space (32) across the 13-char id field. Returns
    // the column's logical data type.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::PostProcessColumnData(char* lpcData, s32 liColumnIndex)
    {
        s32 liColumnType = mpRankings->GetColumnType(liColumnIndex);
        ScoreboardColumn::EDataType leDataType = DirtySockColumnTypeToEDataType(liColumnType);

        if (leDataType == ScoreboardColumn::E_DATATYPE_CAR_CGS_ID)
        {
            for (s32 i = 0; i < 13; ++i)
            {
                if (lpcData[i] == '?')
                {
                    lpcData[i] = ' ';
                }
            }
        }
    }

    // -------------------------------------------------------------------------------------------
    // _ScoreboardSortData  @ 0x82547640  (static qsort comparator)
    // Order two rows by miScore (descending by default); negate the result for ascending sorts. Both
    // operands must agree on the sort direction.
    // -------------------------------------------------------------------------------------------
    int ScoreboardManager::_ScoreboardSortData(const void* lpData1, const void* lpData2)
    {
        const ScoreboardRowSortData* lpSortData1 =
            static_cast<const ScoreboardRowSortData*>(lpData1);
        const ScoreboardRowSortData* lpSortData2 =
            static_cast<const ScoreboardRowSortData*>(lpData2);

        CGS_ASSERT(lpSortData1 != 0, "lpSortData1");
        CGS_ASSERT(lpSortData2 != 0, "lpSortData2");
        CGS_ASSERT(lpSortData1->mbAscending == lpSortData2->mbAscending,
                   "lpSortData1->mbAscending == lpSortData2->mbAscending");

        int liRetVal;
        if (lpSortData1->miScore > lpSortData2->miScore)
        {
            liRetVal = -1;
        }
        else
        {
            liRetVal = (lpSortData1->miScore < lpSortData2->miScore) ? 1 : 0;
        }

        if (lpSortData1->mbAscending)
        {
            liRetVal = -liRetVal;
        }
        return liRetVal;
    }

    // -------------------------------------------------------------------------------------------
    // BuildDownloadedScoreboard  @ 0x8255F948
    // Validate the current selection against the rankings counts, then (when valid) fill lpScoreboard
    // with the column headers and the row data -- sorted or raw per the scoreboard's SORTED param.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::BuildDownloadedScoreboard(Scoreboard* lpScoreboard)
    {
        CGS_ASSERT(lpScoreboard != 0, "lpScoreboard");
        CGS_ASSERT(miCurrentCategory >= 0, "miCurrentCategory >= 0");
        CGS_ASSERT(miCurrentIndex >= 0, "miCurrentIndex >= 0");
        CGS_ASSERT(miCurrentVariation >= 0, "miCurrentVariation >= 0");

        bool lbAreCategoryIndexAndVariationValid =
            (miCurrentCategory  < mpRankings->GetNumberOfCategories()) &&
            (miCurrentIndex     < mpRankings->GetNumberOfIndexes(miCurrentCategory)) &&
            (miCurrentVariation < mpRankings->GetNumberOfVariations(miCurrentCategory,
                                                                    miCurrentIndex));

        if (lbAreCategoryIndexAndVariationValid)
        {
            AddColumnInfoToScoreboard(lpScoreboard);
            if (mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_SORTED))
            {
                AddSortedRowDataToScoreboard(lpScoreboard);
            }
            else
            {
                AddRowDataToScoreboard(lpScoreboard);
            }
        }
    }

    // -------------------------------------------------------------------------------------------
    // AddColumnInfoToScoreboard  @ 0x82553010
    // Copy each rankings column heading into the scoreboard as a typed ScoreboardColumn. The "points"
    // column is given a synthesised data type from the scoreboard params; every other column maps its
    // DirtySock type code through DirtySockColumnTypeToEDataType.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::AddColumnInfoToScoreboard(Scoreboard* lpScoreboard)
    {
        CGS_ASSERT(mpRankings->GetNumberOfColumns() < KI_MAX_SCOREBOARD_COLUMNS,
                   "mpRankings->GetNumberOfColumns() < KI_MAX_SCOREBOARD_COLUMNS");

        for (s32 liColumnCounter = 0; liColumnCounter < KI_MAX_SCOREBOARD_COLUMNS; ++liColumnCounter)
        {
            if (liColumnCounter >= mpRankings->GetNumberOfColumns())
            {
                break;
            }

            ScoreboardColumn::EDataType leDataType;
            if (mpRankings->GetColumnType(liColumnCounter) != KI_COLUMN_TYPE_POINTS)
            {
                s32 liColumnTypeCode = mpRankings->GetColumnType(liColumnCounter);
                leDataType = DirtySockColumnTypeToEDataType(liColumnTypeCode);
                // X360 tests the DirtySock->EDataType mapping against the literal 9 (the
                // out-of-enum "unknown" sentinel DirtySockColumnTypeToEDataType returns);
                // there is no E_DATATYPE value == 9, so don't use E_DATATYPE_COUNT (==8).
                if (static_cast<s32>(leDataType) == 9)
                {
                    CGS_ASSERT(false, "Unknown scoreboard datatype recieved from server");
                    leDataType = ScoreboardColumn::E_DATATYPE_STRING;
                }
            }
            else
            {
                // The synthesised "points" column type depends on which scoreboard params are set.
                if (mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_FRIENDS_ONLY) ||
                    mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_GROUP))
                {
                    leDataType = ScoreboardColumn::E_DATATYPE_TIME;
                }
                else if (!mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_PERCENT))
                {
                    leDataType = ScoreboardColumn::E_DATATYPE_NUMBER;
                }
                else
                {
                    leDataType = ScoreboardColumn::E_DATATYPE_CURRENCY;
                }
            }

            ScoreboardColumn lColumn;
            lColumn.Construct();
            lColumn.Prepare(mpRankings->GetColumnTitle(liColumnCounter),
                            mpRankings->GetColumnWidth(liColumnCounter),
                            mpRankings->GetColumnStyle(liColumnCounter),
                            leDataType);
            lpScoreboard->AddColumn(&lColumn);
        }
    }

    // -------------------------------------------------------------------------------------------
    // AddRowDataToScoreboard  @ 0x8255A448
    // Fill the scoreboard with up to 8 rows starting at the current view offset, reading each cell
    // from the rankings component and post-processing it. The first call recenters the view on the
    // local user's row (offset = max(localUserRow - 4, 0)).
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::AddRowDataToScoreboard(Scoreboard* lpScoreboard)
    {
        if (miCurrentViewOffset == KI_INVALID_HEADING)
        {
            s32 liLocalPlayerIndex = mpRankings->GetRowThatContainsLocalUser();
            if (liLocalPlayerIndex < 0)
            {
                miCurrentViewOffset = 0;
            }
            else
            {
                s32 liOffset = liLocalPlayerIndex - 4;
                if (liOffset < 0)
                {
                    liOffset = 0;
                }
                miCurrentViewOffset = liOffset;
            }
        }

        const s32 liLastRow = miCurrentViewOffset + KI_MAX_SCOREBOARD_ROWS;
        for (s32 liRowCounter = miCurrentViewOffset; liRowCounter < liLastRow; ++liRowCounter)
        {
            if (liRowCounter >= mpRankings->GetNumberOfRows())
            {
                break;
            }

            ScoreboardRow lRow;
            lRow.Clear();

            for (s32 liColumnCounter = 0;
                 liColumnCounter < lpScoreboard->GetNumberOfColumns();
                 ++liColumnCounter)
            {
                char lacBuffer[KI_CELL_BUFFER_SIZE];
                mpRankings->GetCell(liColumnCounter, liRowCounter, lacBuffer, KI_CELL_BUFFER_SIZE);
                PostProcessColumnData(lacBuffer, liColumnCounter);
                lRow.AddCell(lacBuffer);
            }

            lpScoreboard->AddRow(&lRow);
        }

        AddNumberBeforeAndAfter(lpScoreboard,
                                static_cast<s8>(mpRankings->GetNumberOfRows()));
    }

    // -------------------------------------------------------------------------------------------
    // AddSortedRowDataToScoreboard  @ 0x8255A5D0
    // Build a re-sorted scoreboard: gather every row's score (skipping zero-score rows), qsort them,
    // then emit the top 8 rows from the current offset -- synthesising the rank column from the sort
    // position and reading the other cells from the rankings component.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::AddSortedRowDataToScoreboard(Scoreboard* lpScoreboard)
    {
        CGS_ASSERT(mpRankings->GetNumberOfRows() < 101,
                   "mpRankings->GetNumberOfRows() < CgsNetwork::KI_MAX_BUDDIES + 1");

        s32 liPointsColumnIndex = GetColumnIndexOfType(KI_COLUMN_TYPE_POINTS);
        s32 liRankColumnIndex   = GetColumnIndexOfType(KI_COLUMN_TYPE_RANK);
        CGS_ASSERT(liPointsColumnIndex != -1, "liPointsColumnIndex != -1");
        CGS_ASSERT(liRankColumnIndex   != -1, "liRankColumnIndex != -1");

        ScoreboardRowSortData lacSortData[101];
        s32 liRowCount = 0;

        for (s32 liRowIndex = 0; liRowIndex < mpRankings->GetNumberOfRows(); ++liRowIndex)
        {
            char lacBuffer[KI_CELL_BUFFER_SIZE];
            mpRankings->GetCell(liPointsColumnIndex, liRowIndex, lacBuffer, KI_CELL_BUFFER_SIZE);
            s32 liScore = atoi(lacBuffer);

            if (liScore > 0)
            {
                bool lbAscending = mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_FRIENDS_ONLY) ||
                                   mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_GROUP);
                lacSortData[liRowCount].miRowIndex  = liRowIndex;
                lacSortData[liRowCount].miScore     = liScore;
                lacSortData[liRowCount].mbAscending = lbAscending;
                ++liRowCount;
            }
        }

        qsort(lacSortData, liRowCount, sizeof(ScoreboardRowSortData), _ScoreboardSortData);

        if (miCurrentViewOffset == KI_INVALID_HEADING)
        {
            miCurrentViewOffset = 0;
        }

        const s32 liLastRow = miCurrentViewOffset + KI_MAX_SCOREBOARD_ROWS;
        for (s32 liViewRow = miCurrentViewOffset; liViewRow < liLastRow; ++liViewRow)
        {
            ScoreboardRow lScoreboardRow;
            lScoreboardRow.Clear();

            if (liViewRow >= liRowCount)
            {
                break;
            }

            for (s32 liColumnIndex = 0;
                 liColumnIndex < mpRankings->GetNumberOfColumns();
                 ++liColumnIndex)
            {
                char lacBuffer[KI_CELL_BUFFER_SIZE];
                if (liColumnIndex == liRankColumnIndex)
                {
                    // The rank column is synthesised from the (1-based) sorted position.
                    snprintf(lacBuffer, KI_CELL_BUFFER_SIZE, "%d", liViewRow + 1);
                }
                else
                {
                    mpRankings->GetCell(liColumnIndex, lacSortData[liViewRow].miRowIndex,
                                        lacBuffer, KI_CELL_BUFFER_SIZE);
                }
                PostProcessColumnData(lacBuffer, liColumnIndex);
                lScoreboardRow.AddCell(lacBuffer);
            }

            lpScoreboard->AddRow(&lScoreboardRow);
        }

        AddNumberBeforeAndAfter(lpScoreboard, static_cast<s8>(liRowCount));
    }
}
