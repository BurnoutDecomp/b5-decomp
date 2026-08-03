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
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList::GetVehicleData
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // GetLiveryType / GetParentId

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

        // The console form, restored 2026-08-02 (the SUBSTITUTION that stood here while
        // GuiCache::mpWorldDataController had no producer is retired): the X360 reaches the list
        // as GetAccessPointers()->GetGuiCache()->GetWorldDataController()->GetVehicleList() and
        // asserts the result (cpp:62).
        mpVehicleList = mpStateInterface->GetAccessPointers()
                            ->GetGuiCache()->GetWorldDataController()->GetVehicleList();
        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList != NULL");   // cpp:62
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

    // ---------------------------------------------------------------------------------
    // The five bank-level row drivers, RECONSTRUCTED 2026-08-03. They were declared in the
    // header by b5-decomp fd0925f4 with their bodies deferred to "this component's own TU";
    // the same commit landed BrnCarSelectLivery_wJ_01.cpp, whose HandleLobbyPlayerList calls
    // all five, so the exe could not link until they existed. Reconstructed store-for-store
    // from the X360 ARTIST bodies named in the header (asm arbitrates; the row-bank base
    // +0x90 / stride 0x2F0 that every one of them walks is the re-homed layout, so the
    // console's `752 * index + this + 144` is exactly `maPlayerRows[index]`).
    //
    // All five open with the same range assert. The X360 streams "Invalid player index : "
    // followed by the index into the assert buffer; CGS_ASSERT carries the plain condition,
    // matching IsShowing() above. The per-body cpp line numbers are the console's.
    // ---------------------------------------------------------------------------------

    // @ 0x8241B1C8 -- show the row (the console's whole body after the assert is the row's
    // own Show(), which sets mbVisible and pushes the visible/final apt view-state).
    void CarSelectOnlinePlayerList::Show(s32 liPlayerIndex)
    {
        CGS_ASSERT(liPlayerIndex >= 0 && liPlayerIndex < KI_MAX_PLAYERS,
                   "Invalid player index");                                   // cpp:146

        maPlayerRows[liPlayerIndex].Show();
    }

    // @ 0x8241B2A0 -- take the row down. There is no Item::Hide on the console: the bank
    // clears the row's visible byte itself (`*(row + 744) = 0` == row + 0x2E8 == mbVisible)
    // and pushes the "invisible" apt view-state on the ROW component, non-immediate.
    void CarSelectOnlinePlayerList::Hide(s32 liPlayerIndex)
    {
        CGS_ASSERT(liPlayerIndex >= 0 && liPlayerIndex < KI_MAX_PLAYERS,
                   "Invalid player index");                                   // cpp:164

        CarSelectOnlinePlayerListItem& lrRow = maPlayerRows[liPlayerIndex];
        lrRow.mbVisible = false;
        lrRow.AddOutputAptViewState("apt_state", "invisible", false);
    }

    // @ 0x82427948 -- push a gamertag into the row's name field, but ONLY while the row is up
    // (X360 `if (HIBYTE(row + 888))` -- the big-endian MSB of the word at +888 is the byte at
    // +888 itself, i.e. row + 0x2E8 == mbVisible). The console calls TextField::SetText on
    // row + 0x8C == mGamertagTextfield directly.
    void CarSelectOnlinePlayerList::SetPlayerName(s32 liPlayerIndex, const char* lpacPlayerName)
    {
        CGS_ASSERT(liPlayerIndex >= 0 && liPlayerIndex < KI_MAX_PLAYERS,
                   "Invalid player index");                                   // cpp:79

        CarSelectOnlinePlayerListItem& lrRow = maPlayerRows[liPlayerIndex];
        if (lrRow.mbVisible)
        {
            lrRow.mGamertagTextfield.SetText(lpacPlayerName);
        }
    }

    // @ 0x82434B70 -- display a car on the row. A livery variant is shown under its PARENT
    // car's id, so the id is resolved through the vehicle list first.
    //
    // ⚠️ HEX-RAYS MISREADS THE PARENT FETCH. Its `VehicleData[3].field_0` is a 32-bit view of
    // the asm's `ld r11, 8(r31)` @0x82434C90 -- a 64-bit load of the CgsID parent id at
    // entry + 0x08, i.e. VehicleListEntry::GetParentId(). The livery tag beside it is
    // `lbz r11, 0xE9(r31)` == GetLiveryType(). Identical idiom to the committed
    // BrnLeaderboardTableComponent.cpp:214 (`GetLiveryType() != 2 && GetParentId() != 0`);
    // the 2 is a literal in the asm and the committed ELiveryType only models 0/1, so it
    // stays a literal here too, as it does there.
    void CarSelectOnlinePlayerList::SetPlayerCar(s32 liPlayerIndex, CgsID lCarId)
    {
        CGS_ASSERT(liPlayerIndex >= 0 && liPlayerIndex < KI_MAX_PLAYERS,
                   "Invalid player index");                                   // cpp:98

        // The console does GetVehicleIndex() then GetVehicleData(index) and routes BOTH the
        // "id not present" arm and the "no entry" arm into the same assert; the committed
        // GetVehicleData(CgsID) overload is exactly that composite and returns null for both.
        const BrnResource::VehicleListEntry* lpVehicleData = mpVehicleList->GetVehicleData(lCarId);
        CGS_ASSERT(lpVehicleData != 0, "lpVehicleData");                      // cpp:101

        if (lpVehicleData->GetLiveryType() != 2 && lpVehicleData->GetParentId() != 0)
        {
            lCarId = lpVehicleData->GetParentId();
        }

        maPlayerRows[liPlayerIndex].SetPlayerCar(lCarId);
    }

    // @ 0x8241B0C8 -- latch the row's "final selection" tick. Nothing happens unless the flag
    // actually changes; the visible byte is sampled BEFORE the store (X360 reads +888 into a
    // register, then stores +889, then tests the register), and a row that was already up is
    // re-Shown so it swaps to the "final" apt state.
    void CarSelectOnlinePlayerList::SetFinalSelection(s32 liPlayerIndex, bool lbFinalSelection)
    {
        CGS_ASSERT(liPlayerIndex >= 0 && liPlayerIndex < KI_MAX_PLAYERS,
                   "Invalid player index");                                   // cpp:128

        CarSelectOnlinePlayerListItem& lrRow = maPlayerRows[liPlayerIndex];
        if (lrRow.mbFinalSelection != lbFinalSelection)
        {
            const bool lbWasVisible = lrRow.mbVisible;
            lrRow.mbFinalSelection  = lbFinalSelection;
            if (lbWasVisible)
            {
                lrRow.Show();
            }
        }
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
