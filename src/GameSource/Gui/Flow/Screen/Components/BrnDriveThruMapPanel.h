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

        void TransitionIn();
        void TransitionOut();

    private:
        TextField maTextfields[E_TEXTFIELD_COUNT];   // panel+0x94 (NAME, LOCATION; by value)
        CgsID     mDriveThruID;                      // panel+0x2E8
        bool      mbActive;                          // panel+0x2F0
    };
}
