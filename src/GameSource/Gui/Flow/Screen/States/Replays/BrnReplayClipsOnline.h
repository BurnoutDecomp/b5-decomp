#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"              // CgsGui::GuiComponent (by value)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"              // BrnGui::MenuComponent (by value)

namespace CgsModule { struct Event; }

// BrnGui::ReplayClipsOnline -- the "RE_CLIPS_ON" screen: the online twin of ReplayClips. The
// same reel list and slot cursor, but it only moves the highlight, deletes the highlighted
// reel or backs out (GO_BACK); there is no accept and no tab ring. While it is up it keeps
// the "ReplaysInfo" movie on level 4.
// Layout and virtual set from the console build (vtable: OnEnter / OnLeave / Update /
// GetResourcesToLoad overridden; the inline ctor installs the vtable, the MenuComponent and
// the GuiComponent's vtable).
namespace BrnGui
{
    class GuiCache;

    struct ReplayClipsOnline : public CgsGui::State
    {
        // The load/init/run machine (+0x3C).
        enum InternalState
        {
            E_INTERNALSTATE_LOADRESOURCES = 0,
            E_INTERNALSTATE_WFINIT        = 1,
            E_INTERNALSTATE_RUNNING       = 2,
            E_INTERNALSTATE_LEFT          = 3,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // Hands the loader the online clip-list movie resource.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        bool UpdateLoadResources();
        bool UpdateWFInit();
        void UpdateRunning();
        // Rebuild the menu rows from the cache's used reels and re-seat the highlight.
        void RefreshSlots();
        // One controller press; true when the press left the screen.
        bool HandleControllerInputPressed(const CgsModule::Event* lpEvent);

        // ---- statics (.rdata) ----
        static const s32                    maiEventToObserve[1];
        static const s32                    miNumEventsObserved;
        static const CgsGui::sResourceTuple maResourcesToLoad[1];
        static const u32                    muNumResourcesToLoad;
        static const CgsGui::sResourceTuple maInfoResourcesToLoad[1];

        // ---- members (console offsets documentary) ----
        GuiCache*            mpGuiCache;              // +0x38
        InternalState        meInternalState;         // +0x3C
        MenuComponent        mSlotMenu;               // +0x40   ("MenuItem", 6 rows)
        s32                  miNumSlotsShown;         // +0x1100 (rows the menu was last built with)
        CgsGui::GuiComponent mButtonsAnimComponent;   // +0x1104 ("buttons_anim")
    };
}
