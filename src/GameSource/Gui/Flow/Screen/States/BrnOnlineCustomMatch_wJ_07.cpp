// ============================================================================
// b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch_wJ_07.cpp
//
// BrnGui::OnlineCustomMatch -- the two lifecycle bodies wave J left to a "foreign TU":
//
//   OnlineCustomMatch::OnLeave  @0x824970D0  ( 63 insns)
//   OnlineCustomMatch::Update   @0x824AC808  (246 insns)
//
// With these the class is complete (16 wave-J bodies + these two + OnEnter in _wJ_06), so
// this partfile is what lets the six _wJ_ TUs mount and retires the three
// BrnScreenStatesLinkStubs.cpp scaffolds (OnEnter / OnLeave / Update) that stood in for
// the screen's vtable since 2026-08-03. Neither function has a DecFIGS scope (the PS3 unity
// build compiled them into another unit), so everything below is read off the X360 ARTIST
// assembly; the sibling partfiles' names are reused wherever the same store appears.
//
// ---- OnLeave @0x824970D0 -- store-for-store ----------------------------------------------
//   0x824970F8  UnRegisterForEvents(mpStateInterface, maiEventToObserve (unk_8205E758), 8)
//   0x82497100..0x82497144  the inlined StateInterface::PlayAptMovie: record
//               { 8, 18, 12, name = unk_820046A7 (the shared EMPTY rodata byte), level = 3 }
//               on the interface's out queue (this+0x1C, +0xC), channel 41, 20 bytes --
//               i.e. "clear apt level 3", the same call OnlineScoreboards::OnLeave spells.
//   0x82497148..0x82497170  { 1, 253, 12 } + one never-written payload byte, channel 40,
//               16 bytes -- the search-stop record _wJ_04's cancel arm also posts.
//   0x82497174..0x824971B0  { 2, 536, 12, u16 0 } channel 40, 16 bytes: the ticker
//               "clear messages" record with BOTH payload bytes zero (`stb 0 ; stb 0 ;
//               lhz ; sth`), so neither the force-fade nor the delete-challenges bit is set.
//
// ---- Update @0x824AC808 -- the per-frame pump ---------------------------------------------
//   0x824AC820  GetFirstEvent(mpInGuiEventQueue (this+0x18), &event, &size) -> id in r28
//   0x824AC8B0  `addi r11,r28,-6 ; cmplwi 0xF8` -- a 249-way jump table over ids 6..254:
//       6    HandleControllerInput(event)
//       14   nothing        (observed -- maiEventToObserve -- but no arm)
//       21   nothing        (ditto)
//       44   assert mpGuiCache (cpp:344); mpGuiCache->meLastDisconnectedError (+0x4B40) =
//            event ? *(s32*)event : 0; SendStateEvent("DISCONNECT")
//       50   assert mpGuiCache (cpp:316); GuiOverlayWaitFinishRequest::Construct("CNOnlEntGame")
//            -> { 8, 188, 16, <pad>, id } channel 40, 24 bytes; SendStateEvent("ADVANCE")
//       51   HandleInGameFailedEvent(event)
//       64   HandleGuiCacheEvent(event)
//       254  mbHasRecievedSearchResults (+0xDFF2) = 1; memcpy(&mSearchResults (+0xDFF4), event, 604)
//       else the streamed "Unexpected event received : <id> in <file> at line 353" assert
//   0x824ACF3C  GetNextEvent(...) until the event pointer is null
//   0x824ACF54  mpInGuiEventQueue->Clear()
//   0x824ACF5C  CheckForCompletedLoads()
//   0x824ACF60  component vtable slot 5 (+0x14) on this+0x2F8  == mSearchParms.Update()
//   0x824ACF74  slot 5 on this+0x4418                          == mTable.Update()
//   0x824ACF88  slot 5 on this+0x3100                          == mMessageButtons.Update()
//   0x824ACFA4  if (mbHasRecievedSearchResults) HandleSearchResults()
//
// The component slot-5 calls are spelled BY NAME per the project's flat-vtable convention
// (BrnSelectableGroup.h): the toggle group and the menu have no override of their own, so
// they resolve to SelectableGroup::Update @0x824E3FE0; the table DOES override it on the
// console (Table::Update @0x824E4890, DWARF BrnTable.cpp:135) -- that override is landed
// in BrnTable.cpp together with this file so the call binds to the right body.
//
// The streamed default-arm assert is lowered to a CGS_ASSERT with the static text, per the
// standing project rule (see BrnVehicleManager_PerFrameLeaves.cpp).
// ============================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include <cstddef>                                                        // offsetof (wire pins)
#include <cstring>                                                        // memcpy (the 604-byte results copy)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / PlayAptMovie / the out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / Get*Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache::meLastDisconnectedError
#include "GameSource/Gui/Flow/Shared/Components/BrnTable.h"               // Table::Update
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"     // MenuToggleGroupVarSize<3>::Update (SelectableGroup)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"       // MenuComponent::Update (SelectableGroup)

