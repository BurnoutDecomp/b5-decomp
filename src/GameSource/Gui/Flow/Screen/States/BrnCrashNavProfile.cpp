// ===================================================================================
// BrnGui::CrashNavProfile -- implementation
//   GameSource/Gui/Flow/Screen/States/BrnCrashNavProfile.cpp
//
// The offline crash-nav profile screen (script id "CN_PROFILE"): a two-row Save / Load
// menu over the shared profile manager. All ten recovered bodies are here -- the
// constructor is the implicit one (the recovered constructor establishes component
// vtables and nothing else), the wider Construct, OnEnter, OnLeave, Update, ShowMenu,
// the three event handlers and the ProfileTaskResultHandler override.
//
// Rung ladder (EState in the header):
//   LOADING_SCREEN          -- the cache event arrives (handler latches it), then the
//                              screen's own apt resource is pulled through the cache and
//                              the movie is played at display level 3.
//   INITIALISING_COMPONENTS -- wait for every expected apt component, drop the expected
//                              list, build the two rows.
//   MAIN                    -- up/down move the highlight; accept runs the highlighted
//                              row's profile task; back leaves ("GO_BACK"); LB/RB post
//                              the tab-toggle events.
//   PROFILE_MESSAGE         -- the manager's prompt owns the input: accept picks option 0
//                              on a one-option prompt and option 1 on a two-option one,
//                              back picks option 0 on a two-option prompt.
//   PROFILE_LOADED          -- set by the task-result callback; Update posts "ENTER_GAME"
//                              and settles back to MAIN.
//
// The Save/Load split is the menu highlight itself: row 0 (Save) runs the manager's Save
// task, row 1 (Load) its Load task, and any other highlight runs nothing -- reproduced
// exactly, because the recovered body has no third arm.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavProfile.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiFlow
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier (the apt movie name)

namespace BrnGui
{
    namespace
    {
        // ---- observed event ids -- exactly the three Update dispatches on ------------
        const s32 KI_EVENT_CONTROLLER_INPUT = 6;    // controller action (sub-id at payload +0x04)
        const s32 KI_EVENT_APT_TRIGGER      = 21;   // apt-movie trigger
        const s32 KI_EVENT_GUI_CACHE        = 64;   // per-frame cache event (GuiCache* payload)

        // ---- controller action ids (the action event's payload word at +0x04) -------
        const s32 KI_ACTION_MENU_UP   = 0x29;   // 41 -> MenuComponent HighlightPrevious
        const s32 KI_ACTION_MENU_DOWN = 0x2A;   // 42 -> MenuComponent HighlightNext
        const s32 KI_ACTION_SELECT    = 0x31;   // 49 GUI_SELECT
        const s32 KI_ACTION_CANCEL    = 0x32;   // 50 GUI_CANCEL
        const s32 KI_ACTION_TOGGLE_L  = 0x36;   // 54 -> "TOGGLE_LEFT"
        const s32 KI_ACTION_TOGGLE_R  = 0x37;   // 55 -> "TOGGLE_RIGHT"

        // ---- the prompt option indices the two-option / one-option prompts take -----
        const u32 KU_MESSAGE_CHOICE_FIRST  = 0;
        const u32 KU_MESSAGE_CHOICE_SECOND = 1;

        // The apt movie this screen plays, taken from the shared id->name table rather
        // than re-spelled: the recovered load reads gGuiResourceIdentifier[140] ==
        // "BrnCrashNavProfile", and 140 is also maResourceTuplesToLoad[0].muId below.
        const s32 KI_RESOURCE_ID_PROFILE = 140;
        const s32 KI_APT_DISPLAY_LEVEL   = 3;
        const char KAC_EMPTY[]           = "";   // the apt unload sentinel name

        // The two component names the screen builds (recovered string operands; the
        // menu name is the per-row "<name>_<i>" stem).
        const char KAC_PROFILE_MENU_COMPONENT[13]      = "ProfMenuItem";
        const char KAC_MESSAGE_ANIMATION_COMPONENT[22] = "MessageTransitionX360";

        // The two localisation keys ShowMenu pushes into the rows, in highlight order.
        const char* const KAPC_PROFILE_MENU_TEXT[CrashNavProfile::KI_NUM_MENU_ITEMS] =
        {
            "$PROFILE_MENU_SAVE", "$PROFILE_MENU_LOAD"
        };

        // The animation clip is driven through the component base's view-state channel.
        const char KAC_APT_TRANSITION[] = "apt_Transition";
        const char KAC_VIEW_VISIBLE[]   = "visible";
        const char KAC_VIEW_INVISIBLE[] = "invisible";

        // The 32-bit -1 zero-extended into the 64-bit apt-id slot, i.e. the group's
        // "no id" value -- the same spelling the sibling crash-nav states use.
        const u64 KU_INVALID_APT_ID = 0xFFFFFFFFull;

