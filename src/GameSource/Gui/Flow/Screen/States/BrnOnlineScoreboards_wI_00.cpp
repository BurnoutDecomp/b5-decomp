// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 00: the two animator leaves + the audio wire.
//   ShowTableEmpty @0x82486A08  (DWARF cpp:1390)
//   SetupButtons   @0x82486A58  (DWARF cpp:1535)
//   TriggerSound   @0x8249F710  (DWARF cpp:1418, assert cpp:1570)
//
//
// The committed leaf header BrnOnlineScoreboards.h is still the 29-line minimal
// Engine-cone-wave-19 version (the GetResourcesToLoad inline plus the two resource
// statics). The wave-I spec's full-shape class had not been applied when this partfile was
// written, and headers are frozen for implementers, so none of the three bodies can be
// declared as members and none of the five members they touch exists. Measured with the
// compile gate, not assumed: cl /c reports "ShowTableEmpty/SetupButtons/TriggerSound ist
// kein Member von BrnGui::OnlineScoreboards" plus undeclared mbTableEmptyShowing,
// mTableEmptyAnimator, mbUsingFilters, mButtonAnimator, mTable, mpStateInterface.
//
// The complete, drop-in-ready partfile (single `namespace BrnGui { ... }`, one anonymous
// namespace, no duplicated constants) lives at
// with a banner naming the exact declaration lines that unblock it. Copy it over this file
// once the spec's full-shape header lands; no edit is needed.
//
// EXACT DECLARATIONS THAT UNBLOCK IT (all inside
// `struct BrnGui::OnlineScoreboards : public CgsGui::State`, private section -- every one
// of them is already part of the spec's full-shape header):
//     void ShowTableEmpty(bool lbShow);            // @0x82486A08
//     void SetupButtons();                         // @0x82486A58
//     void TriggerSound(s32 leGameInputAction);    // @0x8249F710
//     bool                      mbUsingFilters;      // X360 +3584
//     LeaderboardTableComponent mTable;              // X360 +15376  "Table_mc"
//     AnimationComponent        mButtonAnimator;     // X360 +20048  "ButtonPrompts_anim"
//     AnimationComponent        mTableEmptyAnimator; // X360 +20188  "TableEmpty_anim"
//     bool                      mbTableEmptyShowing; // X360 +20328
// plus the two member-type includes (BrnAnimationComponent.h,
// BrnLeaderboardTableComponent.h).
//
// against a SHADOW copy of the header carrying only those declarations --
// scratchpad/waveI/probe00/ (run_probe.py prints PROBE_STATUS=pass). The faithfulness lint
// reports 0 new findings on it. No other declaration is needed.
//
// CgsGuiStateInterface.h + CgsVariableEventQueue.h + BrnGuiEventTypeDefs.h. TypeDefs is the
// side of the TypeDefs/Demangled hard collision this group needs (GuiAudioTriggerEvent);
// the id-457 wire word is restored by a file-local view over it, as in the two committed
// twins.
//
// LINK NOTE for the conductor: SetupButtons calls
// LeaderboardTableComponent::GetRowsBefore/GetRowsAfter (declared in
// BrnLeaderboardTableComponent.h, bodies owned by their own ledger TUs and defined nowhere
// in the tree yet), GuiAudioTriggerEvent::Construct and
// CgsGui::GuiComponent::AddOutputAptViewState. All are invisible to cl /c and may be
// unresolved at link until those TUs land -- same as wave H.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiAudioTriggerEvent

namespace BrnGui
{

    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // X360 `li r5, 0x28`

        // ---- apt view-state names (rodata literals) ------------------------------------
        // Every component transition on this screen is pushed down the one "apt_Transition"
        // channel (aAptTransition_1 -- the same string all three call sites load).
        const char KAC_APT_TRANSITION[] = "apt_Transition";

        // TableEmpty_anim's two states.
        const char KAC_VIEW_STATE_VISIBLE[]   = "visible";
        const char KAC_VIEW_STATE_INVISIBLE[] = "invisible";

