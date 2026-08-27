// ===================================================================================
// wave-S2 partfile 01 of BrnRaceMainHudState.cpp -- the RACE_MAIN phase machine
//
//   BrnGui::RaceMainHudState::Update          @0x82481898 (cpp:1855)
//   BrnGui::RaceMainHudState::UpdateLoading   @0x8247A410
//   BrnGui::RaceMainHudState::UpdateWFInit    @0x82480200 (cpp:2819)
//   BrnGui::RaceMainHudState::UpdatePermenant @0x824806E8 (cpp:3620 / 3702 / 3737)
//   BrnGui::RaceMainHudState::RevealHud       @0x8247A4E0
//
// Every one of the five was re-verified against
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x<ADDR>.json -- the `name` field of each JSON
// matches the symbol claimed above -- and every body below is transcribed from the raw
// DISASSEMBLY, arbitrated over Hex-Rays wherever the two disagree (they disagree three
// times; each is called out at its site).
//
// The sibling BrnRaceMainHudState.cpp keeps the class's other bodies (OnEnter, OnLeave,
// UpdateSetupState, UpdateRunning, SetExpectedComponent/SetExpectedAptComponentList,
// SetupEventInfo, the countdown pair, the freeburn tickers) and the static .rdata
// resource table. Nothing in this partfile redeclares any of that; the shared class
// definition is GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h.
//
// WHAT THIS PARTFILE IS FOR. RACE_MAIN's phase machine is NOT a copy of the freeburn
// one. Three differences are load-bearing and each is preserved verbatim:
//   1. Update CASCADES. FBurnMainHudState::Update advances at most one phase per frame
//      (BrnFBurnMainHudState.cpp:542). RACE_MAIN's switch FALLS THROUGH -- SETUPSTATE ->
//      LOADING -> WF_INIT -> RUNNING can all execute in a single frame (the console's own
//      `goto LABEL_3/4/5` chain @0x824818EC..0x8248194C). Copying the freeburn shape here
//      would change the reveal timing by up to three frames.
//   2. UpdatePermenant is GATED (`if (meInternalState != IDLE)`, @0x824819E8); the
//      freeburn state calls its own unconditionally.
//   3. UpdateWFInit blocks on a NON-EMPTY expected-apt-component list. RACE_MAIN's
//      SetExpectedAptComponentList installs exactly one hash -- "EventHud_Animator" --
//      where the freeburn state installs an empty list, so AreAllAptComponentsInitialised
//      here is a real handshake, not a formality. If the apt side never reports that
//      component the HUD loads and never reveals; that is what the [hud-reveal] diagnostic
//      below exists to distinguish from a black screen.
// ===================================================================================

#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT / Begin/Fire/EndAssert
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the two streamed asserts)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint ([hud-reveal] diag)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // Start/StopMonitor
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface + the out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // the ShowHide / ticker / overlay records
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // E_GUIFLOW_HUD
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // FreeburnChallengeManager (state + host reads)
#include "GameSource/Gui/BrnGuiPerfmons.h"                                // GuiPerfmons::miHudStateUpdate

#include <cstring>   // std::memcpy (the 8-byte overlay-id payload)

namespace BrnGui
{
    namespace
    {
        // The GUI output channels (the legend is in CgsGuiEvent.h's GuiEventWrapper note;
        // the console spells them `li r5, 0x28` / `li r5, 0x29`).
        const s32 KI_CHANNEL_GUI_OUT    = 40;   // 0x28 -- GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE = 41;   // 0x29 -- GuiOutViewState

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The 16-byte GuiEvent<N> command record { 1, N, 12, flag } this state posts four
        // times (236 / 433 / 215 / and the freeburn state's twin at
        // BrnFBurnMainHudState.cpp:66 -- the same helper, kept file-local per partfile).
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;
            u8 maPad[3];
            explicit GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel, u8 lu8Flag)
        {
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }

