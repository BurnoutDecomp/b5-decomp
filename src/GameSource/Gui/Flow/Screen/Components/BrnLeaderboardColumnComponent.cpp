#include "GameSource/Gui/Flow/Screen/Components/BrnLeaderboardColumnComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

// BrnGui::LeaderboardColumnComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/Components/BrnLeaderboardColumnComponent.cpp):
//   LeaderboardColumnComponent::Construct @0x824194C8
//   LeaderboardColumnComponent::Init      @0x82419500  (LeaderboardTableComponent::DrawScoreboard)
//   LeaderboardColumnComponent::SetCell   @0x824195E8  (LeaderboardTableComponent::SetCell /
//                                                       ::LocaliseTextInCell)
//
// Init (asm walk): apt_Title <- the title text; apt_Highlight / apt_You <- the "%d"
// formatted (sign-extended s8) row numbers via 4-byte SPrintf buffers with a defensive
// NUL at [3]; apt_HalfWidth <- "%d" of liHalfWidth (8-byte buffer, NUL at [7]) only when
// positive; then reset the cell cursor. SetCell: bound-assert the cursor (non-gating),
// push the indexed "apt_Cell_<n>" view state with the cell text, advance.

namespace BrnGui
{
    const char* const LeaderboardColumnComponent::KAPC_CELL_STRINGS[KI_MAX_CELLS] =
    {
        "apt_Cell_0", "apt_Cell_1", "apt_Cell_2", "apt_Cell_3",
        "apt_Cell_4", "apt_Cell_5", "apt_Cell_6", "apt_Cell_7",
    };

    // @ 0x824194C8
    void LeaderboardColumnComponent::Construct(const char* lpacName,
                                               CgsGui::StateInterface* lpStateInterface,
                                               const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
        miCellCount = 0;
    }

    // @ 0x82419500
    void LeaderboardColumnComponent::Init(const char* lpcTitle, s8 liHighlightRow,
                                          s8 liYouRow, s32 liHalfWidth)
    {
        AddOutputAptViewState("apt_Title", lpcTitle, false);

        char lacHighlight[4];
        CgsCore::SPrintf(lacHighlight, 4, "%d", static_cast<s32>(liHighlightRow));
        lacHighlight[3] = 0;
        AddOutputAptViewState("apt_Highlight", lacHighlight, false);

        char lacYou[4];
        CgsCore::SPrintf(lacYou, 4, "%d", static_cast<s32>(liYouRow));
        lacYou[3] = 0;
        AddOutputAptViewState("apt_You", lacYou, false);

        if (liHalfWidth > 0)
        {
            char lacHalfWidth[8];
            CgsCore::SPrintf(lacHalfWidth, 8, "%d", liHalfWidth);
            lacHalfWidth[7] = 0;
            AddOutputAptViewState("apt_HalfWidth", lacHalfWidth, false);
        }

        miCellCount = 0;
    }

    // @ 0x824195E8
    void LeaderboardColumnComponent::SetCell(const char* lpcCellText)
    {
        CGS_ASSERT(miCellCount < KI_MAX_CELLS, "miCellCount < KI_MAX_CELLS");

        AddOutputAptViewState(KAPC_CELL_STRINGS[miCellCount], lpcCellText, false);
        ++miCellCount;
    }
}
