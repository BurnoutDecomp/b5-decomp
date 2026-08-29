// ===================================================================================
// BrnGui::CrashedHudState  -- the "CRASHED" HUD flow state (impact-time slice)
//   class:BrnGui::CrashedHudState
//
//   SetExpectedComponent   @ 0x82473780
//   EnterSteerWreckScreen  @ 0x82473868
//   EnterImpactTimeScreen  @ 0x824738C0
//   OnEnter                @ 0x82475DD0   (PARTIAL -- see the OnEnter banner)
//   OnLeave                @ 0x8247D308   (PARTIAL -- see the OnLeave banner)
//   Update                 @ 0x82481B88   (PARTIAL -- see the Update banner)
//   UpdatePermenant        @ 0x824812A0   (PARTIAL -- every arm enumerated below)
// Reconstructed store-for-store from the X360 asm.
//
// ===================================================================================
// ⭐⭐ WHY THE END_CRASH ARM IS HERE AND NOT IN BrnFBurnMainHudState
// ===================================================================================
// The tree carried a standing note (GameBridgeWorldToGui.cpp, BrnVehicleManager.cpp) saying the
// GUI-377 producer posts LEAVE_CRASHED but "there is NO END_CRASH arm anywhere in the tree" and
// that writing one would be "fabricating a console behaviour". Half right, and the half that was
// wrong sent the search to the wrong file.
//
// FBurnMainHudState::UpdatePermenant @0x824810F0 really does have only the 0|2 -> "START_CRASH"
// arm: the string "END_CRASH" does not occur in that function, and adding the mirror there WOULD
// have been an invented arm. But the console's END_CRASH arm exists -- it is 0x1B0 bytes further
// on, in THIS state, at 0x824812A0:
//
//     if ( mEvent == 377 && (*lpEvent == 1 || *lpEvent == 3) )
//         CgsGui::State::SendStateEvent(this, sEndCrashEvent);      // sEndCrashEvent = "END_CRASH"
//
// which is the coherent design: FBurnMain sends START_CRASH and is LEFT; CRASHED is entered and
// sends END_CRASH to leave ITSELF. A state leaves itself; its predecessor does not leave it for it.
// (PausedHudState carries both arms because it can be entered from either side -- that is why the
// pause wave found the pair there and read it as the whole story.)
//
// ⭐ And 377 really is routed here: maiEventToObserve below is read out of the image, and 377 is
// entry [5] of 21. What was missing was never the arm -- it was the state. Before this wave
// CrashedHudState declared no virtuals at all, so it never registered, never updated and never
// sent anything: a hollow shell the FSM could enter and never leave. That, not a missing arm in
// FBurnMain, is why the HUD did not come back after a crash.
//
// ===================================================================================
// ⭐⭐⭐ THE HUD BLACKOUT IS THIS STATE, AND IT IS NOW ON FILM (showtime-score wave, 2026-08-30)
// ===================================================================================
// A long-standing unattributed report -- "the HUD goes black for ~18-21 s and three waves could
// not reproduce it" -- has a named cause and a frame pair. scratch/flow_run/sthud_score2:
//
//   log 7261  [crash-hud] posting GUI 377 ... state=0 (START_CRASHED)
//   log 7262  [crash-hud] FBurnMainHudState received GUI 377, payload=0
//   log 7263  [crash-hud] SendStateEvent("START_CRASH")
//   log 7265  [crash-hud] CrashedHudState::OnEnter -- registered 21 events (377 included)
//
//   frame bb_006990  FBURN_MAIN: boost bar bottom-left, sat-nav minimap + "RIVER CITY" +
//                    "MILES DRIVEN : 0.0km" bottom-right.
//   frame bb_010020  CRASHED:    NOTHING. No boost bar, no sat-nav, no district panel, no
//                    odometer. Only the build-date/fps/memory debug overlay, which is not the
//                    game HUD.
//
// The mechanism is the FSM, not a renderer: BRNFBFSM's Transition_2FBURN_MAIN_4CRASHED runs
// FBurnMainHudState::OnLeave (which tears the freeburn HUD down) and then enters THIS state,
// which constructs nothing and drives nothing. The blackout lasts until 377's FALLING edge
// (payload 1) reaches UpdatePermenant below and sends END_CRASH -- i.e. exactly as long as the
// car stays in its crash, which is the tens-of-seconds order the original report described.
//
// ⛔ AND IT IS *NOT* THE SHOWTIME PATH, which is what earlier attempts were looking at. The
// GUI-377 producer suppresses itself for game modes 2/16 (GameBridgeWorldToGui.cpp's showtime
// term, console-faithful), so a showtime session NEVER enters CRASHED -- measured: zero
// [crash-hud] lines anywhere in the showtime segment of two runs, while the same runs print all
// four lines above the moment showtime ENDS and the still-crashing car posts 377 in free burn.
// The showtime HUD itself draws fine throughout (the Distance / Cars Crashed panels are on
// screen in scratch/flow_run/wm_meter2 frames bb_002970..bb_004410).
//
// ⇒ THE FIX IS THE REST OF THIS FILE, and OnEnter alone will not do it: the eleven components
// OnEnter builds are only ever DRIVEN by the five phase bodies (UpdateGetCache -> UpdateLoading
// -> UpdateWFInit -> UpdateSetupState -> UpdateRunning) that Update's switch dispatches, and
// UpdateSetupState is the one that pushes the apt view states and posts the enable events. A
// wave that lands OnEnter without the phase machine will construct twelve components that
// nothing ever shows.
// ===================================================================================
#include "GameSource/Gui/Flow/HUD/States/BrnCrashedHudState.h"

