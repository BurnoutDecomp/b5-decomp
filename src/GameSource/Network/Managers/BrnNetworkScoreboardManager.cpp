// ---- GameSource/Network/Managers/BrnNetworkScoreboardManager.cpp ----
// BrnNetwork::ScoreboardManager -- reconstructed from the BURNOUT_X360_ARTIST.XEX ARTIST exports
// (the per-function X360 addresses are cited inline). The bodies recovered in this pass are the 13
// ARTIST-attested functions; the remaining declared methods (
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
#include "GameShared/GameClasses/Development/CgsStrStream.h"                              // CgsDev::StrStream (download asserts)
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"       // E_SERVER_INTERFACE_RANKINGS_ERROR_START
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                    // CgsCore::SPrintf (variation heading formatter)
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceRankings.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                          // VariableEventQueue<14000,16>::AddEvent
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                // NetworkOutScoreboardEvent / NetworkOutDldChallengeableEvent
#include "GameSource/Resource/BrnDLCManager.h"                                            // g_DLCFeatureAvailability (heading view)
#include "GameSource/Network/BrnServerInterface.h"                                        // GetPlayerInfoComponent
#include "GameSource/Network/Managers/X360/BrnNetworkBuddyManagerX360.h"                 // GetAllBuddyNames (friends scoreboards)
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                      // PlayerInfoData (the local player's name)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // GetLocalPlayerInfo
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"                    // KAA_SAVE_GAME_CHALLENGE_ROAD_IDS (per-road headings)
#include "GameSource/Network/BrnNetworkModule.h"                                          // GetNetworkManager / GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"                                         // GetLocalUserControllerPort / GetGamerCardManager
#include "GameSource/Network/Managers/X360/BrnNetworkGamerCardManagerX360.h"             // NetworkGamerCardManagerX360::GetXuidForPlayer
#include "GameSource/Network/Parameters/BrnNetworkEventScoreData.h"                      // KAU64_*_SCOREBOARD_EVENT_IDS / KI_NUM_*_EVENT_SCOREBOARDS
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                              // CgsDev::Log::gpDebugPrint

