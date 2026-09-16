#pragma once

// ===================================================================================
// BrnGui::CrashNavTrax -- THE PAUSE MENU'S "EA TRAX" TAB (CN_TRAX, script id 94).
//
// ⭐⭐ WHY THIS HEADER GREW (2026-09-16). It was a header-only SHELL: it declared exactly
// one thing, GetResourcesToLoad, so OnEnter / Update / OnLeave fell through to the
// do-nothing CgsGui::State base. A state that registers for no events receives none, so
// the tab could not be navigated, no track could be enabled or disabled, the play order
// could not be changed and nothing was ever saved -- the same shape CrashNavSettings and
// CrashNavOptions were fixed out of (see those headers' banners).
//
// ⭐ IT IS REACHABLE. BRNSCREENFSM.BUNDLE's NextState_8CN_SETTINGS routes "TO_TRAX" to
// Transition_8CN_SETTINGS_94CN_TRAX, and CN_SETTINGS is one RB press from the START pause
// screen (CN_D_DETAIL --TOGGLE_RIGHT--> CN_SETTINGS). Unlike CN_STATS, this tab is in the
// shipped flow.
//
// SHAPE. DecFIGS DWARF (references/DecFIGS/dwarfdump/.../BrnCrashNavTrax.h), gated on the
// X360 ledger. Guest offsets are MEASURED from the X360 bodies:
//    +0x0038  meCurrentState        (OnEnter @0x824B8C90: `*(a1+56) = 0`)
//    +0x003C  mpGuiCache            (UpdateInitSetup @0x824C1DE0: `*(a1+60) = *i`)
//    +0x0040  mEATraxMenuComponent  (OnEnter: Construct(a1+64, "EATraxMenuInstance", ...))
//    +0x0230  meTraxPlayOrderMode   (OnEnter: `*(a1+560) = 0`)
//
// ⛔ NOT IMPORTED FROM THE DWARF (declared there, ABSENT from the X360 ledger, i.e. folded
// by the X360 compiler): UpdateRunning (cpp:333) -- Update's RUNNING rung is inlined to a
// single mEATraxMenuComponent.Update() with no call -- and SetExpectedAptComponentList
// (cpp:277), whose whole body is inlined into UpdateLoading's tail. Per the project rule
// the X360 ledger decides what exists, so neither is declared here.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Events/BrnGuiEventAudioTrax.h"   // EATraxArrayType / ETraxPlayOrderMode
#include "GameSource/Gui/Flow/Screen/Components/BrnEATraxMenuComponent.h"  // embedded by value

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // pointer-only member

    struct CrashNavTrax : public CgsGui::State
    {
        // DWARF BrnCrashNavTrax.h:76. Update @0x824E0D50 dispatches 0..4 and asserts above.
        enum EInternalScreenState
        {
            E_INTERNALSCREENSTATE_SETUP        = 0,
            E_INTERNALSCREENSTATE_LOADING      = 1,
            E_INTERNALSCREENSTATE_INITIALISING = 2,
            E_INTERNALSCREENSTATE_RUNNING      = 3,
            E_INTERNALSCREENSTATE_LEAVING      = 4,
            E_RACEINTERNALSTATE_COUNT          = 5,
        };

        virtual void OnEnter();     // @0x824B8C90
        virtual void OnLeave();     // @0x824CF630
        virtual void Update();      // @0x824E0D50

        // @0x825000E0 -- hands the trax screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        bool UpdateInitSetup();                                     // @0x824C1DE0
        bool UpdateLoading();                                       // @0x824CF6C0
        bool UpdateWFInit();                                        // @0x824D9D58
        void UpdatePermanent();                                     // @0x824E03B8
        void HandleControllerInput(const CgsModule::Event* lpEvent);        // @0x824DECF8
        void HandleTriggers(const CgsModule::Event* lpEvent);               // @0x824B8D00
        void HandleOverlayCompleteEvent(const CgsModule::Event* lpEvent);   // @0x824DEF60
        void HandleTraxEnabledStateChange();                        // @0x824CF898
        void OnUpdatePlayOrderMode();                               // @0x824CF7C0
        void PreviewTrack(s32 liTrackIndex);                        // @0x824CF830
        void ApplyAndSaveSettings();                                // @0x824CF938
        void StateCancelFlow();                                     // @0x824D9E30

        static const s32                    maiEventToObserve[4];   // @0x82066620 (.rdata)
        static const s32                    miNumEventsObserved;    // == 4
        static const CgsGui::sResourceTuple maResourcesToLoad[];    // @0x82F27278 (.rdata)
        static const u32                    muNumResourcesToLoad;   // @0x82F27288 (.rdata) == 2
        static const char                   mpacEATraxMenuComponentName[19];

        EInternalScreenState meCurrentState;                        // +0x0038
        GuiCache*            mpGuiCache;                            // +0x003C
        EATraxMenuComponent  mEATraxMenuComponent;                  // +0x0040
        GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode meTraxPlayOrderMode;   // +0x0230
    };
}
