// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 01: the three panel-repaint switches.
//   UpdateEventInfoPanel @0x824B73B0  (cpp:2091 assert site)
//   UpdateDrivethru      @0x824B7158  (cpp:2000 assert site)
//   UpdateRival          @0x824B7258  (cpp:2047 assert site)
//
// All three bodies are landed. Every arm of all three calls a BrnGui::CrashNavPanel
// setter; those five declarations were filed as wave-J shared-header requests and have
// since been applied, so the bodies below compile against the committed
// b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h:
//
//     void SetRivalPanelData();                                              // @0x8243AAC8
//     void SetRivalPanelData(CgsID lRivalCarId);                             // @0x8243AB68
//     void SetRivalPanelData(const CgsNetwork::PlayerName*, CgsID);          // @0x8243ABF0
//     void SetDrivethruPanelData(CgsID lDriveThruId);                        // @0x8243A8F0
//     void SetEventPanelData(u32, const ChallengedEventScore*, bool);        // @0x8243A878
//
// The last one takes THREE parameters on X360 where the PS3 DWARF has two. Its middle
// pointer is the score-override record wave-J group 8 measured (20 bytes: an s32 override
// word plus 16 reserved bytes; only word 0 is ever read, by EventPanel::SetEventData
// @0x82430D70) -- the three functions here all pass it as NULL.
//
// Compile-verified with the group's include set; the enum spellings, the out-of-line
// GuiEventUpdateSatNav::SatNavIconInfo::GetIconType() call and the CgsDev::StrStream
// assert chain all check out.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT machinery
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (streamed assert)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // SatNavIconInfo::SatNavIconType

