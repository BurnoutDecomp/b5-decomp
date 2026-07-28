// ===================================================================================
// BrnGui::CarSelectMain -- wave-G partfile 03: the per-frame pump.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCarSelectMain_wG_03.cpp
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   GetResourcesToLoadForCarSelect @0x824B56C0
//   Update                         @0x824DC9C0
//   ProcesssIncomingEvents         @0x824D73D8   (the triple-s spelling is the original)
//
// Companion partfiles hold the rest of the class (BrnCarSelectMain.cpp and
// BrnCarSelectMain_wG_01/_02.cpp); the owning header is BrnCarSelectMain.h, whose banner
// carries the X360 vtable slot map every dispatch site below is derived from:
//   +0x24 GetResourcesToLoad() (0-arg)  +0x28 GetResourcesToLoadForCarSelect
//   +0x2C IsLoading  +0x30 PlayMovie  +0x34 AppendAptComponents  +0x38 SetupComponents
//   +0x3C HandleControllerInput  +0x40 UpdateGuiCache  +0x44 HandleCarInfoResponseEvent
//   +0x48 HandleCarAudioLoadComplete  +0x4C HandlePlayerInfoResponse
//   +0x50 HandleUnlockedLiveryResponseEvent  +0x54 HandlePlayerCarColourResponseEvent
//   +0x58 SetupCar  +0x5C ExitCarSelection  +0x60 GetNumberResourcesToLoad
// The x64 build lays out its own vtable; every call below is a plain by-name (virtual)
// call and the slot map only records WHICH member each X360 displacement resolved to.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectMain.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID (u64 car ids)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue / Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::StopLoadingScreen
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (+ GuiFlow selector)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::EGameModeType

// NOTE on the event records: the canonical payload homes for events 93/406 are
// GameSource/Gui/BrnGuiDemangledEventTypes.h, but that header and
// GameSource/Gui/BrnGuiEventTypeDefs.h are MUTUALLY EXCLUSIVE by construction (both define
// GuiEventRunFsm / GuiAudioTriggerEvent / GuiOverlayWaitFinishRequest -- see the banner in
// the demangled header), and BrnGuiCache.h pulls BrnGuiEventTypeDefs.h in. So the two
// payload fields this TU reads are modelled here as local in-queue VIEWS, exactly as the
// sibling screen state does (BrnInGame.cpp "in-queue payload views").

namespace BrnGui
{
    namespace
    {
        // The state's inbound GUI queue. CgsGui::State holds it as an opaque
        // InputBuffer::GuiEventQueue*; the X360 drains it through
        // CgsModule::VariableEventQueue<18432,16>::GetFirstEvent / GetNextEvent / Clear
        // (the calls at 0x824D73F4 / 0x824D7D74 / 0x824D7D8C name the instantiation).
        // Same idiom as the sibling screen states (BrnInGame.cpp, BrnPauseScreen.cpp).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- observed event ids (the dispatch labels; the registered set is
        //      CarSelectMain::maiEventToObserve[21], defined in _wG_01) ------------------
        const s32 KI_EVENT_CONTROLLER_FIRST        = 5;    // 5..8: one id per controller port
        const s32 KI_EVENT_CONTROLLER_LAST         = 8;
        const s32 KI_EVENT_NETWORK_DISCONNECTED    = 44;
        const s32 KI_EVENT_ONLINE_LAUNCHING        = 57;
        const s32 KI_EVENT_ONLINE_LAUNCHED         = 58;
        const s32 KI_EVENT_GUI_CACHE               = 64;
        const s32 KI_EVENT_CAR_SELECT_START        = 81;   // GuiCarSelectStartEvent
        const s32 KI_EVENT_CAR_SELECT_EXIT         = 83;
        const s32 KI_EVENT_CAR_SELECT_ABORT        = 84;   // GuiCarSelectAbortEvent
        const s32 KI_EVENT_PREPARE_FOR_MODE_START  = 93;   // GuiEventPrepareForModeStart
        const s32 KI_EVENT_OVERLAY_COMPLETE        = 189;  // registered (maiEventToObserve[15]); handled as a no-op
        const s32 KI_EVENT_TO_CAR_SELECT           = 271;
        const s32 KI_EVENT_NETWORK_LEFT_GAME       = 273;  // GuiEventNetworkLeftGame
        const s32 KI_EVENT_PLAYER_INFO_RESPONSE    = 406;  // GuiPlayerInfoResponse
        const s32 KI_EVENT_CAR_SELECTION           = 412;  // GuiCarSelectionEvent
        const s32 KI_EVENT_CAR_UNLOCKED_LIVERY     = 413;  // GuiCarUnlockedLiveryEvent
        const s32 KI_EVENT_PLAYER_CAR_COLOUR       = 414;  // GuiPlayerCarColourResponse
        const s32 KI_EVENT_CAR_READY_TO_EXIT       = 564;  // GuiCarSelectReadyToExitEvent
        const s32 KI_EVENT_CAR_CHANGED_DROP_IN     = 565;  // GuiCarSelectionChangedDropIn
        const s32 KI_EVENT_CAR_CHANGED_ONLINE      = 566;  // GuiCarSelectionChangedOnline

