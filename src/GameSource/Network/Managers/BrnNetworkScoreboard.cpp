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

#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::StrnCpy
#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (the bounded-copy assert)

#include <xtl.h>
#include <cstring>   // std::strlen / std::strncpy (the cell and title copies)

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkScoreboard.h".
static const char KAC_ARTIST_ASSERT_SOURCE[] =
    "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkScoreboard.cpp";

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
        if (lpcTitle != 0)
        {
            // CgsCore::StrnCpy (ARTIST 0x82546F7C asserts CgsStringUtils.h:55).
            CgsCore::StrnCpy(macTitle, lpcTitle, sizeof(macTitle));
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
        CGS_ASSERT_AT(lpcData != 0, "lpcData", KAC_ARTIST_ASSERT_SOURCE, 330);
        CGS_ASSERT_AT(miNumberOfColumns < KI_MAX_SCOREBOARD_COLUMNS,
                      "miNumberOfColumns < KI_MAX_SCOREBOARD_COLUMNS", KAC_ARTIST_ASSERT_SOURCE, 331);

        // CgsCore::StrnCpy (ARTIST 0x82547100 asserts CgsStringUtils.h:55).
        CgsCore::StrnCpy(maacData[miNumberOfColumns], lpcData, sizeof(maacData[0]));
        ++miNumberOfColumns;
    }

    const char* ScoreboardRow::GetData(s32 liColumnNumber) const
    {
        CGS_ASSERT_AT(liColumnNumber < miNumberOfColumns,
            "liColumnNumber < miNumberOfColumns", KAC_ARTIST_ASSERT_SOURCE, 402 /* ARTIST 0x82547164 */);

        CGS_ASSERT_AT(liColumnNumber >= 0,
            "liColumnNumber >= 0", KAC_ARTIST_ASSERT_SOURCE, 403 /* ARTIST 0x82547188 */);
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
        CGS_ASSERT_AT(miNumberOfColumns < KI_MAX_SCOREBOARD_COLUMNS,
            "miNumberOfColumns < KI_MAX_SCOREBOARD_COLUMNS",
            KAC_ARTIST_ASSERT_SOURCE, 198 /* ARTIST 0x82546E4C */);
        XMemCpy(&maColumns[miNumberOfColumns], lpColumn, sizeof(*lpColumn));
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
        CGS_ASSERT_AT(liRowNumber >= 0,
            "liRowNumber >= 0", __FILE__, 309 /* ARTIST 0x8240E9D4 */);
        CGS_ASSERT_AT(liRowNumber < miNumberOfRows,
            "liRowNumber < miNumberOfRows", __FILE__, 310 /* ARTIST 0x8240EA00 */);

        return &maRows[liRowNumber];
    }
}

// FOLDED FROM BrnNetworkScoreboard_wS34_00.cpp
// BrnNetworkScoreboard_wS34_00.cpp -- ScoreboardColumn's type and width accessors
// (BrnNetworkScoreboard.cpp family). The console inlines both (the leaderboard table reads
// the type word at +0 and the width halfword at +4 directly).

#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"

namespace BrnNetwork
{
    ScoreboardColumn::EDataType ScoreboardColumn::GetType() const
    {
        return meDataType;
    }

    s32 ScoreboardColumn::GetWidth() const
    {
        return miWidth;
    }
}
