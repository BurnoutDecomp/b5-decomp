// ===================================================================================
// BrnGui::CarSelectOnlinePlayerList  -- implementation
//   class:BrnGui::CarSelectOnlinePlayerList
//
// IsShowing              @ 0x82482950
// HandleLoadNotification @ 0x82427A38
//
// See BrnCarSelectOnlinePlayerList.h for the 2026-08-02 re-home note: the component
// derives from CgsGui::GuiComponent and its eight CarSelectOnlinePlayerListItem rows
// start at +0x90, so the X360's `lbz (index * 0x2F0) + 0x378` in IsShowing is
// row + 0x2E8 == CarSelectOnlinePlayerListItem::mbVisible.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController

#include <cstring>   // std::strstr

namespace BrnGui
{
    // @ 0x8241AF88 -- base Construct, then Construct the eight rows as "ListItem_0" ..
    // "ListItem_7", each parented to THIS component (GetName()), and latch the GUI cache's
    // vehicle list into mpVehicleList. The row loop is `v8 = this + 144` (== +0x90) with
    // `v8 += 188` DWORDs (== 0x2F0 bytes) -- the third independent confirmation of the row
    // bank's base and stride (see the header's re-home note).
    void CarSelectOnlinePlayerList::Construct(const char* lpacName,
                                              CgsGui::StateInterface* lpStateInterface,
                                              const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        for (s32 liRow = 0; liRow < KI_MAX_PLAYERS; ++liRow)
        {
            char lacRowName[32];
            CgsCore::SPrintf(lacRowName, 32, "ListItem_%d", liRow);
            lacRowName[31] = 0;
            maPlayerRows[liRow].Construct(lacRowName, mpStateInterface, GetName());
        }

        // ⚠️ FLAGGED SUBSTITUTION (car-select wave 2026-08-02). The X360 reaches the list as
        // GetAccessPointers()->GetGuiCache()->GetWorldDataController()->GetVehicleList() and
        // asserts the result ("mpVehicleList != NULL", cpp:62). NOTHING ON THIS BUILD
        // POPULATES GuiCache::mpWorldDataController, and both the cache accessor's assert and
        // this one are dev asserts that BLOCK the sim -- so the fetch goes through the
        // assert-free GuiCache::PeekWorldDataController and the null result is accepted.
        // Restore the console form (and the assert) with the GUI WorldDataController.
        mpVehicleList = 0;
        CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
        if (lpAccessPointers != 0)
        {
            GuiCache* lpGuiCache = lpAccessPointers->GetGuiCache();
            if (lpGuiCache != 0)
            {
                WorldDataController* lpWorldData = lpGuiCache->PeekWorldDataController();
                if (lpWorldData != 0)
                    mpVehicleList = lpWorldData->GetVehicleList();
            }
        }
    }

    // @ 0x82482950
    // Range-checks the row index (X360: signed cmpwi index,0 / cmpwi index,8 -- the assert
    // fires when index < 0 or index >= 8; the Hex-Rays "a2 >= 8" unsigned compare was a
    // misread of the two signed compares) then returns the row's visible flag. The original
    // streamed an "Invalid player index" message into the assert buffer; CGS_ASSERT carries
    // the plain condition.
    bool CarSelectOnlinePlayerList::IsShowing(s32 liPlayerIndex) const
    {
        CGS_ASSERT(liPlayerIndex >= 0 && liPlayerIndex < KI_MAX_PLAYERS,
                   "Invalid player index");                                   // @0x82482968/0x82482970

        return maPlayerRows[liPlayerIndex].IsVisible();
    }

    // @ 0x82427A38 -- walk the eight rows in order and hand the notification to the first
    // one whose own component name appears inside the reported clip name (a SUBSTRING test,
    // `strstr(lpacComponentName, row.GetName())`, not an equality compare). The X360 keeps
    // the "claimed" flag in a register and still runs the loop counter to 8, breaking out of
    // the body at the top -- reproduced as an early return.
    bool CarSelectOnlinePlayerList::HandleLoadNotification(const char* lpacComponentName)
    {
        for (s32 liRow = 0; liRow < KI_MAX_PLAYERS; ++liRow)
        {
            if (std::strstr(lpacComponentName, maPlayerRows[liRow].GetName()) != 0)
            {
                maPlayerRows[liRow].OnLoad(lpacComponentName);
                return true;
            }
        }
        return false;
    }
}
