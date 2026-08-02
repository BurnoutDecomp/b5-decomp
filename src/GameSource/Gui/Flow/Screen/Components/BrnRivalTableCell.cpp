#include "GameSource/Gui/Flow/Screen/Components/BrnRivalTableCell.h"

#include "GameShared/GameClasses/Core/CgsID.h"            // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

// BrnGui::RivalTableCell -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (4 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/Components/BrnRivalTableCell.cpp):
//   RivalTableCell::Construct         @0x82418E80
//   RivalTableCell::Update            @0x82418F48
//   RivalTableCell::SetWrecked        @0x82419060
//   RivalTableCell::SetScreenPosition @0x824268B8

namespace BrnGui
{

const char RivalTableCell::KAC_RIVALRY_STAGE_VAR[17] = "apt_rivalryState";
const char RivalTableCell::KAC_RIVAL_VEHICLE_VAR[18] = "apt_rivalCarState";

// @ 0x82418E80 -- both base Constructs (the selectable gets the invalid id
// 0xFFFFFFFF, asm `li r7,-1; clrldi r7,32`), clear the rival binding, reset the
// selectable gates through the live vtable (slots 0..3 in call order), mark empty.
void RivalTableCell::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    Selectable::Construct(lpacName, lpStateInterface, lpacParentName, 0xFFFFFFFFull);
    mCarID = 0;
    meRivalryStage = E_RIVALRY_STAGE_UNKNOWN;
    SetActive(true);          // vtbl slot 0
    SetHighlightable(true);   // vtbl slot 1
    SetSelectable(false);     // vtbl slot 2
    SetHighlighted(false);    // vtbl slot 3
    mbDriven  = false;
    mbWrecked = false;
    mbEmpty   = true;
}

// @ 0x82418F48 -- consume the dirty flag: a bound cell pushes "CAR_<id>" and its
// empty/driven-derived rivalry state; an unbound cell hides both.
void RivalTableCell::Update()
{
    if (!IsDirty())
        return;
    ClearFlag(E_FLAG_DIRTY);

    const char* lpacRivalryState;
    if (mCarID != 0)
    {
        char lacIdBuffer[16];   // the CgsID printable form
        char lacCarState[32];
        CgsIDConvertToString(mCarID, lacIdBuffer);
        CgsCore::SPrintf(lacCarState, 31, "CAR_%s", lacIdBuffer);
        lacCarState[31] = 0;
        AddOutputAptViewState(KAC_RIVAL_VEHICLE_VAR, lacCarState, false);

        if (mbEmpty)
            lpacRivalryState = "invisible";
        else if (mbDriven)
            lpacRivalryState = "unselected";
        else
            lpacRivalryState = "unselectedlocked";
    }
    else
    {
        AddOutputAptViewState(KAC_RIVAL_VEHICLE_VAR, "invisible", false);
        lpacRivalryState = "invisible";
    }
    AddOutputAptViewState(KAC_RIVALRY_STAGE_VAR, lpacRivalryState, false);
}

// @ 0x82419060 -- latch the wrecked flag; a live (non-empty) cell also pushes the
// transition state.
void RivalTableCell::SetWrecked(bool lbWrecked)
{
    const bool lbWasEmpty = mbEmpty;
    mbWrecked = lbWrecked;
    if (!lbWasEmpty)
    {
        AddOutputAptViewState("apt_transitionState",
                              lbWrecked ? "wrecked" : "unwrecked", false);
    }
}

// @ 0x824268B8 -- a bound cell pushes its screen X ("%3.3f", the vector's X lane)
// as the "_x" apt variable and dirties itself.
void RivalTableCell::SetScreenPosition(Vector2 lv2ScreenPosition)
{
    if (mCarID != 0)
    {
        char lacPosition[32];
        CgsCore::SPrintf(lacPosition, 32, "%3.3f", lv2ScreenPosition.x);
        AddOutputAptViewState("_x", lacPosition, true);
        SetDirty();
    }
}

// Component vtable SLOT 4 (off_820717E0 + 0x10) holds 0x8284CB38 -- the image-wide
// ICF fold of a bare `blr`, i.e. an EMPTY body, not _purecall (the same address the
// four empty CarSelectMain event handlers occupy, 193 xrefs). So this cell really does
// override Select() with nothing: a carousel icon is a display cell, and the group's
// Select() (slot 4 on the GROUP) is what the screen dispatches. Proven, not assumed --
// the sibling BrnGui::TextSelectionItem::Select is the same fold.
void RivalTableCell::Select()
{
}

// ---- the three small setters, recovered from their INLINED copies -----------------
// The X360 emits no out-of-line symbol for SetEmpty / SetCarID / SetDriven -- every call
// site inlines them -- so they were declaration-only here. BrnGui::CarSelectVehicle::
// SetCarouselComponent @0x824BBE90 inlines all three TWICE (once in the blanking loop at
// 0x824BBF00..0x824BBF8C, once in the binding loop at 0x824BC080..0x824BC3D0), which pins
// their bodies store for store. `r31`/`r27` there point at cell + 0x0C (Selectable::mxFlags),
// so the displacements below decode as: +0(from r31) == mxFlags, +0xA4 == mbDriven,
// +0xA6 == mbEmpty, r31-0xA(=cell+0xA8) == mCarID, r31-0xA2(=cell+0x10) == Selectable::mId.

// SetEmpty: a guarded latch -- only a CHANGE writes the byte and dirties the cell.
//   lbz mbEmpty / cmplwi <new> / beq skip / lbz mxFlags / stb <new> / ori 0x10 / stb mxFlags
void RivalTableCell::SetEmpty(bool lbEmpty)
{
    if (mbEmpty != lbEmpty)
    {
        mbEmpty = lbEmpty;
        SetDirty();
    }
}

// SetCarID: un-empty the cell, push the id into BOTH the Selectable identity and the cell's
// own mCarID, dirtying between the two stores (the console emits `ori 0x10 / stb` twice).
void RivalTableCell::SetCarID(CgsID lCarID)
{
    SetEmpty(false);

    SetId(lCarID);        // std -> cell + 0x10 (Selectable::mId)
    SetDirty();

    mCarID = lCarID;      // std -> cell + 0xA8
    SetDirty();
}

// SetDriven: a bare store -- the console writes the byte with no flag update
// (`stb r21, 0xA4(r31)` / `stb r11, 0xA4(r27)`, no surrounding ori).
void RivalTableCell::SetDriven(bool lbDriven)
{
    mbDriven = lbDriven;
}

}
