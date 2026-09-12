#pragma once

// ===================================================================================
// BrnGui::CrashNavProfile -- THE OFFLINE CRASH-NAV "PROFILE" SCREEN (script id
// "CN_PROFILE"), reached from the settings tab's first menu row.
//
// The screen is a two-row menu -- Save / Load -- over the shared profile manager. Each
// row hands the manager a task (Save or Load) and parks the screen in the
// PROFILE_MESSAGE state while the manager's prompt is on screen; the prompt itself is
// drawn by the embedded ProfileMessageComponent, which the state attaches to the
// manager for the lifetime of the screen. When the manager reports the task finished
// (HandleProfileTaskResult, the ProfileTaskResultHandler override), the screen fades
// its message-transition clip out and posts "ENTER_GAME".
//
// SHAPE. The class derives BOTH CgsGui::State (the flow's state surface) and
// BrnGui::ProfileTaskResultHandler (the task callback the manager dispatches); the
// handler sub-object is the SECOND base, which is why the task-result body reaches the
// members through an adjusted this. Member set and order are the recovered ones:
//
//   meState                     the five-rung screen state (EState below)
//   mProfileMenuComponent       the two-row "ProfMenuItem" menu
//   mProfileMessage             the manager's prompt surface (attached in OnEnter)
//   mSaveLoadMessageAnimation   the save/load message-transition clip
//   mpGuiCache                  latched from the per-frame cache event
//   mpProfileManager            threaded by the wider Construct below
//
// The flow's Prepare calls the WIDER Construct(id, fsm, ProfileManager&) DIRECTLY (not
// through the state-construct virtual) for this one slot, exactly as the boot flow does
// for BF_PROFILE.
//
// GetResourcesToLoad is NOT overridden here: the recovered vtable slot is the shared
// "no resources" accessor, i.e. the loader is handed nothing and the screen pulls its
// own apt resource itself, in Update, through the cache.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"           // CgsGui::State (first base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::sResourceTuple
#include "GameSource/Gui/BrnGuiProfile.h"                                 // ProfileManager + ProfileTaskResultHandler
#include "GameSource/Gui/Flow/HUD/States/BrnBootProfile.h"                // BrnGui::ProfileMessageComponent (its owning header)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"       // BrnGui::MenuComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"  // BrnGui::AnimationComponent (by value)

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // GameSource/Gui/BrnGuiCache.h (held by pointer only)

    struct CrashNavProfile : public CgsGui::State, public ProfileTaskResultHandler
    {
        // The screen's own rung ladder.
        enum EState
        {
            E_STATE_LOADING_SCREEN          = 0,   // wait for the cache + the screen's apt resource
            E_STATE_INITIALISING_COMPONENTS = 1,   // wait for the expected apt components, then build the rows
            E_STATE_MAIN                    = 2,   // the two rows are live
            E_STATE_PROFILE_MESSAGE         = 3,   // a manager prompt is up; accept/back pick an option
            E_STATE_PROFILE_LOADED          = 4,   // the task finished: post "ENTER_GAME" and settle back to MAIN
        };

        // The two menu rows, in highlight order.
        static const s32 KI_NUM_MENU_ITEMS = 2;
        static const s32 KI_MENU_ROW_SAVE  = 0;
        static const s32 KI_MENU_ROW_LOAD  = 1;

        // The recovered constructor establishes the object's own and its embedded
        // components' vtable pointers and does nothing else -- no member is seeded there
        // (Construct threads the manager; OnEnter seeds the ladder and the cache). The C++
        // equivalent is therefore the empty body: vtable setup and sub-object construction
        // are the language's job.
        CrashNavProfile();

        // Keep the base (id, fsm) overload visible: the wider overload below would
        // otherwise hide it.
        using CgsGui::State::Construct;

        // Base Construct, then thread the profile manager. The flow's Prepare calls this
        // form directly for the CN_PROFILE slot.
        void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm, ProfileManager& lrProfileManager);

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // ProfileTaskResultHandler override: the manager reports the Save/Load task
        // finished here -- fade the message clip out and arm the "ENTER_GAME" post.
        virtual void HandleProfileTaskResult();

    private:
        void HandleControllerInput(const CgsModule::Event* lpEvent);
        void HandleTriggers(const CgsModule::Event* lpAptTrigger);
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);
        void ShowMenu();

        static const s32                    maiEventToObserve[3];
        static const s32                    miNumEventsObserved;
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[1];
        static const s32                    miNumResourcesToLoad;

        EState                  meState;                    // +0x03C
        MenuComponent           mProfileMenuComponent;      // +0x040
        ProfileMessageComponent mProfileMessage;            // +0x1100
        AnimationComponent      mSaveLoadMessageAnimation;  // +0x1670
        GuiCache*               mpGuiCache;                 // +0x16FC
        ProfileManager*         mpProfileManager;           // +0x1700
    };
}
