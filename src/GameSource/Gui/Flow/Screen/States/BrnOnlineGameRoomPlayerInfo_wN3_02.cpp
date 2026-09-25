// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-N3 partfile 02: the free-burn challenge page.
//   ShouldShowButton
//   ShowChallengesOptions
//   HandleControllerInputChallengesSubState
//
// The page is a player-count toggle (2..8 players) over the challenge list component.
// Only the lobby host may start a challenge, and only when the chosen player count
// matches the lobby (or a debug "all challenges N-player" override is on) -- that is
// ShouldShowButton. Every highlight change the host makes is published to the other
// players as a GuiChallengeSelectedEvent (573); selecting starts the challenge, backing
// out withdraws the highlight.
//
// Records go out on the GUI channel as the console's StateInterface::OutputGuiEvent<T>
// builds them (a GuiEventWrapper<T, 40>); the in-tree OutputGuiEvent<T> passes the event
// straight through on a channel equal to its id, so this file posts the wrapper itself.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h" // the two debug player-count overrides
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // BrnGui::GuiChallengeSelectedEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // BrnGui::GuiEventNetworkGameParams
#include "SharedClasses/DataLists/ChallengeList.h"                        // ChallengeList::GetChallengeData
#include "SharedClasses/DataLists/ChallengeListEntry.h"                   // ChallengeListEntry::GetChallengeStyle

#include <cstring>   // std::memcpy (the audio payload re-box)

namespace BrnGui
{
    namespace
    {
        // The GUI-out channel every record here is posted on.
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // ---- controller actions this sub-state answers (event-6 payload word +4) --------
        const s32 KI_ACTION_LIST_PREVIOUS   = 0x29;   // ')' challenge list up
        const s32 KI_ACTION_LIST_NEXT       = 0x2A;   // '*' challenge list down
        const s32 KI_ACTION_PLAYERS_PREVIOUS = 0x2B;  // '+' player-count toggle back
        const s32 KI_ACTION_PLAYERS_NEXT    = 0x2C;   // ',' player-count toggle forward
        const s32 KI_ACTION_SELECT          = 0x31;   // '1'
        const s32 KI_ACTION_BACK            = 0x32;   // '2'

        // The GuiChallengeSelectedEvent selector actions. FLAG: the enum has no recovered
        // name; the values are the ones each site stores. A highlight is published with 2
        // when the list's button shows and 3 when it does not (followed by a 1 in that
        // case), backing out publishes 3 then 1, and selecting publishes 0.
        const s32 KI_SELECTOR_ACTION_SELECT        = 0;
        const s32 KI_SELECTOR_ACTION_CLEAR         = 1;
        const s32 KI_SELECTOR_ACTION_HIGHLIGHT     = 2;
        const s32 KI_SELECTOR_ACTION_UNHIGHLIGHT   = 3;

        // The player-count toggle: seven options, ids 2..8, labelled by the class's
        // KAPC_CHALLENGE_TOGGLE_ITEM_STRING_IDS. A lobby of one shows the 2-player row.
        const s32 KI_NUM_PLAYER_COUNT_OPTIONS = 7;
        const s32 KI_MIN_CHALLENGE_PLAYERS    = 2;

        // The back press's audio cue (action 7, "GO_BACK", empty component and movie).
        const s32  KI_AUDIO_ACTION_MENU_CUE = 7;
        const char KAC_SOUND_GO_BACK[]      = "GO_BACK";
        const char KAC_EMPTY_STRING[]       = "";