namespace BrnGui
{
    // ----------------------------------------------------- UpdateDrivethru @0x824B7158
    //
    // Repaint the crash-nav panel while the screen is in its DRIVE-THROUGH sub-mode. Same
    // shape as UpdateEventInfoPanel minus the event arm -- and note the X360 compiles this
    // one as a plain if/else chain, NOT a jump table.
    //
    // Read from the asm rather than the pseudocode:
    //  * `addi r28, r31, 0x6100` + `bl BrnGui__GuiEventUpdateSatNav__SatNavI` is an
    //    out-of-line call on mLockedIconInfo (X360 +24832 == 0x6100). SatNavI @0x823A6B30
    //    is SatNavIconInfo::GetIconType(): it sign-extends the icon-type byte @+0x28 and
    //    fires the two range asserts (BrnGuiEventTypeDefs.h:1911/1912) before returning.
    //    It is a real `bl`, so the inline GetIconTypeByte() accessor would DROP those
    //    asserts -- call GetIconType().
    //  * `addi r3, r31, 0x6E0` on both panel arms is &mCrashNavPanel (X360 +1760); the
    //    Hex-Rays rendering drops that implicit `this` on the drive-through arm.
    //  * `ld r4, 0x60A8(r31)` = mHoveredDriveThruID (X360 +24744), one 8-byte CgsID in one
    //    register.
    //  * `addi r11, r3, -7` + `cmplwi r11, 5` + `bgt` is the compiler's strength-reduced
    //    form of the range test 7 <= type <= 12; de-optimised back to the written range
    //    below (AGENTS.md "strength reduction reversal"). The two forms agree for every
    //    value GetIconType() can return, because it asserts the type is >= 0.
    //
    // X360-LITERAL AUDIT: 0x6100 / 0x6E0 / 0x60A8 are the console offsets of
    // mLockedIconInfo / mCrashNavPanel / mHoveredDriveThruID. None is reproduced as a
    // number below -- every access is by member name, console values in comments only.
    void CrashNavMap::UpdateDrivethru()
    {
        const GuiEventUpdateSatNav::SatNavIconInfo::SatNavIconType leIconType =
            mLockedIconInfo.GetIconType();

        if (leIconType == GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR)
        {
            // The local player's own car icon -> the "player" face of the rival panel.
            mCrashNavPanel.SetRivalPanelData();
        }
        else if (leIconType >= GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNKYARD &&
                 leIconType <= GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_TIRE_SHOP)
        {
            // Any of the six drive-through / junkyard shop icons (types 7..12).
            mCrashNavPanel.SetDrivethruPanelData(mHoveredDriveThruID);
        }
        else
        {
            // Non-fatal, and the panel is left untouched. The X360 opens the assert FIRST
            // and only then builds the message, because it streams into the mutex-guarded
            // global CgsDev::Assert::gpcMessageBuffer; the committed convention
            // (BrnArbStateDriveThru.cpp) uses a stack buffer, so the ordering is cosmetic
            // here and is kept to match the asm.
            CgsDev::Assert::BeginAssert();

            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);

            // The X360 re-reads the icon type here (a second `bl SatNavI` at 0x824B7200)
            // rather than reusing leIconType -- kept. The three string literals are
            // verbatim rodata; there is no space before "while", so the composed message
            // really does read "...of type 3while updating drivethrus".
            lStream << "Should not be snapped to an icon of type "
                    << static_cast<s32>(mLockedIconInfo.GetIconType())
                    << "while updating drivethrus\n";

            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp",
                                       2000);   // `li r5, 0x7D0`
            CgsDev::Assert::EndAssert();
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT machinery
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (streamed assert)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // SatNavIconInfo::SatNavIconType

namespace BrnGui
{
    // ------------------------------------------------- UpdateEventInfoPanel @0x824B73B0
    //
    // Repaint the crash-nav panel for whatever the cursor is currently snapped to while the
    // screen is in its EVENT sub-mode. One jump-table switch on the locked icon's type.
    //
    // Read from the asm rather than the pseudocode:
    //  * `addi r28, r31, 0x6100` then `bl BrnGui__GuiEventUpdateSatNav__SatNavI` is an
    //    out-of-line call on mLockedIconInfo (X360 +24832 == 0x6100). SatNavI @0x823A6B30
    //    is SatNavIconInfo::GetIconType(): it sign-extends the icon-type byte @+0x28 and
    //    fires two range asserts ("leIconType >= 0" / "leIconType < E_SATNAVICON_MAX",
    //    BrnGuiEventTypeDefs.h:1911/1912) before returning it. It is a real `bl`, so the
    //    inline GetIconTypeByte() accessor would DROP those asserts -- call GetIconType().
    //  * `addi r3, r31, 0x6E0` on every panel arm is &mCrashNavPanel (X360 +1760 == 0x6E0);
    //    the Hex-Rays rendering drops that implicit `this` on the two-argument arms.
    //  * case 5 loads `lwz r4, 0x6094(r31)` = muHoveredEventID (X360 +24724), `li r5, 0`
    //    (the null score-override pointer) and `li r6, 1` (lbEventsToChooseFrom = true).
    //  * cases 7-12 load `ld r4, 0x60A8(r31)` = mHoveredDriveThruID (X360 +24744) -- an
    //    8-byte CgsID in ONE register, hence the single-argument call.
    //  * the switch is `cmplwi r3, 0xC` + `bgt default`, an UNSIGNED bound, so a negative
    //    icon type also lands in the default arm. C++ switch/default reproduces that.
    //
    // X360-LITERAL AUDIT: 0x6100 / 0x6E0 / 0x6094 / 0x60A8 are the console offsets of
    // mLockedIconInfo / mCrashNavPanel / muHoveredEventID / mHoveredDriveThruID. None of
    // them is reproduced as a number below -- every access is by member name and the
    // console values live in these comments only.
    void CrashNavMap::UpdateEventInfoPanel()
    {
        switch (mLockedIconInfo.GetIconType())
        {
        // ---- the local player's own car icon -> the "player" face of the rival panel ----
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR:
            mCrashNavPanel.SetRivalPanelData();
            break;

        // ---- a junction (= an event) icon -> the event panel, no score override ---------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNCTION:
            mCrashNavPanel.SetEventPanelData(muHoveredEventID, NULL, true);
            break;

        // ---- any of the six drive-through / junkyard shop icons ------------------------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNKYARD:
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_CAR_PARK:
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_BODYSHOP:
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_GAS_STATION:
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PAINT_SHOP:
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_TIRE_SHOP:
            mCrashNavPanel.SetDrivethruPanelData(mHoveredDriveThruID);
            break;

        // ---- every other icon type is a bug in the caller ------------------------------
        default:
            {
                // Non-fatal, and the panel is left untouched (BeginAssert / FireAssert /
                // EndAssert, then straight to the epilogue). The X360 opens the assert
                // FIRST and only then builds the message, because it streams into the
                // mutex-guarded global CgsDev::Assert::gpcMessageBuffer; the committed
                // convention (BrnArbStateDriveThru.cpp) uses a stack buffer instead, so the
                // ordering is cosmetic here and is kept to match the asm.
                CgsDev::Assert::BeginAssert();

                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);

                // The X360 re-reads the icon type here (a second `bl SatNavI` at
                // 0x824B74C4) rather than reusing the switch value -- kept. The three
                // string literals are verbatim rodata; note there is no space before
                // "while", so the composed message really does read "...of type 6while
                // updating events".
                lStream << "Should not be snapped to an icon of type "
                        << static_cast<s32>(mLockedIconInfo.GetIconType())
                        << "while updating events\n";

                CgsDev::Assert::FireAssert(lacMessageBuffer,
                                           "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp",
                                           2091);   // `li r5, 0x82B`
                CgsDev::Assert::EndAssert();
            }
            break;
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT machinery
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (streamed assert)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // SatNavIconInfo::SatNavIconType

namespace BrnGui
{
    // --------------------------------------------------------- UpdateRival @0x824B7258
    //
    // Repaint the crash-nav panel while the screen is in its RIVAL sub-mode: which of the
    // three rival-panel faces to show is decided by the locked icon's type.
    //
    // Read from the asm rather than the pseudocode:
    //  * `addi r28, r31, 0x6100` + `bl BrnGui__GuiEventUpdateSatNav__SatNavI` is an
    //    out-of-line call on mLockedIconInfo (X360 +24832 == 0x6100). SatNavI @0x823A6B30
    //    is SatNavIconInfo::GetIconType(): it sign-extends the icon-type byte @+0x28 and
    //    fires the two range asserts (BrnGuiEventTypeDefs.h:1911/1912) before returning.
    //    It is a real `bl`, so the inline GetIconTypeByte() accessor would DROP those
    //    asserts -- call GetIconType().
    //  * `addi r3, r31, 0x6E0` is &mCrashNavPanel (X360 +1760) on ALL THREE panel arms,
    //    including the two the dossier renders as free functions `sub_8243AB68(...)` /
    //    `sub_8243ABF0(...)` -- Hex-Rays dropped the implicit `this` because those two
    //    targets are unnamed in the IDA database.
    //  * `ld r5/r4, 0x60B8(r31)` = mHoveringRivalId (X360 +24760), one 8-byte CgsID in one
    //    register; the pseudocode's `(*(a1+24760), *(a1+24764))` word pair is an artifact.
    //  * `addi r4, r31, 0x60C8` = &mPlayerName (X360 +24776), passed by pointer.
    //  * case 12 (E_SATNAVICON_TIRE_SHOP) jumps straight to the epilogue at 0x824B73A8 --
    //    a deliberate no-op arm, not a fall-through into the assert. MoveCursor parks type
    //    12 in mLockedIconInfo while the map is panning (spec section 4), which is why the
    //    rival path has to tolerate it silently.
    //  * the switch is `cmplwi r3, 0xC` + `bgt default`, an UNSIGNED bound, so a negative
    //    icon type also lands in the default arm. C++ switch/default reproduces that.
    //
    // X360-LITERAL AUDIT: 0x6100 / 0x6E0 / 0x60B8 / 0x60C8 are the console offsets of
    // mLockedIconInfo / mCrashNavPanel / mHoveringRivalId / mPlayerName. None is reproduced
    // as a number below -- every access is by member name, console values in comments only.
    void CrashNavMap::UpdateRival()
    {
        switch (mLockedIconInfo.GetIconType())
        {
        // ---- the local player's own car icon -> the "player" face of the panel ---------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR:
            mCrashNavPanel.SetRivalPanelData();
            break;

        // ---- a networked rival: the panel gets the cached lobby name as well -----------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL:
            mCrashNavPanel.SetRivalPanelData(&mPlayerName, mHoveringRivalId);
            break;

        // ---- an offline rival: the car id alone identifies it --------------------------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_RIVAL:
            mCrashNavPanel.SetRivalPanelData(mHoveringRivalId);
            break;

        // ---- the panning placeholder type: leave the panel exactly as it is ------------
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_TIRE_SHOP:
            break;

        // ---- every other icon type is a bug in the caller ------------------------------
        default:
            {
                // Non-fatal, and the panel is left untouched. The X360 opens the assert
                // FIRST and only then builds the message, because it streams into the
                // mutex-guarded global CgsDev::Assert::gpcMessageBuffer; the committed
                // convention (BrnArbStateDriveThru.cpp) uses a stack buffer, so the
                // ordering is cosmetic here and is kept to match the asm.
                CgsDev::Assert::BeginAssert();

                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);

                // The X360 re-reads the icon type here (a second `bl SatNavI` at
                // 0x824B7368) rather than reusing the switch value -- kept. The three
                // string literals are verbatim rodata; there is no space before "while",
                // so the composed message really does read "...of type 4while updating
                // rivals".
                lStream << "Should not be snapped to an icon of type "
                        << static_cast<s32>(mLockedIconInfo.GetIconType())
                        << "while updating rivals\n";

                CgsDev::Assert::FireAssert(lacMessageBuffer,
                                           "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp",
                                           2047);   // `li r5, 0x7FF`
                CgsDev::Assert::EndAssert();
            }
            break;
        }
    }
}