        // The GuiCarSelectStartEvent payload word that means "go to online car select".
        const s32 KI_CAR_SELECT_START_ONLINE = 2;

        // CgsDev::Message log-category bit the three SendStateEvent traces are gated on.
        const u64 KU_MESSAGE_FILTER_DEFAULT = 1;

        // ---- in-queue payload views (see the include-set note above) -----------------
        // GuiPlayerInfoResponse (id 406, 64 bytes = the 12-byte CgsGui::GuiEvent<406>
        // header + a 52-byte payload). The only field this state reads is the responding
        // player's car id at +0x20 (X360 `ld r11, 0x20(r30)`); the rest stays opaque. The
        // 12-byte header + 20 opaque bytes put mCarId at +0x20 on x64 too (align 8, no
        // inserted padding).
        struct GuiPlayerInfoResponseView : public CgsGui::GuiEvent<406>
        {
            u8    maPayload0[20];   // +0x0C..+0x1F (opaque)
            CgsID mCarId;           // +0x20
            u8    maPayload1[24];   // +0x28..+0x3F (opaque)
        };

        // GuiEventPrepareForModeStart (id 93, 152 bytes = the 12-byte
        // CgsGui::GuiEvent<93> header + a 140-byte payload). The mode type is the first
        // payload word (X360 `lwz r11, 0xC(r30)`).
        struct GuiPrepareForModeStartView : public CgsGui::GuiEvent<93>
        {
            s32 meGameModeType;     // +0x0C (GsmIO::EGameModeType)
            u8  maPayload[136];     // +0x10..+0x97 (opaque)
        };
    }

    // ---- GetResourcesToLoadForCarSelect @ 0x824B56C0 -------------------------------
    // Pure forwarder onto the two 0-arg resource getters the derived car-select states
    // override (X360 vtable +0x24 / +0x60).
    void CarSelectMain::GetResourcesToLoadForCarSelect(const CgsGui::sResourceTuple** lppResourceTuples,
                                                       u32* lpuNumberOfResources) const
    {
        CGS_ASSERT(lppResourceTuples != 0, "Invalid pointer");    // cpp:955
        CGS_ASSERT(lpuNumberOfResources != 0, "Invalid pointer"); // cpp:956

        *lppResourceTuples    = GetResourcesToLoad();
        *lpuNumberOfResources = GetNumberResourcesToLoad();
    }

