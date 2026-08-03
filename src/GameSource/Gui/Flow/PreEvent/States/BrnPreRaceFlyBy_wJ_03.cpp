// ===================================================================================
// BrnGui::PreRaceFlyByState -- wave-J partfile 03: the event / flow trio.
//   b5-decomp/src/GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy_wJ_03.cpp
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (the raw `assembly` array
// arbitrated over Hex-Rays throughout -- the pseudocode of two members of this group is
// varargs-mangled):
//   SetupComponents      @0x824D6228  (cpp:638,  assert cpp:689)
//   HandleIncomingEvents @0x824D6410  (cpp:795,  asserts cpp:819/927/984)
//   HandleAptEvents      @0x824C6C48  (cpp:996,  asserts cpp:1001 / cpp:1088)
// All three bodies are HERE. (Two of them were parked while the shared headers were thin;
// GuiCache::GetEventID, StateInterface::OutputViewState/OutputInternalState and
// GuiEventShowHideSatNav's real fields have all landed since, so nothing is parked now.)
//
// The other functions of the class live in sibling wave-J partfiles; the owning header is
// BrnPreRaceFlyBy.h. Every class static named here (KAC_STATE_COMPONENT_NAME) is DECLARED
// there and DEFINED in partfile 01 -- the definitions are deliberately not repeated (one
// definition per program).
//
// CONSOLE-LITERAL NOTE: no X360 member displacement is reproduced as a number anywhere in
// this file -- 0x978 (meCurrentState), 0x981 (mbDoMapPan) and 0x8E4 (mStateAnimator) are
// reached by member name, so the host's own LLP64 layout applies. They appear in comments
// only. There are no float comparisons in these bodies, so there is no NaN-polarity
// decision to make.
// ===================================================================================

#include "GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / VariableEventQueue
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::Output*State
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTrigger(+Payload)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // BrnGui::GuiEventShowHideSatNav

#include <cstring>   // strcmp (the X360 inlines it into HandleAptEvents)

namespace BrnGui
{
    // House file-local alias (precedent: the sibling PreEvent / HUD state TUs).
    namespace GSM = BrnGameState::GameStateModuleIO;

    namespace
    {
        // The state's inbound GUI queue. CgsGui::State only holds an INCOMPLETE
        // `InputBuffer::GuiEventQueue*`, so the concrete queue type has to be named here to
        // drain it; <18432,16> is the committed GUI queue shape (CgsGuiModule.h:44,
        // CgsGuiModuleIO.h:91) and this is the house idiom -- identical typedef and cast in
        // BrnBootAttract.cpp:15/:50.
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- the observed event ids (PreRaceFlyByState::maiEventToObserve @0x82065CAC
        //      == {6, 21, 64, 159, 160, 162, 164, 213}; the array is declared in the
        //      header and defined in partfile 01) --------------------------------------
        // ATTESTED names -- each id is owned by a type already homed in the tree:
        const s32 KI_EVENT_CONTROLLER_INPUT_PRESSED = 6;    // CgsGui::GuiEventControllerInputPressed
        const s32 KI_EVENT_APT_TRIGGER              = 21;   // CgsGui::GuiEventAptTrigger (payload == GuiEventAptTriggerPayload)
        const s32 KI_EVENT_GUI_CACHE                = 64;   // the GuiCache refresh event (BrnCarSelectMain_wG_03.cpp)
        const s32 KI_EVENT_PRERACE_MESSAGES         = 159;  // BrnGui::GuiEventPreRaceMessages
        const s32 KI_EVENT_PRERACE_TRIGGER          = 160;  // BrnGui::GuiEventPreraceTrigger
        const s32 KI_EVENT_SHOW_HIDE_SAT_NAV        = 213;  // BrnGui::GuiEventShowHideSatNav
        // FLAG: role-derived names. Ids 162 and 164 have no homed payload type (164 is
        // emitted as a bare CgsGui::GuiEvent<164> -- CgsGuiModule_AddGuiEvent_Inst.cpp
        // @0x823D2B68 -- and 162 has no instantiation at all). These names record only
        // what THIS state does with each id; they are not attested outside this switch.
        const s32 KI_EVENT_FLYBY_ABORT              = 162;  // exits immediately
        const s32 KI_EVENT_FLYBY_END                = 164;  // drives the graceful trans-out arm
    }

    // -------------------------------------------------------------------------------
    // HandleIncomingEvents  @0x824D6410   (cpp:795)
    // Drain the state's inbound GUI queue once per Update, dispatch the eight observed
    // ids, forward every event to the embedded map component, then clear the queue.
    // -------------------------------------------------------------------------------
    void PreRaceFlyByState::HandleIncomingEvents()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            // A handler that ran a state event has already left the flow; stop dispatching
            // and let the Clear below drop whatever is still queued.
            if (meCurrentState == E_PRERACE_INVALID)
                break;

            switch (liEventType)
            {
                case KI_EVENT_CONTROLLER_INPUT_PRESSED:
                case KI_EVENT_GUI_CACHE:
                case KI_EVENT_SHOW_HIDE_SAT_NAV:
                    // Observed only so the map component below sees them.
                    break;

                case KI_EVENT_APT_TRIGGER:
                    HandleAptEvents(reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent));
                    break;

                case KI_EVENT_PRERACE_MESSAGES:
                {
                    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:819

                    // The online modes have no fly-by: any pre-race message ends it.
                    const s32 liGameMode = mpGuiCache->GetGameMode();
                    if (liGameMode >= GSM::E_MODE_ONLINE_MODE_START
                        && (liGameMode <= GSM::E_MODE_ONLINE_ROAD_RAGE
                            || liGameMode == GSM::E_MODE_ONLINE_BURNING_HOME_RUN))
                    {
                        TriggerExitState();
                    }
                    break;
                }

                case KI_EVENT_PRERACE_TRIGGER:
                    // HandlePreRaceTriggerEvent (DWARF cpp:984) is INLINED here: the X360
                    // emits only its null-payload assert at this arm and then falls to the
                    // shared RecvEvent tail, so the assert is the whole surviving effect.
                    CGS_ASSERT(lpEvent != 0, "lpPreRaceTrigger");   // cpp:984
                    break;

                case KI_EVENT_FLYBY_ABORT:
                    TriggerExitState();
                    break;

                case KI_EVENT_FLYBY_END:
                    if (meCurrentState < E_PRERACE_ACTIVE_EVENT_TITLES || mbHiddenDueToPause)
                    {
                        // Nothing is on screen yet (or the pause menu hid it), so there is
                        // no transition to play -- leave immediately.
                        TriggerExitState();
                        meCurrentState = E_PRERACE_ACTIVE_DONE;
                    }
                    else
                    {
                        meCurrentState = E_PRERACE_ACTIVE_TRANS_OUT;
                        mLargeEventIcon.SetState("transOut");
                        mStateAnimator.AddOutputAptViewState(
                            "apt_Transition",
                            mbDoMapPan ? "transoutMap" : "transout",
                            false);

                        // Fade the sat-nav out over half a second, on both the view and
                        // the internal-state channels (the X360 posts the same record to
                        // 41 and then 42).
                        GuiEventShowHideSatNav lSatNavEvent;
                        lSatNavEvent.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN,
                                               /*lbShow*/ false, /*lfFadeTime*/ 0.5f);
                        mpStateInterface->OutputViewState<GuiEventShowHideSatNav>(lSatNavEvent);
                        mpStateInterface->OutputInternalState<GuiEventShowHideSatNav>(lSatNavEvent);
                    }
                    break;

                default:
                    // cpp:927 -- the message names Update because this drain is inlined
                    // into it on the console. Non-fatal.
                    CGS_ASSERT(false, "Unexpected event in PreRaceFlyByState::Update");
                    break;
            }

            mMainMapComponent.RecvEvent(lpEvent, liEventType);
        }

        lpInQueue->Clear();
    }

    // -------------------------------------------------------------------------------
    // HandleAptEvents  @0x824C6C48   (cpp:996)
    // The apt view's "transition complete" callback for this screen's root clip: step the
    // fly-by presentation on to whatever the state's just-finished transition leads to.
    //
    // Notes taken from the asm rather than the pseudocode:
    //  * The null-payload assert is NON-fatal (BeginAssert / FireAssert / EndAssert with
    //    no early-out, 0x824C6C74..0x824C6CDC) -- the very next instruction dereferences
    //    the payload regardless. Reproduced as written.
    //  * The do-while at 0x824C6CF8..0x824C6D18 is an INLINED strcmp with the literal as
    //    the LEFT operand (`subf r8, r8(name), r9(literal)`); written back as a strcmp
    //    call in the same operand order.
    //  * The state switch is `meCurrentState - 2` over 6 table slots (0x824C6D28
    //    `addi r11, r11, -2` / `cmplwi cr6, r11, 5`), so only states 2..7 reach an arm and
    //    everything else -- including E_PRERACE_INVALID -- falls into the assert. Slots 0
    //    (MAP_ICON_DELAY) and 4 (MEDALS) point at the function's own epilogue: real no-ops.
    //  * Every transition arm writes meCurrentState BEFORE calling AddOutputAptViewState
    //    (the `stw` precedes the `bl` in all four), and the TRANS_OUT arm calls
    //    TriggerExitState FIRST, then overwrites the E_PRERACE_INVALID that call leaves
    //    behind with E_PRERACE_ACTIVE_DONE (0x824C6E1C `bl` then 0x824C6E24 `stw r11,
    //    0x978`). Both orderings are preserved.
    //  * AddOutputAptViewState is the CgsGui::GuiComponent base method invoked on
    //    mStateAnimator (X360 `addi r3, r30, 0x8E4`); the `li r6, 0` is lbImmediate.
    // -------------------------------------------------------------------------------
    void PreRaceFlyByState::HandleAptEvents(const CgsGui::GuiEventAptTriggerPayload* lpTrigger)
    {
        // cpp:1001 -- the X360 streams this through a CgsDev::StrStream; per project policy
        // that is lowered to the static text. Non-fatal: the body reads it either way.
        CGS_ASSERT(lpTrigger != 0, "Invalid event passed to PreRaceFlyByState::HandleAptEvents");

        if (lpTrigger->meEventType != CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
            return;

        if (strcmp(KAC_STATE_COMPONENT_NAME, lpTrigger->mpacComponentName) != 0)
            return;

        switch (meCurrentState)
        {
            case E_PRERACE_ACTIVE_MAP_ICON_DELAY:
                break;

            case E_PRERACE_ACTIVE_EVENT_TITLES:
                // The title bars have finished coming in: pan the map in when this event
                // has one, otherwise go straight to the medals.
                if (mbDoMapPan)
                {
                    meCurrentState = E_PRERACE_ACTIVE_MAP_INTRO;
                    mStateAnimator.AddOutputAptViewState("apt_Transition", "mapIn", false);
                }
                else
                {
                    meCurrentState = E_PRERACE_ACTIVE_MEDALS;
                    mStateAnimator.AddOutputAptViewState("apt_Transition", "medalsInNoMap", false);
                }
                break;

            case E_PRERACE_ACTIVE_MAP_INTRO:
                meCurrentState = E_PRERACE_ACTIVE_SHOW_MAP;
                mStateAnimator.AddOutputAptViewState("apt_Transition", "showMap", false);
                break;

            case E_PRERACE_ACTIVE_SHOW_MAP:
                meCurrentState = E_PRERACE_ACTIVE_MEDALS;
                mStateAnimator.AddOutputAptViewState("apt_Transition", "medalsInMap", false);
                break;

            case E_PRERACE_ACTIVE_MEDALS:
                break;

            case E_PRERACE_ACTIVE_TRANS_OUT:
                TriggerExitState();
                meCurrentState = E_PRERACE_ACTIVE_DONE;
                break;

            default:
                // cpp:1088 -- streamed as "Not expecting to receive a trans complete from
                // the screen when we are in state " << meCurrentState << "\n"; lowered to
                // the static text per project policy, the streamed value being the switch
                // scrutinee itself.
                CGS_ASSERT(false,
                           "Not expecting to receive a trans complete from the screen when we are in state \n");
                break;
        }
    }

    namespace
    {
        // The pre-race mode-name string-id table @0x82F27840 (image read), indexed by
        // GSM::EGameModeType 0..9.
        //
        // DWARF-attested global, NOT this TU's data: dwarfdump GameSource/Gui/BrnGuiShared.cpp:34
        // declares `extern const char *[10] KAPC_GAMEMODE_STRINGIDS;` attributed to
        // BrnGuiShared.cpp:154, and the five consecutive tables at 0x82F277E0..0x82F27868
        // reproduce that file's declaration order exactly (POSITION x8, POSITION_LOWERCASE x8,
        // DIRECTION x8 @0x82F27820, GAMEMODE x10 @0x82F27840, GAMEMODE_PLURAL x10). It carries
        // the DWARF name here but stays file-local because its home does not exist yet.
        // DELETE-WHEN GameSource/Gui/BrnGuiShared.cpp lands (declare it in BrnGuiShared.h and
        // index the shared one).
        const char* const KAPC_GAMEMODE_STRINGIDS[GSM::E_MODE_OFFLINE_COUNT] =
        {
            "GAMEMODE_RACE",          // 0  E_MODE_OFFLINE_RACE
            "GAMEMODE_FACEOFF",       // 1  E_MODE_FACE_OFF
            "GAMEMODE_CRASH",         // 2  E_MODE_OFFLINE_SHOWTIME
            "GAMEMODE_ROADRAGE",      // 3  E_MODE_ROAD_RAGE
            "GAMEMODE_PURSUIT",       // 4  E_MODE_PURSUIT
            "GAMEMODE_BURNINGROUTE",  // 5  E_MODE_BURNING_ROUTE
            "GAMEMODE_ELIMINATOR",    // 6  E_MODE_ELIMINATOR
            "GAMEMODE_STUNTATTACK",   // 7  E_MODE_STUNT_ATTACK
            "GAMEMODE_SURVIVAL",      // 8  E_MODE_MARKED_MAN
            "GAMEMODE_TRAFFICATTACK", // 9  E_MODE_TRAFFIC_ATTACK
        };
    }

    // -------------------------------------------------------------------------------
    // SetupComponents  @0x824D6228   (cpp:638)
    // Once every apt component of the fly-by screen has initialised: fill the per-mode
    // description text, start the large event icon's transition-in, and label the screen
    // with the event's name and its mode name.
    // -------------------------------------------------------------------------------
    void PreRaceFlyByState::SetupComponents()
    {
        // GuiCache far word +0x9E58 == GetGameMode(); reached by accessor, not by offset.
        s32 liGameMode = mpGuiCache->GetGameMode();

        switch (liGameMode)
        {
            case GSM::E_MODE_OFFLINE_RACE:
                SetRaceDescription();
                break;

            case GSM::E_MODE_OFFLINE_SHOWTIME:
                // Crash has no pre-race description text: the arm branches straight to the
                // shared tail at 0x824D6368.
                break;

            case GSM::E_MODE_ROAD_RAGE:
                SetRoadRageDescription();
                break;

            case GSM::E_MODE_BURNING_ROUTE:
                SetBurningRouteDescription();
                break;

            case GSM::E_MODE_STUNT_ATTACK:
                SetFreestyleDescription();
                break;

            case GSM::E_MODE_MARKED_MAN:
                SetMarkedManDescription();
                break;

            default:
                // cpp:689 -- streamed as "Unknown game mode (" << mode << ")\n"; lowered
                // to the static text per project policy. Non-fatal, and the console then
                // executes `mr r29, r26` with r26 == 0 (0x824D6364), i.e. it CLAMPS the
                // mode to E_MODE_OFFLINE_RACE for the rest of the body. The switch bound
                // is the UNSIGNED `cmplwi cr6, r29, 8` / `bgt` at 0x824D624C, and the
                // jumptable sends cases 1, 4 and 6 here as well.
                CGS_ASSERT(false, "Unknown game mode ()\n");
                liGameMode = GSM::E_MODE_OFFLINE_RACE;
                break;
        }

        mLargeEventIcon.SetState("transIn");

        // The localisation id the screen's title field shows, and the format it resolves
        // under. 31 chars + the manual terminator == the console's 32-byte stack buffer.
        char lacEventTextId[32];
        CgsLanguage::LanguageManager::ParameterFormatType leEventTextFormat;

        if (liGameMode == GSM::E_MODE_PURSUIT)
        {
            // Pursuit labels the screen with the hunted car's name rather than an event
            // id -- but this arm is UNREACHABLE on this build, and deliberately kept
            // because the binary has it: mode 4 takes the switch's default arm above,
            // which clamps the mode to 0 before this `cmpwi cr6, r29, 4` at 0x824D6378
            // ever runs.
            char lacPursuitCarId[16];
            CgsIDConvertToString(mpGuiCache->GetPursuitCarID(), lacPursuitCarId);
            CgsCore::SPrintf(lacEventTextId, 31, "CAR_CAPS_%s", lacPursuitCarId);
            leEventTextFormat = CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP;
        }
        else
        {
            // GuiCache far word +0x9E5C == GetEventID() (the declaration this body waits on).
            CgsCore::SPrintf(lacEventTextId, 31, "EV_%06u", mpGuiCache->GetEventID());
            leEventTextFormat = CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP_TOUPPER;
        }
        lacEventTextId[31] = 0;

        mEventName.SetLocalisedText(lacEventTextId, leEventTextFormat);
        mModeType.SetLocalisedText(KAPC_GAMEMODE_STRINGIDS[liGameMode],
                                   CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    }
}
