#include "GameSource/Gui/Flow/HUD/States/BrnBootVideos.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface (OutputGuiEvent/RegisterForEvents)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue (the in-queue)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"         // CgsResource::ID::HashString (resId = name hash)
#include "GameSource/Gui/BrnGuiVideoEvents.h"                             // BrnGui::GuiEventPlayVideo (508)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // CgsDev::Log::WriteToLog (diagnostics)

#include <cstdio>                                                         // snprintf (diagnostics)

// BrnGui::BootVideos -- reconstructed from ARTIST (Construct 0x824744B8 / OnEnter 0x824786B8 /
// Update 0x82478778 / OnLeave 0x82478D38). Plays the boot branding logos in sequence by asking the GUI
// StateInterface to play each video (the X360 emits OutputGuiEvent<BrnGui::GuiEventPlayVideo> with a
// VideoDefinition whose id = CgsResource::ID::HashString(name); StateInterface::PlayVideo is the wrapper)
// and advancing on the video-finished feedback (event 510).
//
// [SCOPED] The faithful-core sequence is reconstructed (LOADING -> EA logo -> Criterion -> DONE). The X360
// refinements are marked follow-ons: the soft-reboot skip (HasGameBeenSoftRebooted), the per-tick stop
// (event 6 + sub-id 49 -> StopVideo) + the minimum EA-logo dwell (GuiCache::GetTime >= start + 4.5s), the
// HD-vs-SD first-movie choice, and the Criterion sub-name ("EA_Criterion_Logo") match. The in-queue is a
// VariableEventQueue<18432,16> per the X360 (CgsGui::State stores it as the opaque InputBuffer::GuiEventQueue).

namespace BrnGui
{
    namespace
    {
        // X360 boot GUI event ids (BootVideos::Update 0x82478778).
        const s32 KI_EVENT_CACHE_READY    = 64;    // the boot resource/cache is ready
        const s32 KI_EVENT_VIDEO_FINISHED = 510;   // 0x1FE: a video finished playing (MovieManager -> state)

        // The events this state observes (registered in OnEnter).
        const s32 KAI_OBSERVED_EVENTS[] = { KI_EVENT_CACHE_READY, KI_EVENT_VIDEO_FINISHED, 6, 21 };
        const s32 KI_NUM_OBSERVED_EVENTS = 4;

        typedef CgsModule::VariableEventQueue<18432, 16> BootInQueue;

        // Emit a GuiEventPlayVideo at the MovieManager (via the StateInterface output queue). The video's
        // resource id is CgsResource::ID::HashString(name) -- matching VIDEOLIST.BUNDLE's VideoDataResource
        // ids. The X360 also fills the VideoDefinition rect/crossfade + the sound name (MakeHash); the rect
        // defaults to full-screen here and the sound is a follow-on.
        void PlayVideoByName(CgsGui::StateInterface* lpStateInterface, const char* lpcName)
        {
            if (lpStateInterface == 0)
                return;
            BrnGui::GuiEventPlayVideo lPlayEvent;
            lPlayEvent.muVideoResourceId =
                static_cast<u32>(CgsResource::ID::HashString(reinterpret_cast<const u8*>(lpcName)));
            {
                char lac[96];
                std::snprintf(lac, sizeof(lac), "[BootVideos] play '%s' (id=0x%08X)\n",
                              lpcName, lPlayEvent.muVideoResourceId);
                CgsDev::Log::WriteToLog(lac);
            }
            lpStateInterface->OutputGuiEvent<BrnGui::GuiEventPlayVideo>(lPlayEvent);
        }
    }

    void BootVideos::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CgsGui::State::Construct(liId, lpFsm);
        meUpdateStage = E_UPDATE_STAGE_LOADING;
        mfHDVideoStartTime = 0.0f;
        mfLogoVideoStartTime = 0.0f;
        mpGuiCache = 0;
    }

    void BootVideos::OnEnter()
    {
        meUpdateStage = E_UPDATE_STAGE_LOADING;
        if (mpStateInterface != 0)
            mpStateInterface->RegisterForEvents(KAI_OBSERVED_EVENTS, KI_NUM_OBSERVED_EVENTS);
    }

    void BootVideos::OnLeave()
    {
        if (mpStateInterface != 0)
            mpStateInterface->UnRegisterForEvents(KAI_OBSERVED_EVENTS, KI_NUM_OBSERVED_EVENTS);
    }

    void BootVideos::Update()
    {
        // The in-queue holds the GUI events delivered to this state (cache-ready, video-finished, ...).
        BootInQueue* lpInQueue = reinterpret_cast<BootInQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (liEventId >= 0 && lpEvent != 0)
        {
            switch (meUpdateStage)
            {
            case E_UPDATE_STAGE_LOADING:
            case E_UPDATE_STAGE_MAIN_HD_MOVIE:
                // Boot resource/cache ready -> play the EA logo. [follow-on: soft-reboot skip; HD vs SD.]
                if (liEventId == KI_EVENT_CACHE_READY)
                {
                    PlayVideoByName(mpStateInterface, "EAFranchise");
                    meUpdateStage = E_UPDATE_STAGE_MAIN_EA_LOGO_MOVIE;
                }
                break;

            case E_UPDATE_STAGE_MAIN_EA_LOGO_MOVIE:
                // EA-logo video finished -> play the Criterion logo. [follow-on: min 4.5s dwell + tick-stop.]
                if (liEventId == KI_EVENT_VIDEO_FINISHED)
                {
                    PlayVideoByName(mpStateInterface, "Criterion");
                    meUpdateStage = E_UPDATE_STAGE_MAIN_CRITERION_LOGO_MOVIE;
                }
                break;

            case E_UPDATE_STAGE_MAIN_CRITERION_LOGO_MOVIE:
                // Criterion-logo video finished -> the boot videos are done; signal the flow to advance.
                if (liEventId == KI_EVENT_VIDEO_FINISHED)
                {
                    meUpdateStage = E_UPDATE_STAGE_DONE;
                    SendStateEvent("done");   // [X360 emits a boot-videos-done GUI event (channel 40)]
                }
                break;

            case E_UPDATE_STAGE_DONE:
            default:
                break;
            }

            const CgsModule::Event* lpNextEvent = 0;
            liEventId = lpInQueue->GetNextEvent(lpEvent, &lpNextEvent, &liSize);
            lpEvent = lpNextEvent;
        }
        lpInQueue->Clear();
    }

    void BootVideos::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
    {
        // [follow-on] the X360 returns the boot-video resource tuple table; none needed for the movie path
        // (the MovieManager loads VIDEOLIST.BUNDLE itself), so the state requests no extra resources here.
        *lppResourceTuples = 0;
        *lpuNumberOfResources = 0;
    }
}
