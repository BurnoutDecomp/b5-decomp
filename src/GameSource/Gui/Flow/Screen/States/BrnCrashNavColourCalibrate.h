#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"   // BrnGui::MenuToggle (mBrightnessToggle, by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"     // BrnGui::HelpItem   (mHelpItemAccept, by value)

namespace CgsModule { struct Event; }

// BrnGui::CrashNavColourCalibrate - the crash-nav colour-calibration screen flow state
// (a 100-step brightness toggle over the grey/white calibration card). Reached from
// CN_SETTINGS on "TO_COLOUR" and leaves back to it on "GO_BACK" (BRNSCREENFSM state 108;
// see the reachability note in the .cpp).
//
// 2026-08-17 -- THE STATE WAS A CTOR-ONLY SHELL AND IS NOW FULLY BODIED. The previous
// revision declared one member (`TextSelection mTextSelection`) and left OnEnter/OnLeave/
// Update to the LogUnreconstructedState stubs in BrnScreenStatesDataLinkStubs.cpp; the
// ledger nevertheless recorded the TU `done`. The real member set falls straight out of
// OnEnter @0x824B82D0's own stores and is confirmed member-for-member, in order, by the
// DecFIGS DWARF for this exact header
// (references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/States/BrnCrashNavColourCalibrate.h).
//
// X360 layout (32-bit console offsets -- DOCUMENTARY ONLY; the x64 gate widens every
// embedded pointer, so every access below is BY NAME):
//   +0x018 CgsGui::State::mpInGuiEventQueue   (base)      -- `*(this+24)` in the asm
//   +0x01C CgsGui::State::mpStateInterface    (base)      -- `*(this+28)`
//   +0x038 MenuToggle           mBrightnessToggle   ("optionToggle_0"; sizeof 0xE98)
//   +0x0E0 ..                     its embedded TextSelection mItemText (MenuToggle +0xA8)
//   +0x185 ..                     its SelectableGroup::miHighlightedIndex (s8)
//   +0xED0 EInternalScreenState meCurrentState
//   +0xED4 GuiCache*            mpGuiCache
//   +0xED8 s32                  mBrightnessSetting
//   +0xEDC s32                  mContrastSetting
//   +0xEE0 ECCOptions           mCurrentlySelectedOption
//   +0xEE4 HelpItem             mHelpItemAccept     ("HelpItemAccept_mc")
//   +0x1090 bool                mbSettingsChanged
// The old header's note that the ctor's "+0x38/+0x50/+0xE0/+0xDA0/+0xEE4/+0xF70/+0x1000
// sub-object slots are not individually recoverable" is RETIRED: +0x38/+0x50/+0xE0/+0xDA0
// are the MenuToggle and the widgets nested inside it (Selectable head, GuiComponent,
// TextSelection, TextField), and +0xEE4/+0xF70/+0x1000 are the HelpItem and its two
// ButtonIconComponent children -- all of which have real committed homes.
//
// Recovered functions (X360 ARTIST):
//   CrashNavColourCalibrate (ctor) @0x82500120   GetResourcesToLoad     @0x825001C0 (inline)
//   OnEnter                        @0x824B82D0   OnLeave                @0x824CE850
//   Update                         @0x824E0228   UpdateInitSetup        @0x824C1B58
//   UpdateLoading                  @0x824CE900   UpdateWFInit           @0x824D98D8
//   UpdatePermanent                @0x824DEAD0   SetupComponents        @0x824CEA00
//   ShowCalibrationCard            @0x824CE7A8   HandleControllerInput  @0x824D9978
//   GetSettingsFromProfile         @0x824B8378   ApplySettings          @0x824CEB50
//   SaveSettings                   @0x824CEBF0
// The DWARF also declares UpdateRunning (cpp:449) and HandleOptionChanged (cpp:672); the
// X360 compiler INLINED both -- Update's `case 3` arm is the whole of UpdateRunning
// (set RUNNING, return true) and no residue of HandleOptionChanged survives in any of the
// fifteen bodies. Neither has a standalone export, so neither is declared here.
namespace BrnGui
{
    // The apt-component cache (BrnGuiCache.h). Pointer-only here; the .cpp includes the
    // full header. (BrnMenuToggle.h already forward-declares it the same way.)
    class GuiCache;