        // The 24-byte road-rule bring-up record { 8, 327, 16 } + a zeroed 8-byte tail.
        // X360 @0x82480460..0x82480490: `li r11,8 -> var_70`, `li r11,0x147 -> var_6C`,
        // `li r11,0x10 -> var_68`, `std r23(==0) -> var_60`, size 0x18, channel 0x28.
        // FLAG (console-uninitialised word): the console never writes var_64 (payload +12),
        // so its byte-4..7 are whatever the caller's frame held. Zeroed here -- the same
        // choice BrnFBurnMainHudState.cpp:717 already made for the identical record.
        struct GuiEvent327 : public CgsGui::GuiEvent<327>
        {
            s32 miPadA;   // +0x0C -- console-uninitialised (see FLAG above)
            s32 miPadB;   // +0x10 -- console `std r23` low word (0)
            s32 miPadC;   // +0x14 -- console `std r23` high word (0)
            GuiEvent327() : CgsGui::GuiEvent<327>(8, 16), miPadA(0), miPadB(0), miPadC(0) {}
        };

        // (2026-08-27 verify round: the TU-local KAPC_PRE_EVENT_OVERLAYS[18] copy that stood
        // here is DELETED -- it was DEAD by name lookup: inside a RaceMainHudState member,
        // the unqualified name binds the CLASS STATIC declared in BrnRaceMainHudState.h:257
        // and defined in BrnRaceMainHudState.cpp:~340, so an edit here would silently have
        // had no effect. One definition now, the class-static one; both copies held the same
        // image-verified data @0x82F261E0.)

