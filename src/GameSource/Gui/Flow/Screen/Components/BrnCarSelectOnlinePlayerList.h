#pragma once

// ===================================================================================
// BrnGui::CarSelectOnlinePlayerList  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerList.h
//
// The online car-select lobby player list: a GUI component owning a bank of eight
// BrnGui::CarSelectOnlinePlayerListItem rows.
//
// 2026-08-02 -- RE-HOMED, and the row layout CORRECTED.
// The previous revision declared a base-less `class CarSelectOnlinePlayerList` whose head
// was an opaque `u8 maHeadReserved[0x378]` and whose rows were a local
// `struct PlayerRow { bool mbShowing; u8 maRowReserved[0x2EF]; }`. Three things were wrong:
//
//  1. IT HAS A BASE. BrnGui::CarSelectVehicle::OnEnter @0x824C9470 Constructs it through
//     the component vtable (`(**(this + 10504))(this + 10504, "PlayerTable_mc", si, 0)`,
//     i.e. CgsGui::GuiComponent's slot-0 Construct) and CarSelectVehicle::HandleAptTrigger
//     @0x824B5918 reads its name at `this + 0x290C` == the component +0x04 ==
//     CgsGui::GuiComponent::macName. So the head is a GuiComponent, not an opaque span.
//
//  2. THE ROWS START AT +0x90, NOT +0x378, and they are
//     BrnGui::CarSelectOnlinePlayerListItem (already reconstructed, 0x2F0 stride).
//     CarSelectOnlinePlayerList::HandleLoadNotification @0x82427A38 walks them with
//     `v5 = this + 144` (== 0x90), `strstr(name, v5 + 4)` (the row's GuiComponent::macName)
//     and `v5 += 752` for eight iterations. 0x90 + 8 * 0x2F0 == 0x1810, and the object's
//     size is pinned independently at 0x1818 by CarSelectVehicle's own layout
//     (mOnlinePlayerList @+0x2908, the next member mbFirstFrame @+0x4120).
//
//  3. IsShowing WAS READING THE WRONG BYTE. Its X360 load is
//     `lbz (index * 0x2F0) + 0x378`, and with the row bank at +0x90 that resolves to
//     row + 0x2E8 -- which is exactly CarSelectOnlinePlayerListItem::mbVisible
//     (BrnCarSelectOnlinePlayerListItem.h, `stb 0, 0x2E8` in its Construct). The old model
//     put the flag at row + 0x000 and therefore reported a different field entirely. The
//     0x378 == 0x90 + 0x2E8 identity is the confirmation that both offsets are right.
//
// Same defect class as BrnGui::TextSelection and BrnGui::CarSelectVehicle: a component
// modelled as a reserved blob, where the blob decomposed exactly with nothing left over.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"        // CgsGui::GuiComponent (base)
#include "GameSource/Gui/Components/BrnCarSelectOnlinePlayerListItem.h"    // BrnGui::CarSelectOnlinePlayerListItem (rows)

namespace BrnResource { struct VehicleList; }

namespace BrnGui
{
    class CarSelectOnlinePlayerList : public CgsGui::GuiComponent
    {
    public:
        // Number of player rows in the lobby bank (X360 range check: index in [0,8)).
        static const s32 KI_MAX_PLAYERS = 8;

        // @ 0x8241AF88 - base Construct, then Construct the eight rows as "ListItem_0".."_7"
        // parented to this component, and latch the GUI cache's vehicle list.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @ 0x82482950 - is the row at liPlayerIndex currently shown? Range-checks the
        // index (asserts on out-of-range) and returns the row's visible flag.
        bool IsShowing(s32 liPlayerIndex) const;

        // @ 0x82427A38 - route an apt load notification to the first row whose component
        // name is a substring of the reported clip name; returns whether one claimed it.
        bool HandleLoadNotification(const char* lpacComponentName);

    private:
        // +0x8C..+0x8F. Four bytes between the GuiComponent base (sizeof 0x8C) and the row
        // bank at +0x90 that no recovered body touches. FLAG: role unknown; kept as reserved
        // storage so the bank lands at its guest offset.
        u8 maHeadReserved[4];                        // +0x8C

        CarSelectOnlinePlayerListItem maPlayerRows[KI_MAX_PLAYERS];   // +0x90 (0x2F0 stride)

        // +0x1810. Construct's closing store (`*(this + 6160) = GetVehicleList()`, asserted
        // "mpVehicleList != NULL" at BrnCarSelectOnlinePlayerList.cpp:62) -- the third
        // independent confirmation that the row bank ends at +0x1810. The X360 pads the
        // object to 0x1818 behind it.
        const BrnResource::VehicleList* mpVehicleList;   // +0x1810
    };
}
