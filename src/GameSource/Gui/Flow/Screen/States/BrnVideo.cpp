// ===================================================================================
// BrnGui::Video -- the screen "video" flow state (FMV_VIDEO).
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnVideo.cpp
//
//   OnEnter / Update / OnLeave, and the controller handler Update inlines.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnVideo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16> (the in-queue view)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"         // CgsResource::ID::HashString
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"              // CgsSound::Playback::Name::MakeHash
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventActivateCrashNav
#include "GameSource/Gui/BrnGuiVideoEvents.h"                             // GuiEventPlayVideo / GuiEventStopVideo

namespace BrnGui
{
namespace
{
    // The state IN-queue is an 18KB variable event queue (VariableEventQueue<18432,16>).
    typedef CgsModule::VariableEventQueue<18432, 16> VideoInQueue;

    const s32 KI_CHANNEL_GUI_OUT      = 40;
    const s32 KI_CHANNEL_GUI_INTERNAL = 42;

    // The two input events this state handles.
    const s32 KI_EVENT_CONTROLLER_ACTION = 6;
    const s32 KI_EVENT_VIDEO_FINISHED    = 510;

    // The controller action (payload word +4 of event 6) that skips the movie.
    const s32 KI_ACTION_SKIP_VIDEO = 49;

    // The movie this state plays (its resource id and its sound stream share the name).
    const char* const KPC_VIDEO_NAME = "Criterion";

    // OnLeave unbinds the apt movie by re-issuing PlayAptMovie with the empty name.
    const char* const KPC_NO_MOVIE_NAME = "";
    const s32         KI_MOVIE_LEVEL    = 3;

    // { 1, 148, 12, <one payload byte> }: show (1) or hide (0) the in-game HUD.
    struct GuiEventShowHideHud : public CgsGui::GuiEvent<148>
    {
        u8 mu8Show;    // +0x0C
        u8 maPad[3];

        explicit GuiEventShowHideHud(bool lbShow)
            : CgsGui::GuiEvent<148>(static_cast<u32>(sizeof(u8)), 12)
            , mu8Show(lbShow ? 1u : 0u)
        {
            maPad[0] = maPad[1] = maPad[2] = 0;
        }
    };

    // { 1, 533, 12 }: the "front-end screen closed" record. The console leaves the payload
    // word unwritten.
    struct GuiEventScreenClosed : public CgsGui::GuiEvent<533>
    {
        u32 muReserved;   // +0x0C

        GuiEventScreenClosed() : CgsGui::GuiEvent<533>(1, 12), muReserved(0) {}
    };
}

const s32 Video::maiEventToObserve[2] = { KI_EVENT_CONTROLLER_ACTION, KI_EVENT_VIDEO_FINISHED };
const s32 Video::miNumEventsObserved   = 2;

// Listen for controller actions and the video-finished event, start in ENTERED, hide the HUD
// (channel 42) and stand the crash nav down (channel 40).
void Video::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    meSubState = E_SUBSTATE_ENTERED;

    GuiEventShowHideHud lHideHud(false);
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lHideHud), KI_CHANNEL_GUI_INTERNAL, 16);

    GuiEventActivateCrashNav lDeactivateCrashNav(false);
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lDeactivateCrashNav), KI_CHANNEL_GUI_OUT,
        static_cast<s32>(sizeof(GuiEventActivateCrashNav)));
}

// The skip action stops the movie (a StopVideo built from a freshly prepared video definition).
void Video::HandleControllerInput(const CgsModule::Event* lpEvent)
{
    const s32 liAction = *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
    if (liAction == KI_ACTION_SKIP_VIDEO)
    {
        GuiEventStopVideo lStopVideo;
        mpStateInterface->OutputGuiEvent<GuiEventStopVideo>(lStopVideo);
    }
}

// ENTERED: start the movie and move to VIDEO_PLAYING. Then, in either state, drain the
// in-queue: controller actions may skip the movie; when it finishes, re-activate the crash nav,
// post screen-closed and go back. Any other sub-state asserts and only clears the queue.
void Video::Update()
{
    VideoInQueue* lpInQueue = reinterpret_cast<VideoInQueue*>(mpInGuiEventQueue);

    switch (meSubState)
    {
        case E_SUBSTATE_ENTERED:
        {
            GuiEventPlayVideo lPlayVideo;
            lPlayVideo.muVideoResourceId = static_cast<u64>(static_cast<u32>(
                CgsResource::ID::HashString(reinterpret_cast<const u8*>(KPC_VIDEO_NAME))));
            lPlayVideo.muSoundStreamName = static_cast<u32>(CgsSound::Playback::Name::MakeHash(KPC_VIDEO_NAME));
            lPlayVideo.mbKeepMemoryWhenFinished = false;
            mpStateInterface->OutputGuiEvent<GuiEventPlayVideo>(lPlayVideo);
            meSubState = E_SUBSTATE_VIDEO_PLAYING;
            break;
        }

        case E_SUBSTATE_VIDEO_PLAYING:
            break;

        default:
            // The console streams "Unhandled state " << meSubState << " in Video::Update".
            CGS_ASSERT(false, "Unhandled state ");
            lpInQueue->Clear();
            return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32 liEventSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
    {
        if (liEventId == KI_EVENT_CONTROLLER_ACTION)
        {
            HandleControllerInput(lpEvent);
        }
        else if (liEventId == KI_EVENT_VIDEO_FINISHED)
        {
            GuiEventActivateCrashNav lActivateCrashNav(true);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lActivateCrashNav), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(GuiEventActivateCrashNav)));

            GuiEventScreenClosed lScreenClosed;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lScreenClosed), KI_CHANNEL_GUI_OUT, 16);

            SendStateEvent("GO_BACK");
        }
        else
        {
            // The console streams "Unexpected event received : " << id << " in " << file
            // << " at line " << line.
            CGS_ASSERT(false, "Unexpected event received : ");
        }
    }

    lpInQueue->Clear();
}

// Unbind the apt movie (PlayAptMovie with the empty name, level 3) and stop listening.
void Video::OnLeave()
{
    mpStateInterface->PlayAptMovie(KPC_NO_MOVIE_NAME, KI_MOVIE_LEVEL);
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
}
}
