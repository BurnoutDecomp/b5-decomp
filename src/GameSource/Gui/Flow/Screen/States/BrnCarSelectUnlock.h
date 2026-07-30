#pragma once

// ===================================================================================
// GameSource/Gui/Flow/Screen/States/BrnCarSelectUnlock.h
//
// BrnGui::CarSelectUnlock -- the "car unlocked" celebration screen flow state. A
// CgsGui::State leaf: on entry it constructs the manufacturer-badge, car-name text
// field, the two apt animation clips and the "continue" help item, then runs an
// internal state machine (GetCache -> LoadResources -> WaitForInit -> Running) that
// plays the unlock movie, shows the unlocked car's badge/name/description ticker and
// waits for the player to advance.
//
// Class shape (virtual layout, member set + order, the InternalState enum) is from the
// DecFIGS DWARF (BrnCarSelectUnlock.h), gated on the X360 ledger. The byte offsets in the
// trailing comments are the X360 32-bit-ABI offsets proven by BURNOUT_X360_ARTIST.XEX
// (mpGuiCache@+0x38 .. mHelpPromptAnimComponent@+0x434); the gate compiles 64-bit, so the
// embedded widgets widen and the offsets are documentary only -- every member is accessed
// BY NAME. The out-of-line bodies live in BrnCarSelectUnlock.cpp.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"              // CgsGui::GuiComponent (AnimationComponent base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple
#include "GameSource/Gui/BrnGuiTextField.h"                                      // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"                    // BrnGui::HelpItem (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnManufacturerIcon.h"           // BrnGui::ManufacturersIcon (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"          // BrnGui::AnimationComponent (by value)

namespace BrnGui { class GuiCache; }

namespace BrnGui
{
    // The apt transition/animation clips embedded at X360 +0x1F4 (LogoAnim) and +0x434
    // (HelpPrompt) are BrnGui::AnimationComponent (DWARF). The local stopgap that used to
    // stand in for it here is RETIRED (intro wave 2026-07-29): the real class is homed in
    // GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h, included above. It
    // adds no members over CgsGui::GuiComponent (X360 stride 0x8C), so the stopgap's
    // 0x40-byte reserved tail was surplus.

    struct CarSelectUnlock : public CgsGui::State
    {
        // DWARF BrnCarSelectUnlock.h:71 -- the load/init/run state machine (meInternalState).
        enum InternalState
        {
            E_INTERNALSTATE_GETCACHE      = 0,
            E_INTERNALSTATE_LOADRESOURCES = 1,
            E_INTERNALSTATE_WFINIT        = 2,
            E_INTERNALSTATE_RUNNING       = 3,
            E_INTERNALSTATE_LEFT          = 4,
            E_INTERNALSTATE_COUNT         = 5,
        };

        // @0x824CA198 -- register for the observed events, stop the loading movie, construct
        // the badge/text/animation/help components and null-latch into E_INTERNALSTATE_GETCACHE.
        virtual void OnEnter();

        // @0x824CA2E8 -- unregister, play the leave apt movie, clear the cache's expected-apt
        // list and mark E_INTERNALSTATE_LEFT.
        virtual void OnLeave();

        // Per-frame tick (dispatches the internal-state machine). Body lands with its own
        // ledger slice; declared here for the vtable shape.
        virtual void Update();

        // @0x824B5A20 -- this state loads its resources internally (UpdateLoadResources), so the
        // generic loader path is handed an EMPTY list: after null-guarding both out params
        // (X360 asserts "Invalid pointer"), it writes *lppResourceTuples = NULL and
        // *lpuNumberOfResources = 0.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        // ---- internal-state-machine steps (DWARF BrnCarSelectUnlock.h; private) ----
        // @0x824C15A8 -- scan the in-event queue for the GuiCache event (type 64) and latch it.
        void UpdateGetCache();
        // @0x824D8268 -- once the cache reports the resources loaded, play the unlock movie,
        // prime the expected-apt-component list and report done.
        bool UpdateLoadResources();
        // @0x824B5B28 -- wait for the flow's apt components to finish initialising.
        bool UpdateWFInit();
        // @0x824CA420 -- drain the in-queue while running (badge/name/description ticker +
        // advance). Body lands with its own ledger slice (see BrnCarSelectUnlock.cpp note).
        void UpdateRunning();

        // @cpp:297 -- play the screen's unlock apt movie. Not in this slice's ledger set;
        // declared here for the UpdateLoadResources call site (body links from its own slice).
        void PlayMovie();

        // ---- statics (DWARF cpp:26-53; .rdata) ----
        static const s32                    maiEventToObserve[6];               // @0x82066054 (6 entries)
        static const s32                    miNumEventsObserved;                // == 6
        static const CgsGui::sResourceTuple maResourcesToLoad[];               // @0x82F26D78 (2 entries)
        static const u32                    muNumResourcesToLoad;               // == 2
        static const char                   KAC_MANUFACTURER_LOGO[20];          // "ManufacturerLogo_mc"
        static const char                   KAC_CAR_NAME[11];                   // "CarName_mc"
        static const char                   macAnimComponentName[13];           // "LogoAnim_mcp"
        static const char                   KAC_HELPITEM_CONTINUE[20];          // "HelpItemContinue_mc"
        static const char                   macHelpPromptAnimComponentName[15]; // "HelpPrompt_mcp"

        // ---- members (DWARF h:82-105; X360 offsets are documentary) ----
        GuiCache*          mpGuiCache;                // +0x38
        InternalState      meInternalState;           // +0x3C
        ManufacturersIcon  mManufacturerLogo;         // +0x40  ("ManufacturerLogo_mc")
        TextField          mCarName;                  // +0xCC  ("CarName_mc")
        AnimationComponent mAnimComponent;            // +0x1F4 ("LogoAnim_mcp")
        f32                mrTimeScreenVisible;        // +0x280
        bool               mbTickerVisible;           // +0x284
        bool               mHelpPromptVisible;        // +0x285
        HelpItem           mHelpItemContinue;         // +0x288 ("HelpItemContinue_mc")
        AnimationComponent mHelpPromptAnimComponent;  // +0x434 ("HelpPrompt_mcp")
    };
}
