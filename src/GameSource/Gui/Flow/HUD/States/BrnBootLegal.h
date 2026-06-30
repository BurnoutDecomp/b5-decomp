#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"          // CgsGui::GuiComponent (the 5 apt animator components)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"          // BrnGui::MenuComponent (the title-screen menu)

// BrnGui::BootLegal - the boot legal / title-screen GUI state (BF_LEGAL). It shows the
// HD compositor + ESRB animation, fades in the "press start" message over a background,
// then runs the attract-loop / title-screen menu and emits the boot-flow command events.
// It displays the title_screen02 apt movie and the 2-entry selection menu.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   OnEnter             @ 0x82477028
//   Update              @ 0x82477270
//   OnLeave             @ 0x82477E28
//   GetResourcesToLoad  @ 0x82508090 (inline)
//   DisplaySelectionMenu@ 0x82475C20
//   UpdateSelectionMenu @ 0x82473978
//
// LAYOUT (X360 console offsets baked into the disasm as raw `this+N`; modelled here as
// named x64 members with [c:0xNN] = the X360 byte offset). The base CgsGui::State occupies
// 0x00..0x37 (mpInGuiEventQueue @c:0x18, mpStateInterface @c:0x1C); BootLegal's own members
// begin at c:0x38. The X360 GuiComponents are an opaque embedded family the state reaches
// through CgsGui::GuiComponent::Construct/AddOutputAptViewState; we model the five 0xDC-stride
// component slots and the menu component as their committed types.
namespace BrnGui
{
    class GuiCache;   // the GUI resource/component cache (BrnGuiCache.h); reached by pointer here

    struct BootLegal : public CgsGui::State
    {
        // The per-frame stage machine (this->meUpdateStage @c:0x48). The X360 switch runs
        // 0..10; the named stages below mirror the case labels of Update().
        enum EUpdateStage
        {
            E_STAGE_WAIT_CACHE        = 0,  // wait for the GuiCache-ready event, then load the apt components
            E_STAGE_START_MOVIE       = 1,  // play the title-screen apt movie + menu-stream music
            E_STAGE_FADE_IN           = 2,  // transition the HD/ESRB components in
            E_STAGE_WAIT_START        = 3,  // wait for "press start" (or the attract timeout)
            E_STAGE_PRESTART_OR_IDLE  = 4,  // start pressed -> transin the start message; else attract timeout
            E_STAGE_ATTRACT_PREPARE   = 5,  // prepare + play the attract video
            E_STAGE_ATTRACT_PLAYING   = 6,  // attract video playing; advance/stop on finish
            E_STAGE_START_PRESSED     = 7,  // start pressed: show the selection menu (or accept immediately)
            E_STAGE_MENU_ACTIVE       = 8,  // the title-screen selection menu is up
            E_STAGE_ACCEPT_DWELL      = 9,  // accept chosen; brief dwell before the load command
            E_STAGE_ACCEPTED          = 10, // accepted -> hand off to the loader
        };

        BootLegal();

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82508090 - hands the legal screen's static resource list to the loader.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // @ 0x82475C20 - build the 2-entry title-screen menu (Normal / BeatTheTeam) and add the
        // transition-out / transition-in apt view states.
        void DisplaySelectionMenu();

        // @ 0x82473978 - dispatch a menu input action (41 = next, 42 = prev), refresh the menu,
        // and add the selected view state ("Normal" / "BeatTheTeam") to the background component.
        void UpdateSelectionMenu(s32 liAction);

        // ----- BootLegal data members (X360 console offsets in [c:0xNN]) -----
        //
        // x64-native: the console byte offsets below are the X360 `this+N` slots the disasm
        // bakes in; on x64 the real offsets differ (8-byte pointers / vptrs), so the members are
        // declared by name/type in field order and the [c:] tags record the console layout only.
        BrnGui::GuiCache* mpGuiCache;               // [c:0x38] cache set when the cache-ready event arrives
        s32               muCurrentAttractMovieIdx; // [c:0x40] index into the attract-movie name table
        bool              mbWaitForStartPressed;    // [c:0x44] true: gate the start message on a "press start"
        s32               meUpdateStage;            // [c:0x48] EUpdateStage
        f32               mfStageStartTime;         // [c:0x4C] GuiCache time the current stage began
        bool              mbSignOutPending;         // [c:0x50] a sign-out overlay is up; suppress the menu
        f32               mfAcceptTime;             // [c:0x54] GuiCache time the accept was registered

        // The five apt animator components the state drives (X360 0xDC-stride slots c:0x58 / 0xE4 /
        // 0x170 / 0x1FC / 0x288). Each is a CgsGui::GuiComponent built in OnEnter and transitioned
        // via AddOutputAptViewState.
        CgsGui::GuiComponent  mHDCompAnimator;        // [c:0x58]  "HDCompAnimator_mc"
        CgsGui::GuiComponent  mEsrbAnimator;          // [c:0xE4]  "esrb_anim"
        CgsGui::GuiComponent  mStartMessageAnimator;  // [c:0x170] "StartMessageAnimatorComponent"
        CgsGui::GuiComponent  mBackgroundAnimator;    // [c:0x1FC] "BackgroundAnimatorComponent"
        CgsGui::GuiComponent  mSelectionMenuAnimator; // [c:0x288] "SelectionMenuAnimatorComponent"

        BrnGui::MenuComponent mSelectionMenu;         // [c:0x318] the "MenuItem" 2-entry menu
        u8                    mu8SelectedMenuIndex;   // [c:0x3BD] chosen entry (0 = Normal, 1 = BeatTheTeam)

        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F25CEC (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F25CF4 (.rdata)
    };
}
