#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"              // CgsGui::GuiComponent (by value)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"              // BrnGui::MenuComponent (by value)

namespace CgsModule { struct Event; }

// BrnGui::ReplayClips -- the "RE_CLIPS" screen: the offline replay clip list on the CrashNav
// tab ring. It lists the cache's used replay reels by name, keeps the cache's slot cursor on
// the highlighted row, and on accept sends ADVANCE (to the replay options); it can delete
// the highlighted reel, tab left/right, or back out to CrashNav. While it is up it keeps the
// "ReplaysInfo" movie on level 4.
// Layout and virtual set from the console build (vtable: OnEnter / OnLeave / Update /
// GetResourcesToLoad overridden; the inline ctor installs the vtable, the MenuComponent and
// the GuiComponent's vtable).
namespace BrnGui
{
    class GuiCache;

    struct ReplayClips : public CgsGui::State
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

        // Hands the loader the clip-list movie resource.
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