namespace BrnGui
{
    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;   // GuiEventOut (the OutputGuiEvent channel)

        // The observed event ids this screen's Update dispatches on (maiEventToObserve ==
        // { 14, 21, 6, 64, 254, 50, 51, 44 }). Named by their consumers; 14 and 21 are
        // observed but have no arm (the jump table sends both straight to the loop tail).
        const s32 KI_EVENT_CONTROLLER_INPUT       = 6;
        const s32 KI_EVENT_OBSERVED_NO_ARM_14     = 14;
        const s32 KI_EVENT_OBSERVED_NO_ARM_21     = 21;
        const s32 KI_EVENT_NETWORK_DISCONNECTED   = 44;
        const s32 KI_EVENT_ENTER_GAME             = 50;
        const s32 KI_EVENT_IN_GAME_FAILED         = 51;
        const s32 KI_EVENT_GUI_CACHE              = 64;
        const s32 KI_EVENT_CUSTOM_MATCH_RESULTS   = 254;   // == GuiEventNetworkCustomMatchResults::GetEventType()

        // OnLeave's apt-level clear (the inlined PlayAptMovie): the shared empty rodata
        // byte unk_820046A7 at level 3, as every sibling OnLeave spells it.
        const char* const KPC_EMPTY_STRING   = "";
        const s32         KI_APT_MOVIE_LEVEL = 3;

        const char KAC_ENTER_GAME_OVERLAY_ID[] = "CNOnlEntGame";   // 0x824AC894
        const char KAC_ADVANCE_EVENT[]         = "ADVANCE";        // 0x824AC88C
        const char KAC_DISCONNECT_EVENT[]      = "DISCONNECT";     // 0x824AC880

        // Id 44: the server-interface error the disconnect popup shows (`lwz 0(r21)`).
        // Same shape as BrnOnlineGameRoomPlayerInfo_wH_00.cpp's.
        struct NetworkDisconnectedPayload : public CgsModule::Event
        {
            s32 meError;   // +0x00
        };

        // { 1, 253, 12 } + one never-written payload byte, channel 40, 16 bytes -- the
        // twin of _wJ_04's CustomMatchSearchStopWire (consumer-named there; see that note).
        // The payload byte is zero-initialised because the host cannot reproduce the
        // console's uninitialised stack slot.
        struct CustomMatchSearchStopWire : public CgsGui::GuiEvent<253>
        {
            u8 muUnwrittenPayload;

            CustomMatchSearchStopWire()
                : CgsGui::GuiEvent<253>(
                      static_cast<u32>(sizeof(u8)),                                              // X360 1
                      static_cast<u32>(offsetof(CustomMatchSearchStopWire, muUnwrittenPayload))) // X360 12
                , muUnwrittenPayload(0)
            {
            }
        };

        // { 2, 536, 12, u16 0 }, channel 40, 16 bytes -- GuiEventTickerClearMessages with
        // both bytes CLEAR (0x82497190 `stb 0 ; stb 0`). Field names per the HUD twin
        // (BrnRaceMainHudState_wS2.cpp GuiTickerClearWire536).
        struct GuiTickerClearWire536 : public CgsGui::GuiEvent<536>
        {
            u8 mbForceFadeOut;            // +0x0C == 0
            u8 mbDeleteChallengeMessages; // +0x0D == 0
            u8 mau8Pad[2];