#include "GameShared/GameClasses/Containers/CgsHash.h"    // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // Register/UnRegisterForEvents
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // the state in-queue
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // CgsDev::Log (witnesses)

#include <cstring>   // std::strlen

namespace BrnGui
{
namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;
}

// @0x8205B070 -- the 21 event ids OnEnter registers. The IDA export set has no data symbols, so
// this address was decoded from OnEnter's own instruction bytes (`lis r11, ..@ha` @0x82475E6C =
// 0x3D608206, `addi r4, r11, ..@l` @0x82475E78 = 0x388BB070 -> 0x8205B070) and the words were read
// out of the image, not guessed. The `li r5, 0x15` between them is the count, 21.
const s32 CrashedHudState::maiEventToObserve[21] =
{
      5,   6,   7,  21,  64, 377, 154, 156, 148, 320, 291,
    140, 325, 574, 576, 578, 581, 573, 579, 547, 309,
};
const s32 CrashedHudState::miNumEventsObserved = 21;

// =======================================================================
//  The static .rdata resource table @0x82F263A0 (count @0x82F263C0)
// =======================================================================
// Read straight out of the XEX image; see the header for the two-instruction address decode and
// for why the table's extent is self-confirming (four 8-byte tuples ending exactly where the
// count word begins, and that word reads 4).
//
// Each id is named via off_82F278E0[id], the same name table the CrashedStuntHudState,
// FBurnMainHudState and RaceMainHudState recoveries used -- re-checked here by resolving the
// stunt state's OWN first entry through it: id 194 comes back "B5CrashedStuntHud", which is
// what that already-committed table says it is. All four entries are type 7 ==
// E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE.
//
// ⭐ The list is the CrashedStunt one with ONE entry different -- 193 B5CrashedHud in place of
// 194 B5CrashedStuntHud -- which is the shape you would expect of two crash screens that share
// their messages, helper components and button glyphs and differ only in their own movie. Two
// independently-decoded tables agreeing on three of four entries is a further check that the
// address decode is right.
const CgsGui::sResourceTuple CrashedHudState::maResourcesToLoad[] =
{
    { 193u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CrashedHud
    {  38u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CrashedHudMessages
    {  63u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5HelperComponents
    {  61u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ControllerButtons
};
const u32 CrashedHudState::muNumResourcesToLoad = 4;

    // @ 0x82473780 -- hash the component name and append it to the expected-apt-component id list.
    // DWARF declares this void; the X360 leaves the hash in r3 (the id it just stored). The
    // observable effect is the store into mauExpectedComponentIds[count] + count++.
    void CrashedHudState::SetExpectedComponent(const char* lpacComponentName)
    {
        CGS_ASSERT(muNumExpectedComponents < KU_MAX_INIT_COMPONENTS_NUM,
                   "No space for new expected component");

        // X360: inline strlen (char* walk to the NUL) then CalculateHash(name, len).
        const s32 liLength = static_cast<s32>(std::strlen(lpacComponentName));
        const u32 luHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpacComponentName), liLength);

        mauExpectedComponentIds[muNumExpectedComponents] = luHash;
        ++muNumExpectedComponents;
    }

    // @ 0x82473868 -- switch the impact-time apt page to "SteerWreck" and show the LTHUMB glyph.
    void CrashedHudState::EnterSteerWreckScreen()
    {
        ImpactTimePageChanger().AddOutputAptViewState("apt_Transition", "SteerWreck", false); // this+0x4B8
        ImpactTimeButton().SetButton(ButtonIconComponent::E_PADBUTTON_LTHUMB,
                                     ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);          // this+0x544, (13, 0)
    }

    // @ 0x824738C0 -- switch the impact-time apt page to "ImpactTime" and show the SELECT glyph.
    void CrashedHudState::EnterImpactTimeScreen()
    {
        ImpactTimePageChanger().AddOutputAptViewState("apt_Transition", "ImpactTime", false); // this+0x4B8
        ImpactTimeButton().SetButton(ButtonIconComponent::E_PADBUTTON_SELECT,
                                     ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);          // this+0x544, (4, 0)
    }

    // ===============================================================================
    //  OnEnter  @ 0x82475DD0   -- ⚠️ PARTIAL, AND SAYING SO IS THE POINT
    // ===============================================================================
    // The console's OnEnter is ~110 lines: it resets the phase word, grabs the FLAPT file, REGISTERS
    // FOR THE 21 EVENTS, ends the replay static-layout message, resolves "CrashHUD_mc" and resets its
    // timeline, then constructs+prepares eleven sub-components (InGameMessagesComponent,
    // ImpactTimePageChanger, ImpactTimeButton, ShowTimeButton1/2, ShowTimeAnimator, MudAnimator,
    // MugShotComponent, the Gamertag text field, RoadRuleShotComponent, SkipPromptAnimator,
    // SkipPromptButton) and sets the six enable bytes.
    //
    // ⛔ ONLY THE RegisterForEvents LEG IS REPRODUCED HERE. Every other leg needs a component TU or a
    // member this header still carries as an opaque guest span (+0x5C..+0x4B8), and fabricating
    // storage for them is exactly what this header's PHASE NOTE forbids. The omission is stated, not
    // hidden: this state still draws nothing. What it now does is OBSERVE and LEAVE.
    //
    // ⭐ The registration is the leg that had to come first, and the pause wave paid for that lesson
    // already: its first cut omitted CrashNavMapMain's RegisterForEvents and shipped a perfectly
    // faithful exit arm as DEAD CODE, because a state that observes nothing receives nothing. Here
    // the state did not even update, so it could not observe either.
    void CrashedHudState::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            // [crash-hud] witness. NOT X360. Proves the FSM really enters CRASHED and that the
            // 377 route is armed -- the two things no static read could settle.
            static bool sbLoggedEnter = false;
            if (!sbLoggedEnter)
            {
                sbLoggedEnter = true;
                *CgsDev::Log::gpDebugPrint
                    << "[crash-hud] CrashedHudState::OnEnter -- registered "
                    << miNumEventsObserved << " events (377 included)\n";
            }
        }
    }

    // ===============================================================================
    //  OnLeave  @ 0x8247D308   -- ⚠️ PARTIAL (the mirror of the above)
    // ===============================================================================
    // Console: ResetTimeline(mpMovieClipInst) -> UnRegisterForEvents(table, 21) -> post an id-18
    // record {8, 18, 12} + {1, &unk_820046A7} on channel 41 (size 20) -> clear three in-game-message
    // queue bytes off the GuiCache. Only the UnRegisterForEvents leg is reproduced, for the same
    // reason as OnEnter: the rest reaches members this phase carries as opaque storage.
    void CrashedHudState::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // ===============================================================================
    //  Update  @ 0x82481B88   -- ⚠️ PARTIAL: the phase machine is deferred, the tail is not
    // ===============================================================================
    // Console: `mbInImpactTime = 0`, then a six-way switch on meInternalState that falls THROUGH its
    // phases (GETCACHE -> LOADING -> WF_INIT -> SETUPSTATE -> RUNNING, each advancing on its body's
    // return; IDLE is a no-op; anything else asserts "Should never call update in the following
    // state" at BrnCrashedHudState.cpp:337), and then ALWAYS the two-line tail:
    //     UpdatePermenant();  mpInGuiEventQueue->Clear();
    //
    // ⛔ The five phase bodies (UpdateGetCache @0x82476698, UpdateLoading @0x8247CE98, UpdateWFInit
    // @0x824753A8, UpdateSetupState @0x8247CF60, UpdateRunning @0x824767D8) are NOT reconstructed --
    // they drive the eleven components OnEnter does not construct here, so calling them would be
    // calling into storage that does not exist. They are deferred as a set, and meInternalState is
    // left where the pool's value-initialisation put it (0 == GETCACHE). Nothing in this file reads
    // it, so no arm silently depends on the deferral.
    //
    // ⭐ The tail is the console's, verbatim in shape, and it is unconditional there -- UpdatePermenant
    // runs in EVERY phase including GETCACHE. So reproducing the tail alone is not a shortcut past the
    // phase machine: it is the one part of Update whose behaviour does not depend on it.
    void CrashedHudState::Update()
    {
        // FLAG deferred: `mbInImpactTime = 0` and the five-phase dispatch (see the banner).

        UpdatePermenant();

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue != 0)
            lpInQueue->Clear();
    }

    // ===============================================================================
    //  UpdatePermenant  @ 0x824812A0   -- the ten console arms, each accounted for
    // ===============================================================================
    // Drains the state in-queue every frame. The console dispatches TEN ids. Every one is listed
    // here with its console behaviour and its status, so that nothing is dropped silently:
    //
    //   LIVE (dependencies all present in this tree):
    //     377 -> payload word 1|3 : SendStateEvent("END_CRASH")     <-- THE ARM THIS WAVE IS FOR
    //     320 -> SendStateEvent("PAUSE")                (unconditional)
    //     291 -> SendStateEvent("PAUSE")                (unconditional)
    //     148 -> payload word == 0 : SendStateEvent("PAUSE")
    //     309 -> payload[0]==1 && payload[1]==0 && game-mode-type == -1 : SendStateEvent("PAUSE")
    //
    //   ⛔ FLAG deferred (needs a member inside the +0x5C..+0x4B8 opaque span, or an unhomed TU):
    //       5  -> if payload word 4 == 49 : mbInImpactTime = 1        (mbInImpactTime is opaque)
    //     547  -> mbCrashIsSkippable = 1; if meInternalState > SETUPSTATE :
    //             mSkipPromptAnimator.Run("transIn")                  (both members opaque)
    //     573  -> payload[2] 0|1|3 fall through; 2 -> StartFreeburnChallengeTicker; else assert
    //             "Unknown freeburn challenge selector action " (:937)
    //     574  -> assert lpChallengeEvent (:869); payload[2]==0 -> StartFreeburnChallengeTicker
    //     576  -> StartFreeburnChallengeTicker
    //     581  -> GetFreeburnChallengeManager(mpCache)+4 in {2,3,4} -> StartFreeburnChallengeTicker
    //     578  -> post {2, 536, 12} + s16 1 on channel 40 (size 16) via the access-pointer queue
    //     579  -> the same record as 578
    //     (StartFreeburnChallengeTicker @0x8247D410 and the FreeburnChallengeManager reach are not
    //      reconstructed; 578/579 need the StateInterface[1] access-pointer queue this phase does
    //      not model.)
    //
    //   Ids observed but NOT dispatched by the console's UpdatePermenant (they fall through its
    //   `default`): 6, 7, 21, 64, 154, 156, 140, 325. Registered, deliberately ignored here.
    void CrashedHudState::UpdatePermenant()
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
                // 0x82481C74: `cmpwi r11, 1 / beq` + `cmpwi r11, 3 / beq` -> "END_CRASH".
                // The producer (GameBridgeWorldToGui) posts 1 == E_CRASHBARSTATE_LEAVE_CRASHED on
                // the falling edge; 3 is the showtime-side spelling of the same leave.
                if (lpiPayload[0] == 1 || lpiPayload[0] == 3)
                {
                    if (CgsDev::Log::gpDebugPrint != 0)
                    {
                        // [crash-hud] witness. NOT X360.
                        *CgsDev::Log::gpDebugPrint
                            << "[crash-hud] CrashedHudState: GUI 377 payload=" << lpiPayload[0]
                            << " -> SendStateEvent(\"END_CRASH\")\n";
                    }
                    SendStateEvent("END_CRASH");
                }
                // 0|2 (START_CRASHED) is a no-op here: the state is already crashed. The console
                // has no arm for it either -- its test is `== 1 || == 3` and nothing else.
                break;

            case 320:
            case 291:
                SendStateEvent("PAUSE");
                break;

            case 148:
                // 0x82481D24 `lwz r11, 0(r19)` -- a WORD here (PausedHudState reads the same id as
                // a BYTE; that difference is the console's, and both are kept as written).
                if (lpiPayload[0] == 0)
                    SendStateEvent("PAUSE");
                break;

            case 309:
                // Console: payload[0]==1 && payload[1]==0 && *(guiCache + 40536) == -1.
                // ⚠️ FLAG, and it is the && identity in this build: cache+40536 is the game-mode-type
                // word (-1 == offline/none) -- the same word FBurnMainHudState reaches through its
                // GuiCache_GetGameModeType leaf, which is itself a PC-platform stub returning -1
                // unconditionally (BrnFBurnMainHudState.cpp:208). Rather than fork a second copy of
                // that stub, the term is pinned to its constant and named here.
                // DELETE-WHEN: GuiCache_GetGameModeType becomes a real cache read -- then this arm
                // must consult it.
                if (lpiPayload[0] == 1 && lpiPayload[1] == 0 /* && game-mode-type == -1 */)
                    SendStateEvent("PAUSE");
                break;

            default:
                // The deferred arms (5, 547, 573, 574, 576, 578, 579, 581) and the eight registered-
                // but-undispatched ids land here. The console has no assert on its default path in
                // this function, so neither does this -- adding one would be an invented arm.
                break;
            }
        }
    }
}
