#pragma once

// ===================================================================================
// BrnGui::CrashNavSettings -- THE CRASH-NAV "SETTINGS" TAB.
//
// ⭐⭐ WHY THIS TU EXISTS: IT WAS A PLAYER-FACING SOFT-LOCK.
// The offline pause screen is CrashNavDriverDetails (CN_D_DETAIL, script id 111). Its
// HandleControllerInputPressed @0x824CF3B8 cases '6'/'7' (actions 54/55 == LB/RB) post a
// bare SendStateEvent("TOGGLE_LEFT"/"TOGGLE_RIGHT") -- faithful, and the screen itself
// draws the LB/RB prompts, so a player is INVITED to press them. BRNSCREENFSM's
// NextState_111CN_D_DETAIL routes TOGGLE_RIGHT to Transition_111CN_D_DETAIL_8CN_SETTINGS,
// i.e. straight into this state.
// Until this TU landed the class was a header-only shell: only GetResourcesToLoad was
// declared, so OnEnter/Update/OnLeave fell through to the do-nothing CgsGui::State base.
// That state therefore registered for NO events, so it received none, so nothing could
// ever be handled -- and CrashNavDriverDetails::OnLeave had already hidden the pause
// screen WITHOUT posting the resume pair. MEASURED 2026-08-29 on b5 cb80d9b7: after RB the
// sim froze permanently -- ZERO changed pixels across 16,290 dumped presents (200 s), with
// the throttle held down the whole time, and three GUI_CANCEL taps did nothing.
// ⛔ The console's bare 54/55 arm is NOT the bug and must not be "fixed" with a guard the
// X360 lacks. The bug was that the DESTINATION did not exist.
//
// SHAPE. DecFIGS DWARF (dwarfdump .../BrnCrashNavSettings.h), gated on the X360 ledger.
// Guest offsets in the trailing comments are MEASURED from the X360 bodies (OnEnter
// @0x824B7640, Update @0x824DE0D8, HandleControllerInput @0x824D90E8,
// InputSponsorProductCode @0x824CCFC0), not inferred from declaration order:
//    +0x0038  meState                        (OnEnter's `stw r11, 0x38`)
//    +0x0040  mMenuComponent                 (OnEnter's `addi r3, r31, 0x40` -> Construct)
//    +0x1100  mpGuiCache                     (HandleGuiCacheEvent's only store)
//    +0x1104  macDefaultHeading[64]          (Update's `sth r20, 0x1104`)
//    +0x1184  macTitleHeading[64]            (Update's `sth r20, 0x1184`)
//    +0x1204  macDialogHeading[64]           (Update: ConvertUtf8ToUtf16 destination)
//    +0x1284  mpGuiKeyboard                  (Update's `stw r11, 0x1284`)
//    +0x1288  mKeyboardListener              (Update: FillString(this+0x1288))
//    +0x12A4  mbCreditsUnlockedOnEnter       (ShowMenu's `stb r11, 0x12A4`)
//    +0x12A8  miCreditsButtonSequenceStage   (OnEnter's `stw r11, 0x12A8`)
// sizeof == 4784 on the console (BrnScreenFlow.h:146). The host is LLP64 and every
// embedded component widens, so the offsets are DOCUMENTARY -- access is by name.
//
// ⛔ NOT IMPORTED FROM THE DWARF (PS3-only; absent from the X360 ledger): the
// sys_memory_container_t mMemoryContainer / mbCleanUpSysConfigThisFrame pair and the six
// sysconf methods (HandleCollisionWorldEvent, ShowSysConfigMenu, ShowSettingsMenuScreen,
// ShowSysConfigSysUtil, CleanUpSysConfigUtil, SysConfigCallback) that go with the PS3
// system-configuration utility, together with the EState enumerators 5..9 that drive them.
// The X360 Update @0x824DE0D8 never reaches any state above 4.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Gui/CgsGuiKeyboard.h"                  // GuiKeyboardListener / CgsUtf16
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"     // MenuComponent (embedded)

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;         // pointer-only member
    class BrnGuiKeyboard;   // pointer-only member (BrnGuiKeyboard.h)

    // DWARF BrnCrashNavSettings.h:44 -- the on-screen-keyboard completion listener the
    // sponsor product-code prompt hands to BrnGuiKeyboard::Show. Its ONE out-of-line
    // virtual, KeyboardClosed @0x824C1850, is NOT defined here: the X360 folds that body
    // into CgsSaveLoad.cpp (it calls CgsGui::ConvertWideCharToAsciiSafe, declared in
    // CgsSaveLoad.h) and this tree already reconstructs it there. ⭐ This declaration is
    // that TU's declaration -- CgsSaveLoad.cpp includes THIS header rather than carrying a
    // second copy, so there is exactly one BrnGui::CrashNavProductCodeKeyboardListener in
    // the program. (A local re-declaration of a type that has a real home is the ODR fork
    // the project rules forbid, and it is invisible to every gate but the link.)
    struct CrashNavProductCodeKeyboardListener : public CgsGui::GuiKeyboardListener
    {
        static const s32 KI_PRODUCT_CODE_LENGTH = 20;   // DWARF :65

        // @0x824C1850 -- body in GameShared/GameClasses/Gui/CgsSaveLoad.cpp.
        virtual void KeyboardClosed(const CgsGui::CgsUtf16* lpResultText);

        // DWARF :248 -- inline predicate; no standalone X360 symbol.
        bool HasJustClosed() const { return mbKeyboardClosed; }

        // @0x824B7A28 -- consume the latched result. Asserts the dialog closed, clears the
        // "closed" latch, and returns the buffer only when the close carried new data.
        char* FillString();

        char macKeyboardString[KI_PRODUCT_CODE_LENGTH];   // +0x04 .. +0x17
        bool mbKeyboardClosed;                            // +0x18
        bool mbNewData;                                   // +0x19
    };

    struct CrashNavSettings : public CgsGui::State
    {
        // DWARF BrnCrashNavSettings.h:135. Only the enumerators the X360 build reaches are
        // spelled -- see the PS3-only note in the banner.
        enum EState
        {
            E_STATE_LOADING_SCREEN            = 0,
            E_STATE_INITIALISING_COMPONENTS   = 1,
            E_STATE_MAIN                      = 2,
            E_STATE_PRODUCT_CODE_INPUT_PENDING = 3,
            E_STATE_PRODUCT_CODE_INPUT        = 4,
        };

        // The seven menu rows SetupMenu builds (X360 `li r4, 7` in both OnEnter's
        // MenuComponent::Construct and ShowMenu's SetupMenu; the KAPC_MENU_TEXT span
        // off_82F26EFC..off_82F26F18 is exactly 7 pointers wide).
        static const s32 KI_NUM_MENU_ITEMS = 7;

        // DWARF :159 -- the three heading buffers handed to the keyboard.
        static const s32 KI_MAX_UTF16_CHARS = 64;

        virtual void OnEnter();     // @0x824B7640
        virtual void OnLeave();     // @0x824CCEC8
        virtual void Update();      // @0x824DE0D8

        // @0x82500028 -- hands the settings screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }


    private:
        void HandleControllerInput(const CgsModule::Event* lpEvent);   // @0x824D90E8
        void ShowMenu();                                               // @0x824BCDF8
        void HandleTriggers(const CgsModule::Event* lpAptTrigger);     // @0x824B78E8
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);     // @0x824B7980 (DWARF: const GuiEventCache*)
        void InputSponsorProductCode();                                // @0x824CCFC0
        void OnInputSponsorProductCode(const char* lpacProductCode);   // @0x824CD030
        bool SteelWheelsSponsorCode(const char* lpacProductCode);      // @0x824B76B8

        // @0x820663A8 (.rdata, 5 words) / @0x820663C0 + @0x820663C8 (.rdata).
        static const s32                    maiEventToObserve[5];
        static const s32                    miNumEventsObserved;
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[];
        static const s32                    miNumResourcesToLoad;

        EState        meState;                                // +0x0038
        MenuComponent mMenuComponent;                         // +0x0040
        GuiCache*     mpGuiCache;                             // +0x1100
        CgsGui::CgsUtf16 macDefaultHeading[KI_MAX_UTF16_CHARS];  // +0x1104
        CgsGui::CgsUtf16 macTitleHeading[KI_MAX_UTF16_CHARS];    // +0x1184
        CgsGui::CgsUtf16 macDialogHeading[KI_MAX_UTF16_CHARS];   // +0x1204
        BrnGuiKeyboard*  mpGuiKeyboard;                       // +0x1284
        CrashNavProductCodeKeyboardListener mKeyboardListener; // +0x1288
        bool          mbCreditsUnlockedOnEnter;               // +0x12A4
        s32           miCreditsButtonSequenceStage;           // +0x12A8
    };
}
