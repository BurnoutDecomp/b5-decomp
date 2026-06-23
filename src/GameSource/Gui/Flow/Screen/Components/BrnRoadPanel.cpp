// ===================================================================================
// BrnGui::RoadPanel  -- implementation
//   class:BrnGui::RoadPanel
//
// GetSelectedFriendName @ 0x82417FF8
//   Asserts the panel's scoring mode (+0xDA0) equals the ONLINE road-panel mode
//   ("GetScoringMode() == GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE",
//   BrnRoadPanel.h:231), then returns the address of the selected friend row:
//   this + 0x1A2 + index*0xA2 where index = *(this+0xD9C). The row's leading bytes are
//   the friend-name string, so the computed address is the name pointer. Reconstructed
//   store-for-store from the X360 asm (mulli 0xA2 / add this / addi 0x1A2).
//   CrashNavPanel::IsRoadRuleFriendSelected consumes the returned name.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnRoadPanel.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{
    // @ 0x82417FF8
    const char* RoadPanel::GetSelectedFriendName() const
    {
        CGS_ASSERT(miScoringMode == KI_ROAD_PANEL_MODE_ONLINE,
                   "GetScoringMode() == GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE"); // @0x82418018

        // X360: 0xA2*index + this + 0x1A2  ==  &maFriendRows[index] (name at row start).
        return maFriendRows[miSelectedFriendIndex].macName;
    }
}