    // ---- Update @ 0x824DC9C0 -------------------------------------------------------
    // The load/interactive state machine. Every path -- including the two early bails and
    // the unhandled-state assert -- falls through to ProcesssIncomingEvents().
    void CarSelectMain::Update()
    {
        switch (meCurrentState)
        {
        case E_CARSELECT_UNLOADED:
        {
            // Nothing can be done until the cache-ready event (64) has landed; keep
            // pumping the queue until it does.
            if (!mpGuiCache)
                break;

            mpStateInterface->StopLoadingScreen();

            // The X360 pre-zeroes only the list pointer (stw of r10==0 into var_40); the
            // count slot is written by the callee.
            const CgsGui::sResourceTuple* lpResourceTuples = 0;
            u32 luNumberOfResources;
            GetResourcesToLoadForCarSelect(&lpResourceTuples, &luNumberOfResources);

            if (luNumberOfResources != 0)
            {
                CGS_ASSERT(lpResourceTuples != 0,
                           "resource count > 1, but resource list pointer is zero");   // cpp:246

                // Still streaming -- stay in this state and re-try next frame.
                if (!mpGuiCache->EnsureResourcesAreLoaded(lpResourceTuples, luNumberOfResources))
                    break;

                PlayMovie();
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                AppendAptComponents();
            }

            meCurrentState = E_CARSELECT_LOADING_COMPONENTS;
            break;
        }

        case E_CARSELECT_LOADING_COMPONENTS:
            // "aquired" is the original spelling baked into the image.
            CGS_ASSERT(mpGuiCache != 0,
                       "Should have aquired GuiCache in the previous state");   // cpp:269

            if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                SetupComponents();
                meCurrentState = E_CARSELECT_VISIBLE_INTERACTIVE;
            }
            break;

        case E_CARSELECT_VISIBLE_INTERACTIVE:
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:280

            // Full 64-bit id compare (X360 ld/ld + cmpld): a pending selection is anything
            // whose desired car id differs from the committed one.
            if (mCurrentSetupInfo.mCarId != mDesiredSetupInfo.mCarId)
                TriggerSetupCar();
            break;

        default:
            // E_CARSELECT_INVALID (-1) and anything >= E_CARSELECT_COUNT land here.
            CGS_ASSERT(false, "Invalid, unhandled state");   // cpp:291
            break;
        }

