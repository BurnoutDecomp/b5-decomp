// ===================================================================================
// BrnGui::CrashedStuntHudState  -- the CRASHEDSTNT HUD flow state (exit-arm slice)
//   class:BrnGui::CrashedStuntHudState
//
//   OnEnter          @ 0x82476318   (PARTIAL -- see the OnEnter banner)
//   OnLeave          @ 0x8247DF68   (PARTIAL -- see the OnLeave banner)
//   Update           @ 0x82481CF0   (PARTIAL -- see the Update banner)
//   UpdatePermenant  @ 0x82476A00   (COMPLETE -- all five console arms)
//   GetResourcesToLoad @ 0x82508510 (inline in the header)
// Reconstructed store-for-store from the X360 asm.
//
// ===================================================================================
// WHY THE RACE UI NEVER CAME BACK AFTER A STUNT-RUN CRASH
// ===================================================================================
// BrnGame.log showed the race HUD vanish on the same frame as
//     "[HudFlow] CrashedStuntHudState::OnEnter -- un-reconstructed state (FLAG)."
// and the mode kept ticking state 2 (IN_PROGRESS) afterwards. So the game was fine; the HUD
// FLOW was parked. The producer side was already correct: RaceMainHudState::UpdatePermenant's
// mode-7 crash arm sends "START_CSTNT" (BrnRaceMainHudState_wS2.cpp:575), and the lua FSM
// (BRNEVENTFSM.BUNDLE) routes it to state 8 CRASHEDSTNT. What was missing was the state itself
// -- BrnHudStatesLinkStubs.cpp's log-and-return lifecycle registered for nothing, observed
// nothing and therefore could never send the event that leaves.
//
// This is the SAME shape as the freeburn crash bug closed earlier today in
// BrnCrashedHudState.cpp: "the END_CRASH arm was never missing, the STATE was". The stunt-side
// arm is the exact mirror, 0x360 bytes earlier at 0x82476A00:
//
//     if ( mEvent == 377 && (*lpEvent == 1 || *lpEvent == 3) )
//         CgsGui::State::SendStateEvent(this, aEndCstnt);      // aEndCstnt = "END_CSTNT"
//
// and the lua closes the loop exactly:
//
//     function NextState_8CRASHEDSTNT( eventId )
//         if eventId == "PAUSE" then          Transition_8CRASHEDSTNT_9PAUSED()
//         elseif eventId == "END_CSTNT" then  Transition_8CRASHEDSTNT_2RACE_MAIN()
//         end
//     end
//
// -- state 2 is RACE_MAIN, i.e. the race UI (score/timer) coming back. The set of events this
// UpdatePermenant can send is {PAUSE, END_CSTNT}, and that is EXACTLY the set the lua state
// handles: the console function and the console FSM close over each other with nothing left
// over, which is the strongest available check that no arm is missing.
// ===================================================================================
#include "GameSource/Gui/Flow/HUD/States/BrnCrashedStuntHudState.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // Register/UnRegisterForEvents
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // the state in-queue
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // CgsDev::Log (witnesses)

namespace BrnGui
{
namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

    // ARTIST event-64 payload (the per-frame cache hand-off). The X360 reads the first payload
    // WORD and stores it straight into mpCache; on the x64 host that word is not wide enough to
    // hold the pointer, so the record is read BY NAME through the same file-local shape the
    // sibling states use (BrnRaceMainHudState.cpp:50, BrnFBurnMainHudState.cpp:55) rather than
    // by casting the guest-width payload word.
    struct GuiEventCache : public CgsModule::Event
    {
        BrnGui::GuiCache* mpGuiCache;
    };
}

// @0x8205B17C -- the 12 event ids OnEnter registers and OnLeave unregisters. The IDA export set
// has no data symbols, so the words were read out of the XEX image rather than guessed. Two
// independent checks pin the width at 12: both call sites pass `li r5, 12`, and the word that
// immediately follows the run in .rdata (0x8205B1AC) is itself 12 -- the count the DWARF records
// as miNumEventsObserved -- with ASCII string data ("StuntScore...") starting right after it.
//
// These 12 are exactly the first 12 entries of CrashedHudState::maiEventToObserve, in the same
// order; the freeburn state then continues for another 9 (325, 574, 576, 578, 581, 573, 579,
// 547, 309) that the stunt state does not observe. Two independently-read tables agreeing on
// their common prefix is a further check that the address decode is right.
const s32 CrashedStuntHudState::maiEventToObserve[12] =
{
      5,   6,   7,  21,  64, 377, 154, 156, 148, 320, 291, 140,
};
const s32 CrashedStuntHudState::miNumEventsObserved = 12;

