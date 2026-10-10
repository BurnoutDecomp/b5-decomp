// BrnGui::OnlinePause -- the online pause screen state.
//
//   OnEnter / OnLeave / Update, and the three helpers Update reaches: the load sequencer
//   (CheckForCompletedLoads), the controller handler and the overlay-complete handler.
//   Out-queue records are posted through GetOutputEventQueue()->AddEvent with their wire
//   sizes, on channel 40 (GUI out) unless noted.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePause.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest, GuiEventActivateCrashNav, GuiFlow
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                          // GuiTracker::ClearTracker

namespace BrnGui
{
    const CgsGui::sResourceTuple OnlinePause::maResourceTuplesToLoad[] =
        { { 169, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlinePause::miNumResourcesToLoad = 1;

    // The events the screen subscribes to, in the console's order: round finished, controller
    // action, gui cache, leave game, overlay complete, network disconnected.
    const s32 OnlinePause::maiEventToObserve[6] = { 320, 6, 64, 322, 189, 44 };
    const s32 OnlinePause::miNumEventsObserved  = 6;

    const char OnlinePause::KAC_PAUSE_OPTIONS_COMPONENT[7] = "Option";

    // The menu rows, top to bottom (indexed by the highlighted row).
    const char* const OnlinePause::KAPC_PAUSE_OPTION_STRING_IDS[OnlinePause::KI_NUM_COMPONENTS_TO_LOAD] =
    {
        "$ONLINE_PAUSE_OPTION_CONTINUE",
        "$ONLINE_PAUSE_OPTION_QUIT",
        "DEBUG Finish Round",
    };

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT      = 40;
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;

        // The in-queue events Update dispatches on (the maiEventToObserve set).
        const s32 KI_EVENT_CONTROLLER_INPUT     = 6;
        const s32 KI_EVENT_NETWORK_DISCONNECTED = 44;
        const s32 KI_EVENT_GUI_CACHE            = 64;
        const s32 KI_EVENT_OVERLAY_COMPLETE     = 189;
        const s32 KI_EVENT_ROUND_FINISHED       = 320;
        const s32 KI_EVENT_LEAVE_GAME           = 322;

        // The controller actions HandleControllerInput reacts to (payload word +4 of event 6).
        const s32 KI_ACTION_MENU_UP   = 41;
        const s32 KI_ACTION_MENU_DOWN = 42;
        const s32 KI_ACTION_BACK      = 45;
        const s32 KI_ACTION_SELECT    = 49;
        const s32 KI_ACTION_BACK_ALT  = 50;

        // The menu rows (KAPC_PAUSE_OPTION_STRING_IDS order).
        const s32 KI_OPTION_CONTINUE     = 0;
        const s32 KI_OPTION_QUIT         = 1;
        const s32 KI_OPTION_FINISH_ROUND = 2;

        // The out-queue ids: car control on/off, show/hide HUD, the debug finish-round
        // request, the leave-game-confirmed request and the front-end-screen-closed record.
        const s32 KI_GUI_EVENT_CAR_CONTROL_CHANGE = 65;
        const s32 KI_GUI_EVENT_SHOW_HIDE_HUD      = 148;
        const s32 KI_GUI_EVENT_DEBUG_FINISH_ROUND = 231;
        const s32 KI_GUI_EVENT_LEAVE_GAME         = 250;
        const s32 KI_GUI_EVENT_SCREEN_CLOSED      = 533;

        // The game modes that leave through the showtime post-event screens
        // (GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME / E_MODE_ONLINE_SHOWTIME).
        const s32 KI_MODE_OFFLINE_SHOWTIME = 2;
        const s32 KI_MODE_ONLINE_SHOWTIME  = 16;

        // The screen's apt movie is its own resource NAME (gGuiResourceIdentifier[169] ==
        // "ON_PAUSE", the same id as maResourceTuplesToLoad[0]); level 3. OnLeave unbinds the
        // level with the empty name.
        const s32         KI_RESOURCE_ID_ONLINE_PAUSE = 169;
        const s32         KI_APT_MOVIE_LEVEL          = 3;
        const char* const KPC_NO_MOVIE_NAME           = "";

        // The menu component's apt id argument (no parent, unset id).
        const u64 KU_NO_APT_ID = 0xFFFFFFFFull;

        // Overlay ids.
        const char KAC_LEAVE_GAME_QUESTION_OVERLAY[] = "GMOnlLvGmQn";
        const char KAC_LOST_CONNECTION_OVERLAY[]     = "OnLostConn";
        const char KAC_LEAVING_GAME_OVERLAY[]        = "CNOnlLvgGame";

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpCachePointer;
        };

        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04
        };

