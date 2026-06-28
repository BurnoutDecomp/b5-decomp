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
    // File-static apt names for the two child text fields (X360 off_82F25168 table). The
    // location field reuses the shared "RivalName" apt string. (DWARF KAPC_TEXTFIELD_NAMES,
    // BrnDriveThruMapPanel.cpp:31.)
    static const char* const KAPC_TEXTFIELD_NAMES[DriveThruMapPanel::E_TEXTFIELD_COUNT] =
    {
        "DriveThruName",
        "RivalName",
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
}
