// ===================================================================================
// BrnGui::DriveThruMapPanel  -- implementation
//   class:BrnGui::DriveThruMapPanel
//
//   Construct                  @0x824174D0
//   AppendExpectedAptComponents @0x82417560
//   SetDriveThruData           @0x824175E0
//
// Reconstructed store-for-store from the X360 ARTIST pseudocode/asm. Member access is BY
// NAME. The drive-thru panel labels a map drive-thru: two child text fields driven off a
// CgsID, localised as "DT_NAME_<id>" / "DT_LOC_<id>" (E_FORMAT_ID_LOOKUP).
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnDriveThruMapPanel.h"

#include "GameSource/Gui/BrnGuiCache.h"                  // BrnGui::GuiCache
#include "GameShared/GameClasses/Core/CgsStringUtils.h"  // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"  // ParameterFormatType

namespace BrnGui
{
    // ⭐ ADDITIVE (main-menu wave G1, 2026-08-29). The one drive-thru second-level filter
    // option label (DWARF KAPC_DRIVETHRU_FILTER_OPTIONS, BrnDriveThruMapPanel.h:4).
    // CrashNavPanel::RefreshSecondLevelFilter passes the whole table to
    // MenuToggleGroupVarSize<3>::SetupToggle (`addi r8, r11, off_82F25164`, `li r5, 1`
    // @0x8243ADA4). IMAGE-CITED: image.bin (file offset = VA - 0x82000000, big-endian) --
    // the table at VA 0x82F25164 holds one pointer, 0x820494E0 -> "$CN_PANEL_NO_TYPES".
    const char* DriveThruMapPanel::KAPC_DRIVETHRU_FILTER_OPTIONS[1] =
    {
        "$CN_PANEL_NO_TYPES",
    };

    // File-static apt names for the two child text fields (X360 off_82F25168 table, both
    // entries read from the image: 0x82F25168 -> 0x820494D0 "DriveThruName", 0x82F2516C ->
    // 0x820494C0 "DriveThruLoc"; the next pointer, 0x82F25170 "RivalName", is the FIRST
    // entry of the RivalMapPanel table and is the loop's exclusive upper bound, not a
    // member). (DWARF KAPC_TEXTFIELD_NAMES, BrnDriveThruMapPanel.cpp:31.)
    static const char* const KAPC_TEXTFIELD_NAMES[DriveThruMapPanel::E_TEXTFIELD_COUNT] =
    {
        "DriveThruName",
        "DriveThruLoc",
    };

    // Scratch buffer length for the localisation key the panel formats on the stack (X360
    // uses 32 -- the stack temp is 0x20 bytes).
    static const u32 KU_DT_KEY_BUFFER_SIZE = 32;

    // @0x824174D0 -----------------------------------------------------------------------
    void DriveThruMapPanel::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                      const char* lpacParentName)
    {
        // Base IconComponent construct: no state-identifier table for this panel.
        IconComponent::Construct(lpacName, lpStateInterface, 0, lpacParentName);

        // Park the icon on its idle state.
        SetState("idle");

        // Construct the two child text fields from the file-static name table; each is
        // parented under this panel's own name.
        for (s32 liField = 0; liField < E_TEXTFIELD_COUNT; ++liField)
        {
            maTextfields[liField].Construct(KAPC_TEXTFIELD_NAMES[liField],
                                            lpStateInterface, GetName());
        }

        mDriveThruID = 0;       // X360 std 0, 0x2E8
        mbActive     = false;   // X360 stb 0, 0x2F0
    }

    // @0x82417560 -----------------------------------------------------------------------
    void DriveThruMapPanel::AppendExpectedAptComponents(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");   // X360 line 88

        // Register the panel's own apt component, then each of the two text fields, by name
        // hash (X360 reads each component's hashed-name word and forwards it).
        lpGuiCache->AppendExpectedAptComponent(leFlow, GetNameHash());

        for (s32 liField = 0; liField < E_TEXTFIELD_COUNT; ++liField)
        {
            lpGuiCache->AppendExpectedAptComponent(leFlow, maTextfields[liField].GetNameHash());
        }
    }

    // @0x824175E0 -----------------------------------------------------------------------
    void DriveThruMapPanel::SetDriveThruData(CgsID lDriveThruID)
    {
        // No change -> nothing to do.
        if (mDriveThruID == lDriveThruID)
            return;

        const bool lbWasActive = mbActive;
        mDriveThruID = lDriveThruID;          // X360 std, 0x2E8

        // If the panel was already showing, re-park it on idle for the transition.
        if (lbWasActive)
            SetState("idle");

        if (lDriveThruID == 0)
        {
            // A zero id clears both fields (X360 inlines the text-buffer clear + OutputAptData).
            maTextfields[E_TEXTFIELD_NAME].SetText("");
            maTextfields[E_TEXTFIELD_NAME].OutputAptData();
            maTextfields[E_TEXTFIELD_LOCATION].SetText("");
            maTextfields[E_TEXTFIELD_LOCATION].OutputAptData();
            return;
        }

        // Otherwise localise "DT_NAME_<id>" / "DT_LOC_<id>" into the two fields. The id is
        // formatted as an unsigned 64-bit value into a stack key, then looked up in the
        // localisation database (E_FORMAT_ID_LOOKUP).
        char lacKey[KU_DT_KEY_BUFFER_SIZE];

        CgsCore::SPrintf(lacKey, KU_DT_KEY_BUFFER_SIZE, "DT_NAME_%llu", lDriveThruID);
        maTextfields[E_TEXTFIELD_NAME].SetLocalisedText(
            lacKey, CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);

        CgsCore::SPrintf(lacKey, KU_DT_KEY_BUFFER_SIZE, "DT_LOC_%llu", lDriveThruID);
        maTextfields[E_TEXTFIELD_LOCATION].SetLocalisedText(
            lacKey, CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    }

    // Inlined into CrashNavPanel::ChangeVisiblePanelState @0x8243A548 ------------------
    // Push the named show/hide state at the panel's own apt clip, then latch the active
    // flag. Store-for-store: the state push comes FIRST in both arms, the flag store
    // second (asm order at the two E_PANEL_DRIVETHRU arms).
    void DriveThruMapPanel::TransitionIn()
    {
        SetState("transIn");    // sub_824E2B90(panel, "transIn")
        mbActive = true;        // stb 1, +0x2F0
    }

    void DriveThruMapPanel::TransitionOut()
    {
        SetState("transOut");   // sub_824E2B90(panel, "transOut")
        mbActive = false;       // stb 0, +0x2F0
    }
}
