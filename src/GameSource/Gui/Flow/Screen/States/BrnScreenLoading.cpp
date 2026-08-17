#include "GameSource/Gui/Flow/Screen/States/BrnScreenLoading.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventPlayAptMovie
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / GuiEventQueueSmall
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (unexpected-event log)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (GetOptionsDataProfile)
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // BrnGui::OptionsDataProfile

#include <cstring>   // memcpy (the trax repost payload copies)

// BrnGui::ScreenLoading -- reconstructed from BURNOUT_X360_ARTIST.XEX:
//   OnEnter @0x824D0CA8   OnLeave @0x824B8FD0   Update @0x824DA770
//   ApplyOptionsDataProfileSettings @0x824D0DC0
//   GetResourcesToLoad = vtable @0x82074130 slot +0x20 -> 0x825011B0 (ICF fold with
//                        BrnGui::Video::GetResourcesToLoad)
// Class shape from the DecFIGS DWARF (BrnScreenLoading.h:42). The SCREEN flow's
// loading state: posted after the intro hand-off, it drops any lingering apt movies,
// waits for the world-load-complete feedback (event 137), replays the loaded options
// profile's settings to the rest of the game, and hands the flow to online play /
// the game room / the in-game state.
namespace BrnGui
{
    namespace
    {
        // ---- observed-event ids (the 2-entry registered table below) ----------------
        const s32 KI_EVENT_GUI_CACHE           = 64;   // per-frame cache event (GuiCache* payload)
        const s32 KI_EVENT_WORLD_LOAD_COMPLETE = 137;  // world loading finished

        const s32 KI_CHANNEL_GUI_OUT    = 40;  // GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE = 41;  // GuiOutViewState

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        // 16-byte GuiEvent<N> command { 1, N, 12 } (the shared channel-command record;
        // same shape as BrnBootProfile.cpp's).
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
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }

        // ---- the options-profile repost records (ApplyOptionsDataProfileSettings
        //      re-publishes the profile's load-complete feedback onto the GUI out
        //      channel; record shapes from the X360 emitters) -------------------------

        // { 4, N, 12, word } (16 bytes) -- the word reposts (278 network update, 462
        // trax play order, 475 settings word).
        template <s32 N>
        struct GuiWordEvent16 : public CgsGui::GuiEvent<N>
        {
            u32 muValue;   // +0x0C
            explicit GuiWordEvent16(u32 luValue) : CgsGui::GuiEvent<N>(4, 12), muValue(luValue) {}
        };

