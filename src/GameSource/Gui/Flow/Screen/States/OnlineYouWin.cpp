// BrnGui::OnlineYouWin -- the online "you win" flow state.
//   class:BrnGui::OnlineYouWin
//
// Wait on the cached online post-event record (WF_WIN_RESULT): the local player's finisher
// row can advance the screen at once. Otherwise load the screen's movie, wait for its
// components, run the camera count-down (TAKING, the photo "Click" at the end) and hold the
// result (SHOWING) before advancing. Every frame a timed-out player ends the screen early.

#include "GameSource/Gui/Flow/Screen/States/OnlineYouWin.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                          // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"     // StateInterface
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"              // ParameterFormatType
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"             // VariableEventQueue<18432,16>
#include "GameSource/Gui/BrnGuiCache.h"                                      // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                              // GuiRequestOnlinePhotoFinishEvent, E_GUIFLOW_SCREEN
#include "GameSource/Gui/BrnGuiShared.h"                                     // gGuiResourceIdentifier
#include "GameSource/Gui/Events/BrnGuiEventOnlinePostEvent.h"                // GuiEventOnlinePostEvent
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                             // GuiEventCachePointer (event 64)

#include <cmath>     // ceil
#include <cstring>   // strcmp

namespace BrnGui
{
namespace
{
    // The state in-queue is an 18KB variable event queue.
    typedef CgsModule::VariableEventQueue<18432, 16> YouWinInQueue;

    const s32 KI_CHANNEL_GUI_OUT  = 40;
    const s32 KI_EVENT_APT_TRIGGER = 21;
    const s32 KI_EVENT_GUI_CACHE   = 64;

    const u32 KU_YOU_WIN_RESOURCE_ID = 171;   // "ON_YOU_WIN"
    const s32 KI_MOVIE_LEVEL         = 3;
    const char* const KPC_NO_MOVIE_NAME = "";

    // The online stunt-run game modes (the three the mark-man screen gives its stunt-run
    // waiting line); this screen has nothing to show for them. Spelled as values because the
    // tree's GameStateModuleIO::EGameModeType is one mode short of the console's.
    bool IsStuntRunGameMode(s32 liGameMode)
    {
        return liGameMode == 14 || liGameMode == 12 || liGameMode == 17;
    }
}

const s32 OnlineYouWin::maiEventToObserve[2] = { KI_EVENT_APT_TRIGGER, KI_EVENT_GUI_CACHE };
const s32 OnlineYouWin::miNumEventsObserved  = 2;
const CgsGui::sResourceTuple OnlineYouWin::maResourcesToLoad[1] =
    { { KU_YOU_WIN_RESOURCE_ID, CgsGui::E_GUI_RESOURCETYPE_APT } };
const u32 OnlineYouWin::muNumResourcesToLoad = 1;
const f32 OnlineYouWin::KF_WIN_CHECK_DURATION = 2.0f;
const f32 OnlineYouWin::KF_POSING_DURATION    = 5.0f;
const f32 OnlineYouWin::KF_SHOWING_DURATION   = 2.0f;
const char OnlineYouWin::KAC_TIME_UNTILL_CLICK_TEXTFIELD_NAME[13] = "PhotoTime_mc";
const char OnlineYouWin::KAC_PHOTO_COMPONENT_NAME[18]             = "PhotoAnimation_mc";
const char OnlineYouWin::KAC_MODE_ANIMATOR[11]                    = "Pages_anim";
const char OnlineYouWin::KAC_TICK_ANIMATOR_NAME[15]               = "PhotoTime_anim";

// ---- OnEnter -------------------------------------------------------------------
// Listen on the two input events, forget the cache (UpdateGetCache latches it from event 64),
// zero both timers and build the count-down field, the photo icon and the two animators.
void OnlineYouWin::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    mpGuiCache    = 0;
    mfCurrentTime = 0.0f;
    mfTickTime    = 0.0f;
    mTimeUntillClickTextfield.Construct(KAC_TIME_UNTILL_CLICK_TEXTFIELD_NAME, mpStateInterface,
                                        KAC_PHOTO_COMPONENT_NAME);
    mPhotoComponent.Construct(KAC_PHOTO_COMPONENT_NAME, mpStateInterface, 0, 0);
    mModeAnimator.Construct(KAC_MODE_ANIMATOR, mpStateInterface, 0);
    mTickAnimator.Construct(KAC_TICK_ANIMATOR_NAME, mpStateInterface, KAC_PHOTO_COMPONENT_NAME);

    meInternalState = E_INTERNALSTATE_GETCACHE;
    mbUseCamera     = false;
}