        // The freeburn-challenge selector actions UpdatePermenant's case-573 arm switches on
        // (`lwz r11, 8(r28)`, switch 4 cases @0x824808A0). Only action 2 does anything here.
        const s32 KI_SELECTOR_ACTION_START_TICKER = 2;
    }

    // =======================================================================
    //  Update  @ 0x82481898
    // =======================================================================
    // The cascading phase machine, bracketed by the "HUD state Update" CPU monitor.
    //
    // dword_82F27640 is GuiPerfmons::miHudStateUpdate, not a bare global: GuiPerfmons::
    // Initialise @0x824EF050 stores AddMonitor's handle to 0x82F2763C right after loading
    // "        Gui - HudFlow Update" (@0x824EF19C/0x824EF1C4) and to 0x82F27640 right after
    // loading "          HUD state Update" (@0x824EF1CC/0x824EF1F0) -- so +0x82F27640 is the
    // second of that pair. BrnGuiPerfmons.cpp:90 registers the same monitor.
    //
    // ⭐ THE FALLTHROUGH IS THE POINT. Each case re-stores its own enum value FIRST and then
    // falls into the next case on a true return, so one Update() call can walk the whole
    // ladder. Written as C++ fallthrough because that is literally the console's control
    // flow (`beq loc_824819E4` on false, straight-line otherwise); an if/else chain would
    // read the same but hide why the re-stores exist -- they are what lands the new phase.
    void RaceMainHudState::Update()
    {
        CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miHudStateUpdate);

        switch (meInternalState)
        {
        case E_RACEINTERNALSTATE_SETUPSTATE:
            meInternalState = E_RACEINTERNALSTATE_SETUPSTATE;   // @0x824818EC/F4
            if (!UpdateSetupState())
                break;
            // fall through -- @0x82481904 `beq` only on false
        case E_RACEINTERNALSTATE_LOADING:
            meInternalState = E_RACEINTERNALSTATE_LOADING;      // @0x82481908/10
            if (!UpdateLoading())
                break;
            // fall through -- @0x82481920
        case E_RACEINTERNALSTATE_WF_INIT:
            meInternalState = E_RACEINTERNALSTATE_WF_INIT;      // @0x82481924/2C
            if (!UpdateWFInit())
                break;
            // fall through -- @0x8248193C
        case E_RACEINTERNALSTATE_RUNNING:
            meInternalState = E_RACEINTERNALSTATE_RUNNING;      // @0x82481940/48
            UpdateRunning();
            break;
        case E_RACEINTERNALSTATE_IDLE:
            meInternalState = E_RACEINTERNALSTATE_IDLE;         // @0x82481954/58
            break;
        default:
            {
                // The streamed assert @0x82481960..0x824819E0: the message text, then the
                // offending enum value, then FireAssert at cpp:1855 (`li r5, 0x73F`).
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Should never call update in the following state";
                lStrStream << static_cast<s32>(meInternalState);
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    lacMessage,
                    "..\\..\\..\\GameSource\\Gui/Flow/HUD/States/BrnRaceMainHudState.cpp",
                    1855);
                CgsDev::Assert::EndAssert();
            }
            break;
        }

        // GATED, unlike the freeburn state's unconditional call (@0x824819E4..0x824819F4).
        if (meInternalState != E_RACEINTERNALSTATE_IDLE)
            UpdatePermenant();

        // The pumps read without consuming; the state clears its in-queue at frame end
        // (`lwz r3, 0x18(r29)` == mpInGuiEventQueue, @0x824819F8).
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue != 0)
            lpInQueue->Clear();

        CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miHudStateUpdate);
    }

    // =======================================================================
    //  UpdateLoading  @ 0x8247A410
    // =======================================================================
    // Four statements, and the console asserts NOTHING here (unlike the freeburn twin at
    // BrnFBurnMainHudState.cpp:674, which does assert its cache) -- UpdateSetupState has
    // already refused to advance until mpCache is non-null, so this phase cannot run without
    // one. Kept assert-free to match.
    bool RaceMainHudState::UpdateLoading()
    {
        // @0x8247A424..0x8247A440: r4 = &maResourcesToLoad (unk_82F25F88),
        // r5 = muNumResourcesToLoad (dword_82F25F84 == 21). Hex-Rays drops both operands and
        // prints a one-argument call; the asm is authoritative.
        if (!mpCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
            return false;

        if (mbSatNav)
            mSatNavComponent.LoadResources();   // @0x8247A45C (this + 0x6A0)

        // Mount the HUD apt movie at level 1. The console builds the 20-byte { 8, 18, 12,
        // name, 1 } record by hand (@0x8247A464..0x8247A4B0, channel 0x29) with the name
        // taken from off_82F27BE0[0]; that pointer is verified as "B5RaceHud" both by IDA's
        // own operand comment here and by an image read of 0x82F27BE0. Posted through
        // StateInterface::PlayAptMovie rather than a hand-rolled record for the reason
        // BrnFBurnMainHudState.cpp:684 documents: the record's name field is an 8-byte
        // pointer on x64, so a hardcoded 20-byte post truncates the trailing level number.
        //
        // ⚠️ SEAM: the freeburn state mounts the SAME movie at the SAME level. The HUD flow
        // must have left FBURN_MAIN before RACE_MAIN gets here or the level-1 mount collides.
        mpStateInterface->PlayAptMovie("B5RaceHud", 1);

        SetExpectedAptComponentList();   // @0x8247A4B8 -- installs the "EventHud_Animator" hash
        return true;
    }

    // =======================================================================
    //  UpdateWFInit  @ 0x82480200
    // =======================================================================
    bool RaceMainHudState::UpdateWFInit()
    {
        // THE gate. One hash in the list ("EventHud_Animator"), so this really does wait.
        if (!mpCache->AreAllAptComponentsInitialised(E_GUIFLOW_HUD))
            return false;

        if (mbPaybackComponent)
            mPaybackComponent.Initialize(mpCache);                       // @0x82480244

        // The sat-nav animator is parked visible/invisible UNCONDITIONALLY -- the flag only
        // picks which label, it does not gate the call (@0x82480248..0x8248027C).
        mSatNavAnimationComponent.AddOutputAptViewState(
            "apt_Transition", mbSatNav ? "visible" : "invisible", false);

        if (mbPlayerPositionTable)
            mPlayerPositionTable.SetupGameMode();                        // @0x82480290

        if (mbOnlineTimeoutTimer && mpCache->IsOnlineTimeoutPending())   // @0x824802A8 (+0x13B5C)
            mOnlineTimeoutTimer.Show();

        if (mbFreeburnChallengeButtonStart)
        {
            // @0x824802D0..0x82480320. Hex-Rays renders GetFreeburnChallengeManager with no
            // `this`; the asm passes mpCache in r3. The three reads off the returned manager
            // are `lwz r10, 4(r3)` (meInternalState, tested against the {2,3,4} set) and
            // `lbz r11, 0x18(r3)` (mbIsLocalHost, tested == 1). The START button shows only
            // when the manager is active, NOT already running, and this machine is the host.
            const FreeburnChallengeManager* lpManager = mpCache->GetFreeburnChallengeManager();
            if (lpManager->IsActive() && !lpManager->IsRunning() && lpManager->IsLocalHost())
                mChallengeComponent.Show();
        }

        if (mbFreeburnChallengeSelector)
        {
            // @0x82480338 `stw r3, 0x78FC(r31)` -- +0x78FC is mChallengeSelectorComponent
            // (+0x67A0) + 0x115C, i.e. ChallengeSelector::mpChallengeList. The console
            // inlined SetChallengeList; restored here as the real call.
            mChallengeSelectorComponent.SetChallengeList(mpCache->GetFreeburnChallengeList());
        }

        if (mbFreeburnChallengeTicker)
        {
            // @0x8248034C..0x82480398 -- same manager state word, three-way.
            const FreeburnChallengeManager* lpManager = mpCache->GetFreeburnChallengeManager();
            if (lpManager->IsActive())
                StartFreeburnChallengeTicker();
            else if (lpManager->IsNotActive())
                StartFreeburnChallengeNotActiveTicker();
        }

        if (mbEventInfo)
            SetupEventInfo();                                            // @0x824803AC

        // ---- THE REVEAL LADDER (@0x824803B0..0x82480448) --------------------------------
        // mpCache+0xA014 == mbEventPreparedForModeStart, read as a byte through the
        // materialised offset pair `ori r26, r10, 0xA014 ; lbzx r10, r11, r26`.
        const bool lbInEvent = mpCache->IsEventPreparedForModeStart();

        bool lbRevealNow  = false;
        bool lbImmediate  = false;
        if (!lbInEvent)
        {
            // Not in an event: the countdown is over before it started.
            meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_DONE;   // @0x824803D0
            if (mbPreRaceCountdownRenders)
                mEventCountdownIcon.SetState("invisible");                 // vslot +0xC @0x824803E8
            lbRevealNow = true;
            lbImmediate = true;
        }
        else if (!mbPreRaceCountdown)
        {
            // In an event with no countdown widget: tell the view the countdown is finished
            // ({ 1, 236, 12 }, 16 bytes, channel 0x28 @0x82480404..0x82480428) and transition
            // the HUD in rather than snapping it on.
            //
            // FLAG (console-uninitialised byte): the console writes only the three header
            // words of that record -- its flag byte at +12 is left holding whatever the frame
            // had. Sent as 0 here; the record's consumer reads the id, not the flag.
            PostCommand16<236>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);
            lbRevealNow = true;
            lbImmediate = false;
        }
        else if (mpCache->IsOnlineStartInProgress())                       // @0x82480434 (+0x4B4C)
        {
            lbRevealNow = true;
            lbImmediate = true;
        }
        // else: in an event, countdown armed, offline start -- NO reveal here. The HUD waits
        // for UpdateEventCountdown's "GO" arm to call RevealHud(false).

        // [hud-reveal] RACE_MAIN. NOT X360 -- the PC-side twin of the freeburn state's
        // engine-state diagnostic (BrnFBurnMainHudState.cpp:743). This ladder has four
        // outcomes and three of them look identical from outside (a HUD that is simply not
        // there yet), so print which arm was taken. The fourth -- "waiting for GO" -- is the
        // one that legitimately leaves the screen bare, and the one worth telling apart from
        // an apt-init hang before anyone starts bisecting a black screen.
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[hud-reveal] RACE_MAIN UpdateWFInit inEvent=" << (lbInEvent ? 1 : 0)
                << " preRaceCountdown=" << (mbPreRaceCountdown ? 1 : 0)
                << " onlineStart=" << (mpCache->IsOnlineStartInProgress() ? 1 : 0)
                << (lbRevealNow
                        ? (lbImmediate ? " -> RevealHud(IMMEDIATE)\n" : " -> RevealHud(TRANSIN)\n")
                        : " -> DEFERRED, waiting for the countdown GO\n");
        }

        if (lbRevealNow)
            RevealHud(lbImmediate);                                        // @0x82480448

        if (mbRoadRuleComponent)
        {
            // The road-rule bring-up post, then the show-time latch, then the replay of any
            // rule the cache already has live (@0x82480460..0x82480518).
            GuiEvent327 lEvent;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT, 24);

            // @0x82480494..0x824804B4: `stb r11, 0x646C(r31)` -- +0x646C is
            // mRoadRuleComponent (+0x5F60) + 0x50C == RoadRuleComponent::mbInShowTime, with
            // r11 == 1 only for game modes 2 (offline showtime) and 16 (online showtime).
            // The console inlined SetIfInShowTime; restored here as the real call.
            const s32 liGameMode = mpCache->GetGameMode();
            mRoadRuleComponent.SetIfInShowTime(liGameMode == 2 || liGameMode == 16);

            // The replay sweep. The console runs it as a do/while with the bound assert
            // INSIDE the loop and the `< 2` re-test after it (@0x824804CC..0x82480518), so
            // index 2 is reached, asserted on, and then rejected -- an off-by-one the retail
            // build ships. Reproduced with the assert on the post-increment value, which is
            // what the console tests (`cmpwi r30, 2 ; ble` -> assert when > 2 is false...
            // i.e. it fires only if the index ever exceeded 2, which it cannot here).
            for (s32 leEnumIndex = 0; leEnumIndex < 2; ++leEnumIndex)
            {
                if (mpCache->IsRoadRuleActive(leEnumIndex))
                {
                    mRoadRuleComponent.HandleRoadRuleBegin(
                        static_cast<BrnStreetData::ScoreType>(leEnumIndex));
                }
                CGS_ASSERT(leEnumIndex + 1 <= 2, "leEnumIndex <= E_SCORE_TYPE_COUNT");   // BrnChallengeData.h:56
            }
        }

        // @0x8248051C..0x8248054C: assert the cache, then store the gameplay-HUD gate byte
        // through it REGARDLESS (the assert is non-gating on console).
        CGS_ASSERT(mpCache != 0, "mpCache");                               // cpp:2819
        mpCache->SetGameplayHudActive(true);                               // stb 1 @cache+0x407C

        // @0x82480550..0x82480590: in an event (byte == 1) AND mode 4 (E_MODE_PURSUIT) ->
        // post { 1, 433, 12 } on channel 0x28. Same console-uninitialised flag byte as the
        // 236 post above.
        if (mpCache->IsEventPreparedForModeStart() && mpCache->GetGameMode() == 4)
            PostCommand16<433>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        if (mbFriendsList)
        {
            mFriendsList.SetGuiCachePointer(mpCache);                      // @0x824805AC
            if (mpCache->IsFriendsListChangePending())                     // @0x824805BC (+0xB86D, == 1)
                mFriendsListChangeIcon.ShowNow();
            mFriendsList.AttemptStateRestore();                            // @0x824805D4
        }

        if (mbDistrictMarker)
        {
            // @0x824805E4..0x824805EC: `lbz r11, 0x4B4C(cache) ; stb r11, 0x1062(r31)` --
            // +0x1062 is mDistrictMarker (+0xFFC) + 0x66 == DistrictMarkerComponent::mbOnline.
            // The console inlined SetOnline; restored here as the real call.
            mDistrictMarker.SetOnline(mpCache->IsOnlineStartInProgress());
        }

        if (mbPaybackComponent)
        {
            // @0x824805FC..0x82480620. Gate byte first, then the two argument words; a type
            // word of 3 is skipped outright (that arm never calls).
            if (mpCache->IsPaybackAvailable())
            {
                const s32 liPaybackType = mpCache->GetPaybackAvailableType();
                if (liPaybackType != 3)
                {
                    mPaybackComponent.ShowAvailableInstantly(
                        static_cast<BrnNetwork::EPaybackType>(liPaybackType),
                        static_cast<::EActiveRaceCarIndex>(mpCache->GetPaybackVictimRaceCarIndex()));
                }
            }
        }

        // @0x82480624..0x82480640 -- clear the showtime bounce-boost prompt. unk_820046A7 is
        // the shared empty string (image read at 0x820046A7 == ""), and both glyph arguments
        // are 15 == FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE.
        mbBounceBoostPromptVisible = false;
        mShowtimeBounceBoostButton.SetItem("",
                                           FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                           FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                           false);

        if (mbCompass)
        {
            // @0x82480658..0x82480690. The "lpGuiCache" assert at BrnCompassComponent.h:208
            // belongs to the INLINED CompassComponent::SetGuiCachePointer, not to this
            // function -- its file/line argument names the compass header. Restored as the
            // real call (the assert travels with it).
            mCompass.SetGuiCachePointer(mpCache);
            mCompass.SetVisibility(true, false);
        }

        if (mbFreeburnChallengeSelector)
        {
            // @0x824806A0..0x824806C8. Same shape: the "lpGuiCache" assert at
            // BrnChallengeSelector.h:277 is the inlined ChallengeSelector::
            // SetGuiCachePointer's own, and +0x7900 is that component's mpGuiCache (+0x1160).
            mChallengeSelectorComponent.SetGuiCachePointer(mpCache);
        }

        return true;
    }

    // =======================================================================
    //  RevealHud  @ 0x8247A4E0
    // =======================================================================
    // One-shot on mbHudVisible. The parameter is the DWARF's `lbReveal`, but every use is
    // "snap rather than transition", so it is spelled lbImmediate at the definition: it
    // picks "visible" over "transin" and nothing else.
    void RaceMainHudState::RevealHud(bool lbImmediate)
    {
        if (mbHudVisible)                                  // @0x8247A4F0 (+0x5F54)
            return;
        mbHudVisible = true;

        // THE ENGINE GATE -- the same cache word (+0x4B20) the freeburn state documents at
        // BrnFBurnMainHudState.cpp:735. 0 == E_ENGINE_OFF, 1 == E_ENGINE_ON. With the engine
        // off the whole compose block is skipped and the HUD stays on its invisible frame;
        // note that mbHudVisible has ALREADY been latched by then, so this state never
        // re-tries the compose. That is the console's behaviour, not an oversight to fix.
        if (mpCache->GetPlayerEngineState() == 1)
        {
            const char* const lpcViewState = lbImmediate ? "visible" : "transin";

            // BOTH halves of the "EventHud_Animator" pair: the apt view-state write the
            // movie's ActionScript polls, and the FLAPT goto-and-play.
            mGeneralTransitionComponentApt.AddOutputAptViewState(
                "apt_Transition", lpcViewState, false);                     // @0x8247A548
            mGeneralTransitionComponentFlapt.Run(lpcViewState);             // @0x8247A554

            if (mbBoostBar)
            {
                // @0x8247A564..0x8247A570 -- a single show byte, wrapped onto channel 41.
                GuiEventShowHideBoostBar lShowBoostBar;
                lShowBoostBar.maData[0] = 1;
                mpStateInterface->OutputViewState(lShowBoostBar);
            }

            if (mbSatNav)
            {
                // @0x8247A580..0x8247A5B8. The 12-byte payload is { 1, flt_82001CC0, 1 }:
                // map type 1 (E_MAPTYPE_GPS), fade time 0.0f -- flt_82001CC0 read out of the
                // image, NOT assumed -- and show 1. One record, three consumers: the view
                // channel, the internal-state mirror, and the component itself.
                GuiEventShowHideSatNav lShowSatNav;
                lShowSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS, true, 0.0f);
                mpStateInterface->OutputViewState(lShowSatNav);
                mpStateInterface->OutputInternalState(lShowSatNav);
                mSatNavComponent.RecvEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lShowSatNav), 213);
            }
        }

        // OUTSIDE the engine gate (@0x8247A5BC) -- { 1, 215, 12, 1 }, 16 bytes, channel 0x29.
        // ⭐ [A3 SEAM] +0x15B. This is the ONE consumer of the flag the E1 header carried
        // twice (mbTemporaryReplayIndicator / mbAboveCarIcons at +0x15B/+0x15C, where the
        // console has exactly 25 flag bytes and OnEnter @0x82478EF8 zeroes only
        // +0x150..+0x168). Agent A3 owns that deletion; at the time this partfile was
        // written A3's in-flight edit had already dropped mbTemporaryReplayIndicator from the
        // constructor's initialiser list and kept mbAboveCarIcons, which is also what the s2
        // scout dossier concluded from the flag's mode profile (ON for showtime / road rage /
        // every online mode, OFF for race / face-off / pursuit / burning route / eliminator /
        // stunt attack / marked man / traffic attack). If A3 lands the other name instead,
        // this identifier is the single token that has to change.
        if (mbAboveCarIcons)
            PostCommand16<215>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
    }

    // =======================================================================
    //  UpdatePermenant  @ 0x824806E8
    // =======================================================================
    // The SECOND pass over the same in-queue each frame (Update's own phase bodies made the
    // first). Runs in every phase except IDLE.
    void RaceMainHudState::UpdatePermenant()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
            const u8*  lpu8Payload = reinterpret_cast<const u8*>(lpEvent);

            // The console's dispatch is a compare ladder (0x23D / 0x123 / 0x15 / 0x94)
            // followed by an 8-case jumptable over `id - 0x23E` (574..581); flattened here.
            switch (liEventId)
            {
            case 21:
                ProcessAptEvents(lpEvent);                                  // @0x824807DC
                break;

            case 148:
                // @0x8248079C `lbz r11, 0(r28)` -- a BYTE test, not a word. Payload 0 audio-
                // cues and then pauses; anything else is ignored entirely.
                if (lpu8Payload[0] == 0)
                {
                    // X360 @0x824807C0: OutputGuiEvent<GuiAudioEvent> with { 2, 0, -1 } and a
                    // zeroed qword tail. FLAG deferred: the GuiAudioEvent record's field
                    // layout is not homed (BrnGuiDemangledEventTypes.h carries only an opaque
                    // 24-byte placeholder), and BrnFBurnMainHudState.cpp:1252 already parked
                    // the identical post for the identical reason. The PAUSE below is the
                    // load-bearing half and is NOT deferred.
                    // DELETE-WHEN: BrnGui::GuiAudioEvent gets real fields.
                    SendStateEvent("PAUSE");
                }
                break;

            case 291:
            case 320:
                SendStateEvent("PAUSE");                                    // @0x824807C4 (shared arm)
                break;

            case 377:
                // @0x824807F4 `lwz r11, 0(r28)` -- a WORD here (contrast case 148's byte).
                if (lpiPayload[0] == 0 || lpiPayload[0] == 2)
                {
                    if (mbFriendsList)
                        mFriendsList.SaveCurrentState();                    // @0x82480818
                    // @0x8248081C..0x82480898: switch on meGameModeType - 7, 11 cases; the
                    // jumptable's cases 0/5/7/10 (== modes 7, 12, 14, 17) take START_CSTNT,
                    // every other mode takes START_CRASH.
                    switch (mpCache->GetGameMode())
                    {
                    case 7:    // E_MODE_STUNT_ATTACK
                    case 12:
                    case 14:
                    case 17:
                        // ⚠️ A stunt-run crash hands off to CRASHEDSTNT, which is still a
                        // stub in BrnHudStatesLinkStubs.cpp -- so on the stunt-race bring-up
                        // path this transition currently lands nowhere. The console call is
                        // kept EXACTLY as-is: the hole is in the destination state, not here,
                        // and swapping in START_CRASH to "make it work" would hide it.
                        SendStateEvent("START_CSTNT");
                        break;
                    default:
                        SendStateEvent("START_CRASH");
                        break;
                    }
                }
                break;

            case 573:
                // @0x8248089C `lwz r11, 8(r28)` -- the selector action word, 4 cases.
                // Actions 0, 1 and 3 do nothing; action 2 shares the 576 arm; anything else
                // asserts with the value streamed in hex.
                if (lpiPayload[2] == KI_SELECTOR_ACTION_START_TICKER)
                {
                    if (mbFreeburnChallengeTicker)
                        StartFreeburnChallengeTicker();
                }
                else if (lpiPayload[2] < 0 || lpiPayload[2] > 3)
                {
                    char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "Unknown freeburn challenge selector action ";
                    lStrStream << lpiPayload[2];   // console formats it "0x%X" (off_82F31944)
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        lacMessage,
                        "..\\..\\..\\GameSource\\Gui/Flow/HUD/States/BrnRaceMainHudState.cpp",
                        3702);
                    CgsDev::Assert::EndAssert();
                }
                break;

            case 574:
                CGS_ASSERT(lpEvent != 0, "lpChallengeEvent");               // cpp:3620 (li r5, 0xE24)
                // @0x824809D4 `lbz r11, 8(r28)` -- a BYTE at +8 here, where case 573 read a
                // word at the same offset. Only a ZERO byte falls into the 576 arm.
                if (lpu8Payload[8] == 0 && mbFreeburnChallengeTicker)
                    StartFreeburnChallengeTicker();
                break;

            case 576:
                if (mbFreeburnChallengeTicker)                              // @0x824809E0
                    StartFreeburnChallengeTicker();
                break;

            case 578:
            case 579:
                // @0x824809F8 (case 578) and @0x82480A68 (case 579) build the SAME
                // ticker-clear record -- byte pair { 0, 1 } -- at two different stack slots
                // and share one post (@0x82480A88). Hex-Rays renders the pair as two
                // differently-shaped writes into one __int64 local; the asm shows two
                // identical `stb r26(==0) ; stb 1` pairs.
                // ⭐ 2026-08-27 verify round: the console queues the FULL 16-byte
                // {2,536,12}+{0,1} wire on CHANNEL 40 (`li r5, 0x28 ; li r6, 0x10`), NOT the
                // raw 2-byte GuiEventTickerClearMessages through OutputGuiEvent (which
                // direct-passes and would land 2 bytes on channel 536 -- a record the ticker
                // consumer never sees, so clears would silently drop and challenge lines
                // would accumulate). TU-local wire struct per the partfile precedent
                // (wS4's GuiTickerClearWire536 / BrnRaceMainHudState.cpp's GuiEvent536).
                if (mbFreeburnChallengeTicker)                              // both gate on +0x167
                {
                    struct GuiTickerClearWire536 : public CgsGui::GuiEvent<536>
                    {
                        u8 mbForceFadeOut;            // +0x0C == 0
                        u8 mbDeleteChallengeMessages; // +0x0D == 1
                        u8 mau8Pad[2];
                        GuiTickerClearWire536()
                            : CgsGui::GuiEvent<536>(2, 12)
                            , mbForceFadeOut(0), mbDeleteChallengeMessages(1)
                        { mau8Pad[0] = mau8Pad[1] = 0; }
                    };
                    GuiTickerClearWire536 lClear;
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lClear), 40, 16);
                }
                break;

            case 581:
                // @0x82480A18..0x82480A64 -- the manager's own state has to be active too.
                if (mbFreeburnChallengeTicker)
                {
                    if (mpCache->GetFreeburnChallengeManager()->IsActive())
                        StartFreeburnChallengeTicker();
                }
                break;

            default:
                break;
            }
        }

        // ---- the pre-event overlay expiry (@0x82480AAC..0x82480B30) ---------------------
        // Retail-dead as shipped: UpdateSetupState clears mbPreEventOverlay on EVERY mode
        // path, so this arm only runs under the debug component-override table. Transcribed
        // anyway -- it is the only recovered consumer of KAPC_PRE_EVENT_OVERLAYS.
        if (mbPreEventOverlay && mbOverlayInProgress
            && mpCache->GetTime() > mfOverlayRemovalTime)
        {
            mbOverlayInProgress = false;                                    // @0x82480AE0
            CGS_ASSERT(KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed] != 0,
                       "KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed]");   // cpp:3737 (li r5, 0xE99)

            // FLAG (two homes, one type): the CANONICAL GuiOverlayWaitFinishRequest
            // (BrnGuiOverlaysDirector.h:29) carries the Construct that compresses the overlay
            // name into its CgsID but has no GetEventType(); the catalogue copy in
            // BrnGuiDemangledEventTypes.h -- which this TU already includes for the ShowHide
            // and ticker records, and which is MUTUALLY EXCLUSIVE with the director header --
            // carries GetEventType() == 188 over an opaque 8-byte body. So the id is built
            // here and copied in; the eight queued bytes are identical either way.
            // DELETE-WHEN: the two homes are reconciled into one type.
            const CgsID lOverlayId =
                CgsIDCompress(KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed]);
            GuiOverlayWaitFinishRequest lRequest;
            std::memcpy(lRequest.maData, &lOverlayId, sizeof(lRequest.maData));
            mpStateInterface->OutputGuiEvent(lRequest);
        }

        if (mbFriendsList)
        {
            if (mpCache != 0)
            {
                // @0x82480B40..0x82480B70: `stb r11, 0x25F1(r29)` -- +0x25F1 is mFriendsList
                // (+0x1D58) + 0x899 == FriendsListComponent::mabEntryFlags[1], set for game
                // modes 15 and 16 only. No accessor exists for that byte in the ledger, so
                // BrnFriendsList.h grants this state friendship (the same pattern
                // BrnDistrictMarker.h and BrnRoadRuleComponent.h use for the freeburn state).
                // FLAG: the byte's semantics are unrecovered -- it is set, not interpreted.
                const s32 liGameMode = mpCache->GetGameMode();
                mFriendsList.mabEntryFlags[1] =
                    static_cast<u8>((liGameMode == 15 || liGameMode == 16) ? 1 : 0);
            }
            mFriendsList.UpdateAptVariables();                              // @0x82480B78
        }
    }
}
