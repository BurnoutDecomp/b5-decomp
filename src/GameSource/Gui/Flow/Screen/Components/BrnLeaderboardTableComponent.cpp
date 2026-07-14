#include "GameSource/Gui/Flow/Screen/Components/BrnLeaderboardTableComponent.h"

#include <cstdlib>   // std::atof
#include <cstring>   // std::memcpy

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDCompress / CgsIDConvertToString
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface / GuiAccessPointers
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // GuiAccessPointers::GetGuiCache
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController::GetVehicleList
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry

// BrnGui::LeaderboardTableComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (7 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/Components/BrnLeaderboardTableComponent.cpp):
//   LeaderboardTableComponent::Construct            @0x824190B0
//   LeaderboardTableComponent::SetupScoreboard      @0x8243BBD0
//   LeaderboardTableComponent::DrawScoreboard       @0x82436188
//   LeaderboardTableComponent::SetCell              @0x824346C8
//   LeaderboardTableComponent::LocaliseTextInCell   @0x82426938
//   LeaderboardTableComponent::GetHighlightedScore  @0x82419368
//   LeaderboardTableComponent::SetTargetGamertag    @0x82436580

namespace BrnGui
{
    const char LeaderboardTableComponent::KAC_COLUMN_NAME_TEMPLATE[10] = "Column_%d";

    const char* const LeaderboardTableComponent::KAPC_COLUMN_STRINGS[KI_MAX_COLUMNS] =
    {
        "apt_Pos_0", "apt_Pos_1", "apt_Pos_2", "apt_Pos_3", "apt_Pos_4",
        "apt_Pos_5", "apt_Pos_6", "apt_Pos_7", "apt_Pos_8", "apt_Pos_9",
    };

    // @ 0x824190B0
    void LeaderboardTableComponent::Construct(const char* lpacName,
                                              CgsGui::StateInterface* lpStateInterface,
                                              const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        // Construct each child column as "Column_<n>", parented under this component's name and
        // sharing its state interface (the X360 calls the column's virtual Construct through its
        // vtable; each maColumns[n] is a concrete LeaderboardColumnComponent).
        for (s32 liColumn = 0; liColumn < KI_MAX_COLUMNS; ++liColumn)
        {
            char lacColumnName[128];
            CgsCore::SPrintf(lacColumnName, 64, KAC_COLUMN_NAME_TEMPLATE, liColumn);
            lacColumnName[63] = 0;
            maColumns[liColumn].Construct(lacColumnName, mpStateInterface, macName);
        }

        miColumnsUsed = 0;
        miRowsUsed    = 0;
        miHighlight   = -1;
        miLocalPlayer = -1;

        // Walk state interface -> access pointers -> gui cache -> world-data controller and cache
        // the vehicle list. The X360 inlines each accessor's non-null assert into this body.
        CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
        CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");

        BrnGui::GuiCache* lpGuiCache = lpAccessPointers->GetGuiCache();
        CGS_ASSERT(lpGuiCache != 0, "mpGuiCache");

        BrnGui::WorldDataController* lpWorldDataController = lpGuiCache->GetWorldDataController();
        CGS_ASSERT(lpWorldDataController != 0, "mpWorldDataController");

        mpVehicleList = lpWorldDataController->GetVehicleList();
        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList");
    }

    // @ 0x8243BBD0
    void LeaderboardTableComponent::SetupScoreboard(const BrnNetwork::Scoreboard* lpScoreboard)
    {
        if (lpScoreboard != 0)
            std::memcpy(&mScoreboard, lpScoreboard, sizeof(BrnNetwork::Scoreboard));
        else
            mScoreboard.Construct();

        miColumnsUsed = mScoreboard.GetNumberOfColumns();
        miRowsUsed    = mScoreboard.GetNumberOfRows();
        miLocalPlayer = -1;

        DrawScoreboard();
    }

