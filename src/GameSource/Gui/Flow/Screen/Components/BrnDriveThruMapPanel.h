#pragma once

// ===================================================================================
// BrnGui::DriveThruMapPanel  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnDriveThruMapPanel.h
//   class:BrnGui::DriveThruMapPanel
//
// The map panel that labels a drive-thru (junkyard / bodyshop / paint shop / ...) on the
// crash-nav map: an IconComponent that owns two text fields (the drive-thru's NAME and its
// LOCATION) and drives them off a CgsID. When a non-zero id is set it localises
// "DT_NAME_<id>" / "DT_LOC_<id>" into the two fields; a zero id clears them.
//
// CLASS SHAPE (DecFIGS DWARF GameSource/Gui/Flow/Screen/Components/BrnDriveThruMapPanel.h,
// gated on the X360 ledger). DriveThruMapPanel : public IconComponent; layout proven from
// the X360 ARTIST asm (Construct @0x824174D0, SetDriveThruData @0x824175E0). Member byte
// offsets are guest references; the gate compiles 64-bit, so members are accessed BY NAME:
//   maTextfields[2]  panel+0x94 .. +0x2E3  (TextField NAME @+0x94, LOCATION @+0x1BC; 0x128 each)
//   mDriveThruID     panel+0x2E8           (CgsID; std/ld 8 bytes)
//   mbActive         panel+0x2F0           (stb; true once a non-zero id has been shown)
//
// KAPC_DRIVETHRU_FILTER_OPTIONS / KAPC_TEXTFIELD_NAMES are FILE-STATIC string tables in
// BrnDriveThruMapPanel.cpp (DWARF "extern ... BrnDriveThruMapPanel.cpp:26/31"), NOT
// members -- they live in the .cpp, not here.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                             // CgsID (== u64)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"              // BrnGui::IconComponent (base)
#include "GameSource/Gui/BrnGuiTextField.h"                            // BrnGui::TextField (by value)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                        // BrnGui::GuiFlow
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"    // CgsGui::StateInterface

namespace BrnGui
{
    class GuiCache;

    class DriveThruMapPanel : public IconComponent
    {
    public:
        // The two text fields the panel drives: the drive-thru name and its location.
        enum ETextField
        {
            E_TEXTFIELD_NAME     = 0,
            E_TEXTFIELD_LOCATION = 1,
            E_TEXTFIELD_COUNT    = 2,
        };

        // ADDITIVE GROW (main-menu wave F1, 2026-08-29). DWARF BrnDriveThruMapPanel.h:4 --
        // the ONE drive-thru second-level filter option label. PUBLIC static (the DWARF lists
        // it ahead of the class's first `private:`; the banner above called it file-static,
        // which is what a private static looks like once the class is stripped). It is passed
        // whole by CrashNavPanel::RefreshSecondLevelFilter to SetupToggle
        // (`addi r8, r11, off_82F25164` -> "$CN_PANEL_NO_TYPES" @0x8243ADA4, `li r5, 1`).
        // The DEFINITION belongs to the DriveThruMapPanel TU -- declaration only here.
        static const char* KAPC_DRIVETHRU_FILTER_OPTIONS[1];

        // @0x824174D0 -- run the base IconComponent construct (no state-identifier table),
        // park the icon on its "idle" state, then construct the two child text fields from
        // the file-static name table, and clear the drive-thru id + active flag.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);

        // @0x82417560 -- register the panel's own apt component and its two text fields with
        // the GUI cache as expected on the given flow layer.
        void AppendExpectedAptComponents(GuiFlow leFlow, GuiCache* lpGuiCache);

        // @0x824175E0 -- point the panel at a drive-thru id. If unchanged, do nothing. On a
        // change: re-park on "idle" if already active; for a non-zero id, localise
        // "DT_NAME_<id>" / "DT_LOC_<id>" into the two fields; for a zero id, clear them.
        void SetDriveThruData(CgsID lDriveThruID);

        // BODIED (main-menu wave G1, 2026-08-29). Neither has a standalone X360 symbol: the
        // console INLINES both into BrnGui::CrashNavPanel::ChangeVisiblePanelState
        // @0x8243A548, on the E_PANEL_DRIVETHRU arms, as a bare named-state push plus the
        // active-flag store --
        //   out: `sub_824E2B90(this + 0x3788, "transOut"); *(this + 14968) = 0;`
        //   in : `sub_824E2B90(this + 0x3788, "transIn");  *(this + 14968) = 1;`
        // with 0x3788 == CrashNavPanel::mDrivethruPanel and 14968 == 0x3A78 == that panel's
        // own mbActive (+0x2F0). Bodies live here (this is the member's home) rather than in
        // the caller, which is what the DWARF method set says.
        void TransitionIn();
        void TransitionOut();

    private:
        TextField maTextfields[E_TEXTFIELD_COUNT];   // panel+0x94 (NAME, LOCATION; by value)
        CgsID     mDriveThruID;                      // panel+0x2E8
        bool      mbActive;                          // panel+0x2F0
    };
}
