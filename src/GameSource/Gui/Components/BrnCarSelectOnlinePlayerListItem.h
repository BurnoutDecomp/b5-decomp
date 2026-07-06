#pragma once

// ===================================================================================
// BrnGui::CarSelectOnlinePlayerListItem  -- owning header
//   b5-decomp/src/GameSource/Gui/Components/BrnCarSelectOnlinePlayerListItem.h
//   class:BrnGui::CarSelectOnlinePlayerListItem
//   (DecFIGS DWARF primary_file GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerListItem.h)
//
// One row of the online car-select player list: a CgsGui::GuiComponent that owns two
// embedded BrnGui::TextField sub-components (the gamertag row "gamertag_mc" and the
// car-name row "carName_mc"), plus the currently displayed car id and the row's
// visible/final-selection flags. It drives an apt/Flash view-state when shown/hidden and
// routes apt load-notifications to the matching child.
//
// CLASS SHAPE per the DecFIGS DWARF (BrnCarSelectOnlinePlayerListItem.h:53) and confirmed
// store-for-store against BURNOUT_X360_ARTIST.XEX:
//   base CgsGui::GuiComponent  sizeof 0x8C  (vptr@+0, macName[128]@+4, muHashedName@+0x84,
//                                            mpStateInterface@+0x88)
//   mGamertagTextfield  BrnGui::TextField  @+0x8C   (sizeof(TextField)==0x128; its macName
//                                                    @+0x90, macText @+0x130)
//   mCarTextfield       BrnGui::TextField  @+0x1B4  (0x8C + 0x128; macName @+0x1B8,
//                                                    macText @+0x258)
//   mCurrentCarID       CgsID (u64)        @+0x2E0  (Construct zeroes it with std; SetPlayerCar
//                                                    ld/cmpld/std it -- 8 bytes)
//   mbVisible           bool               @+0x2E8
//   mbFinalSelection    bool               @+0x2E9
//
// Named clip-name constants come from the DWARF (BrnCarSelectOnlinePlayerListItem.cpp:23/24):
//   KAC_GAMERTAG_TEXTFIELD_NAME  char[12] = "gamertag_mc"
//   KAC_CAR_TEXTFIELD_NAME       char[11] = "carName_mc"
//
// (An earlier revision of this header modelled the body as reserved padding and named the
// +0x2E8 flag `mbShown`; the DWARF names it `mbVisible`. Show() @0x8241A598 reads that flag.)
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsID (u64)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)
#include "GameSource/Gui/BrnGuiTextField.h"                           // BrnGui::TextField (embedded x2)

namespace BrnGui
{
    struct CarSelectOnlinePlayerListItem : public CgsGui::GuiComponent
    {
        // @0x8241B390 -- base Construct, Construct both text fields, zero the state.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x8241A598 -- mark visible and push the "final"/"visible" apt view-state.
        void Show();

        // @0x8241B4A0 -- route an apt load-notification (by clip name) to the child that
        // owns it; the component's own clip toggles the visible/invisible apt state.
        void OnLoad(const char* lpacName);

        // @0x8241B420 -- push the selected car's uppercased name into the car text field
        // (localisation key "CAR_CAPS_<id>"), gated on visibility + id change.
        void SetPlayerCar(CgsID lCarID);

    private:
        // BrnCarSelectOnlinePlayerListItem.cpp:23/24 (DWARF-named clip-name constants).
        static const char KAC_GAMERTAG_TEXTFIELD_NAME[12];   // "gamertag_mc"
        static const char KAC_CAR_TEXTFIELD_NAME[11];        // "carName_mc"

        TextField mGamertagTextfield;   // +0x8C
        TextField mCarTextfield;        // +0x1B4
        CgsID     mCurrentCarID;        // +0x2E0 -- last car id pushed to mCarTextfield
        bool      mbVisible;            // +0x2E8 -- row is (to be) shown
        bool      mbFinalSelection;     // +0x2E9 -- selects "final" vs "visible" apt state
    };
}
