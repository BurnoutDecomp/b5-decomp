#include "GameSource/Gui/Flow/HUD/States/BrnPostTitleScreenLoad.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/Gui/BrnGuiCache.h"            // BrnGui::GuiCache (the intro-video gate at +0x4B78)
#include "GameSource/Gui/BrnGuiVideoEvents.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"   // CgsResource::ID::HashString (video id)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"        // CgsSound::Playback::Name::MakeHash

// BrnGui::PostTitleScreenLoad::GetResourcesToLoad, reconstructed from BURNOUT_X360_ARTIST.XEX
// @ 0x825080B0 (semantic parity, not byte match).
//
// X360 body: the function ignores both out-params and unconditionally runs the assert sequence
//   BeginAssert(); FireAssert("Should not get here", "<...>/BrnPostTitleScreenLoad.h", 63); EndAssert();
// then returns -- a "Should not get here" tripwire. This state is driven by its own
// mpGuiCache / video bookkeeping and is never asked for resources through the FSM's
// sResourceTuple loading path, so the override exists only to flag misuse. (Contrast
// BrnGui::BootLegal::GetResourcesToLoad @ 0x82508090, which returns a real .rdata table.)
//
// The Hex-Rays render drops the two pointer args because the body never touches them; the asm
// signature is the CgsGui::State virtual `void GetResourcesToLoad(const sResourceTuple**, u32*)`.
// Under CGS_ASSERT the original's d:\p4-baked file/line is dropped per policy; the plain
// condition string is forwarded. The assert is unconditional, so it is modelled as CGS_ASSERT(false, ...).

namespace BrnGui
{
    namespace
    {
        enum
        {
            KI_EVENT_UNLOAD_OR_STOP = 6,
            KI_EVENT_GUI_CACHE      = 64,
            KI_EVENT_VIDEO_FINISHED = 510,
        };

        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;
    }

    const s32 PostTitleScreenLoad::maiEventToObserve[PostTitleScreenLoad::miNumEventsObserved] =
    {
        KI_EVENT_UNLOAD_OR_STOP,
        KI_EVENT_GUI_CACHE,
        KI_EVENT_VIDEO_FINISHED,
    };

    void PostTitleScreenLoad::OnEnter()
    {
        mpGuiCache = 0;
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
        meState = E_IDLE;
    }

    void PostTitleScreenLoad::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    void PostTitleScreenLoad::HandleIncomingEvents()
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        StateInputQueue* lpQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        for (s32 liEventId = lpQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_UNLOAD_OR_STOP:
                if (meState == E_PLAYING_VIDEO)
                {
                    GuiEventStopVideo lStopVideoEvent;
                    lStopVideoEvent.mVideo.Prepare();
                    mpStateInterface->OutputGuiEvent(lStopVideoEvent);
                }
                break;

            case KI_EVENT_GUI_CACHE:
                if (mpGuiCache == 0)
                    mpGuiCache = static_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                break;

            case KI_EVENT_VIDEO_FINISHED:
                mbVideoFinished = true;
                break;

            default:
                CGS_ASSERT(false, "Unhandled event");
                break;
            }
        }
    }

    void PostTitleScreenLoad::GetResourcesToLoad(const CgsGui::sResourceTuple** /*lppResourceTuples*/,
                                                 u32* /*lpuNumberOfResources*/) const
    {
        CGS_ASSERT(false, "Should not get here");
    }

    namespace
    {
        // 16-byte GuiEvent<N> command with header { 1, N, 12 } (the shared boot-state
        // channel-command record; same shape as BrnBootLegal.cpp's GuiCommandEvent16).
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;
            u8 maPad[3];
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel)
        {
            GuiCommandEvent16<N> lEvent(0);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }

        const s32 KI_CHANNEL_GUI_OUT = 40;   // GuiEventOut

        // ARTIST 0x8247E474..490 calls OutputGuiEvent<GuiEventPlayMusicOnMenuStream>.
        typedef CgsGui::GuiEventPlayMusicOnMenuStream MusicOnMenuStreamEvent;
    }

    // @ 0x8247E2B8 -- consume this frame's events, then walk the post-title phases:
    //   E_IDLE: mark the cache's post-title phase; if the intro video is enabled, drop
    //     the loading screen (command 20), post the empty-name hash (zero: silence), and
    //     play the "intro" video with its own sound-stream name;
    //     otherwise post phase-complete (70) straight away.
    //   E_PLAYING_VIDEO: when the video-finished feedback lands, restore the menu music
    //     ("Guns_And_Roses"), raise the loading screen (command 19), and post 70.
    //   E_FINISHED_VIDEO: idle (drain the queue).
    void PostTitleScreenLoad::Update()
    {
        HandleIncomingEvents();

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        switch (meState)
        {
        case E_IDLE:
        {
            if (mpGuiCache == 0)
                break;
            mpGuiCache->SetAllowingSignInOutEvents(true); // ARTIST 0x8247E400
            if (mpGuiCache->ShouldPlayIntroVideo())
            {
                BrnGui::GuiEventPlayVideo lPlayEvent;
                lPlayEvent.mVideo.Prepare();
                lPlayEvent.mVideo.mVideoResourceId = static_cast<u32>(
                    CgsResource::ID::HashString(reinterpret_cast<const u8*>("intro")));
                // X360 @0x8247E430..438 stores MakeHash("intro") into the
                // VideoDefinition sound-stream slot before posting event 508.
                lPlayEvent.mVideo.mSoundStreamName = static_cast<u32>(
                    CgsSound::Playback::Name::MakeHash("intro"));
                lPlayEvent.mVideo.mbDisableCustomSoundtracks = true;

                PostCommand16<20>(mpStateInterface, KI_CHANNEL_GUI_OUT);   // loading screen down
                // X360 @0x8247E474..490 posts dword_830082A8. Its sole dynamic
                // initializer hashes the empty string, which MakeHash returns as 0:
                // stop the title/menu stream while the movie-owned stream is active.
                MusicOnMenuStreamEvent lMusic(0);
                mpStateInterface->OutputGuiEvent<MusicOnMenuStreamEvent>(lMusic);
                mbVideoFinished = false;
                mpStateInterface->OutputGuiEvent<BrnGui::GuiEventPlayVideo>(lPlayEvent);
                meState = E_PLAYING_VIDEO;
                if (lpInQueue != 0)
                    lpInQueue->Clear();
                return;
            }
            // No intro video: the phase completes immediately.
            PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            meState = E_FINISHED_VIDEO;
            break;
        }

        case E_PLAYING_VIDEO:
            if (!mbVideoFinished)
                break;
            {
                MusicOnMenuStreamEvent lMusic(static_cast<s32>(
                    CgsSound::Playback::Name::MakeHash("Guns_And_Roses")));
                mpStateInterface->OutputGuiEvent<MusicOnMenuStreamEvent>(lMusic);
            }
            PostCommand16<19>(mpStateInterface, KI_CHANNEL_GUI_OUT);   // loading screen up
            PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);   // phase complete
            meState = E_FINISHED_VIDEO;
            break;

        case E_FINISHED_VIDEO:
            break;

        default:
            CGS_ASSERT(false, "Invalid state");
            break;
        }

        if (lpInQueue != 0)
            lpInQueue->Clear();
    }
}