    // @ 0x82436188
    void LeaderboardTableComponent::DrawScoreboard()
    {
        // apt_Used <- the number of columns in use.
        char lacUsed[8];
        CgsCore::SPrintf(lacUsed, 8, "%d", miColumnsUsed);
        lacUsed[7] = 0;
        AddOutputAptViewState("apt_Used", lacUsed, false);

        // apt_Selection <- "invisible" when nothing is highlighted, else "Row<highlight>".
        const char* lpcSelection;
        char lacRow[8];
        if (miHighlight == -1)
        {
            lpcSelection = "invisible";
        }
        else
        {
            CgsCore::SPrintf(lacRow, 8, "Row%d", static_cast<s32>(miHighlight));
            lacRow[7] = 0;
            lpcSelection = lacRow;
        }
        AddOutputAptViewState("apt_Selection", lpcSelection, false);

        // Total width across the used columns.
        s32 liWidth = 0;
        for (s32 liColumn = 0; liColumn < miColumnsUsed; ++liColumn)
            liWidth += mScoreboard.GetColumn(liColumn)->GetWidth();

        // Lay the table out symmetrically about its centre: the running position starts at the
        // left edge (-liWidth/2) and walks right by each column's full width.
        s32 liCurrentPosition = -(liWidth / 2);
        char lacSelectionPos[8];
        CgsCore::SPrintf(lacSelectionPos, 8, "%d", liCurrentPosition);
        lacSelectionPos[7] = 0;
        AddOutputAptViewState("apt_SelectionPos", lacSelectionPos, false);

        for (s32 liColumn = 0; liColumn < miColumnsUsed; ++liColumn)
        {
            const BrnNetwork::ScoreboardColumn* lpColumn = mScoreboard.GetColumn(liColumn);
            const s32 liHalfWidth = lpColumn->GetWidth() / 2;
            const s32 liColumnCentre = liHalfWidth + liCurrentPosition;

            // apt_Pos_<n> <- this column's centre position.
            char lacPos[8];
            CgsCore::SPrintf(lacPos, 8, "%d", liColumnCentre);
            lacPos[7] = 0;
            AddOutputAptViewState(KAPC_COLUMN_STRINGS[liColumn], lacPos, false);

            // Drive the column header, then fill its cells from the scoreboard rows.
            maColumns[liColumn].Init(lpColumn->GetTitle(), miHighlight, miLocalPlayer, liHalfWidth);
            for (s32 liRow = 0; liRow < miRowsUsed; ++liRow)
                SetCell(liRow, liColumn, lpColumn->GetType());

            // apt_CellsUsed on the column <- how many cells it now holds.
            char lacCellsUsed[4];
            CgsCore::SPrintf(lacCellsUsed, 4, "%d", static_cast<s32>(maColumns[liColumn].miCellCount));
            lacCellsUsed[3] = 0;
            maColumns[liColumn].AddOutputAptViewState("apt_CellsUsed", lacCellsUsed, false);

            liCurrentPosition = liHalfWidth + liColumnCentre;
        }

        CGS_ASSERT(liCurrentPosition == liWidth / 2, "liCurrentPosition == liWidth / 2");

        // Blank the unused columns: empty title, the current highlight/local-player markers, and
        // a zero cell count.
        for (s32 liColumn = miColumnsUsed; liColumn < KI_MAX_COLUMNS; ++liColumn)
        {
            // FLAG: X360 rodata &unk_820046A7 -- reconstructed as the empty string (blank title).
            maColumns[liColumn].AddOutputAptViewState("apt_Title", "", false);

            char lacHighlight[4];
            CgsCore::SPrintf(lacHighlight, 4, "%d", static_cast<s32>(miHighlight));
            lacHighlight[3] = 0;
            maColumns[liColumn].AddOutputAptViewState("apt_Highlight", lacHighlight, false);

            char lacYou[4];
            CgsCore::SPrintf(lacYou, 4, "%d", static_cast<s32>(miLocalPlayer));
            lacYou[3] = 0;
            maColumns[liColumn].AddOutputAptViewState("apt_You", lacYou, false);

            maColumns[liColumn].miCellCount = 0;

            char lacCellsUsed[4];
            CgsCore::SPrintf(lacCellsUsed, 4, "%d", 0);
            lacCellsUsed[3] = 0;
            maColumns[liColumn].AddOutputAptViewState("apt_CellsUsed", lacCellsUsed, false);
        }
    }