            GuiTickerClearWire536()
                : CgsGui::GuiEvent<536>(2, 12)
                , mbForceFadeOut(0), mbDeleteChallengeMessages(0)
            {
                mau8Pad[0] = mau8Pad[1] = 0;
            }
        };

        // The id-188 "this wait overlay has finished" request: payload = one compressed
        // overlay id at +0x10 (8-aligned), record 24 bytes -- _wJ_05's wire, restated here
        // because the partfiles keep their wire records TU-local.
        struct GuiOverlayWaitFinishRequestWire : public CgsGui::GuiEvent<188>
        {
            CgsID mOverlayId;   // +0x10

            explicit GuiOverlayWaitFinishRequestWire(CgsID lOverlayId)
                : CgsGui::GuiEvent<188>(
                      static_cast<u32>(sizeof(CgsID)),
                      static_cast<u32>(offsetof(GuiOverlayWaitFinishRequestWire, mOverlayId)))
                , mOverlayId(lOverlayId)
            {
            }
        };

        typedef char KAC_ASSERT_STOP_WIRE_SIZE[sizeof(CustomMatchSearchStopWire) == 16 ? 1 : -1];
        typedef char KAC_ASSERT_CLEAR_WIRE_SIZE[sizeof(GuiTickerClearWire536) == 16 ? 1 : -1];
        typedef char KAC_ASSERT_WAIT_FINISH_WIRE_SIZE[sizeof(GuiOverlayWaitFinishRequestWire) == 24 ? 1 : -1];
        typedef char KAC_ASSERT_WAIT_FINISH_PAYLOAD_OFFSET[
            offsetof(GuiOverlayWaitFinishRequestWire, mOverlayId) == 16 ? 1 : -1];
        // The 604-byte AddOutputGuiEvent record IS the member (headerless on both sides).
        typedef char KAC_ASSERT_RESULTS_RECORD_SIZE[sizeof(GuiEventNetworkCustomMatchResults) == 604 ? 1 : -1];
    }

    // ================================================================================
    //  OnLeave  @ 0x824970D0
    // ================================================================================
    void OnlineCustomMatch::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The X360 inlines StateInterface::PlayAptMovie here (record { 8, 18, 12, name,
        // level } on channel 41, 20 bytes). An empty name at level 3 is "clear level 3".
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

        // Stop any custom-match search still in flight.
        const CustomMatchSearchStopWire lStop;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lStop), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lStop)));   // X360 record size 16

        // ...and clear the ticker (no fade-out force, challenge messages kept).
        const GuiTickerClearWire536 lClear;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lClear), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lClear)));  // X360 record size 16
    }

    // ================================================================================
    //  Update  @ 0x824AC808
    // ================================================================================
    void OnlineCustomMatch::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                HandleControllerInput(lpEvent);
                break;

            case KI_EVENT_OBSERVED_NO_ARM_14:
            case KI_EVENT_OBSERVED_NO_ARM_21:
                break;

            case KI_EVENT_NETWORK_DISCONNECTED:
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:344
                // 0x824ACDB0..0x824ACDD8: a null event still writes the slot (0).
                if (lpEvent != 0)
                {
                    mpGuiCache->meLastDisconnectedError =
                        reinterpret_cast<const NetworkDisconnectedPayload*>(lpEvent)->meError;
                }
                else
                {
                    mpGuiCache->meLastDisconnectedError = 0;
                }
                SendStateEvent(KAC_DISCONNECT_EVENT);
                break;

            case KI_EVENT_ENTER_GAME:
            {
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:316

                // Take down the "entering game" wait overlay, then advance the flow.
                const GuiOverlayWaitFinishRequestWire lRequest(CgsIDCompress(KAC_ENTER_GAME_OVERLAY_ID));
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lRequest), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lRequest)));   // X360 record size 24

                SendStateEvent(KAC_ADVANCE_EVENT);
                break;
            }

            case KI_EVENT_IN_GAME_FAILED:
                HandleInGameFailedEvent(lpEvent);
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;

            case KI_EVENT_CUSTOM_MATCH_RESULTS:
                // 0x824ACD7C `stbx 1` then the 604-byte memcpy of the headerless record.
                mbHasRecievedSearchResults = true;
                std::memcpy(&mSearchResults, lpEvent, sizeof(GuiEventNetworkCustomMatchResults));
                break;

            default:
                // The streamed "Unexpected event received : <id> in <file> at line 353"
                // assert, lowered to the static text.
                CGS_ASSERT(false, "Unexpected event received");   // cpp:353
                break;
            }
        }

        lpInQueue->Clear();

        CheckForCompletedLoads();

        // Component vtable slot 5 on the three interactive components, in the console's
        // order: the parameter toggles, the found-games table, the message buttons.
        mSearchParms.Update();     // SelectableGroup::Update @0x824E3FE0 (no override)
        mTable.Update();           // Table::Update @0x824E4890 (the override, BrnTable.cpp)
        mMessageButtons.Update();  // SelectableGroup::Update @0x824E3FE0 (no override)

        if (mbHasRecievedSearchResults)
        {
            HandleSearchResults();
        }
    }

    // =====================================================================================
    // The class statics no partfile defined (the link listed all thirteen once the six
    // _wJ_ TUs were mounted). Pointer tables read from the image (headless IDA, 2026-09-02;
    // the .data slots are initialised on disk, no thunk); the char[] literals are the
    // DWARF-sized strings the header records.
    // =====================================================================================
    const char* const OnlineCustomMatch::KAPC_ANIMATION_STATES[3] =                  // @0x82F266B0
    {
        "Visible", "Invisible", "Refresh"
    };

    const char* const OnlineCustomMatch::KAPC_GAME_MODE_STRING_IDS[8] =              // @0x82F266BC
    {
        "$ONLINE_GAME_OPTION_MODE_RACE",                  // 0  (mode 10)
        "$ONLINE_GAME_OPTION_MODE_ROAD_RAGE",             // 1  (mode 11)
        "$ONLINE_GAME_OPTION_MODE_STUNT",                 // 2  (mode 12)
        "$ONLINE_GAME_OPTION_MODE_BURNING_HOME_RUN",      // 3  (mode 13)
        "$ONLINE_GAME_OPTION_MODE_STUNT_FREE_FOR_ALL",    // 4  (mode 14)
        "$ONLINE_GAME_OPTION_MODE_FREEBURN_LOBBY",        // 5  (mode 15)
        "Invalid game mode",                              // 6  (mode 16 -- the shipped literal, kept)
        "$ONLINE_GAME_OPTION_MODE_STUNT_COOP",            // 7  (mode 17)
    };

    const char* const OnlineCustomMatch::KAPC_OPPONENT_OPTION_STRING_IDS[4] =        // @0x82F266DC
    {
        "$ONLINE_GAME_SEARCH_OPTION_ANY",
        "$ONLINE_GAME_SEARCH_OPTION_FRIENDS_AND_RIVALS",
        "$ONLINE_GAME_SEARCH_OPTION_FRIENDS",
        "$ONLINE_GAME_SEARCH_OPTION_RIVALS",
    };

    const char* const OnlineCustomMatch::KAPC_YES_NO_BUTTON_STRING_ID[2] =           // @0x82F266A4
    {
        "$GENERAL_OPTION_YES", "$GENERAL_OPTION_NO"
    };

    const char* const OnlineCustomMatch::KAPC_OK_BUTTON_STRING_ID[1] =               // @0x82F266AC
    {
        "$GENERAL_OPTION_OK"
    };

    // @0x8205E964 -- three { string id, BrnNetwork::ESearchGameModes } pairs:
    // any (0) / race (1) / freeburn lobby (4).
    const OnlineCustomMatch::StringGameModeMapping
    OnlineCustomMatch::KA_GAME_MODE_SEARCH_OPTION_STRING_IDS[3] =
    {
        { "$ONLINE_GAME_SEARCH_OPTION_ANY",         0 },
        { "$ONLINE_GAME_OPTION_MODE_RACE",          1 },
        { "$ONLINE_GAME_OPTION_MODE_FREEBURN_LOBBY", 4 },
    };

    const char OnlineCustomMatch::KAC_GAME_MODE_STRING_ID[25]              = "$ONLINE_GAME_OPTION_MODE";
    const char OnlineCustomMatch::KAC_NO_GAMES_FOUND_STRING_ID[29]         = "$ONLINE_GAME_SEARCH_NO_GAMES";
    const char OnlineCustomMatch::KAC_NO_PREVIOUS_GAME_MODE_STRING_ID[33]  = "$ONLINE_GAME_SEARCH_NO_PREV_MODE";
    const char OnlineCustomMatch::KAC_NUM_GAMES_FOUND_STRING_ID[35]        = "ONLINE_GAME_SEARCH_NUM_GAMES_FOUND";
    const char OnlineCustomMatch::KAC_NUM_GAMES_FOUND_SINGULAR_STRING_ID[43] = "ONLINE_GAME_SEACH_NUM_GAMES_FOUND_SINGULAR";   // X360 typo, kept
    const char OnlineCustomMatch::KAC_NUM_PLAYERS_STRING_ID[31]            = "ONLINE_GAME_SEARCH_NUM_PLAYERS";
    const char OnlineCustomMatch::KAC_OPPONENT_OPTION_STRING_ID[30]        = "$ONLINE_GAME_SEARCH_OPPONENTS";
    const char OnlineCustomMatch::KAC_SEARCHING_STRING_ID[30]              = "$ONLINE_GAME_SEARCH_SEARCHING";

    namespace
    {
        const char KAC_APT_TRANSITION_NAME[]  = "apt_Transition";
        const s32  KI_ANIMATION_STATE_VISIBLE   = 0;   // KAPC_ANIMATION_STATES[0]
        const s32  KI_ANIMATION_STATE_INVISIBLE = 1;   // KAPC_ANIMATION_STATES[1]
        const s32  KI_NUM_OK_BUTTONS            = 1;
        const s32  KI_OK_BUTTON_INDEX           = 0;
    }

    // ================================================================================
    //  ShowMessage  @ 0x82484A90  (39 insns; the ledger files it under the
    //  BrnAnimationComponent.h catch-all -- it is this class's own body)
    //
    //  Hide the buttons / search form / found-games table, show the button prompts and
    //  the message body, and put lpacTextID in it. Order is the console's.
    // ================================================================================
    void OnlineCustomMatch::ShowMessage(const char* lpacTextID)
    {
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                       KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                       false);
        mSearchParamsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                     false);
        mFoundGamesAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                   KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                   false);
        mButtonPromptAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                     false);
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                false);

        mMessageText.SetText(lpacTextID);
    }

    // ================================================================================
    //  ShowNoGamesFoundInGame  @ 0x8248BE68  (51 insns; same catch-all attribution)
    //
    //  The in-game flavour of "no games found": message + a single OK button over the
    //  hidden form and table, and the sub-state moves to NO_GAMES_FOUND_IN_GAME.
    // ================================================================================
    void OnlineCustomMatch::ShowNoGamesFoundInGame()
    {
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                false);
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                       KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                       false);
        mButtonPromptAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                     false);
        mSearchParamsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                     false);
        mFoundGamesAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                   KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                   false);

        mMessageText.SetText(KAC_NO_GAMES_FOUND_STRING_ID);

        // One live button, no wrap (X360 `li r4, 1` / `li r5, 0`), captioned OK.
        mMessageButtons.SetupMenu(KI_NUM_OK_BUTTONS, false);
        mMessageButtons.SetText(KI_OK_BUTTON_INDEX, KAPC_OK_BUTTON_STRING_ID[0]);

        meSubState = E_SUBSTATE_NO_GAMES_FOUND_IN_GAME;   // stw 7, 0x38
    }
}
