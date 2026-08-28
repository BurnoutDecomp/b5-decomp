#include "GameSource/Gui/Flow/HUD/States/BrnBootVideos.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface (OutputGuiEvent/RegisterForEvents)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue (the in-queue)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"         // CgsResource::ID::HashString (resId = name hash)
#include "GameSource/Gui/BrnGuiVideoEvents.h"                             // BrnGui::GuiEventPlayVideo (508)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // CgsDev::Log::WriteToLog (diagnostics)
#include "GameSource/Gui/BrnGuiCache.h"                                    // GuiCache::GetTime (the EA-logo dwell)
#include "GameSource/Game/BrnGameModule.hpp"                          // BrnGame::GetMainGameModule (soft-reboot query)
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"               // CgsSound::Playback::Name::MakeHash (the video sound name)
#include <cstring>                                                         // strcmp (the Criterion sub-name match)

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

        // The event-64 record: the GUI module posts the cache pointer each frame.
        struct GuiEventCacheReady : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        // The boot GUI sub-ids/channels the X360 uses (BootVideos::Update @0x82478778).
        const s32 KI_EVENT_APT_NAME       = 21;    // apt-name notification (the Criterion sub-name match)
        const s32 KI_EVENT_VIDEO_TICK     = 6;     // per-tick video notification (sub-id 49 = skip request)
        const s32 KI_VIDEO_TICK_SKIP_SUB  = 49;    // payload+4 == 49 -> the player asked to stop
        const s32 KI_CHANNEL_GUI_OUT      = 40;
        const s32 KI_CHANNEL_VIEW_STATE   = 41;
        const f32 KF_EA_LOGO_MIN_DWELL    = 4.5f;  // flt_8205ABF4 -- the EA logo cannot be skipped sooner

        // 16-byte GuiEvent<N> command record { 1, N, 12 } + one trailing flag byte -- the
        // shape every boot state posts on the GUI-out / view-state channels.
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;
            u8 maPad[3];
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel, u8 lu8Flag = 0)
        {
            if (lpInterface == 0)
                return;
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }

        // [loading-fade seat 2026-08-27] the 20-byte clear-screen record OnEnter posts on
        // the view-state channel -- DECODED from the @0x824786B8 asm (stw 0 / stfs
        // flt_82001C98(=1.0f) into the payload pair, head {8, 25, 12}, AddEvent ch 0x29
        // size 0x14): {8, 25, 12, mode 0, alpha 1.0f} == the ViewModule case-25
        // ClearScreenBody enable at full black. Same shape BrnBootLegal.cpp's
        // GuiOptionEvent20 already posts; the old "field semantics unresolved" FLAG that
        // kept it unposted is PAID OFF.
        struct GuiClearScreenEvent20 : public CgsGui::GuiEvent<25>
        {
            s32 miMode;    // 0 == enable the black clear, 1 == disable
            f32 mfAlpha;   // the clear alpha (1.0f == opaque black)
            explicit GuiClearScreenEvent20(s32 liMode)
                : CgsGui::GuiEvent<25>(8, 12), miMode(liMode), mfAlpha(1.0f) {}
        };

        // Emit a GuiEventPlayVideo at the MovieManager (via the StateInterface output queue).
        //
        // ⭐ CORRECTED 2026-08-16 (boot audit F-P8b-5). The X360 payload carries THREE things
        // this only half-filled: the 64-bit ID::HashString at +0x10 (the PC truncated it to
        // u32), the Playback::Name::MakeHash SOUND name at +0x18, and the keep-memory byte at
        // +0x25 -- which is 1 for EAFranchise and 0 for Criterion, and is why the console does
        // NOT tear the movie allocator down between the two logos.
        void PlayVideoByName(CgsGui::StateInterface* lpStateInterface, const char* lpcName,
                             bool lbKeepMemoryWhenFinished)
        {
            if (lpStateInterface == 0)
                return;
            BrnGui::GuiEventPlayVideo lPlayEvent;
            // ⚠️ ZERO-extend, never sign-extend: the console's HashString ends
            // `clrldi r3,r11,32`, so the 64-bit id's top half is always 0. Our hasher
            // returns s32, and letting that widen directly turns every hash with bit 31 set
            // into 0xFFFFFFFF........ -- which matches nothing in the pool. (EAFranchise,
            // 0xF4DE3E5C, is exactly such a hash.)
            lPlayEvent.muVideoResourceId = static_cast<u64>(static_cast<u32>(
                CgsResource::ID::HashString(reinterpret_cast<const u8*>(lpcName))));
            lPlayEvent.muSoundStreamName  = static_cast<u32>(CgsSound::Playback::Name::MakeHash(lpcName));
            lPlayEvent.mbKeepMemoryWhenFinished = lbKeepMemoryWhenFinished;
            {
                char lac[128];
                std::snprintf(lac, sizeof(lac),
                              "[BootVideos] play '%s' (id=0x%016llX sound=0x%08X keep=%d)\n",
                              lpcName,
                              static_cast<unsigned long long>(lPlayEvent.muVideoResourceId),
                              lPlayEvent.muSoundStreamName, (int)lbKeepMemoryWhenFinished);
                CgsDev::Log::WriteToLog(lac);
            }
            lpStateInterface->OutputGuiEvent<BrnGui::GuiEventPlayVideo>(lPlayEvent);
        }

        // @0x824787C8 -- the per-tick stop: a video-tick event carrying sub-id 49 is the
        // player asking to skip, and the console answers it with a StopVideo every Update,
        // BEFORE the stage switch.
        void StopCurrentVideo(CgsGui::StateInterface* lpStateInterface)
        {
            if (lpStateInterface == 0)
                return;
            BrnGui::GuiEventStopVideo lStopEvent;
            lpStateInterface->OutputGuiEvent<BrnGui::GuiEventStopVideo>(lStopEvent);
        }
    }

    void BootVideos::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CgsGui::State::Construct(liId, lpFsm);
        mbIsVideoState = true;   // ARTIST 0x824744EC: stb r9(=1), 0x32(r31) -- overrides base Construct's zero
        meUpdateStage = E_UPDATE_STAGE_LOADING;
        mfHDVideoStartTime = 0.0f;
        mfLogoVideoStartTime = 0.0f;
        mpGuiCache = 0;
    }

    // @ 0x824786B8 -- register, drop the loading screen, and raise the black clear under
    // it (both posts asm-decoded; the old P8b "unresolved record" FLAG is paid off --
    // the 20-byte record is the ViewModule's case-25 ClearScreenBody, enable @ alpha 1.0,
    // NOT a string-carrying shape; the strlen concern belonged to the id-17..20 apt family).
    void BootVideos::OnEnter()
    {
        if (mpStateInterface != 0)
        {
            mpStateInterface->RegisterForEvents(KAI_OBSERVED_EVENTS, KI_NUM_OBSERVED_EVENTS);
            PostCommand16<20>(mpStateInterface, KI_CHANNEL_GUI_OUT);      // stop the loading screen
            // {8, 25, 12, 0, 1.0f} ch 41 size 20 (@0x82478738-58): black clear ON under the
            // loading screen's hide fade, so the fade lands on black and the logo fades in
            // from it -- the console's fade-out-into-the-first-video look.
            GuiClearScreenEvent20 lClearOn(0);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lClearOn), KI_CHANNEL_VIEW_STATE, 20);
        }
    }

    // @ 0x82478D38 -- unregister and clear the level-1 apt movie (asm-decoded: the
    // {8, 18, 12, &"", 1} channel-41 record IS StateInterface::PlayAptMovie("", 1) --
    // the type-18 play-apt-movie record with the empty-string constant unk_820046A7 and
    // level 1; the old "unresolved -> unposted" FLAG is paid off).
    void BootVideos::OnLeave()
    {
        if (mpStateInterface != 0)
        {
            mpStateInterface->UnRegisterForEvents(KAI_OBSERVED_EVENTS, KI_NUM_OBSERVED_EVENTS);
            mpStateInterface->PlayAptMovie("", 1);   // {8,18,12,"",1} @0x82478D60-A8
        }
    }

    void BootVideos::Update()
    {
        // The in-queue holds the GUI events delivered to this state (cache-ready, video-finished, ...).
        BootInQueue* lpInQueue = reinterpret_cast<BootInQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        // @0x824787A4-C8 -- THE PRE-PASS, before the stage switch and on EVERY call: any
        // video-tick event carrying sub-id 49 is a skip request, and it is answered with a
        // StopVideo regardless of which stage we are in.
        {
            const CgsModule::Event* lpScan = 0;
            s32 liScanSize = 0;
            for (s32 liScanId = lpInQueue->GetFirstEvent(&lpScan, &liScanSize);
                 lpScan != 0;
                 liScanId = lpInQueue->GetNextEvent(lpScan, &lpScan, &liScanSize))
            {
                if (liScanId != KI_EVENT_VIDEO_TICK)
                    continue;
                const s32* lpiPayload = reinterpret_cast<const s32*>(lpScan);
                if (lpiPayload[1] == KI_VIDEO_TICK_SKIP_SUB)
                    StopCurrentVideo(mpStateInterface);
            }
        }

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (liEventId >= 0 && lpEvent != 0)
        {
            switch (meUpdateStage)
            {
            case E_UPDATE_STAGE_LOADING:
            case E_UPDATE_STAGE_MAIN_HD_MOVIE:
                // @0x8247882C -- a soft reboot skips the branding entirely: post the
                // phase-complete command and jump straight to DONE.
                // ⭐ ...and so does the TUB "-skipvideos" command-line latch, restored
                // 2026-08-16 (boot audit F-P0-10). It takes the same exit for the same
                // reason: skip the branding, tell the flow the phase is complete.
                if (BrnGame::GetMainGameModule() != 0 &&
                    (BrnGame::GetMainGameModule()->HasGameBeenSoftRebooted() != 0 ||
                     BrnGame::GetMainGameModule()->GetSkipVideos()))
                {
                    PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                    meUpdateStage = E_UPDATE_STAGE_DONE;
                    break;
                }
                // Boot resource/cache ready -> latch the cache (the dwell clock lives on it)
                // and play the EA logo with the keep-memory byte set.
                if (liEventId == KI_EVENT_CACHE_READY)
                {
                    GuiCache* lpCache = reinterpret_cast<const GuiEventCacheReady*>(lpEvent)->mpGuiCache;
                    CGS_ASSERT(lpCache != 0, "Invalid cache in BootVideos::Update");
                    mpGuiCache = lpCache;
                    PlayVideoByName(mpStateInterface, "EAFranchise", /*keepMemory*/ true);
                    meUpdateStage = E_UPDATE_STAGE_MAIN_EA_LOGO_MOVIE;
                    // @0x82478A18 -- stamp the EA start time; the skip below is gated on it.
                    mfLogoVideoStartTime = (mpGuiCache != 0) ? mpGuiCache->GetTime() : 0.0f;
                }
                break;

            case E_UPDATE_STAGE_MAIN_EA_LOGO_MOVIE:
                // @0x82478B10-44 -- the EA logo may only be skipped once it has been up for
                // KF_EA_LOGO_MIN_DWELL seconds; before that the skip request is ignored.
                if (liEventId == KI_EVENT_VIDEO_TICK && mpGuiCache != 0)
                {
                    const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
                    if (lpiPayload[1] == KI_VIDEO_TICK_SKIP_SUB &&
                        mpGuiCache->GetTime() > mfLogoVideoStartTime + KF_EA_LOGO_MIN_DWELL)
                    {
                        StopCurrentVideo(mpStateInterface);
                    }
                }
                // EA-logo video finished -> the Criterion logo (keep-memory CLEAR: this is
                // the last branding movie, so its memory is handed back).
                if (liEventId == KI_EVENT_VIDEO_FINISHED)
                {
                    PlayVideoByName(mpStateInterface, "Criterion", /*keepMemory*/ false);
                    meUpdateStage = E_UPDATE_STAGE_MAIN_CRITERION_LOGO_MOVIE;
                }
                break;

            case E_UPDATE_STAGE_MAIN_CRITERION_LOGO_MOVIE:
                // @0x82478C34-70 -- the apt-name notification for "EA_Criterion_Logo" ends the
                // phase as well as the video-finished event does; both post command 70 on the
                // GUI-out channel, which is what MarketingScreens::Update advances on.
                if (liEventId == KI_EVENT_APT_NAME)
                {
                    const s32*  lpiPayload = reinterpret_cast<const s32*>(lpEvent);
                    const char* lpcName    = reinterpret_cast<const char*>(lpEvent) + 8;
                    if (lpiPayload[0] == 4 && std::strcmp(lpcName, "EA_Criterion_Logo") == 0)
                    {
                        PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                        meUpdateStage = E_UPDATE_STAGE_DONE;
                    }
                }
                if (liEventId == KI_EVENT_VIDEO_FINISHED)
                {
                    PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                    meUpdateStage = E_UPDATE_STAGE_DONE;
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
