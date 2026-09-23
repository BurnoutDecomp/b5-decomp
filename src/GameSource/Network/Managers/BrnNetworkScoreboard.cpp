// ---- GameSource/Network/Managers/BrnNetworkScoreboard.cpp ----
// Definition home for BrnNetwork::Scoreboard (and its column/row sub-objects).
//
// BrnNetwork::Scoreboard::GetRow  @ 0x8240E9A8
//   (called by BrnGui::LeaderboardTableComponent::SetCell)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 body:
//   * assert liRowNumber >= 0                 ("liRowNumber >= 0",        :309)
//   * assert liRowNumber < miNumberOfRows      ("liRowNumber < miNumberOfRows", :310)
//       miNumberOfRows is read as a sign-extended byte (lbz + extsb at +0xB68), i.e. the int8
//       member; the comparison is signed (cmpw) against that sign-extended value.
//   * return &maRows[liRowNumber]  (mulli 0x137 == row stride 311, add this, addi 0x190 == maRows
//     base offset 400).
// Both streamed file-line asserts are reduced to the project CGS_ASSERT convention. Member access
// is by name; the row stride / base offset are produced by the committed Scoreboard layout, not by
// raw-offset arithmetic.

#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (the bounded-copy assert)

#include <cstring>   // std::strlen / std::strncpy (the cell and title copies)

namespace BrnNetwork
{
    // ---------------------------------------------------------------------------------------
    // ScoreboardColumn
    // ---------------------------------------------------------------------------------------

    // Copy the title (length-checked into the 31-char buffer, or empty when none is given) and
    // store the width, style and data type. The width and style are narrowed into their s16 / s8
    // members.
    bool ScoreboardColumn::Prepare(const char* lpcTitle, s32 liWidth, s32 liStyle, EDataType leDataType)
    {
        if (lpcTitle != nullptr)
        {
            // The bounded copy names the offending string when it does not fit, then copies anyway.
            if (!(std::strlen(lpcTitle) < sizeof(macTitle)))
            {
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "String too long: " << lpcTitle;
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
            std::strncpy(macTitle, lpcTitle, sizeof(macTitle));
        }
        else
        {
            macTitle[0] = 0;
        }

        miWidth    = static_cast<s16>(liWidth);
        miStyle    = static_cast<s8>(liStyle);
        meDataType = leDataType;
        return true;
    }

    // ---------------------------------------------------------------------------------------
    // ScoreboardRow
    // ---------------------------------------------------------------------------------------

    // Append one cell: length-check the text into the next 31-char cell and count it.
    void ScoreboardRow::AddCell(const char* lpcData)
    {
        CGS_ASSERT(lpcData != nullptr, "lpcData");
        CGS_ASSERT(miNumberOfColumns < KI_MAX_SCOREBOARD_COLUMNS, "miNumberOfColumns < KI_MAX_SCOREBOARD_COLUMNS");

        char* lpcCell = maacData[miNumberOfColumns];
        // The bounded copy names the offending string when it does not fit, then copies anyway.
        if (!(std::strlen(lpcData) < sizeof(maacData[0])))
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "String too long: " << (lpcData != nullptr ? lpcData : "<NULLSTRING>");
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
        std::strncpy(lpcCell, lpcData, sizeof(maacData[0]));
        ++miNumberOfColumns;
    }

    const char* ScoreboardRow::GetData(s32 liColumnNumber) const
    {
        CGS_ASSERT(liColumnNumber < miNumberOfColumns, "liColumnNumber < miNumberOfColumns");
        CGS_ASSERT(liColumnNumber >= 0, "liColumnNumber >= 0");
        return maacData[liColumnNumber];
    }

    // ---------------------------------------------------------------------------------------
    // Scoreboard
    // ---------------------------------------------------------------------------------------

    void Scoreboard::Construct()
    {
        macTitle[0]       = 0;
        miNumberOfColumns = 0;
        miNumberOfRows    = 0;
        ClearAllScoreboardData();
    }

    void Scoreboard::Destruct()
    {
        ClearAllScoreboardData();
    }

    // Copy a finished row into the next free row slot.
    void Scoreboard::AddRow(ScoreboardRow* lpRow)
    {
        CGS_ASSERT(miNumberOfRows < KI_MAX_SCOREBOARD_ROWS, "miNumberOfRows < KI_MAX_SCOREBOARD_ROWS");
        maRows[miNumberOfRows] = *lpRow;
        ++miNumberOfRows;
    }

    // Copy a prepared column heading into the next free column slot.
    void Scoreboard::AddColumn(ScoreboardColumn* lpColumn)
    {
        CGS_ASSERT(miNumberOfColumns < KI_MAX_SCOREBOARD_COLUMNS, "miNumberOfColumns < KI_MAX_SCOREBOARD_COLUMNS");
        maColumns[miNumberOfColumns] = *lpColumn;
        ++miNumberOfColumns;
    }

    // Clear every column heading and forget the column count.
    void Scoreboard::ClearAllColumnData()
    {
        for (s32 liColumn = 0; liColumn < KI_MAX_SCOREBOARD_COLUMNS; ++liColumn)
        {
            maColumns[liColumn].Clear();
        }
        miNumberOfColumns = 0;
    }

    // Clear the headings, every row, and the row / before / after counts.
    void Scoreboard::ClearAllScoreboardData()
    {
        ClearAllColumnData();

        for (s32 liRow = 0; liRow < KI_MAX_SCOREBOARD_ROWS; ++liRow)
        {
            maRows[liRow].Clear();
        }

        miNumberOfRows       = 0;
        miNumberOfRowsBefore = 0;
        miNumberOfRowsAfter  = 0;
    }

    const ScoreboardRow* Scoreboard::GetRow(s32 liRowNumber) const
    {
        CGS_ASSERT(liRowNumber >= 0, "liRowNumber >= 0");
        CGS_ASSERT(liRowNumber < miNumberOfRows, "liRowNumber < miNumberOfRows");
        return &maRows[liRowNumber];
    }
}
