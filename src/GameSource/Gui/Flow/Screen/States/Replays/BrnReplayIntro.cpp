// BrnGui::ReplayIntro -- the "RE_INTRO" screen state, reconstructed from the console build.

#include "GameSource/Gui/Flow/Screen/States/Replays/BrnReplayIntro.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                         // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                        // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"    // StateInterface / GuiEventClearScreenSet
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // VariableEventQueue<18432,16>
#include "GameSource/Gui/BrnGuiCache.h"                                     // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiShared.h"                                    // gGuiResourceIdentifier

namespace BrnGui
{
namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> InGuiEventQueue;

    const s32 KI_CHANNEL_GUI_OUT    = 40;
    const s32 KI_CHANNEL_VIEW_STATE = 41;

    const s32 KI_EVENT_APT_TRIGGER = 21;

    // gGuiResourceIdentifier ids: [158] "ReplaysIntro", [159] "ReplaysMain".
    const u32 KU_REPLAYS_INTRO_RESOURCE = 158;
    const u32 KU_REPLAYS_MAIN_RESOURCE  = 159;

    // The movie layers: the intro card sits on level 4, the replay HUD movie on level 3.
    const s32 KI_INTRO_MOVIE_LEVEL = 4;
    const s32 KI_MAIN_MOVIE_LEVEL  = 3;

    // GuiEventShowHideAboveCar (console id 215): { 1, 215, 12 } + the show byte, 16 bytes.
    struct GuiEventShowHideAboveCarWire : public CgsGui::GuiEvent<215>
    {
        bool mbShow;   // +0x0C
        explicit GuiEventShowHideAboveCarWire(bool lbShow)
            : CgsGui::GuiEvent<215>(1, 12), mbShow(lbShow) {}
    };

    // The intro card's outro request (console id 530): { 1, 530, 12 }, 16 bytes; the console
    // writes no payload byte.
    struct GuiEventReplayIntroOutroWire : public CgsGui::GuiEvent<530>
    {
        u8 muReserved;   // +0x0C (never written by the console)
        GuiEventReplayIntroOutroWire() : CgsGui::GuiEvent<530>(1, 12), muReserved(0) {}
    };

    // The view's clear-screen control, { 8, 25, 12, mode, alpha } on the view-state channel.
    void PostClearScreen(CgsGui::StateInterface* lpStateInterface,
                         CgsGui::GuiEventClearScreenSet::EClearScreen leClearScreen)
    {
        CgsGui::GuiEventClearScreenSet lEvent;
        lEvent.meClearScreen = leClearScreen;
        lEvent.mfAlpha       = 1.0f;
        lpStateInterface->GetOutputEventQueue()->AddEvent(
            &lEvent, KI_CHANNEL_VIEW_STATE, static_cast<s32>(sizeof(lEvent)));
    }
}

const s32 ReplayIntro::maiEventToObserve[1] = { KI_EVENT_APT_TRIGGER };
const s32 ReplayIntro::miNumEventsObserved  = 1;

const CgsGui::sResourceTuple ReplayIntro::maIntroResourcesToLoad[1] =
{
    { KU_REPLAYS_INTRO_RESOURCE, CgsGui::E_GUI_RESOURCETYPE_APT_LOADING_SCREEN },
};
const CgsGui::sResourceTuple ReplayIntro::maMainResourcesToLoad[1] =
{
    { KU_REPLAYS_MAIN_RESOURCE, CgsGui::E_GUI_RESOURCETYPE_APT_LOADING_SCREEN },
};

void ReplayIntro::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    mpGuiCache      = mpStateInterface->GetAccessPointers()->GetGuiCache();
    meInternalState = E_INTERNALSTATE_LOADRESOURCES;
    meIntroStage    = E_INTROSTAGE_SHOWING;

    GuiEventShowHideAboveCarWire lShowAboveCar(true);
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        &lShowAboveCar, KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lShowAboveCar)));

    mpGuiCache->ClearReplayPlayerActive();
}

void ReplayIntro::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

    mpStateInterface->PlayAptMovie("", KI_INTRO_MOVIE_LEVEL);
    mpGuiCache->UnloadResources(maIntroResourcesToLoad, 1);
    PostClearScreen(mpStateInterface, CgsGui::GuiEventClearScreenSet::E_CLEAR_SCREEN_INACTIVE);

    meInternalState = E_INTERNALSTATE_LEFT;
}

void ReplayIntro::Update()
{
    InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

    switch (meInternalState)
    {
    case E_INTERNALSTATE_LOADRESOURCES:
        meInternalState = E_INTERNALSTATE_LOADRESOURCES;
        if (!UpdateLoadResources())
            break;
        // The frame the resources land only moves the machine on; the init post waits a frame.
        meInternalState = E_INTERNALSTATE_WFINIT;
        break;

    case E_INTERNALSTATE_WFINIT:
        meInternalState = E_INTERNALSTATE_WFINIT;
        if (!UpdateWFInit())
            break;
        /* fallthrough */

    case E_INTERNALSTATE_RUNNING:
        meInternalState = E_INTERNALSTATE_RUNNING;
        UpdateRunning();
        break;

    case E_INTERNALSTATE_LEFT:
        break;

    default:
        CGS_ASSERT(false, "Invalid internal state : ");
        break;
    }

    lpInQueue->Clear();
}

bool ReplayIntro::UpdateLoadResources()
{
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    if (mpGuiCache->mbReplayDisplayHud == true &&
        !mpGuiCache->EnsureResourcesAreLoaded(maMainResourcesToLoad, 1))
    {
        return false;
    }
    if (!mpGuiCache->EnsureResourcesAreLoaded(maIntroResourcesToLoad, 1))
        return false;

    mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_REPLAYS_INTRO_RESOURCE],
                                   KI_INTRO_MOVIE_LEVEL);
    if (mpGuiCache->mbReplayDisplayHud == true)
    {
        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_REPLAYS_MAIN_RESOURCE],
                                       KI_MAIN_MOVIE_LEVEL);
    }
    return true;
}

bool ReplayIntro::UpdateWFInit()
{
    PostClearScreen(mpStateInterface, CgsGui::GuiEventClearScreenSet::E_CLEAR_SCREEN_INACTIVE);
    return true;
}

void ReplayIntro::UpdateRunning()
{
    InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liEventId != KI_EVENT_APT_TRIGGER)
            continue;

        const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
            reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
        if (lpTrigger->meEventType != CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
            continue;

        if (meIntroStage == E_INTROSTAGE_SHOWING)
        {
            meIntroStage = E_INTROSTAGE_OUTRO;
            GuiEventReplayIntroOutroWire lOutro;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                &lOutro, KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lOutro)));
        }
        else if (meIntroStage == E_INTROSTAGE_OUTRO)
        {
            meIntroStage = E_INTROSTAGE_DONE;
            SendStateEvent("ADVANCE");
        }
    }
}
}