        template <s32 N>
        void PostWordEvent16(CgsGui::StateInterface* lpInterface, u32 luValue)
        {
            GuiWordEvent16<N> lEvent(luValue);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT, 16);
        }

        // { 1, 473, 12, byte } (16 bytes) -- the game-options settings byte repost.
        struct GuiByteEvent16 : public CgsGui::GuiEvent<473>
        {
            u8 mu8Value;   // +0x0C
            u8 maPad[3];
            explicit GuiByteEvent16(u8 lu8Value) : CgsGui::GuiEvent<473>(1, 12), mu8Value(lu8Value)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        // { 8, 463, 12, w0, w1 } (20 bytes) -- the settings qword repost.
        struct GuiSettingsEvent20 : public CgsGui::GuiEvent<463>
        {
            u32 muWord0;   // +0x0C
            u32 muWord1;   // +0x10
            GuiSettingsEvent20(u32 luWord0, u32 luWord1)
                : CgsGui::GuiEvent<463>(8, 12), muWord0(luWord0), muWord1(luWord1) {}
        };

        // { 32, 458, 16, <pad>, 32 payload bytes } (48 bytes) -- the trax-availability
        // repost (two 128-bit trax bit sets; the OutputGuiEvent<GuiEventAudioTraxUpdate>
        // wire @0x824C30A0).
        struct GuiTraxUpdateEvent48 : public CgsGui::GuiEvent<458>
        {
            u32 muPad0C;          // +0x0C (payload is 16-aligned past the header)
            u8  maTraxBits[32];   // +0x10 (EATraxArrayType pair)
            GuiTraxUpdateEvent48() : CgsGui::GuiEvent<458>(32, 16), muPad0C(0) {}
        };

        // { 24, 459, 16, <pad>, 24 payload bytes } (40 bytes) -- the last-played-indexes
        // repost (the OutputGuiEvent<GuiEventAudioTraxLastPlayedIndexes> wire @0x824C3110).
        struct GuiTraxLastPlayedEvent40 : public CgsGui::GuiEvent<459>
        {
            u32 muPad0C;          // +0x0C
            u8  maIndexes[24];    // +0x10
            GuiTraxLastPlayedEvent40() : CgsGui::GuiEvent<459>(24, 16), muPad0C(0) {}
        };

        // { 3, 472, 12, 3 bytes } (16 bytes) -- the controller-settings repost (the
        // OutputGuiEvent<GuiEventControllerSettings> wire @0x82493778).
        struct GuiControllerSettingsEvent16 : public CgsGui::GuiEvent<472>
        {
            u8 maSettings[3];   // +0x0C
            u8 muPad;
            explicit GuiControllerSettingsEvent16(const u8* lpau8Settings)
                : CgsGui::GuiEvent<472>(3, 12), muPad(0)
            {
                maSettings[0] = lpau8Settings[0];
                maSettings[1] = lpau8Settings[1];
                maSettings[2] = lpau8Settings[2];
            }
        };

        // { 8, 545, 12, brightness, contrast } (20 bytes) -- the display-calibration
        // hand-off closing ApplyOptionsDataProfileSettings.
        struct GuiBrightnessContrastEvent20 : public CgsGui::GuiEvent<545>
        {
            s32 miBrightness;   // +0x0C
            s32 miContrast;     // +0x10
            GuiBrightnessContrastEvent20(s32 liBrightness, s32 liContrast)
                : CgsGui::GuiEvent<545>(8, 12), miBrightness(liBrightness), miContrast(liContrast) {}
        };

        // ---- GuiCache boundary (X360 members past the committed layout tail; same
        //      FLAG'd discipline as BrnBootProfile.cpp / BrnInGame.cpp) ----------------

        // FLAG PC-platform leaf: GuiCache::mbIsPerformInviteReceived (DWARF h:1667; X360
        // +0x4B4F) has no committed accessor; no invites on the PC boot.
        bool CacheIsPerformInviteReceived(const GuiCache* /*lpCache*/) { return false; }

        // FLAG PC-platform leaf: GuiCache::mbIsStartingGameDueToPlayerJoin (DWARF
        // h:1666; X360 +0x4B4E) has no committed accessor; no online joins on PC.
        bool CacheIsStartingGameDueToPlayerJoin(const GuiCache* /*lpCache*/) { return false; }
    }

    // @ 0x820666FC (.rdata, read from the image): the two observed ids, in table order.
    const s32 ScreenLoading::maiEventToObserve[2] =
        { KI_EVENT_WORLD_LOAD_COMPLETE, KI_EVENT_GUI_CACHE };
    const s32 ScreenLoading::miNumEventsObserved = 2;   // @ 0x82066704

    // @ 0x824D0CA8 -- register the observed pair, post the loading-entry command (135),
    // the view command (214), and drop any apt movie still owning display levels 3 and 1
    // (the empty-name unload sentinel on the view channel), then reset the per-visit
    // members.
    void ScreenLoading::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        PostCommand16<135>(mpStateInterface, KI_CHANNEL_GUI_OUT);
        PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);

        // X360 records are 20 bytes with 4-byte pointers; the record carries a pointer,
        // so the x64 gate posts with sizeof (the pointer widens).
        CgsGui::GuiEventPlayAptMovie lUnloadLevel3;
        lUnloadLevel3.mpacMovieName = "";
        lUnloadLevel3.miLevelNum    = 3;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lUnloadLevel3), KI_CHANNEL_VIEW_STATE,
            static_cast<s32>(sizeof(CgsGui::GuiEventPlayAptMovie)));

        CgsGui::GuiEventPlayAptMovie lUnloadLevel1;
        lUnloadLevel1.mpacMovieName = "";
        lUnloadLevel1.miLevelNum    = 1;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lUnloadLevel1), KI_CHANNEL_VIEW_STATE,
            static_cast<s32>(sizeof(CgsGui::GuiEventPlayAptMovie)));

        mpGuiCache                    = 0;
        mbInviteInProgress            = false;
        mbStartingGameDueToPlayerJoin = false;
    }

    // @ 0x824B8FD0 -- on leaving the loading screen the state unregisters from the two
    // GUI events it observed while active.
    void ScreenLoading::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // ⭐ RE-ANCHORED 2026-08-16 (boot audit F-P8b-14). This cited the ICF fold @0x824B5A20
    // and reproduced THAT body -- two "Invalid pointer" asserts and a null tuple store.
    // Wrong slot by one: 0x824B5A20 is the end of the PREVIOUS vtable. ScreenLoading's
    // vtable @0x82074130 slot +0x20 is 0x825011B0 (== BrnGui::Video::GetResourcesToLoad,
    // itself an ICF fold), and it is three instructions with no asserts and no tuple store:
    //     li r11, 0 ; stw r11, 0(r5) ; blr
    // i.e. it writes the COUNT only and leaves the caller's tuple pointer untouched. The
    // two asserts were invented; so was the *lppResourceTuples = 0.
    void ScreenLoading::GetResourcesToLoad(const CgsGui::sResourceTuple** /*lppResourceTuples*/,
                                           u32* lpuNumberOfResources) const
    {
        *lpuNumberOfResources = 0;
    }

    // @ 0x824DA770 -- two-pass in-queue drain: latch the cache pointer (64) and mirror
    // its invite / player-join flags (once each), then consume the world-load-complete
    // feedback (137): replay the options profile's settings and hand the flow on.
    void ScreenLoading::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        // ---- pass 1: the cache pointer -----------------------------------------------
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
            {
                const GuiEventCache* lpCacheEvent = reinterpret_cast<const GuiEventCache*>(lpEvent);
                CGS_ASSERT(lpCacheEvent->mpGuiCache != 0, "lpCacheEvent->mpCachePointer");   // cpp:106
                mpGuiCache = lpCacheEvent->mpGuiCache;
            }
        }

        // Mirror the cache's online bookkeeping into the state (latched -- once set they
        // stay set for this visit).
        if (mpGuiCache != 0)
        {
            if (!mbInviteInProgress)
                mbInviteInProgress = CacheIsPerformInviteReceived(mpGuiCache);
            if (!mbStartingGameDueToPlayerJoin)
                mbStartingGameDueToPlayerJoin = CacheIsStartingGameDueToPlayerJoin(mpGuiCache);
        }

        // ---- pass 2: the load-complete hand-off ---------------------------------------
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
                continue;   // consumed by pass 1

            if (liEventId == KI_EVENT_WORLD_LOAD_COMPLETE)
            {
                ApplyOptionsDataProfileSettings();

                const char* lpcStateEvent;
                if (mbInviteInProgress)
                    lpcStateEvent = "TO_ON_PLAY";
                else if (mbStartingGameDueToPlayerJoin)
                    lpcStateEvent = "TO_GAME_ROOM";
                else
                    lpcStateEvent = "ENTER_GAME";
                SendStateEvent(lpcStateEvent);
            }
            else if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                // The X360 streams the diagnostic through the debug print (cpp:169).
                *CgsDev::Log::gpDebugPrint
                    << "Unexpected event received : " << liEventId
                    << " in "
                    << "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnScreenLoading.cpp"
                    << " at line " << 169 << "\n";
            }
        }

        lpInQueue->Clear();
    }

    // @ 0x824D0DC0 -- ask the loaded options profile to publish its "other modules"
    // settings into a local queue, then re-post each record onto the GUI out channel
    // (the wire shapes of the X360 OutputGuiEvent instantiations), closing with the
    // brightness/contrast hand-off (545).
    void ScreenLoading::ApplyOptionsDataProfileSettings()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:201

        CgsGui::GuiEventQueueSmall lLoadCompleteQueue;
        lLoadCompleteQueue.Construct();

        OptionsDataProfile* lpProfile = mpGuiCache->GetOptionsDataProfile();
        // BrnGuiOptionsDataProfile.h forward-declares its queue param as the (not yet
        // homed) BrnGui::GuiEventQueueSmall; the real object is the CgsGui small queue --
        // bridged here until that header adopts the CgsGui typedef.
        lpProfile->NotifyOtherModulesLoadComplete(
            reinterpret_cast<GuiEventQueueSmall*>(&lLoadCompleteQueue));

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lLoadCompleteQueue.GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lLoadCompleteQueue.GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case 278:   // network camera-feed update word
                CGS_ASSERT(lpEvent != 0, "lpNetworkUpdate");   // cpp:272
                PostWordEvent16<278>(mpStateInterface, *reinterpret_cast<const u32*>(lpEvent));
                break;

            case 458:   // trax availability (two 128-bit sets)
            {
                CGS_ASSERT(lpEvent != 0, "lpTraxEvent");       // cpp:216
                GuiTraxUpdateEvent48 lTraxUpdate;
                std::memcpy(lTraxUpdate.maTraxBits, lpEvent, sizeof(lTraxUpdate.maTraxBits));
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lTraxUpdate),
                    KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(GuiTraxUpdateEvent48)));
                break;
            }

            case 459:   // trax last-played indexes
            {
                CGS_ASSERT(lpEvent != 0, "lpTraxEvent");       // cpp:224
                GuiTraxLastPlayedEvent40 lLastPlayed;
                std::memcpy(lLastPlayed.maIndexes, lpEvent, sizeof(lLastPlayed.maIndexes));
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lLastPlayed),
                    KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(GuiTraxLastPlayedEvent40)));
                break;
            }

            case 462:   // trax play-order mode
                CGS_ASSERT(lpEvent != 0, "lpPlayOrderEvent");  // cpp:240
                PostWordEvent16<462>(mpStateInterface, *reinterpret_cast<const u32*>(lpEvent));
                break;

            case 463:   // settings pair
            {
                CGS_ASSERT(lpEvent != 0, "lpSettingsEvent");   // cpp:232
                const u32* lpauWords = reinterpret_cast<const u32*>(lpEvent);
                GuiSettingsEvent20 lSettings(lpauWords[0], lpauWords[1]);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lSettings),
                    KI_CHANNEL_GUI_OUT, 20);
                break;
            }

            case 472:   // controller settings (three bytes)
            {
                CGS_ASSERT(lpEvent != 0, "lpControllerSettingsEvent");   // cpp:256
                GuiControllerSettingsEvent16 lController(reinterpret_cast<const u8*>(lpEvent));
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lController),
                    KI_CHANNEL_GUI_OUT, 16);
                break;
            }

            case 473:   // game-options settings byte
            {
                CGS_ASSERT(lpEvent != 0, "lpGameOptionsSettingsEvent");   // cpp:264
                GuiByteEvent16 lGameOptions(*reinterpret_cast<const u8*>(lpEvent));
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lGameOptions),
                    KI_CHANNEL_GUI_OUT, 16);
                break;
            }

            case 475:   // settings word
                CGS_ASSERT(lpEvent != 0, "lpSettingsEvent");   // cpp:248
                PostWordEvent16<475>(mpStateInterface, *reinterpret_cast<const u32*>(lpEvent));
                break;

            default:
                break;
            }
        }

        GuiBrightnessContrastEvent20 lCalibration(lpProfile->GetBrightness(),
                                                  lpProfile->GetContrast());
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lCalibration), KI_CHANNEL_GUI_OUT, 20);
    }
}