        // The console's OutputGuiEvent<T> body: box the payload in a GuiEventWrapper<T,40>
        // and queue the wrapper on the GUI-out channel.
        template <class TEvent>
        void OutputGuiEventRecord(CgsGui::StateInterface* lpStateInterface, TEvent& lrEvent)
        {
            CgsGui::GuiEventWrapper<TEvent, KI_CHANNEL_GUI_OUT> lRecord(lrEvent);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord), lRecord.GetChannel(),
                static_cast<s32>(sizeof(lRecord)));
        }

        // The audio trigger's wire payload: the 100-byte body of GuiAudioTriggerEvent
        // under id 457 (OutputGuiEvent<GuiAudioTriggerEvent> copies the payload out and
        // boxes it as { 100, 457, 12, payload }). The committed GuiAudioTriggerEvent keeps
        // a 12-byte header of its own in front of the payload, so the payload is re-boxed.
        struct GuiAudioTriggerPayload457
        {
            char macComponent[32];
            s32  meAction;
            char macLabel[32];
            char macMovie[32];
            s32 GetEventType() const { return 457; }
        };

        void PostAudioTrigger(CgsGui::StateInterface* lpStateInterface, s32 leAction,
                              const char* lpcLabel)
        {
            GuiAudioTriggerEvent lAudioTrigger;
            lAudioTrigger.Construct(leAction, KAC_EMPTY_STRING, lpcLabel, KAC_EMPTY_STRING);

            GuiAudioTriggerPayload457 lPayload;
            std::memcpy(&lPayload, lAudioTrigger.macComponent, sizeof(lPayload));
            OutputGuiEventRecord(lpStateInterface, lPayload);
        }

        static_assert(sizeof(GuiAudioTriggerPayload457) == 100, "audio trigger payload is 100 bytes");
        static_assert(sizeof(CgsGui::GuiEventWrapper<GuiChallengeSelectedEvent, 40>) == 32,
                      "573 record is 32 bytes");
        static_assert(sizeof(CgsGui::GuiEventWrapper<GuiEventNetworkGameParams, 40>) == 492,
                      "257 record is 492 bytes");
    }

    // ------------------------------------------------------------ ShouldShowButton
    // The host may start the highlighted challenge when the toggle's player count equals
    // the lobby's (either debug override lifts that requirement).
    bool OnlineGameRoomPlayerInfo::ShouldShowButton()
    {
        bool lbShowButton = false;

        if (mpGuiCache->IsLocalPlayerHost())
        {
            if (BrnGameState::ChallengeManager::mbChallengesAreAllOnePlayer ||
                BrnGameState::ChallengeManager::mbChallengesAreAllTwoPlayer ||
                mpGuiCache->GetNumActivePlayers() == static_cast<s32>(mChallengeToggle.GetHighlightedId()))
            {
                lbShowButton = true;
            }
        }

        return lbShowButton;
    }

    // ------------------------------------------------------------ ShowChallengesOptions
    // Build the player-count toggle on the lobby's size, set the list up for it, show the
    // scroll arrows that apply, and (host only, when startable) publish the highlight.
    void OnlineGameRoomPlayerInfo::ShowChallengesOptions()
    {
        u64 laSelectableIDs[KI_NUM_PLAYER_COUNT_OPTIONS];
        for (s32 liIndex = 0; liIndex < KI_NUM_PLAYER_COUNT_OPTIONS; ++liIndex)
        {
            laSelectableIDs[liIndex] = static_cast<u64>(KI_MIN_CHALLENGE_PLAYERS + liIndex);
        }

        mChallengeToggle.Clear();
        mChallengeToggle.SetActive(true);
        mChallengeToggle.SetupMenuToggle(KI_NUM_PLAYER_COUNT_OPTIONS, true,
                                         KAC_CHALLENGE_TOGGLE_TITLE_STRING_ID,
                                         const_cast<const char**>(KAPC_CHALLENGE_TOGGLE_ITEM_STRING_IDS),
                                         laSelectableIDs);
        mChallengeToggle.SetHighlightable(true);
        mChallengeToggle.SetHighlighted(true);
        mChallengeToggle.SetSelectable(true);

        const s32 liNumPlayers = static_cast<s32>(mpGuiCache->GetNumActivePlayers());
        const s32 liToggleId   = (liNumPlayers == 1) ? KI_MIN_CHALLENGE_PLAYERS : liNumPlayers;
        if (mChallengeToggle.mItemText.HighlightId(static_cast<u64>(static_cast<s64>(liToggleId))))
        {
            mChallengeToggle.SetDirty();
        }

        mChallengeListComponent.Setup(mpGuiCache,
                                      static_cast<s32>(mChallengeToggle.GetHighlightedId()),
                                      ShouldShowButton());

        mChallengeListUpArrowAnimation.Run(mChallengeListComponent.IsAtTopOfList()
                                               ? KAPC_ANIMATION_STATES[1] : KAPC_ANIMATION_STATES[0]);
        mChallengeListDownArrowAnimation.Run(mChallengeListComponent.IsAtBottomOfList()
                                                 ? KAPC_ANIMATION_STATES[1] : KAPC_ANIMATION_STATES[0]);

        if (mChallengeListComponent.GetHighlightedChallengeID() != 0 && ShouldShowButton())
        {
            GuiChallengeSelectedEvent lChallengeSelected;
            lChallengeSelected.miSelectorAction = KI_SELECTOR_ACTION_HIGHLIGHT;
            lChallengeSelected.mChallengeID     = mChallengeListComponent.GetHighlightedChallengeID();
            lChallengeSelected.miChall = static_cast<s32>(
                mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeSelected.mChallengeID)
                    ->GetChallengeStyle());
            OutputGuiEventRecord(mpStateInterface, lChallengeSelected);
        }
    }

    // ------------------------------------------------------------ HandleControllerInputChallengesSubState
    void OnlineGameRoomPlayerInfo::HandleControllerInputChallengesSubState(const CgsModule::Event* lpEvent)
    {
        // The header-stripped GuiEventControllerInputPressed payload: pad id, action id.
        const s32 liAction = reinterpret_cast<const s32*>(lpEvent)[1];

        bool lbListChanged = false;

        switch (liAction)
        {
            case KI_ACTION_LIST_PREVIOUS:
                lbListChanged = mChallengeListComponent.HighlightPrevious();
                break;

            case KI_ACTION_LIST_NEXT:
                lbListChanged = mChallengeListComponent.HighlightNext();
                break;

            case KI_ACTION_PLAYERS_PREVIOUS:
                if (mChallengeToggle.HighlightPrevious())
                {
                    const bool lbShowButton = ShouldShowButton();
                    mChallengeListComponent.Setup(mpGuiCache,
                                                  static_cast<s32>(mChallengeToggle.GetHighlightedId()),
                                                  lbShowButton);
                    lbListChanged = true;
                }
                break;

            case KI_ACTION_PLAYERS_NEXT:
                if (mChallengeToggle.HighlightNext())
                {
                    const bool lbShowButton = ShouldShowButton();
                    mChallengeListComponent.Setup(mpGuiCache,
                                                  static_cast<s32>(mChallengeToggle.GetHighlightedId()),
                                                  lbShowButton);
                    lbListChanged = true;
                }
                break;

            case KI_ACTION_SELECT:
            {
                GuiChallengeSelectedEvent lChallengeSelected;
                lChallengeSelected.mChallengeID     = mChallengeListComponent.GetHighlightedChallengeID();
                lChallengeSelected.miSelectorAction = KI_SELECTOR_ACTION_SELECT;

                if (lChallengeSelected.mChallengeID != 0 && mChallengeListComponent.mbShowButton)
                {
                    lChallengeSelected.miChall =
                        mChallengeListComponent.GetChallengeStyle(lChallengeSelected.mChallengeID);
                    OutputGuiEventRecord(mpStateInterface, lChallengeSelected);

                    // Starting a challenge turns the lobby into the free-burn lobby mode if
                    // it is not one already.
                    if (mpGuiCache->GetOnlineGameMode() !=
                        BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
                    {
                        GuiEventNetworkGameParams lGameOptions =
                            *reinterpret_cast<const GuiEventNetworkGameParams*>(
                                mpGuiCache->maOnlineGameModeOptionsStorage);
                        lGameOptions.meGameMode =
                            BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY;
                        OutputGuiEventRecord(mpStateInterface, lGameOptions);
                    }

                    mChallengeToggle.Unloaded();
                    UnloadChallengeResources();
                    HideChallengePage();
                }
                break;
            }

            case KI_ACTION_BACK:
            {
                PostAudioTrigger(mpStateInterface, KI_AUDIO_ACTION_MENU_CUE, KAC_SOUND_GO_BACK);

                if (mChallengeListComponent.GetHighlightedChallengeID() != 0 &&
                    mChallengeListComponent.mbShowButton)
                {
                    GuiChallengeSelectedEvent lChallengeSelected;
                    lChallengeSelected.mChallengeID     = mChallengeListComponent.GetHighlightedChallengeID();
                    lChallengeSelected.miSelectorAction = KI_SELECTOR_ACTION_UNHIGHLIGHT;
                    lChallengeSelected.miChall =
                        mChallengeListComponent.GetChallengeStyle(lChallengeSelected.mChallengeID);
                    OutputGuiEventRecord(mpStateInterface, lChallengeSelected);

                    lChallengeSelected.miSelectorAction = KI_SELECTOR_ACTION_CLEAR;
                    OutputGuiEventRecord(mpStateInterface, lChallengeSelected);
                }

                if (mbShownFromPause)
                {
                    mChallengeToggle.Unloaded();
                    UnloadChallengeResources();
                    HideHUDAndEnterMenus();
                    ShowPauseScreen();
                }
                else
                {
                    UnloadChallengeResources();
                    HideChallengePage();
                }
                break;
            }

            default:
                break;
        }

        if (lbListChanged)
        {
            mChallengeListUpArrowAnimation.Run(mChallengeListComponent.IsAtTopOfList()
                                                   ? KAPC_ANIMATION_STATES[1] : KAPC_ANIMATION_STATES[0]);
            mChallengeListDownArrowAnimation.Run(mChallengeListComponent.IsAtBottomOfList()
                                                     ? KAPC_ANIMATION_STATES[1] : KAPC_ANIMATION_STATES[0]);

            if (mChallengeListComponent.GetHighlightedChallengeID() != 0 && mpGuiCache->IsLocalPlayerHost())
            {
                GuiChallengeSelectedEvent lChallengeSelected;
                lChallengeSelected.miSelectorAction = mChallengeListComponent.mbShowButton
                                                          ? KI_SELECTOR_ACTION_HIGHLIGHT
                                                          : KI_SELECTOR_ACTION_UNHIGHLIGHT;
                lChallengeSelected.mChallengeID = mChallengeListComponent.GetHighlightedChallengeID();
                lChallengeSelected.miChall = static_cast<s32>(
                    mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeSelected.mChallengeID)
                        ->GetChallengeStyle());
                OutputGuiEventRecord(mpStateInterface, lChallengeSelected);

                if (!mChallengeListComponent.mbShowButton)
                {
                    lChallengeSelected.miSelectorAction = KI_SELECTOR_ACTION_CLEAR;
                    OutputGuiEventRecord(mpStateInterface, lChallengeSelected);
                }
            }
        }
    }
}