        struct GuiOverlayCompletePayload : public CgsModule::Event
        {
            CgsID                                mOverlayId;     // +0x00
            GuiOverlayCompleteEvent::LeaveMethod meLeaveMethod;  // +0x08
        };

        // ---- out-queue wire records ----------------------------------------------------
        // { 1, N, 12, <one payload byte> }: 16 bytes. The console leaves the byte unwritten on
        // the finish-round / leave-game / screen-closed records; zero here.
        template <s32 N>
        struct GuiByteEventWire : public CgsGui::GuiEvent<N>
        {
            u8 mu8Value;    // +0x0C
            u8 maPad[3];

            explicit GuiByteEventWire(u8 lu8Value)
                : CgsGui::GuiEvent<N>(static_cast<u32>(sizeof(u8)), 12)
                , mu8Value(lu8Value)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }
        };

        // { 288, 184, 16, <pad>, the 288-byte request }: 304 bytes.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C
            GuiOverlayRequest mRequest;   // +0x10

            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0)
            {
            }
        };

        static_assert(sizeof(GuiByteEventWire<1>) == 16, "byte record is 16 bytes");
        static_assert(sizeof(GuiOverlayRequestWire) == 304, "overlay request record is 304 bytes");
        static_assert(sizeof(GuiEventActivateCrashNav) == 20, "activate-crashnav record is 20 bytes");

        template <s32 N>
        void PostByteEvent(CgsGui::StateInterface* lpStateInterface, s32 liChannel, u8 lu8Value)
        {
            GuiByteEventWire<N> lRecord(lu8Value);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord), liChannel,
                static_cast<s32>(sizeof(lRecord)));
        }

        void PostActivateCrashNav(CgsGui::StateInterface* lpStateInterface, bool lbActivate)
        {
            GuiEventActivateCrashNav lRecord(lbActivate);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lRecord)));
        }

        void PostOverlayRequest(CgsGui::StateInterface* lpStateInterface, const char* lpcOverlayId)
        {
            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct(lpcOverlayId);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lWire)));
        }
    }

    // Subscribe, start loading the screen, hide the HUD (internal channel), stand the crash
    // nav down, take car control away, and forget any previous cache.
    void OnlinePause::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meSubState = E_SUBSTATE_LOADING_SCREEN;

        PostByteEvent<KI_GUI_EVENT_SHOW_HIDE_HUD>(mpStateInterface, KI_CHANNEL_GUI_INTERNAL, 0);
        PostActivateCrashNav(mpStateInterface, false);
        PostByteEvent<KI_GUI_EVENT_CAR_CONTROL_CHANGE>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        mpGuiCache = 0;
    }

    // Unbind the apt movie, stop listening, bring the crash nav back, post screen-closed and
    // give car control back.
    void OnlinePause::OnLeave()
    {
        mpStateInterface->PlayAptMovie(KPC_NO_MOVIE_NAME, KI_APT_MOVIE_LEVEL);
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        PostActivateCrashNav(mpStateInterface, true);
        PostByteEvent<KI_GUI_EVENT_SCREEN_CLOSED>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);
        PostByteEvent<KI_GUI_EVENT_CAR_CONTROL_CHANGE>(mpStateInterface, KI_CHANNEL_GUI_OUT, 1);
    }

    // Drain the in-queue, then update the menu and step the load sequencer.
    void OnlinePause::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            switch (liEventId)
            {
                case KI_EVENT_GUI_CACHE:
                {
                    const GuiEventCache* lpCacheEvent = reinterpret_cast<const GuiEventCache*>(lpEvent);
                    CGS_ASSERT(lpCacheEvent->mpCachePointer != 0, "lpCacheEvent->mpCachePointer");
                    if (mpGuiCache == 0)
                    {
                        mpGuiCache = lpCacheEvent->mpCachePointer;
                        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                        mPauseOptions.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
                    }
                    break;
                }

                case KI_EVENT_NETWORK_DISCONNECTED:
                    PostOverlayRequest(mpStateInterface, KAC_LOST_CONNECTION_OVERLAY);
                    break;

                case KI_EVENT_CONTROLLER_INPUT:
                    HandleControllerInput(lpEvent);
                    break;

                case KI_EVENT_OVERLAY_COMPLETE:
                    HandleOverlayComplete(lpEvent);
                    break;

                case KI_EVENT_LEAVE_GAME:
                    PostOverlayRequest(mpStateInterface, KAC_LEAVING_GAME_OVERLAY);
                    SendStateEvent("ADVANCE");
                    break;

                case KI_EVENT_ROUND_FINISHED:
                {
                    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                    CGS_ASSERT(mpGuiCache->GetGuiTracker() != 0, "mpGuiCache->GetGuiTracker()");
                    mpGuiCache->GetGuiTracker()->ClearTracker();

                    const s32  liGameMode = mpGuiCache->GetGameMode();
                    const bool lbShowtime = (liGameMode == KI_MODE_OFFLINE_SHOWTIME) ||
                                            (liGameMode == KI_MODE_ONLINE_SHOWTIME);
                    SendStateEvent(lbShowtime ? "TO_ST_POST" : "TO_ON_POST");
                    break;
                }

                default:
                    // The console streams "Unexpected event received : " << id << " in "
                    // << file << " at line " << line.
                    CGS_ASSERT(false, "Unexpected event received : ");
                    break;
            }
        }

        lpInQueue->Clear();

        mPauseOptions.Update();
        CheckForCompletedLoads();
    }

    // LOADING_SCREEN: once the screen's package is in, play its movie and wait for the menu's
    // apt components. LOADING_COMPONENTS: once they are all initialised, build and dress the
    // three-row menu and move to PROMPT. (The console reaches the cache unguarded in the first
    // state: the cache event is always drained by Update before this runs.)
    void OnlinePause::CheckForCompletedLoads()
    {
        switch (meSubState)
        {
            case E_SUBSTATE_LOADING_SCREEN:
                if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                         static_cast<u32>(miNumResourcesToLoad)))
                {
                    mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KI_RESOURCE_ID_ONLINE_PAUSE],
                                                   KI_APT_MOVIE_LEVEL);
                    meSubState = E_SUBSTATE_LOADING_COMPONENTS;
                }
                break;

            case E_SUBSTATE_LOADING_COMPONENTS:
                if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
                {
                    mPauseOptions.Construct(KAC_PAUSE_OPTIONS_COMPONENT, mpStateInterface,
                                            KI_NUM_COMPONENTS_TO_LOAD, 0, KU_NO_APT_ID);
                    mPauseOptions.SetupMenu(KI_NUM_COMPONENTS_TO_LOAD, true);
                    for (s32 liOption = 0; liOption < KI_NUM_COMPONENTS_TO_LOAD; ++liOption)
                    {
                        mPauseOptions.SetText(liOption, KAPC_PAUSE_OPTION_STRING_IDS[liOption]);
                    }
                    meSubState = E_SUBSTATE_PROMPT;
                }
                break;

            default:
                break;
        }
    }

    // PROMPT only: up/down move the highlight, back leaves, select acts on the highlighted row
    // (continue leaves; quit raises the leave-game question; the debug row asks for the round
    // to finish, then leaves).
    void OnlinePause::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        if (meSubState != E_SUBSTATE_PROMPT)
        {
            return;
        }

        switch (reinterpret_cast<const ControllerButtonPayload*>(lpEvent)->miButtonId)
        {
            case KI_ACTION_MENU_UP:
                mPauseOptions.HighlightPrevious();
                break;

            case KI_ACTION_MENU_DOWN:
                mPauseOptions.HighlightNext();
                break;

            case KI_ACTION_BACK:
            case KI_ACTION_BACK_ALT:
                SendStateEvent("GO_BACK");
                break;

            case KI_ACTION_SELECT:
                switch (mPauseOptions.GetHighlightedIndex())
                {
                    case KI_OPTION_CONTINUE:
                        SendStateEvent("GO_BACK");
                        break;

                    case KI_OPTION_QUIT:
                        PostOverlayRequest(mpStateInterface, KAC_LEAVE_GAME_QUESTION_OVERLAY);
                        break;

                    case KI_OPTION_FINISH_ROUND:
                        PostByteEvent<KI_GUI_EVENT_DEBUG_FINISH_ROUND>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);
                        SendStateEvent("GO_BACK");
                        break;

                    default:
                        CGS_ASSERT(false, "Invalid pause option selected");
                        break;
                }
                break;

            default:
                break;
        }
    }

    // The leave-game question was answered OK: ask the game to leave.
    void OnlinePause::HandleOverlayComplete(const CgsModule::Event* lpOverlayCompleteEvent)
    {
        CGS_ASSERT(lpOverlayCompleteEvent != 0, "lpOverlayCompleteEvent");

        const GuiOverlayCompletePayload* lpPayload =
            reinterpret_cast<const GuiOverlayCompletePayload*>(lpOverlayCompleteEvent);

        if (lpPayload->mOverlayId == CgsIDCompress(KAC_LEAVE_GAME_QUESTION_OVERLAY) &&
            lpPayload->meLeaveMethod == GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK)
        {
            PostByteEvent<KI_GUI_EVENT_LEAVE_GAME>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);
        }
    }
}