#include <cstdlib>   // atoi, qsort
#include <cstdio>    // snprintf (the rank-column "%d" formatter)
#include <cstdint>   // uintptr_t (this <-> s32 user-data round-trip)

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

    // The scoreboard-headings network event the Copy* builders broadcast: event-type 0x34 (52),
    // payload 0x808 (2056) == sizeof(NetworkOutScoreboardHeadingList). Used by CopyCategories /
    // CopyIndexes (and CopyVariations) on AddEvent.
    static const s32 KI_NETEVENT_SCOREBOARD_HEADINGS     = 0x34;   // 52  (AddEvent event-type)
    static const s32 KI_SCOREBOARD_HEADINGS_PAYLOAD_SIZE = 0x808;  // 2056 == sizeof(NetworkOutScoreboardHeadingList)

    // The DirtySock scoreboard-param slots CopyVariations probes to pick each variation's heading
    // format. Params 4/5 are named from the range asserts CopyVariations fires (the stunt-run and
    // burn-route event scoreboards); param 3 selects the qword_82029FA0 heading format table. Each
    // slot routes to a per-mode file-scope format-string table (all three un-recovered; see the
    // FLAGGED note on CopyVariations).
    static const s32 KI_SCOREBOARD_PARAM_VARIATION_FMT = 3;   // -> qword_82029FA0 format table
    static const s32 KI_SCOREBOARD_PARAM_STUNT_RUN     = 4;   // -> qword_8207C440 format table
    static const s32 KI_SCOREBOARD_PARAM_BURN_ROUTE    = 5;   // -> qword_8207C4B0 format table

    // The rank views the headings download asks for: the base one and the one carrying the
    // downloadable-content leaderboards. FLAG: identifiers are ours; the strings are the image's.
    static const s32 KI_RANK_VIEW_BASE = 0;
    static const s32 KI_RANK_VIEW_DLC  = 1;
    static const char* const KAPC_RANK_VIEWS[] = { "PS2", "DLC" };

    // The rankings user type of a friends-only scoreboard ('budd'), and the room for its user
    // list: every buddy plus the local player.
    static const s32 KI_USER_TYPE_BUDDIES     = 0x62756464;
    static const s32 KI_MAX_USER_LIST_NAMES   = 101;

    // The incoming GUI "challenge this event score" event handed to HandleEvScoreTargetEvent.
    // Forward-declared in the header (BrnNetwork::EvScoreTargetEvent); defined here. The leading
    // bytes are the challenged player's name string (fed to UniquePlayerIDX360::Construct); the
    // trailing fields are read at the X360-attested offsets. Total size is not attested (a
    // queue-payload span), so no closing member is modelled.
    struct EvScoreTargetEvent
    {
        u8  macOpaqueName[0x10];   // +0x00  player-name string span (Construct source)
        u32 muScoreValue;          // +0x10
        s32 miCategory;            // +0x14
        s32 miIndex;               // +0x18
        s32 miVariation;           // +0x1C
        u8  muChallengeType;       // +0x20
    };

    // ---- score-target challenge event protocol (private to this manager) ----------------------
    namespace
    {
        // The concrete network-event queue behind BrnNetworkModuleIO::NetworkEventQueue (same
        // 14000/16 instantiation the sibling BrnEventScoresManager.cpp re-casts to).
        typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueueConcrete;

        inline NetworkEventQueueConcrete* AsConcreteQueue(BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
        {
            return reinterpret_cast<NetworkEventQueueConcrete*>(lpQueue);
        }

        // The outgoing "score-target challenge" network event (type 0x35 == 53, 40-byte payload).
        // Layout X360-attested from the HandleEvScoreTargetEvent / RequestXUIDForPlayerCallback
        // payload builds: the challenged player's id (UniquePlayerIDX360, 24B) then the scoreboard
        // slot, the challenge value and the challenge-type byte.
        struct ScoreTargetChallengeEvent
        {
            CgsNetwork::UniquePlayerIDX360 mPlayerID;         // +0x00  (24B)
            u64                            mScoreboardSlot;   // +0x18
            s32                            miValue;           // +0x20
            u8                             muChallengeType;   // +0x24
        };

        // The outgoing network-event type code (X360 AddEvent(queue, payload, 0x35, 0x28)).
        const s32 KI_NETEVENT_SCORE_TARGET     = 0x35;   // 53
        const s32 KI_SCORE_TARGET_PAYLOAD_SIZE = 0x28;   // 40 == sizeof(ScoreTargetChallengeEvent)

        // The scoreboard params HandleEvScoreTargetEvent probes to pick the per-param slot table.
        const s32 KI_SCORE_TARGET_PARAM_A = 4;
        const s32 KI_SCORE_TARGET_PARAM_B = 5;
    } // anonymous namespace

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
    // Kick off a headings download and advance into the GETTING_HEADINGS state. The rank view is
    // the downloadable-content one when both event leaderboards are available, the base one
    // otherwise.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::SetViewAndDownloadHeaders()
    {
        CGS_ASSERT(mpRankings->IsBusy() == false, "mpRankings->IsBusy() == false");

        const bool lbLeaderboardContent =
            BrnResource::g_DLCFeatureAvailability.IsFeatureAvailable(BrnResource::E_DLC_FEATURE_LEADERBOARDS_BURNING_ROUTE)
            && BrnResource::g_DLCFeatureAvailability.IsFeatureAvailable(BrnResource::E_DLC_FEATURE_LEADERBOARDS_STUNT_RUN);
        const char* lpcView = KAPC_RANK_VIEWS[lbLeaderboardContent ? KI_RANK_VIEW_DLC : KI_RANK_VIEW_BASE];

        mpRankings->DownloadHeadings(lpcView);
        meCurrentState = E_STATE_GETTING_HEADINGS;
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
        meCurrentState      = E_STATE_GETTING_SCOREBOARD;   // rebuild the page at the new offset
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

    // -------------------------------------------------------------------------------------------
    // DirtySockColumnTypeToEDataType  @ 0x825474F8
    // Map a DirtySock column type-code onto the scoreboard's logical EDataType. The X360 body
    // linear-scans a paired file-scope rodata lookup table (unk_8207C69C key words / dword_8207C6A0
    // result words, stride 8 == {type,EDataType} pairs); the first pair whose key matches liDSType
    // yields that pair's EDataType. An unrecognised code asserts "Bad dirtysock data type" and
    // returns the out-of-enum "unknown" sentinel 9 (the value AddColumnInfoToScoreboard tests
    // against; there is no EDataType == 9).
    //
    // FLAGGED rodata gap (per project rule: never fabricate un-recovered rodata): the paired
    // {DirtySock-type, EDataType} lookup table (unk_8207C69C / dword_8207C6A0) is file-scope rodata
    // NOT recovered in this slice, so the individual type->EDataType mappings cannot be
    // reconstructed without fabricating data. The recoverable structure -- the linear scan, the
    // not-found assert and the `return 9` sentinel -- is reconstructed; the table walk is left as a
    // documented placeholder that always reports "not found" until that rodata is homed.
    // -------------------------------------------------------------------------------------------
    ScoreboardColumn::EDataType ScoreboardManager::DirtySockColumnTypeToEDataType(s32 liDSType)
    {
        // FLAGGED placeholder: scan the un-recovered {type,EDataType} lookup table for liDSType.
        // for (each {liKey, leResult} pair in the table)
        //     if (liDSType == liKey) return leResult;
        (void)liDSType;

        // No matching column type: report the bad type and return the out-of-enum "unknown"
        // sentinel (== 9; see AddColumnInfoToScoreboard, which routes 9 to E_DATATYPE_STRING).
        CGS_ASSERT(false, "Bad dirtysock data type");
        return static_cast<ScoreboardColumn::EDataType>(9);
    }

    // -------------------------------------------------------------------------------------------
    // HandleEvScoreTargetEvent  @ 0x82568960
    // Handle a GUI "challenge this event score" request: point the rankings component at the target
    // scoreboard, resolve which per-param scoreboard-slot table applies, and build the outgoing
    // score-target challenge network event. When the challenged player's XUID already travels in
    // the event (challenge-type byte set) the event is queued immediately; otherwise the XUID is
    // resolved by-name through the gamer-card manager and the event is queued from
    // RequestXUIDForPlayerCallback once it arrives.
    //
    // The target scoreboard is the event id of the selected variation, read from the stunt-run
    // (param 4) or burn-route (param 5) event table.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::HandleEvScoreTargetEvent(const EvScoreTargetEvent* lpScoreTargetEvent)
    {
        CGS_ASSERT(lpScoreTargetEvent != 0, "lpScoreboardScoreTargetEvent");

        mpRankings->SelectScoreboard(lpScoreTargetEvent->miCategory,
                                     lpScoreTargetEvent->miIndex,
                                     lpScoreTargetEvent->miVariation);

        // Resolve which event table applies.
        const u64* lpau64EventIDs = nullptr;
        if (mpRankings->ScoreboardHasParam(KI_SCORE_TARGET_PARAM_A))
        {
            lpau64EventIDs = KAU64_STUNT_RUN_SCOREBOARD_EVENT_IDS;
        }
        else if (mpRankings->ScoreboardHasParam(KI_SCORE_TARGET_PARAM_B))
        {
            lpau64EventIDs = KAU64_BURN_ROUTE_SCOREBOARD_EVENT_IDS;
        }
        else
        {
            CGS_ASSERT(false, "Trying to challenge event score for event without a leaderboard\n");
            return;
        }

        mScoreTargetScoreboard = lpau64EventIDs[lpScoreTargetEvent->miVariation];
        miScoreTargetValue     = static_cast<s32>(lpScoreTargetEvent->muScoreValue);

        if (lpScoreTargetEvent->muChallengeType != 0)
        {
            // The challenged player's identity travels in the event: build the challenge and queue it.
            mScoreTargetPlayerID.Construct(reinterpret_cast<const char*>(lpScoreTargetEvent), 0);

            ScoreTargetChallengeEvent lChallengeEvent;
            lChallengeEvent.mPlayerID       = mScoreTargetPlayerID;
            lChallengeEvent.mScoreboardSlot = mScoreTargetScoreboard;
            lChallengeEvent.miValue         = miScoreTargetValue;
            lChallengeEvent.muChallengeType = lpScoreTargetEvent->muChallengeType;

            CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
            AsConcreteQueue(mpNetworkModule->GetNetworkEventQueue())->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lChallengeEvent),
                KI_NETEVENT_SCORE_TARGET, KI_SCORE_TARGET_PAYLOAD_SIZE);
        }
        else
        {
            // The challenged player's XUID is not yet known: seed the name-only id, then ask the
            // gamer-card manager to resolve the XUID and re-enter through the callback.
            mScoreTargetPlayerID.Construct(reinterpret_cast<const char*>(lpScoreTargetEvent), 0);

            CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
            CGS_ASSERT(mpNetworkModule->GetNetworkManager() != 0,
                       "mpNetworkModule->GetNetworkManager()");
            CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetGamerCardManager() != 0,
                       "mpNetworkModule->GetNetworkManager()->GetGamerCardManager()");

            BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
            const s32 liControllerPort = lpNetworkManager->GetLocalUserControllerPort();
            // The X360 passes `this` through the s32 user-data slot (32-bit pointers). On the
            // x64 gate the pointer is widened; round-trip it through uintptr_t so the recovered
            // callback can restore it (no information is lost within a single process image).
            s32 liResult = lpNetworkManager->GetGamerCardManager()->GetXuidForPlayer(
                static_cast<u32>(liControllerPort),
                reinterpret_cast<const PlayerName*>(lpScoreTargetEvent),
                &ScoreboardManager::RequestXUIDForPlayerCallback,
                static_cast<s32>(reinterpret_cast<uintptr_t>(this)));

            if (liResult == 0)
            {
                // Request did not start: clear the pending challenge state.
                mScoreTargetScoreboard          = 0;
                miScoreTargetValue              = 0;
                mScoreTargetPlayerID.macName[0] = 0;
                mScoreTargetPlayerID.mqXuid     = 0;
            }
        }
    }

    // -------------------------------------------------------------------------------------------
    // RequestXUIDForPlayerCallback  @ 0x82562E50  (static gamer-card-manager completion callback)
    // Fired by NetworkGamerCardManagerX360::GetXuidForPlayer once a challenged player's XUID has
    // been resolved by-name. Stamp the resolved XUID onto the pending player id and queue the same
    // score-target challenge network event HandleEvScoreTargetEvent would have queued directly.
    // (Register order: r3 lbSuccess, r4 lqRequestedXuid, r5 the ScoreboardManager `this`.)
    // -------------------------------------------------------------------------------------------
    s32 ScoreboardManager::RequestXUIDForPlayerCallback(s32 lbSuccess, u64 lqRequestedXuid,
                                                        s32 liUserData)
    {
        s32 liResult = lbSuccess;
        if (lbSuccess)
        {
            ScoreboardManager* lpScoreboardManager =
                reinterpret_cast<ScoreboardManager*>(static_cast<uintptr_t>(static_cast<u32>(liUserData)));
            CGS_ASSERT(lpScoreboardManager != 0, "lpScoreboardManager");
            CGS_ASSERT(lqRequestedXuid != 0, "lRequestedXuid != 0");

            // Re-build the pending id from its own (already-copied) name, now carrying the XUID.
            lpScoreboardManager->mScoreTargetPlayerID.Construct(
                lpScoreboardManager->mScoreTargetPlayerID.macName, lqRequestedXuid);

            ScoreTargetChallengeEvent lChallengeEvent;
            lChallengeEvent.mPlayerID       = lpScoreboardManager->mScoreTargetPlayerID;
            lChallengeEvent.mScoreboardSlot = lpScoreboardManager->mScoreTargetScoreboard;
            lChallengeEvent.miValue         = lpScoreboardManager->miScoreTargetValue;
            lChallengeEvent.muChallengeType = 0;

            CGS_ASSERT(lpScoreboardManager->mpNetworkModule != 0,
                       "lpScoreboardManager->mpNetworkModule");
            CGS_ASSERT(lpScoreboardManager->mpNetworkModule->GetNetworkEventQueue() != 0,
                       "lpScoreboardManager->mpNetworkModule->GetNetworkEventQueue()");
            liResult = AsConcreteQueue(
                lpScoreboardManager->mpNetworkModule->GetNetworkEventQueue())->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lChallengeEvent),
                    KI_NETEVENT_SCORE_TARGET, KI_SCORE_TARGET_PAYLOAD_SIZE);
        }
        return liResult;
    }

    // -------------------------------------------------------------------------------------------
    // CopyCategories  @ 0x82562590
    // Build a category heading list from the rankings component, broadcast it on the network output
    // queue (event-type 0x34, 2056 bytes) and hand it to the debug component to (re)render.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::CopyCategories()
    {
        BrnNetworkModuleIO::NetworkOutScoreboardHeadingList lHeadingList;
        lHeadingList.miLength                = 0;
        lHeadingList.meHeadingType           = BrnNetworkModuleIO::E_HEADING_CATEGORY;
        lHeadingList.mbIsPerRoad             = false;
        lHeadingList.mb807                   = false;

        for (s32 liCategoryCounter = 0;
             liCategoryCounter < mpRankings->GetNumberOfCategories();
             ++liCategoryCounter)
        {
            lHeadingList.AddHeading(mpRankings->GetCategoryName(liCategoryCounter));
        }

        AsConcreteQueue(mpNetworkModule->GetNetworkEventQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHeadingList),
            KI_NETEVENT_SCOREBOARD_HEADINGS, KI_SCOREBOARD_HEADINGS_PAYLOAD_SIZE);

        mDebugComponent.HandleScoreboardHeadingEvent(&lHeadingList);
    }

    // -------------------------------------------------------------------------------------------
    // CopyIndexes  @ 0x82562638
    // Build an index heading list for liCategory, broadcast it (event-type 0x34, 2056 bytes) and
    // hand it to the debug component to (re)render.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::CopyIndexes(s32 liCategory)
    {
        BrnNetworkModuleIO::NetworkOutScoreboardHeadingList lHeadingList;
        lHeadingList.miLength                = 0;
        lHeadingList.meHeadingType           = BrnNetworkModuleIO::E_HEADING_INDEX;
        lHeadingList.mbIsPerRoad             = false;
        lHeadingList.mb807                   = false;

        for (s32 liIndexCounter = 0;
             liIndexCounter < mpRankings->GetNumberOfIndexes(liCategory);
             ++liIndexCounter)
        {
            lHeadingList.AddHeading(mpRankings->GetIndexName(liCategory, liIndexCounter));
        }

        AsConcreteQueue(mpNetworkModule->GetNetworkEventQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHeadingList),
            KI_NETEVENT_SCOREBOARD_HEADINGS, KI_SCOREBOARD_HEADINGS_PAYLOAD_SIZE);

        mDebugComponent.HandleScoreboardHeadingEvent(&lHeadingList);
    }

    // -------------------------------------------------------------------------------------------
    // CopyVariations  @ 0x825626D8
    // Build a variation heading list for (liCategory, liIndex), broadcast it (event-type 0x34,
    // 2056 bytes) and hand it to the debug component to (re)render. Most variations take their name
    // straight from the rankings component (GetVariationName); the stunt-run / burn-route event
    // scoreboards instead synthesise their heading through a per-mode format-string table and flag
    // the list via the two trailing flag bytes.
    //
    // The SPrintf FORMAT strings are recovered rodata literals ("$%d" for param 3, "$EV_%06u" for
    // the param-4 stunt-run and param-5 burn-route branches -- asm r5 == aD_11 / aEv06u_0). Their
    // vararg VALUE is loaded (asm `ldx r6`) from a per-mode numeric table indexed by the loop
    // counter: the stunt-run / burn-route event-id tables (BrnNetworkEventScoreData.h) and, for
    // param 3, the 64-entry per-road id table.
    //
    // The per-road table is the street manager's save-game challenge road-id table (also read by
    // the online scoreboards screen).
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::CopyVariations(s32 liCategory, s32 liIndex)
    {
        BrnNetworkModuleIO::NetworkOutScoreboardHeadingList lHeadingList;
        lHeadingList.miLength                = 0;
        lHeadingList.meHeadingType           = BrnNetworkModuleIO::E_HEADING_VARIATION;
        lHeadingList.mbIsPerRoad             = false;
        lHeadingList.mb807                   = false;

        // The X360 body tracks the format-table index in its own register (r31) alongside the
        // variation counter (r30); both start at 0 and step together, so they stay equal.
        s32 liTableIndex = 0;
        for (s32 liVariationCounter = 0;
             liVariationCounter < mpRankings->GetNumberOfVariations(liCategory, liIndex);
             ++liVariationCounter)
        {
            mpRankings->SelectScoreboard(liCategory, liIndex, liVariationCounter);

            if (mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_VARIATION_FMT))
            {
                char lacHeading[32];
                CgsCore::SPrintf(lacHeading, KI_CELL_BUFFER_SIZE, "$%d",
                                 static_cast<s32>(BrnGameState::KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liTableIndex]));
                lHeadingList.AddHeading(lacHeading);
                lHeadingList.mbIsPerRoad             = true;
            }
            else if (mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_STUNT_RUN))
            {
                CGS_ASSERT(liVariationCounter < KI_NUM_STUNT_RUN_EVENT_SCOREBOARDS,
                           "liVariationLoopCounter < KI_NUM_STUNT_RUN_EVENT_SCOREBOARDS");
                char lacHeading[32];
                CgsCore::SPrintf(lacHeading, KI_CELL_BUFFER_SIZE, "$EV_%06u",
                                 static_cast<u32>(KAU64_STUNT_RUN_SCOREBOARD_EVENT_IDS[liTableIndex]));
                lHeadingList.AddHeading(lacHeading);
                lHeadingList.mbIsPerRoad             = false;
                lHeadingList.mb807                   = true;
            }
            else if (mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_BURN_ROUTE))
            {
                CGS_ASSERT(liVariationCounter < KI_NUM_BURN_ROUTE_EVENT_SCOREBOARDS,
                           "liVariationLoopCounter < KI_NUM_BURN_ROUTE_EVENT_SCOREBOARDS");
                char lacHeading[32];
                CgsCore::SPrintf(lacHeading, KI_CELL_BUFFER_SIZE, "$EV_%06u",
                                 static_cast<u32>(KAU64_BURN_ROUTE_SCOREBOARD_EVENT_IDS[liTableIndex]));
                lHeadingList.AddHeading(lacHeading);
                lHeadingList.mbIsPerRoad             = false;
                lHeadingList.mb807                   = true;
            }
            else
            {
                lHeadingList.mbIsPerRoad             = false;
                lHeadingList.AddHeading(
                    mpRankings->GetVariationName(liCategory, liIndex, liVariationCounter));
            }

            ++liTableIndex;
        }

        AsConcreteQueue(mpNetworkModule->GetNetworkEventQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHeadingList),
            KI_NETEVENT_SCOREBOARD_HEADINGS, KI_SCOREBOARD_HEADINGS_PAYLOAD_SIZE);

        mDebugComponent.HandleScoreboardHeadingEvent(&lHeadingList);
    }
    // -------------------------------------------------------------------------------------------
    // ProcessBeforeSimulation
    // Step the download state machine. While waiting, drain the GUI select events; a finished
    // heading download returns to waiting; a finished scoreboard download is built, posted to the
    // game (out-event 51) together with the matching challengeable event id for the stunt-run /
    // burn-route boards (out-event 54), and handed to the debug component.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::ProcessBeforeSimulation(
        BrnNetwork::BrnNetworkModuleIO::OutputBuffer* /*lpOutputBuffer*/)
    {
        switch (meCurrentState)
        {
        case E_STATE_WAITING_FOR_EVENTS:
            ProcessEventQueue();
            break;

        case E_STATE_GETTING_HEADINGS:
            if (mpRankings->GetStatus() == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
            {
                meCurrentState = E_STATE_WAITING_FOR_EVENTS;
            }
            break;

        case E_STATE_GETTING_SCOREBOARD:
        {
            const s32 liStatus = mpRankings->GetStatus();
            if (liStatus == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_BUSY)
            {
                break;
            }

            if (liStatus == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR)
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Downloading of scoreboard failed: " << mpRankings->GetLastError()
                           << " note E_SERVER_INTERFACE_RANKINGS_ERROR_START="
                           << static_cast<s32>(CgsNetwork::E_SERVER_INTERFACE_RANKINGS_ERROR_START) << "\n";
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();

                meCurrentState = E_STATE_WAITING_FOR_EVENTS;
                mpRankings->ClearLastError();
            }
            else if (liStatus == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
            {
                BrnNetworkModuleIO::NetworkOutScoreboardEvent lScoreboardEvent;
                lScoreboardEvent.Construct();
                BuildDownloadedScoreboard(lScoreboardEvent.GetScoreboard());
                AsConcreteQueue(mpNetworkModule->GetNetworkEventQueue())->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lScoreboardEvent),
                    lScoreboardEvent.GetEventType(), sizeof(lScoreboardEvent));

                const u64* lpau64EventIDs = nullptr;
                if (mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_STUNT_RUN))
                {
                    CGS_ASSERT(miCurrentVariation < KI_NUM_STUNT_RUN_EVENT_SCOREBOARDS,
                               "miCurrentVariation < KI_NUM_STUNT_RUN_EVENT_SCOREBOARDS");
                    lpau64EventIDs = KAU64_STUNT_RUN_SCOREBOARD_EVENT_IDS;
                }
                else if (mpRankings->ScoreboardHasParam(KI_SCOREBOARD_PARAM_BURN_ROUTE))
                {
                    CGS_ASSERT(miCurrentVariation < KI_NUM_BURN_ROUTE_EVENT_SCOREBOARDS,
                               "miCurrentVariation < KI_NUM_BURN_ROUTE_EVENT_SCOREBOARDS");
                    lpau64EventIDs = KAU64_BURN_ROUTE_SCOREBOARD_EVENT_IDS;
                }

                if (lpau64EventIDs != nullptr)
                {
                    BrnNetworkModuleIO::NetworkOutDldChallengeableEvent lChallengeableEvent;
                    lChallengeableEvent.mID = lpau64EventIDs[miCurrentVariation];
                    CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
                    CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != 0,
                               "mpNetworkModule->GetNetworkEventQueue()");
                    AsConcreteQueue(mpNetworkModule->GetNetworkEventQueue())->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lChallengeableEvent),
                        lChallengeableEvent.GetEventType(), sizeof(lChallengeableEvent));
                }

                meCurrentState = E_STATE_WAITING_FOR_EVENTS;
                mDebugComponent.HandleScoreboardEvent(lScoreboardEvent.GetScoreboard());
            }
            else
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Rankings component in bad state: " << liStatus;
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
            break;
        }

        default:
            break;
        }
    }

    // -------------------------------------------------------------------------------------------
    // ProcessEventQueue
    // Drain the GUI select requests. A heading request (category / index / variation list or a
    // scoreboard) cancels the rankings action in flight and returns to waiting first. A scoreboard
    // request selects it and downloads its rows -- for a friends scoreboard with every buddy plus
    // the local player as the user list -- then waits for that download, centred on the local
    // player. Page up / down move the visible window by a page.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::ProcessEventQueue()
    {
        BrnNetworkModuleIO::NetworkInSelectScoreboardEvent lEvent;
        lEvent.Prepare();

        while (mScoreboardEventQueue.GetLength() > 0)
        {
            mScoreboardEventQueue.Pop(&lEvent);

            const BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::EScoreboardEventType leType =
                lEvent.GetScoreboardEventType();
            if (leType > BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::E_TYPE_NONE
                && leType <= BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::E_TYPE_GET_SCOREBOARD)
            {
                mpRankings->CancelCurrentActionAndInvalidateScoreboard();
                meCurrentState = E_STATE_WAITING_FOR_EVENTS;
            }

            switch (leType)
            {
            case BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::E_TYPE_GET_CATEGORY:
                CopyCategories();
                miCurrentCategory  = KI_INVALID_HEADING;
                miCurrentIndex     = KI_INVALID_HEADING;
                miCurrentVariation = KI_INVALID_HEADING;
                break;

            case BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::E_TYPE_GET_INDEX:
                miCurrentCategory = lEvent.GetChosenCategory();
                CopyIndexes(miCurrentCategory);
                miCurrentIndex     = KI_INVALID_HEADING;
                miCurrentVariation = KI_INVALID_HEADING;
                break;

            case BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::E_TYPE_GET_VARIATION:
                CGS_ASSERT(miCurrentCategory != KI_INVALID_HEADING, "miCurrentCategory != KI_INVALID_HEADING");
                miCurrentIndex = lEvent.GetChosenIndex();
                CopyVariations(miCurrentCategory, miCurrentIndex);
                miCurrentVariation = KI_INVALID_HEADING;
                break;

            case BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::E_TYPE_GET_SCOREBOARD:
            {
                PlayerInfoData lPlayerInfo;

                CGS_ASSERT(mpRankings != nullptr, "mpRankings");
                CGS_ASSERT(miCurrentCategory != KI_INVALID_HEADING, "miCurrentCategory != KI_INVALID_HEADING");
                CGS_ASSERT(miCurrentIndex != KI_INVALID_HEADING, "miCurrentIndex != KI_INVALID_HEADING");

                miCurrentVariation = lEvent.GetChosenVariation();
                mpRankings->SelectScoreboard(miCurrentCategory, miCurrentIndex, miCurrentVariation);
                miCurrentViewOffset = 0;

                const char* lapcUserListNames[KI_MAX_USER_LIST_NAMES];
                s32         liUserListCount = 0;
                const s32   liUserType      = mpRankings->GetUserType();
                if (liUserType == KI_USER_TYPE_BUDDIES)
                {
                    CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
                    CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr, "mpNetworkModule->GetNetworkManager()");
                    BuddyManagerX360* lpBuddyManager = mpNetworkModule->GetNetworkManager()->GetBuddyManager();
                    CGS_ASSERT(lpBuddyManager != nullptr, "lpBuddyManager");
                    liUserListCount = lpBuddyManager->GetAllBuddyNames(lapcUserListNames);

                    CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetServerInterface() != nullptr,
                               "mpNetworkModule->GetNetworkManager()->GetServerInterface()");
                    CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetPlayerInfoComponent() != nullptr,
                               "mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetPlayerInfoComponent()");
                    mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetPlayerInfoComponent()
                        ->GetLocalPlayerInfo(&lPlayerInfo);
                    lapcUserListNames[liUserListCount] = lPlayerInfo.GetName();
                    ++liUserListCount;
                }
                else if (liUserType != 0)
                {
                    CgsDev::Assert::BeginAssert();
                    char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "User type for C" << miCurrentCategory << " I" << miCurrentIndex
                               << " V" << miCurrentVariation << " is invalid :" << liUserType << "\n";
                    CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }

                mpRankings->DownloadScoreboardData(lapcUserListNames, liUserListCount);
                miCurrentViewOffset = KI_INVALID_HEADING;   // centre the first page on the local player
                meCurrentState      = E_STATE_GETTING_SCOREBOARD;
                break;
            }

            case BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::E_TYPE_PAGE_UP:
                OffsetScoreboard(-KI_MAX_SCOREBOARD_ROWS);
                break;

            case BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::E_TYPE_PAGE_DOWN:
                OffsetScoreboard(KI_MAX_SCOREBOARD_ROWS);
                break;

            default:
                CGS_ASSERT(false, "Unknown scoreboard event");
                break;
            }
        }
    }

    // -------------------------------------------------------------------------------------------
    // HandleScoreboardEvent
    // A GUI "select scoreboard" request. The first request after a reset also starts the heading
    // download; the queue holds only the newest request.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::HandleScoreboardEvent(
        const BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent* lpSelectScoreboardEvent)
    {
        if (meCurrentState == E_STATE_IDLE)
        {
            SetViewAndDownloadHeaders();
        }

        mScoreboardEventQueue.Clear();
        mScoreboardEventQueue.Push(lpSelectScoreboardEvent);
    }

    // -------------------------------------------------------------------------------------------
    // AddNumberBeforeAndAfter
    // Record how many rankings rows sit above the visible window (the view offset) and below it
    // (clamped at 0), logging both.
    // -------------------------------------------------------------------------------------------
    void ScoreboardManager::AddNumberBeforeAndAfter(Scoreboard* lpScoreboard, s8 liCurrentRows)
    {
        s8 liAfter = static_cast<s8>(liCurrentRows - miCurrentViewOffset - 9);

        *CgsDev::Log::gpDebugPrint << "RG\nBefore : " << miCurrentViewOffset
                                   << "\nAfter : " << static_cast<s32>(liAfter) << "\n";

        if (liAfter <= 0)
        {
            liAfter = 0;
        }
        lpScoreboard->AddNumberBeforeAndAfter(static_cast<s8>(miCurrentViewOffset), liAfter);
    }
}