    struct CrashNavColourCalibrate : public CgsGui::State
    {
        // DWARF BrnCrashNavColourCalibrate.h:73 -- the sub-state ladder Update walks.
        // (The COUNT enumerator really is spelled E_RACEINTERNALSTATE_COUNT in the DWARF;
        // kept verbatim rather than "corrected".)
        enum EInternalScreenState
        {
            E_INTERNALSCREENSTATE_SETUP        = 0,
            E_INTERNALSCREENSTATE_LOADING      = 1,
            E_INTERNALSCREENSTATE_INITIALISING = 2,
            E_INTERNALSCREENSTATE_RUNNING      = 3,
            E_INTERNALSCREENSTATE_LEAVING      = 4,
            E_RACEINTERNALSTATE_COUNT          = 5,
        };

        // DWARF BrnCrashNavColourCalibrate.h:84 -- the screen carries exactly one option row.
        enum ECCOptions
        {
            E_CCOPTION_BRIGHTNESS = 0,
            E_CCOPTION_COUNT      = 1,
        };

        // @ 0x82500120 - construct the colour-calibrate state and its embedded widgets.
        CrashNavColourCalibrate();

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825001C0 - hands the colour-calibrate state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // ---- the sub-state ladder (Update's fall-through switch) ---------------------
        // @ 0x824C1B58 -- latch the GuiCache off the per-frame cache event (64).
        bool UpdateInitSetup();
        // @ 0x824CE900 -- wait for the two apt resources, then arm the expected components
        //                 and play the screen's own movie.
        bool UpdateLoading();
        // @ 0x824D98D8 -- wait for the apt components, then read the profile and build the row.
        bool UpdateWFInit();
        // @ 0x824DEAD0 -- per-frame: drain controller input, then tick the toggle row.
        void UpdatePermanent();

        // ---- the screen's own work ---------------------------------------------------
        // @ 0x824CEA00 -- show the calibration card and (re)build the 100-step toggle row.
        void SetupComponents();
        // @ 0x824CE7A8 -- post the calibration-card show/hide GUI events.
        void ShowCalibrationCard(bool lbShow);
        // @ 0x824D9978 -- act on one controller-action event (event id 6).
        void HandleControllerInput(const CgsModule::Event* lpEvent);
        // @ 0x824B8378 -- seed mBrightnessSetting from the options profile.
        void GetSettingsFromProfile();
        // @ 0x824CEB50 -- publish the live brightness/contrast pair (GUI event 545).
        void ApplySettings();
        // @ 0x824CEBF0 -- commit the pair to the options profile and request the save.
        void SaveSettings();

        // ---- statics (DWARF cpp:30/:36/:39/:45/:50/:52/:54; .rdata) -------------------
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F27008 (.rdata, 2 entries)
        static const u32                    muNumResourcesToLoad; // @ 0x82F27018 (.rdata)
        static const s32                    maiEventToObserve[2]; // @ 0x820664C0 (.rdata, 2 entries)
        static const s32                    miNumEventsObserved;  // @ 0x820664C8 (.rdata) == 2
        static const char                   KAC_OPTION_NAME[15];       // "optionToggle_0"
        static const char                   KAC_HELPITEM_ACCEPT[18];   // "HelpItemAccept_mc"
        // DWARF cpp:54 `const char *[100]`. Non-const element type so it decays to the
        // `const char**` MenuToggle::SetupMenuToggle takes (the same spelling
        // BrnPauseScreen.h uses for KAPC_PAUSE_OPTION_STRING_IDS).
        static const char*                  KAPC_BRIGHTNESS_OPTION_VALUES[100];

        // ---- members (DWARF h:94/:99/:101/:109/:110/:112/:115/:118) -------------------
        MenuToggle           mBrightnessToggle;         // +0x038
        EInternalScreenState meCurrentState;            // +0xED0
        GuiCache*            mpGuiCache;                // +0xED4
        s32                  mBrightnessSetting;        // +0xED8
        s32                  mContrastSetting;          // +0xEDC
        ECCOptions           mCurrentlySelectedOption;  // +0xEE0
        HelpItem             mHelpItemAccept;           // +0xEE4
        bool                 mbSettingsChanged;         // +0x1090
    };
}