        // ButtonPrompts_anim's five states. "SingleX360" is the TABLE-focus prompt set: the
        // X360 picks it when mbUsingFilters is 0 (`lbz r11, 0xE00(r3)` / `bne` at
        // 0x82486A58-64), and 0 is the table-focus value -- UpdateRunning @0x824ADA54 routes a
        // 0 to HandleControllerInputPressedUsingTable, that handler stores 1 there to hand
        // focus to the filters (@0x824A9C64), and HandleControllerInputPressedUsingFilters
        // stores 0 to hand it back to the table (@0x824A9F64). The four "General*" variants
        // are therefore the FILTER-row prompt set, and which one is picked says whether the
        // table below can still page up, page down, both, or neither. Consistent with the
        // DWARF, which sizes KAE_HELP_BAR_TABLE at 1 entry and KAE_HELP_BAR_FILTER at 4.
        const char KAC_VIEW_STATE_SINGLE_X360[]   = "SingleX360";
        const char KAC_VIEW_STATE_GENERAL[]       = "General";
        const char KAC_VIEW_STATE_GENERAL_UP[]    = "GeneralUp";
        const char KAC_VIEW_STATE_GENERAL_DOWN[]  = "GeneralDown";
        const char KAC_VIEW_STATE_GENERAL_BOTH[]  = "GeneralBoth";

        // ---- controller action ids (the in-queue payload's second word) ----------------
        // FLAG: the producer-side enum name (EGameInputActions) is not in the recovered
        // DWARF slice, so these are named for the role the X360 code gives them. The X360
        // switches on `r4 - 0x25` over 8 contiguous cases. Same family split as the two
        // committed twins (BrnOnlineGameRoomPlayerInfo_wH_08.cpp, BrnOnlineGameOptions
        // group 02): 0x25/0x26/0x29/0x2A are ROW moves, 0x27/0x28/0x2B/0x2C are toggle-
        // VALUE moves.
        const s32 KI_ACTION_MENU_MOVE_A   = 0x25;   // '%'  row-move family
        const s32 KI_ACTION_MENU_MOVE_B   = 0x26;   // '&'  row-move family
        const s32 KI_ACTION_ITEM_MOVE_A   = 0x27;   // '\'' value-move family
        const s32 KI_ACTION_ITEM_MOVE_B   = 0x28;   // '('  value-move family
        const s32 KI_ACTION_MENU_PREVIOUS = 0x29;   // ')'  row-move family
        const s32 KI_ACTION_MENU_NEXT     = 0x2A;   // '*'  row-move family
        const s32 KI_ACTION_ITEM_PREVIOUS = 0x2B;   // '+'  value-move family
        const s32 KI_ACTION_ITEM_NEXT     = 0x2C;   // ','  value-move family

        // The GuiAudioTriggerEvent::meAction value every menu cue in the GUI flow uses
        // (X360 `li r4, 7`). FLAG: the action enum's name is not in the recovered DWARF
        // slice.
        const s32 KI_AUDIO_ACTION_MENU_CUE = 7;

        // The two audio labels the X360 selects between (rodata literals).
        const char KAC_SOUND_LABEL_MENU_TOGGLE[]      = "MenuToggleDefault";
        const char KAC_SOUND_LABEL_MENU_ITEM_TOGGLE[] = "MenuItemToggleDefault";

        // ---- out-queue wire record -----------------------------------------------------
        // The record the inlined OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> builds:
        // { 100, 457, 12, the 100-byte payload }, channel 40, 112 bytes (X360 stores
        // `li r11, 0x64` / `li r11, 0x1C9` / `li r11, 0xC` then AddEvent with r5 = 0x28,
        // r6 = 0x70). The committed PC GuiAudioTriggerEvent already carries that 12-byte
        // queue header in front of its 100-byte payload, so the event object IS the record
        // -- but its GuiEvent<201> base seeds the payload-size word 0 and the id 201, while
        // the X360 wire words are the payload size and the id 457. This view restores them
        // without forking the payload type (BrnOnlineGameRoomPlayerInfo_wH_08.cpp carries
        // the identical view).
        const u32 KU_WIRE_ID_AUDIO_TRIGGER = 457;   // X360 `li r11, 0x1C9`

        struct GuiAudioTriggerWire : public GuiAudioTriggerEvent
        {
            GuiAudioTriggerWire()
            {
                muHeader0 = static_cast<u32>(sizeof(GuiAudioTriggerEvent) -
                                             sizeof(CgsGui::GuiEvent<201>));
                muEventType = KU_WIRE_ID_AUDIO_TRIGGER;
            }
        };

