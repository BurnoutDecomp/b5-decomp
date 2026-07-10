#include "GameSource/Gui/Flow/HUD/States/BrnPostTitleScreenLoad.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
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

        // The menu-music request record (hash @+0x0C; 0 == stop). Same 155-keyed record
        // BrnBootLegal.cpp posts -- the module-out consumer dispatches on id 155.
        struct MusicOnMenuStreamEvent : public CgsGui::GuiEvent<155>
        {
            s32 miHash;     // +0x0C (the MakeHash result, or gCurrentMenuMusicHash)
            u8  mu8FlagA;   // +0x10
            u8  mu8FlagB;   // +0x11
            u8  maPad[2];
            explicit MusicOnMenuStreamEvent(s32 liHash)
                : CgsGui::GuiEvent<155>(0, 12), miHash(liHash), mu8FlagA(0), mu8FlagB(0)
            { maPad[0] = maPad[1] = 0; }
        };
    }

    // The current menu-music stream hash global (X360 dword_830082A8): the last hash
    // posted on the menu-music channel; 0 == silence. Defined in BrnGuiModule.cpp
    // (the module's movie tail also reads it).
    extern s32 gCurrentMenuMusicHash;

    // ---- the GuiCache boundary this state touches (X360 far members; same FLAG'd
    // boundary discipline as BrnBootLegalBoundary.cpp) --------------------------------
    namespace
    {
        // X360 *(cache+19285) = 1 -- the "post-title phase" flag the cache carries.
        // FLAG PC-platform leaf: no named accessor on the committed GuiCache yet; a
        // no-op until the cache member is recovered (nothing on the PC boot reads it).
        void CacheSetPostTitlePhase(GuiCache* /*lpCache*/) {}

        // X360 *(cache+19320) -- "play the post-title intro video" gate. Faithful
        // retail default: the intro montage plays after the title screen (the
        // missing-file skip path still advances the flow if INTRO is absent).
        bool CacheShouldPlayIntroVideo(const GuiCache* /*lpCache*/) { return true; }
    }

    // @ 0x8247E2B8 -- consume this frame's events, then walk the post-title phases:
    //   E_IDLE: mark the cache's post-title phase; if the intro video is enabled, drop
    //     the loading screen (command 20), post the current menu-music hash (silence on
    //     a cold boot -- the video carries its own audio), and play the "intro" video;
    //     otherwise post phase-complete (70) straight away.
    //   E_PLAYING_VIDEO: when the video-finished feedback lands, restore the menu music
    //     ("GunsAndRoses"), raise the loading screen (command 19), and post 70.
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
            CacheSetPostTitlePhase(mpGuiCache);
            if (CacheShouldPlayIntroVideo(mpGuiCache))
            {
                BrnGui::GuiEventPlayVideo lPlayEvent;
                lPlayEvent.muVideoResourceId = static_cast<u32>(
                    CgsResource::ID::HashString(reinterpret_cast<const u8*>("intro")));
                // The X360 fills the definition's sound-stream slot with
                // MakeHash("intro") and sets the trailing flag byte; the PC movie
                // player plays the video's own audio track, so the hash ride-along is
                // carried by the definition when that slot lands. [follow-on]
                (void)CgsSound::Playback::Name::MakeHash("intro");
                lPlayEvent.mbDisableCustomSoundtracks = true;

                PostCommand16<20>(mpStateInterface, KI_CHANNEL_GUI_OUT);   // loading screen down
                MusicOnMenuStreamEvent lMusic(gCurrentMenuMusicHash);
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
                    CgsSound::Playback::Name::MakeHash("GunsAndRoses")));
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