        // The state's in-event queue is the concrete queue instantiation the flow drains.
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The event-64 payload view (the queue delivers the header-stripped payload).
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpCachePointer;
        };

        // The event-6 payload view: the action sub-id rides in the payload's +0x04 word.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04
        };
    }

    // ---- static data ---------------------------------------------------------------
    const s32 CrashNavProfile::maiEventToObserve[3] =
    {
        KI_EVENT_CONTROLLER_INPUT, KI_EVENT_APT_TRIGGER, KI_EVENT_GUI_CACHE
    };
    const s32 CrashNavProfile::miNumEventsObserved = 3;

    const CgsGui::sResourceTuple CrashNavProfile::maResourceTuplesToLoad[1] =
    {
        { static_cast<u32>(KI_RESOURCE_ID_PROFILE), CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const s32 CrashNavProfile::miNumResourcesToLoad = 1;

    // ---- constructor ---------------------------------------------------------------
    // Vtable establishment only (see the header): the embedded menu / prompt / animation
    // components construct themselves, and no member is seeded here.
    CrashNavProfile::CrashNavProfile()
    {
    }

    // ---- Construct -----------------------------------------------------------------
    // The flow's Prepare calls this wider form directly for the CN_PROFILE slot: run the
    // base construct, then thread the profile manager.
    void CrashNavProfile::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm,
                                    ProfileManager& lrProfileManager)
    {
        CGS_ASSERT(lpFsm != 0, "lpFsm");

        CgsGui::State::Construct(liId, lpFsm);
        mpProfileManager = &lrProfileManager;
    }

    // ---- OnEnter -------------------------------------------------------------------
    // Register the observed set FIRST (a state that observes nothing receives nothing,
    // so every arm below would be dead code), build the menu + the message-transition
    // clip + the prompt surface, attach the prompt to the manager, and seed the ladder.
    void CrashNavProfile::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mProfileMenuComponent.Construct(KAC_PROFILE_MENU_COMPONENT, mpStateInterface,
                                        KI_NUM_MENU_ITEMS, 0, KU_INVALID_APT_ID);
        mSaveLoadMessageAnimation.Construct(KAC_MESSAGE_ANIMATION_COMPONENT,
                                            mpStateInterface, 0);
        mProfileMessage.Construct(mpStateInterface);
        mpProfileManager->AttachMessageDisplay(&mProfileMessage);

        meState    = E_STATE_LOADING_SCREEN;
        mpGuiCache = 0;
    }

    // ---- OnLeave -------------------------------------------------------------------
    // Detach the prompt, clear the menu, unregister, and post the apt unload sentinel
    // (the empty movie name at the same display level the screen played at).
    void CrashNavProfile::OnLeave()
    {
        mpProfileManager->DetachMessageDisplay(mProfileMessage);

        // The prompt surface's own teardown is the shared empty one: nothing to run.

        mProfileMenuComponent.Clear();

        // Symmetric with OnEnter: the observer table is only a few slots wide, so
        // registering on every entry without releasing runs it out.
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mpStateInterface->PlayAptMovie(KAC_EMPTY, KI_APT_DISPLAY_LEVEL);
    }

    // ---- HandleGuiCacheEvent -------------------------------------------------------
    // Latch the cache the FIRST time one arrives (the guard is on the member, not on the
    // event), then arm the expected-component watch for the menu rows, the prompt's five
    // sub-components and the message-transition clip.
    void CrashNavProfile::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        if (mpGuiCache != 0)
        {
            return;
        }

        const GuiEventCache* lpCacheEvent = reinterpret_cast<const GuiEventCache*>(lpEvent);
        CGS_ASSERT(lpCacheEvent->mpCachePointer != 0,
                   "Invalid cache in CrashNavProfile::HandleGuiCacheEvent");

        mpGuiCache = lpCacheEvent->mpCachePointer;

        mProfileMenuComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
        mProfileMessage.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mSaveLoadMessageAnimation.GetName());
    }

    // ---- HandleTriggers ------------------------------------------------------------
    // The recovered body is the NULL assert and NOTHING ELSE -- the apt-trigger arm was
    // never written (its message even names CrashNavMapMain, the original's own
    // copy/paste). Event 21 is observed and dispatched here purely so it does not reach
    // Update's "unexpected event" log. Reproduced as-is: adding a handler would be
    // inventing behaviour the build does not have.
    void CrashNavProfile::HandleTriggers(const CgsModule::Event* lpAptTrigger)
    {
        CGS_ASSERT(lpAptTrigger != 0, "Invalid event in CrashNavMapMain::HandleTriggers");
    }

    // ---- HandleControllerInput -----------------------------------------------------
    void CrashNavProfile::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent)->miButtonId;

        switch (liAction)
        {
        case KI_ACTION_MENU_UP:
            if (meState == E_STATE_MAIN)
            {
                mProfileMenuComponent.HighlightPrevious();
            }
            break;

        case KI_ACTION_MENU_DOWN:
            if (meState == E_STATE_MAIN)
            {
                mProfileMenuComponent.HighlightNext();
            }
            break;

        case KI_ACTION_SELECT:
            if (meState == E_STATE_MAIN)
            {
                // The highlighted row IS the task: row 0 saves, row 1 loads, anything
                // else runs nothing. Both arms fade the message clip in first and park
                // the screen on the manager's prompt.
                const s32 liHighlightedRow = mProfileMenuComponent.miHighlightedIndex;
                if (liHighlightedRow == KI_MENU_ROW_SAVE)
                {
                    meState = E_STATE_PROFILE_MESSAGE;
                    mSaveLoadMessageAnimation.AddOutputAptViewState(
                        KAC_APT_TRANSITION, KAC_VIEW_VISIBLE, false);
                    mpProfileManager->Save(*this);
                }
                else if (liHighlightedRow == KI_MENU_ROW_LOAD)
                {
                    meState = E_STATE_PROFILE_MESSAGE;
                    mSaveLoadMessageAnimation.AddOutputAptViewState(
                        KAC_APT_TRANSITION, KAC_VIEW_VISIBLE, false);
                    mpProfileManager->Load(*this);
                }
            }
            else if (meState == E_STATE_PROFILE_MESSAGE)
            {
                // Accept on a one-option prompt picks option 0; on a two-option prompt
                // it picks option 1 (the second, "yes" slot).
                if (mProfileMessage.GetNumOptions() == 1)
                {
                    mpProfileManager->HandleMessageChoice(KU_MESSAGE_CHOICE_FIRST);
                }
                else if (mProfileMessage.GetNumOptions() == 2)
                {
                    mpProfileManager->HandleMessageChoice(KU_MESSAGE_CHOICE_SECOND);
                }
            }
            break;

        case KI_ACTION_CANCEL:
            if (meState == E_STATE_MAIN)
            {
                SendStateEvent("GO_BACK");
            }
            else if (meState == E_STATE_PROFILE_MESSAGE &&
                     mProfileMessage.GetNumOptions() == 2)
            {
                mpProfileManager->HandleMessageChoice(KU_MESSAGE_CHOICE_FIRST);
            }
            break;

        case KI_ACTION_TOGGLE_L:
            if (meState == E_STATE_MAIN)
            {
                SendStateEvent("TOGGLE_LEFT");
            }
            break;

        case KI_ACTION_TOGGLE_R:
            if (meState == E_STATE_MAIN)
            {
                SendStateEvent("TOGGLE_RIGHT");
            }
            break;

        default:
            break;
        }
    }

    // ---- HandleProfileTaskResult ---------------------------------------------------
    // The manager's task-completion dispatch lands here: fade the message clip out and
    // arm the "ENTER_GAME" post Update makes on the next tick.
    void CrashNavProfile::HandleProfileTaskResult()
    {
        mSaveLoadMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                        KAC_VIEW_INVISIBLE, false);
        meState = E_STATE_PROFILE_LOADED;
    }

    // ---- ShowMenu ------------------------------------------------------------------
    // Build the two rows (wrapping) and push their localisation keys.
    void CrashNavProfile::ShowMenu()
    {
        mProfileMenuComponent.SetupMenu(KI_NUM_MENU_ITEMS, true);

        for (s32 liRow = 0; liRow < KI_NUM_MENU_ITEMS; ++liRow)
        {
            mProfileMenuComponent.SetText(liRow, KAPC_PROFILE_MENU_TEXT[liRow]);
        }
    }

    // ---- Update --------------------------------------------------------------------
    void CrashNavProfile::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                HandleControllerInput(lpEvent);
                break;

            case KI_EVENT_APT_TRIGGER:
                HandleTriggers(lpEvent);
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;

            default:
                // The dev-only "Unexpected event received : <id>" stream, behind the
                // message filter flags.
                break;
            }
        }
        lpInQueue->Clear();

        // Rung 0 -> 1: once the screen's own apt resource is in, play the movie.
        if (meState == E_STATE_LOADING_SCREEN && mpGuiCache != 0)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KI_RESOURCE_ID_PROFILE],
                                               KI_APT_DISPLAY_LEVEL);
                meState = E_STATE_INITIALISING_COMPONENTS;
            }
        }

        // Rung 1 -> 2: once every expected apt component has initialised, drop the
        // expected list and fill the rows.
        if (meState == E_STATE_INITIALISING_COMPONENTS && mpGuiCache != 0)
        {
            if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                ShowMenu();
                meState = E_STATE_MAIN;
            }
        }

        // The task-result rung: post the flow transition once, then settle back to MAIN.
        if (meState == E_STATE_PROFILE_LOADED)
        {
            SendStateEvent("ENTER_GAME");
            meState = E_STATE_MAIN;
        }

        mProfileMenuComponent.Update();
    }
}