    // @ 0x824346C8
    void LeaderboardTableComponent::SetCell(s32 liRow, s32 liColumn,
                                            BrnNetwork::ScoreboardColumn::EDataType leDataType)
    {
        const char* lpcData = mScoreboard.GetRow(liRow)->GetData(liColumn);

        switch (leDataType)
        {
            case BrnNetwork::ScoreboardColumn::E_DATATYPE_NUMBER:   // 0
            case BrnNetwork::ScoreboardColumn::E_DATATYPE_COUNT:    // 8
                LocaliseTextInCell(liRow, liColumn, lpcData,
                                   CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                break;

            case BrnNetwork::ScoreboardColumn::E_DATATYPE_PERCENT:  // 1
                LocaliseTextInCell(liRow, liColumn, lpcData,
                                   CgsLanguage::LanguageManager::E_FORMAT_PERCENTAGE);
                break;

            case BrnNetwork::ScoreboardColumn::E_DATATYPE_NAME:     // 2
            case BrnNetwork::ScoreboardColumn::E_DATATYPE_STRING:   // 3
                maColumns[liColumn].SetCell(lpcData);
                break;

            case BrnNetwork::ScoreboardColumn::E_DATATYPE_CAR_CGS_ID:  // 4
            {
                CgsID lCarId = CgsIDCompress(lpcData);
                const BrnResource::VehicleListEntry* lpVehicleData =
                    mpVehicleList->GetVehicleFromId(lCarId);
                CGS_ASSERT(lpVehicleData != 0, "lpVehicleData");

                // A derived/livery variant displays under its parent car's id.
                if (lpVehicleData->GetLiveryType() != 2 && lpVehicleData->GetParentId() != 0)
                    lCarId = lpVehicleData->GetParentId();

                char lacCarId[16];
                CgsIDConvertToString(lCarId, lacCarId);
                char lacCarString[128];
                CgsCore::SPrintf(lacCarString, 128, "CAR_%s", lacCarId);
                lacCarString[127] = 0;
                LocaliseTextInCell(liRow, liColumn, lacCarString,
                                   CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
                break;
            }

            case BrnNetwork::ScoreboardColumn::E_DATATYPE_TIME:     // 5
            {
                char lacTime[128];
                CgsCore::SPrintf(lacTime, 128, "%f",
                                 static_cast<f32>(std::atof(lpcData)) * 0.001f);
                lacTime[127] = 0;
                LocaliseTextInCell(liRow, liColumn, lacTime,
                                   CgsLanguage::LanguageManager::E_FORMAT_MINUTES_SECONDS_HUNDREDTHS);
                break;
            }

            case BrnNetwork::ScoreboardColumn::E_DATATYPE_DISTANCE: // 6
                LocaliseTextInCell(liRow, liColumn, lpcData,
                                   CgsLanguage::LanguageManager::E_FORMAT_SMALL_DISTANCE);
                break;

            case BrnNetwork::ScoreboardColumn::E_DATATYPE_CURRENCY: // 7
                LocaliseTextInCell(liRow, liColumn, lpcData,
                                   CgsLanguage::LanguageManager::E_FORMAT_MONEY);
                break;

            default:
                maColumns[liColumn].SetCell(lpcData);
                // X360 streams "Unhandled cell type : " << leDataType << "\n" into the assert
                // buffer; folded here to a static message (the value/newline drop matches the
                // file/line drop the CGS_ASSERT convention already applies).
                CGS_ASSERT(false, "Unhandled cell type : ");
                break;
        }
    }

    // @ 0x82426938
    void LeaderboardTableComponent::LocaliseTextInCell(
            s32 liRow, s32 liColumn, const char* lpcData,
            CgsLanguage::LanguageManager::ParameterFormatType leFormat)
    {
        CGS_ASSERT(mpStateInterface != 0,
                   "Invalid state interface in TextField::SetLocalisedText");

        // Build a unique "$"-sigil localisation reference for this cell.
        char lacKey[128];
        CgsCore::SPrintf(lacKey, 128, "$LEADERBOARD_%d_%d", liRow, liColumn);
        lacKey[127] = 0;

        // Resolve the raw cell data through the requested format into a local buffer, then
        // register it in the localisation database under the key WITHOUT its '$' sigil.
        CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
        char lacValue[1024];
        lpLanguageManager->FormatText(lacValue, 1024, lpcData, leFormat);

        mpStateInterface->GetLanguageManager()->AddString(lacKey + 1,
                                                          reinterpret_cast<const u8*>(lacValue));

        // Point the column cell at the "$"-sigil reference so the display layer resolves it.
        maColumns[liColumn].SetCell(lacKey);
    }

    // @ 0x82419368
    s32 LeaderboardTableComponent::GetHighlightedScore() const
    {
        // Scan the used columns for the first score-bearing data type (a plain number, a time,
        // or a currency); the scoreboard's own GetColumn asserts the per-column index bounds.
        s32 liColumn = 0;
        while (liColumn < miColumnsUsed)
        {
            const BrnNetwork::ScoreboardColumn::EDataType leType =
                mScoreboard.GetColumn(liColumn)->GetType();
            if (leType == BrnNetwork::ScoreboardColumn::E_DATATYPE_NUMBER ||
                leType == BrnNetwork::ScoreboardColumn::E_DATATYPE_TIME ||
                leType == BrnNetwork::ScoreboardColumn::E_DATATYPE_CURRENCY)
                break;
            ++liColumn;
        }
        CGS_ASSERT(liColumn < miColumnsUsed, "liColumn < miColumnsUsed");

        // Read that column's cell for the currently-highlighted row and parse it as an integer
        // (GetRow asserts the row-index bounds against the scoreboard's row count).
        const char* lpcData = mScoreboard.GetRow(miHighlight)->GetData(liColumn);
        return std::atoi(lpcData);
    }

    // @ 0x82436580
    void LeaderboardTableComponent::SetTargetGamertag(const char* lpacPlayerName)
    {
        CGS_ASSERT(lpacPlayerName != 0, "lpPlayerName");

        // An empty target clears the local-player highlight and redraws.
        if (*lpacPlayerName == 0)
        {
            miLocalPlayer = -1;
            DrawScoreboard();
            return;
        }

        // Find the name column (GetColumn asserts the per-column index bounds).
        s32 liColumn = 0;
        while (liColumn < miColumnsUsed)
        {
            if (mScoreboard.GetColumn(liColumn)->GetType() ==
                BrnNetwork::ScoreboardColumn::E_DATATYPE_NAME)
                break;
            ++liColumn;
        }

        // No name column: nothing to match against, so clear the highlight and redraw.
        if (liColumn >= miColumnsUsed)
        {
            miLocalPlayer = -1;
            DrawScoreboard();
            return;
        }

        // Walk the used rows, matching each row's name cell against the target gamertag;
        // miLocalPlayer doubles as the row cursor and lands on the matching index.
        for (miLocalPlayer = 0; miLocalPlayer < miRowsUsed; miLocalPlayer = miLocalPlayer + 1)
        {
            const char* lpcName = mScoreboard.GetRow(miLocalPlayer)->GetData(liColumn);
            if (std::strcmp(lpcName, lpacPlayerName) == 0)
                break;
        }

        // Running off the end means no row matched: clear the highlight.
        if (miLocalPlayer >= miRowsUsed)
            miLocalPlayer = -1;

        DrawScoreboard();
    }
}