        ProcesssIncomingEvents();
    }

    // ---- ProcesssIncomingEvents @ 0x824D73D8 ---------------------------------------
    // Drain the state's inbound GUI queue, dispatch the observed events, then clear it.
    // The loop aborts the moment a handler has invalidated the state (a state event that
    // takes the FSM elsewhere), leaving the remaining events for the Clear() below.
    void CarSelectMain::ProcesssIncomingEvents()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (meCurrentState == E_CARSELECT_INVALID)
                break;

            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_FIRST:
            case KI_EVENT_CONTROLLER_FIRST + 1:
            case KI_EVENT_CONTROLLER_FIRST + 2:
            case KI_EVENT_CONTROLLER_LAST:
                // Input is only accepted once the screen is up and interactive. The event
                // id doubles as the controller selector (the X360 leaves it live in r5).
                if (meCurrentState > E_CARSELECT_LOADING_COMPONENTS)
                    HandleControllerInput(lpEvent, liEventId);
                break;

            case KI_EVENT_NETWORK_DISCONNECTED:
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:431

                // Latch the error word the disconnect popup will show (0 when the event
                // carried no payload) -- GuiCache::SetDoDisconnectPopup, inlined here.
                mpGuiCache->SetDoDisconnectPopup(lpEvent);

                if ((CgsDev::Message::gxMessageFilterFlags & KU_MESSAGE_FILTER_DEFAULT) != 0)
                    *CgsDev::Log::gpDebugPrint << "RG :: CSM : SendStateEvent( \"DISCONNECT\" ) 1\n";
                SendStateEvent("DISCONNECT");
                break;

            case KI_EVENT_ONLINE_LAUNCHING:
                HandleLaunchingEvent(lpEvent);
                break;

            case KI_EVENT_ONLINE_LAUNCHED:
                HandleLaunchedEvent(lpEvent);
                break;

            case KI_EVENT_GUI_CACHE:
                UpdateGuiCache(lpEvent, liEventId);
                break;

            case KI_EVENT_CAR_SELECT_START:
                // GuiCarSelectStartEvent: the leading word selects the flow to enter.
                // External event blob -- its layout is fixed by the producer, so the
                // leading word is read positionally (X360 lwz r11, 0(r30)).
                if (*reinterpret_cast<const s32*>(lpEvent) == KI_CAR_SELECT_START_ONLINE)
                {
                    if ((CgsDev::Message::gxMessageFilterFlags & KU_MESSAGE_FILTER_DEFAULT) != 0)
                        *CgsDev::Log::gpDebugPrint << "RG :: CSM : SendStateEvent( \"TO_ON_CARSEL\" )\n";
                    SendStateEvent("TO_ON_CARSEL");
                }
                break;

            case KI_EVENT_CAR_SELECT_EXIT:
                ExitCarSelection();
                break;

            case KI_EVENT_CAR_SELECT_ABORT:
                // GuiCarSelectAbortEvent: a single leading byte (X360 lbz r11, 0(r30));
                // zero means "accept the current selection".
                if (*reinterpret_cast<const u8*>(lpEvent) == 0)
                {
                    if ((CgsDev::Message::gxMessageFilterFlags & KU_MESSAGE_FILTER_DEFAULT) != 0)
                        *CgsDev::Log::gpDebugPrint << "RG :: CSM : SendStateEvent( \"ACCEPT\" ) 1\n";
                    SendStateEvent("ACCEPT");
                }
                break;

            case KI_EVENT_PREPARE_FOR_MODE_START:
            {
                // GuiEventPrepareForModeStart: the mode type is the first payload word
                // after the 12-byte GuiEvent<93> header (X360 lwz r11, 0xC(r30)).
                const GuiPrepareForModeStartView* lpModeEvent =
                    reinterpret_cast<const GuiPrepareForModeStartView*>(lpEvent);
                const s32 liGameModeType = lpModeEvent->meGameModeType;

                // NOTE: no mpGuiCache null-check on this path in the X360 build.
                if ((liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
                     liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) &&
                    !mpGuiCache->IsStartingGameDueToPlayerJoin())
                {
                    SendStateEvent("ENTER_GAME");
                }
                break;
            }

            case KI_EVENT_OVERLAY_COMPLETE:
                // Registered but handled as a no-op. I re-dumped maiEventToObserve
                // @0x82065E80 big-endian: 21 words (the count at 0x82065ED4 is 21), and 189
                // IS index 15 of that set -- the sibling wG_01 table already lists it as
                // "overlay complete (189)". The X360 pivots on it at 0x824D7470
                // (cmpwi r5,0xBD / beq default), which is MSVC's three-way form for a real,
                // empty case. (414 is the value that is NOT registered.)
                break;

            case KI_EVENT_TO_CAR_SELECT:
                SendStateEvent("TO_CSELECT");
                break;

            case KI_EVENT_NETWORK_LEFT_GAME:
                HandleLeftGameEvent(lpEvent);
                break;

            case KI_EVENT_PLAYER_INFO_RESPONSE:
            {
                // GuiPlayerInfoResponse: adopt the car the responding player is on. The
                // committed id is copied straight into the desired slot (so no
                // TriggerSetupCar fires for it) and remembered as the drop-in car.
                const GuiPlayerInfoResponseView* lpResponse =
                    reinterpret_cast<const GuiPlayerInfoResponseView*>(lpEvent);

                mCurrentSetupInfo.mCarId = lpResponse->mCarId;
                mDesiredSetupInfo        = mCurrentSetupInfo;   // id + selectable (X360 two-qword copy)
                miMostRecentDropInId     = lpResponse->mCarId;

                HandlePlayerInfoResponse(lpEvent, liEventId);
                break;
            }

            case KI_EVENT_CAR_SELECTION:
                HandleCarInfoResponseEvent(lpEvent, liEventId);
                break;

            case KI_EVENT_CAR_UNLOCKED_LIVERY:
                HandleUnlockedLiveryResponseEvent(lpEvent, liEventId);
                break;

            case KI_EVENT_PLAYER_CAR_COLOUR:
                HandlePlayerCarColourResponseEvent(lpEvent, liEventId);
                break;

            case KI_EVENT_CAR_READY_TO_EXIT:
                HandleCarAudioLoadComplete();
                break;

            case KI_EVENT_CAR_CHANGED_DROP_IN:
            case KI_EVENT_CAR_CHANGED_ONLINE:
                // Both records lead with the newly selected car id (X360 ld r11, 0(r30));
                // the pending car change is satisfied by the notification itself.
                mbCarChangeInProgress = false;
                miMostRecentDropInId  = *reinterpret_cast<const CgsID*>(lpEvent);
                break;

            default:
                break;
            }
        }

        lpInQueue->Clear();
    }
}