// =======================================================================
//  The static .rdata resource table @0x82F26488 (count @0x82F264A8)
// =======================================================================
// Read straight out of the XEX image; the four 8-byte tuples end exactly where the count word
// begins, and that word reads 4, so the table's extent is self-confirming. Each id is named via
// off_82F278E0[id] from the same image -- the name table the FBurnMainHudState 42-entry and
// RaceMainHudState 21-entry recoveries used (re-checked here against RaceMain's published names:
// 192 -> "B5RaceHud", 32 -> "Timer", 24 -> "B5CompassComponent" all reproduce).
//
// All four are type 7 == E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE. Note the first entry names this
// state's own movie, B5CrashedStuntHud -- the crash screen the stub could never load.
const CgsGui::sResourceTuple CrashedStuntHudState::maResourcesToLoad[] =
{
    { 194u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CrashedStuntHud
    {  38u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CrashedHudMessages
    {  63u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5HelperComponents
    {  61u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ControllerButtons
};
const u32 CrashedStuntHudState::muNumResourcesToLoad = 4;

    // ===============================================================================
    //  OnEnter  @ 0x82476318   -- PARTIAL, and saying so is the point
    // ===============================================================================
    // The console's OnEnter: zero the two phase words and the cache pointer, REGISTER FOR THE 12
    // EVENTS, end the replay static-layout message, take the FLAPT file, resolve "CrashHUD_mc"
    // into mCrashHudAnimator and reset its timeline, Construct+Prepare the InGameMessagesComponent
    // (wiring it to the GuiCache in-game-message queue at cache+16512), Construct the three
    // animators ("StuntScore_anim", "MultiplierScore_anim", "ScoreTally_cpt") and the two text
    // fields ("StuntScore_mc", "MultiplierScore_mc"), then set the two enable bytes and zero the
    // six score words.
    //
    // REPRODUCED HERE: every store that reaches a member this header models -- the two phase
    // words, mpCache, the RegisterForEvents call, both enable bytes, and all six score words.
    // DEFERRED: the replay static-layout leg, the FLAPT/"CrashHUD_mc" leg, and the six
    // component Construct/Prepare calls. Each of those reaches a member this header carries as
    // an opaque guest span, and fabricating storage for them is what the PHASE NOTE forbids.
    // The state therefore still DRAWS NOTHING. What it now does is OBSERVE and LEAVE.
    //
    // The registration is the leg that had to come first: a state that observes nothing receives
    // nothing, so without it the exit arm below would be dead code. The freeburn wave paid for
    // that lesson twice already (the pause wave shipped a faithful exit arm with no
    // RegisterForEvents and it never fired).
    //
    // ORDER NOTE: the console does the three zero stores BEFORE RegisterForEvents and the enable
    // bytes + six score words AFTER the component legs. That order is preserved as written, with
    // the deferred legs marked in place, so the gap is visible rather than closed over.
    void CrashedStuntHudState::OnEnter()
    {
        meInternalState = E_CRASHINTERNALSTATE_LOADING;   // this+0x38 = 0
        meRunningState  = E_CRASHRUNNINGSTATE_NONE;       // this+0x3C = 0
        mpCache         = 0;                              // this+0x40 = 0

        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // FLAG deferred: GuiModuleStaticLayout::EndMessage, FlaptManager::GetFile +
        // FindChildMovieClip("CrashHUD_mc") -> mCrashHudAnimator + ResetTimeline, and the six
        // component Construct/Prepare calls (see the banner).

        mbHudMessages = true;    // this+0x44 = 1
        mbBoostBar    = true;    // this+0x45 = 1

        miStartScore        = 0;   // this+0x890
        miStartMultiplier   = 0;   // this+0x88C
        miFinishScore       = 0;   // this+0x898
        miFinishMultiplier  = 0;   // this+0x894
        miCurrentScore      = 0;   // this+0x89C
        miCurrentMultiplier = 0;   // this+0x8A0

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            // [cstnt-hud] witness. NOT X360. Proves the FSM really enters CRASHEDSTNT and that
            // the 377 route is armed -- the two things no static read could settle, and the two
            // the old stub's log could only report as absent.
            static bool sbLoggedEnter = false;
            if (!sbLoggedEnter)
            {
                sbLoggedEnter = true;
                *CgsDev::Log::gpDebugPrint
                    << "[cstnt-hud] CrashedStuntHudState::OnEnter -- registered "
                    << miNumEventsObserved << " events (377 included)\n";
            }
        }
    }

    // ===============================================================================
    //  OnLeave  @ 0x8247DF68   -- PARTIAL (the mirror of the above)
    // ===============================================================================
    // Console: assert mpMovieClipInst then ResetTimeline(mCrashHudAnimator) -> UnRegisterForEvents
    // (table, 12) -> post an id-18 record {8, 18, 12} + {1, &unk_820046A7} on channel 41 (size 20)
    // -> clear three in-game-message queue bytes off the GuiCache at cache+16512.
    //
    // ONLY THE UnRegisterForEvents LEG IS REPRODUCED, and the reason for each omission is
    // specific rather than blanket:
    //   * the ResetTimeline leg is deferred AS A MATCHED PAIR with OnEnter's FindChildMovieClip
    //     leg. Its guard is `CGS_ASSERT(mpMovieClipInst, ...)` -- and because the only producer
    //     of mCrashHudAnimator is deferred, transcribing this leg would fire that assert on
    //     EVERY leave. An assert is not a guard: the honest move is to defer the consumer with
    //     its producer, not to keep the call and soften the assert.
    //   * the channel-41 AddEvent leg needs the StateInterface access-pointer queue this phase
    //     does not model (the same boundary CrashedHudState.cpp records for its 578/579 arms).
    //   * the cache in-game-message clear reaches three un-named bytes inside cache+16512; there
    //     is no named accessor for them yet, and reaching them by raw offset is exactly the
    //     invention the faithfulness gate exists to catch.
    void CrashedStuntHudState::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // ===============================================================================
    //  Update  @ 0x82481CF0   -- PARTIAL: the phase machine is deferred, the tail is not
    // ===============================================================================
    // Console: a five-way switch on meInternalState that falls THROUGH its phases (LOADING ->
    // WF_INIT -> SETUPSTATE -> RUNNING, each advancing on its body returning true; IDLE is a
    // no-op; anything else asserts "Should never call update in the following state" at
    // BrnCrashedStuntHudState.cpp:201), and then ALWAYS the two-line tail:
    //     UpdatePermenant();  mpInGuiEventQueue->Clear();
    //
    // The four phase bodies (UpdateLoading @0x8247D918, UpdateWFInit @0x82475890,
    // UpdateSetupState @0x8247D9E0, UpdateRunning @0x82481650) and the tally sub-machine
    // (UpdateCrashRunningState @0x8247DC10) are NOT reconstructed: they drive the six components
    // OnEnter does not Construct here, so calling them would be calling into storage that does
    // not exist. They are deferred AS A SET, and meInternalState is left where OnEnter's own
    // store put it (LOADING). Nothing in this file reads it, so no arm silently depends on the
    // deferral -- and the default-arm assert is deferred with the dispatch it belongs to rather
    // than being re-sited onto a switch this file no longer performs.
    //
    // The tail is the console's, verbatim in shape, and it is unconditional there -- UpdatePermenant
    // runs in EVERY phase including LOADING. So reproducing the tail alone is not a shortcut past
    // the phase machine: it is the one part of Update whose behaviour does not depend on it. That
    // is precisely why the exit arm works while the crash screen itself is still dark.
    void CrashedStuntHudState::Update()
    {
        // FLAG deferred: the five-phase dispatch and its default-arm assert (see the banner).

        UpdatePermenant();

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue != 0)
            lpInQueue->Clear();
    }

    // ===============================================================================
    //  UpdatePermenant  @ 0x82476A00   -- COMPLETE: all five console arms
    // ===============================================================================
    // Drains the state in-queue every frame. The console dispatches FIVE ids and this
    // reconstruction lands all five -- there is nothing deferred in this function, because every
    // arm reaches either a base-class service or mpCache, both of which this header models.
    //
    //   377 -> payload word 1|3 : SendStateEvent("END_CSTNT")   <-- THE ARM THIS WAVE IS FOR
    //   320 -> SendStateEvent("PAUSE")                (unconditional)
    //   291 -> SendStateEvent("PAUSE")                (unconditional)
    //   148 -> payload word == 0 : SendStateEvent("PAUSE")
    //    64 -> assert the payload, then mpCache = payload[0]
    //
    //   Ids observed but NOT dispatched (they fall through the console's `default`):
    //   5, 6, 7, 21, 154, 156, 140. Registered, deliberately ignored here -- the console has no
    //   assert on its default path in this function, so neither does this.
    //
    // The dispatch order below follows the asm's own compare chain at 0x82476A68: `cmpwi r3,
    // 0x123` splits the range first (291), then 0x40 (64) and 0x94 (148) below it, then 0x140
    // (320) and 0x179 (377) above it. A plain switch reproduces that selection exactly.
    void CrashedStuntHudState::UpdatePermenant()
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
            switch (liEventId)
            {
            case 377:
                // 0x82476B38: `cmpwi r11, 1 / beq` + `cmpwi r11, 3 / bne` -> "END_CSTNT".
                // The producer (GameBridgeWorldToGui) posts 1 == E_CRASHBARSTATE_LEAVE_CRASHED on
                // the falling edge; 3 is the showtime-side spelling of the same leave. The lua
                // maps END_CSTNT back to state 2 RACE_MAIN -- the race UI returning.
                if (lpiPayload[0] == 1 || lpiPayload[0] == 3)
                {
                    if (CgsDev::Log::gpDebugPrint != 0)
                    {
                        // [cstnt-hud] witness. NOT X360.
                        *CgsDev::Log::gpDebugPrint
                            << "[cstnt-hud] CrashedStuntHudState: GUI 377 payload=" << lpiPayload[0]
                            << " -> SendStateEvent(\"END_CSTNT\")\n";
                    }
                    SendStateEvent("END_CSTNT");
                }
                // 0|2 (START_CRASHED) is a no-op here: the state is already crashed. The console
                // has no arm for it either -- its test is `== 1 || == 3` and nothing else.
                break;

            case 320:
            case 291:
                SendStateEvent("PAUSE");
                break;

            case 148:
                // 0x82476A84 `lwz r11, 0(r21)` -- a WORD here, matching CrashedHudState's reading
                // of the same id (PausedHudState reads it as a BYTE; that difference is the
                // console's, and each is kept as written).
                if (lpiPayload[0] == 0)
                    SendStateEvent("PAUSE");
                break;

            case 64:
                // The per-frame cache event: this is the ONLY producer of mpCache, which is why
                // the deferred phase bodies (all of which assert on it) could never have run
                // without this arm. Assert text transcribed exactly as the console has it --
                // including its reference to the SIBLING class, which is a copy-paste in the
                // original source: the string is "Invalid cache in CrashedHudState::Update" but
                // the file/line the console passes are BrnCrashedStuntHudState.cpp:571
                // (`li r5, 0x23B` at 0x82476B10). The message is the console's; it is not
                // corrected here.
                //
                // The X360 reads the payload WORD (`lwz r11, 0(r21)` / `stw r11, 0x40(r23)`);
                // this reads the same field BY NAME off the typed record, because a guest word
                // cannot carry a host pointer.
                {
                    GuiCache* lpCache = reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                    CGS_ASSERT(lpCache != 0, "Invalid cache in CrashedHudState::Update");
                    mpCache = lpCache;
                }
                break;

            default:
                // The seven registered-but-undispatched ids land here. The console has no assert
                // on its default path in this function, so neither does this -- adding one would
                // be an invented arm.
                break;
            }
        }
    }
}