        // Record-size pin. The payload is pointer-free, so the X360's AddEvent size
        // immediate must survive unchanged on the x64 host; if a host layout ever drifts,
        // this is where it is caught rather than on the wire.
        static_assert(sizeof(GuiAudioTriggerWire) == 112, "audio trigger record is 112 bytes");
    }

    // ---- ShowTableEmpty @ 0x82486A08 -----------------------------------------------
    // Raise or drop the "no scores to show" overlay. The state is latched so the
    // transition is only pushed when it actually changes (the X360 returns straight out of
    // the compare when the flag already matches).
    void OnlineScoreboards::ShowTableEmpty(bool lbShow)
    {
        if (mbTableEmptyShowing == lbShow)
        {
            return;
        }

        mbTableEmptyShowing = lbShow;

        mTableEmptyAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION,
            lbShow ? KAC_VIEW_STATE_VISIBLE : KAC_VIEW_STATE_INVISIBLE,
            false);
    }

    // ---- SetupButtons @ 0x82486A58 -------------------------------------------------
    // Put the right button-prompt set up for whatever the player is driving. mbUsingFilters
    // is 0 while the TABLE has focus and nonzero while the filter row has focus --
    // UpdateRunning @0x824ADA54 dispatches to HandleControllerInputPressedUsingTable when it
    // reads 0. So while the table has focus there is a single prompt layout ("SingleX360");
    // while the filter row has focus the prompt set advertises the paging the table below
    // still has, which the table's rows-before / rows-after counters answer.
    void OnlineScoreboards::SetupButtons()
    {
        if (!mbUsingFilters)
        {
            mButtonAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                  KAC_VIEW_STATE_SINGLE_X360, false);
            return;
        }

        // The X360 reads the two byte-wide counters straight out of the table component
        // (`lbz 0x4DAD` / `lbz 0x4DAE`) because both accessors inline; restored as the
        // declared accessor calls.
        const s8 liRowsBefore = mTable.GetRowsBefore();
        const s8 liRowsAfter  = mTable.GetRowsAfter();

        const char* lpcViewState;

        if (liRowsBefore == 0)
        {
            lpcViewState = (liRowsAfter != 0) ? KAC_VIEW_STATE_GENERAL_DOWN
                                              : KAC_VIEW_STATE_GENERAL;
        }
        else
        {
            lpcViewState = (liRowsAfter != 0) ? KAC_VIEW_STATE_GENERAL_BOTH
                                              : KAC_VIEW_STATE_GENERAL_UP;
        }

        mButtonAnimator.AddOutputAptViewState(KAC_APT_TRANSITION, lpcViewState, false);
    }

    // ---- TriggerSound @ 0x8249F710 -------------------------------------------------
    // Raise the menu-move audio cue for a controller action: pick the label for the
    // action's family and publish a GuiAudioTriggerEvent with it.
    void OnlineScoreboards::TriggerSound(s32 leGameInputAction)
    {
        const char* lpcLabel = 0;

        switch (leGameInputAction)
        {
            case KI_ACTION_MENU_MOVE_A:
            case KI_ACTION_MENU_MOVE_B:
            case KI_ACTION_MENU_PREVIOUS:
            case KI_ACTION_MENU_NEXT:
                lpcLabel = KAC_SOUND_LABEL_MENU_TOGGLE;
                break;

            case KI_ACTION_ITEM_MOVE_A:
            case KI_ACTION_ITEM_MOVE_B:
            case KI_ACTION_ITEM_PREVIOUS:
            case KI_ACTION_ITEM_NEXT:
                lpcLabel = KAC_SOUND_LABEL_MENU_ITEM_TOGGLE;
                break;

            default:
                break;
        }

        // cpp:1570 -- non-fatal, and the X360 sinks it into the switch's default arm (the
        // other arms provably set lpcLabel and branch past it). Either placement runs the
        // same code: the event is constructed with the null label on the unrecognised path.
        CGS_ASSERT(lpcLabel, "lpcLabel");

        // X360: Construct(r3 = the stack event, r4 = 7, r5 = "", r6 = the label, r7 = "").
        GuiAudioTriggerWire lAudio;
        lAudio.Construct(KI_AUDIO_ACTION_MENU_CUE, "", lpcLabel, "");

        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAudio), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lAudio)));
    }
}