// ---- OnLeave -------------------------------------------------------------------
// Stop listening, unbind the screen's movie (the empty name at level 3) and go inert.
void OnlineYouWin::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    mpStateInterface->PlayAptMovie(KPC_NO_MOVIE_NAME, KI_MOVIE_LEVEL);
    meInternalState = E_INTERNALSTATE_LEFT;
    mbUseCamera     = false;
}

// ---- Update --------------------------------------------------------------------
// The fall-through sub-state ladder: each rung re-stamps meInternalState and, when it
// completes, falls straight into the next in the same frame. GETCACHE always falls through.
// The permanent handler runs every frame (LEFT included) and the in-queue is cleared last.
void OnlineYouWin::Update()
{
    switch (meInternalState)
    {
        case E_INTERNALSTATE_GETCACHE:
            UpdateGetCache();
            // fall through

        case E_INTERNALSTATE_WF_WIN_RESULT:
            meInternalState = E_INTERNALSTATE_WF_WIN_RESULT;
            if (!UpdateWFWinResult())
            {
                break;
            }
            // fall through

        case E_INTERNALSTATE_LOADRESOURCES:
            meInternalState = E_INTERNALSTATE_LOADRESOURCES;
            if (!UpdateLoadResources())
            {
                break;
            }
            // fall through

        case E_INTERNALSTATE_WFINIT:
            meInternalState = E_INTERNALSTATE_WFINIT;
            if (!UpdateWFInit())
            {
                break;
            }
            // fall through

        case E_INTERNALSTATE_TAKING:
            meInternalState = E_INTERNALSTATE_TAKING;
            if (!UpdateTaking())
            {
                break;
            }
            // fall through

        case E_INTERNALSTATE_SHOWING:
            meInternalState = E_INTERNALSTATE_SHOWING;
            UpdateShowing();
            break;

        case E_INTERNALSTATE_LEFT:
            break;

        default:
            // The console streams "Invalid internal state : " << state << "\n".
            CGS_ASSERT(false, "Invalid internal state : ");
            break;
    }

    UpdatePermanent();
    reinterpret_cast<YouWinInQueue*>(mpInGuiEventQueue)->Clear();
}

// ---- UpdateGetCache ------------------------------------------------------------
// Latch the cache from the first cache-pointer event in the in-queue. The stunt-run modes
// leave the screen as soon as the cache is known.
void OnlineYouWin::UpdateGetCache()
{
    CGS_ASSERT(mpGuiCache == 0, "NULL == mpGuiCache");

    YouWinInQueue* lpInQueue = reinterpret_cast<YouWinInQueue*>(mpInGuiEventQueue);
    const CgsModule::Event* lpEvent = 0;
    s32 liEventSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
    {
        if (liEventId == KI_EVENT_GUI_CACHE)
        {
            const GuiEventCachePointer* lpCacheEvent = reinterpret_cast<const GuiEventCachePointer*>(lpEvent);
            CGS_ASSERT(lpCacheEvent->mpCachePointer != 0, "NULL != lpCacheEvent->mpCachePointer");
            mpGuiCache = lpCacheEvent->mpCachePointer;
            if (IsStuntRunGameMode(mpGuiCache->GetGameMode()))
            {
                SendStateEvent("ADVANCE");
            }
            break;
        }
    }

    CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");
}

// ---- UpdateWFWinResult ---------------------------------------------------------
// For up to KF_WIN_CHECK_DURATION, look the local player up among the finishers of the
// cached online post-event record; a positive word +0x1C in that row advances the screen.
// When the wait runs out the screen moves on to its own resources.
bool OnlineYouWin::UpdateWFWinResult()
{
    CGS_ASSERT(mpGuiCache, "mpGuiCache");

    if (!(mfCurrentTime < KF_WIN_CHECK_DURATION))
    {
        mfCurrentTime = 0.0f;
        return true;
    }

    mfCurrentTime += mpGuiCache->GetTimeStep();

    const GuiEventOnlinePostEvent* lpPostEventData = mpGuiCache->GetOnlinePostEventData();
    const s32 liPlayerActiveRaceCarIndex = mpGuiCache->GetPlayerActiveRaceCarIndex();
    s32 liIndex = 0;
    for (; liIndex < lpPostEventData->miNumPlayersFinishedEvent; ++liIndex)
    {
        const GuiEventOnlinePostEvent::Record& lrRecord = lpPostEventData->maRecords[liIndex];
        if (lrRecord.miIndex == liPlayerActiveRaceCarIndex)
        {
            if (static_cast<s32>(lrRecord.muValue1C) > 0)
            {
                SendStateEvent("ADVANCE");
            }
            break;
        }
    }

    CGS_ASSERT(liIndex < lpPostEventData->miNumPlayersFinishedEvent,
               "liIndex < lpPostEventData->miNumPlayersFinishedEvent");
    return false;
}

// ---- UpdateLoadResources -------------------------------------------------------
// Once the screen's package is loaded: bind its movie and wait for the mode animator and the
// photo icon to initialise.
bool OnlineYouWin::UpdateLoadResources()
{
    CGS_ASSERT(mpGuiCache, "mpGuiCache");

    if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
    {
        return false;
    }

    mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_YOU_WIN_RESOURCE_ID], KI_MOVIE_LEVEL);
    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mModeAnimator.GetName());
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mPhotoComponent.GetName());
    return true;
}

// ---- UpdateWFInit --------------------------------------------------------------
// When the components are up: start the "OnlineRace" page, use the camera when one is
// attached and the local player is still connected (then show the photo icon), and ask for
// the photo finish.
bool OnlineYouWin::UpdateWFInit()
{
    CGS_ASSERT(mpGuiCache, "mpGuiCache");

    if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
    {
        return false;
    }

    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
    mModeAnimator.AddOutputAptViewState("apt_Transition", "OnlineRace", false);

    mbUseCamera = mpGuiCache->GetCamStatus() != 0 &&
                  !mpGuiCache->GetOnlinePlayerDisconnected(
                      static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex()));
    if (mbUseCamera)
    {
        mPhotoComponent.SetState("Show");
    }

    GuiRequestOnlinePhotoFinishEvent lRequest;
    CgsGui::GuiEventWrapper<GuiRequestOnlinePhotoFinishEvent, KI_CHANNEL_GUI_OUT> lWrapper(lRequest);
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lWrapper), KI_CHANNEL_GUI_OUT,
        static_cast<s32>(sizeof(lWrapper)));
    return true;
}

// ---- UpdateTaking --------------------------------------------------------------
// The camera count-down. With the camera in use, every new whole second left ticks the
// animator and prints the number; when KF_POSING_DURATION is up the field is cleared and the
// icon goes to "Click".
bool OnlineYouWin::UpdateTaking()
{
    if (!(mfCurrentTime < KF_POSING_DURATION))
    {
        mfCurrentTime = 0.0f;
        if (mbUseCamera)
        {
            mTimeUntillClickTextfield.ClearText();
            mTimeUntillClickTextfield.OutputAptData();
            mPhotoComponent.SetState("Click");
        }
        return true;
    }

    mfCurrentTime += mpGuiCache->GetTimeStep();
    if (mbUseCamera)
    {
        const f32 lfSecondsLeft = static_cast<f32>(ceil(KF_POSING_DURATION - mfCurrentTime));
        if (mfTickTime != lfSecondsLeft)
        {
            mfTickTime = lfSecondsLeft;
            mTickAnimator.AddOutputAptViewState("apt_Transition", "tick", false);
            mTimeUntillClickTextfield.SetLocalisedText(static_cast<s32>(lfSecondsLeft),
                                                       CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        }
    }
    return false;
}

// ---- UpdateShowing -------------------------------------------------------------
// Hold the result for KF_SHOWING_DURATION, then advance.
void OnlineYouWin::UpdateShowing()
{
    if (!(mfCurrentTime < KF_SHOWING_DURATION))
    {
        mfCurrentTime = 0.0f;
        SendStateEvent("ADVANCE");
    }
    else
    {
        mfCurrentTime += mpGuiCache->GetTimeStep();
    }
}

// ---- UpdatePermanent -----------------------------------------------------------
// Every frame: a timed-out player advances the screen. Otherwise, when the count-down
// field's movie reports loaded, re-push its stored text.
void OnlineYouWin::UpdatePermanent()
{
    CGS_ASSERT(mpGuiCache, "mpGuiCache");

    if (HasAnyoneTimedOut())
    {
        SendStateEvent("ADVANCE");
        return;
    }

    YouWinInQueue* lpInQueue = reinterpret_cast<YouWinInQueue*>(mpInGuiEventQueue);
    const CgsModule::Event* lpEvent = 0;
    s32 liEventSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
    {
        if (liEventId == KI_EVENT_APT_TRIGGER)
        {
            const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
                reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
            if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD &&
                std::strcmp(lpTrigger->mpacComponentName, mTimeUntillClickTextfield.GetName()) == 0)
            {
                mTimeUntillClickTextfield.SetText(mTimeUntillClickTextfield.GetText());
                return;
            }
        }
    }
}

// ---- HasAnyoneTimedOut ---------------------------------------------------------
// True when any player in the cached online post-event record carries its timed-out flag.
bool OnlineYouWin::HasAnyoneTimedOut()
{
    const GuiEventOnlinePostEvent* lpPostEventData = mpGuiCache->GetOnlinePostEventData();
    for (s32 liIndex = 0; liIndex < lpPostEventData->miNumPlayersInEvent; ++liIndex)
    {
        if (lpPostEventData->maRecords[liIndex].mbFlag34 != 0)
        {
            return true;
        }
    }
    return false;
}
}
