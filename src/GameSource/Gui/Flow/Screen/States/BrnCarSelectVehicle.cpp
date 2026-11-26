#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCarSelectVehicle.cpp"
// ===================================================================================
// BrnGui::CarSelectVehicle  -- partfile 01: statics + the FSM/resource surface
//   class:BrnGui::CarSelectVehicle
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm + pseudocode):
//   CarSelectVehicle (ctor)   @ 0x82508670
//   Construct                 @ 0x824BEBF0   (DWARF cpp:113)
//   OnEnter                   @ 0x824C9470   (DWARF cpp:138)
//   OnLeave                   @ 0x824C9938   (DWARF cpp:229)
//   Update                    @ 0x824DCBF0   (DWARF cpp:245)
//   AppendAptComponents       @ 0x824B57F8   (DWARF cpp:328)
//   PlayMovie                 @ 0x824C9DE8   (DWARF cpp:1338)
//   GetResourcesToLoad        @ 0x824B5A08   (DWARF cpp:1354)
//   GetNumberResourcesToLoad  @ 0x824B5A18   (DWARF cpp:1367)
//   IsLoading                 @ 0x824C14B8   (DWARF cpp:1475)
//   HandleAptTrigger          @ 0x824B5918   (DWARF cpp:1287)
//   IsCarSelectable           (DWARF cpp:488 -- inlined at every X360 call site)
//
// The screen-build helpers live in BrnCarSelectVehicle_Components.cpp and the input /
// selection / event handlers in BrnCarSelectVehicle_Input.cpp; see BrnCarSelectVehicle.h
// for the layout proof and the vtable slot map.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h"

#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (register / play-apt / out-queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / VariableEventQueue
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (debug print)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"  // GuiEventAptTriggerPayload
#include "GameSource/Gui/BrnGuiShared.h"
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (+ GuiFlow)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiOverlayRequest (the ticker wire)
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry

#include <cstring>   // std::strstr

namespace BrnGui
{
    namespace
    {
        // The AddEvent channel every out-queue record below uses (GuiEventOut). Same
        // constant the sibling screen states carry.
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The state's inbound GUI queue: CgsGui::State holds it as an opaque
        // InputBuffer::GuiEventQueue*, the X360 drains it through
        // CgsModule::VariableEventQueue<18432,16>::GetFirstEvent / GetNextEvent (the calls
        // at 0x824DCC0C / 0x824DCCB0 name the instantiation). Same idiom as CarSelectMain.
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- the five observed event ids (maiEventToObserve, dispatched by Update) ------
        const s32 KI_EVENT_APT_TRIGGER          = 21;
        const s32 KI_EVENT_COUNTDOWN_TIME       = 82;
        const s32 KI_EVENT_LOBBY_PLAYER_LIST    = 244;
        const s32 KI_EVENT_VOICE_OVER_STARTED   = 466;
        const s32 KI_EVENT_VOICE_OVER_FINISHED  = 467;

        // ---- out-queue wire records (the { size, id, offset, payload } shape every
        //      OutputGuiEvent<T> instantiation builds; see BrnCarSelectMain_wG_02.cpp) ----

        // The two-flag ticker command OnEnter posts as id 409 and SetupComponents /
        // SetTicker post as id 536: { 2, <id>, 12, u8, u8 } on channel 40, 16 bytes.
        struct GuiTickerFlagsPayload
        {
            u8 mbFlagA;   // +0x00 (both call sites store 1)
            u8 mbFlagB;   // +0x01 (both call sites store 0)
        };

        // id 409 -- posted by OnEnter right after mTitleText is Constructed.
        struct GuiTickerFlagsWire409 : public CgsGui::GuiEvent<409>
        {
            GuiTickerFlagsPayload mPayload;   // +0x0C
            GuiTickerFlagsWire409()
            {
                mPayload.mbFlagA = 1;
                mPayload.mbFlagB = 0;
            }
        };

        // ---- in-queue payload views (the queue delivers the header-stripped payload) ----

        // Event 82 (online countdown time): one float at +0x00.
        struct GuiCountdownTimePayload : public CgsModule::Event
        {
            f32 mfTimeLeft;   // +0x00
        };
    }

    // ================================================================================
    // statics
    // ================================================================================

    // @0x82F26CF8 -- re-dumped from the image this wave. Six APT resources; the array ends
    // exactly at 0x82F26D28 where mpacCarSelectorName ("CarName") begins.
    //   149 BrnCarSelectMain, 52 B5RivalIcon, 70 B5CarsIcon,
    //    94 B5OnlineCarSelectComponents, 54 B5CarouselScrollBar, 55 B5ManufacturersIcon
    CgsGui::sResourceTuple       CarSelectVehicle::maResourcesToLoad[6] =
    {
        { E_GUI_RESOURCEID_APT_CARSELECT_MAIN, CgsGui::E_GUI_RESOURCETYPE_APT },
        {  52u, CgsGui::E_GUI_RESOURCETYPE_APT },
        {  70u, CgsGui::E_GUI_RESOURCETYPE_APT },
        {  94u, CgsGui::E_GUI_RESOURCETYPE_APT },
        {  54u, CgsGui::E_GUI_RESOURCETYPE_APT },
        {  55u, CgsGui::E_GUI_RESOURCETYPE_APT },
    };
    const u32 CarSelectVehicle::muNumResourcesToLoad = 6;

    // @0x82065F40 -- dumped big-endian. Self-corroborating: these are exactly the five ids
    // Update dispatches (21 apt trigger, 82 countdown time, 244 lobby player list,
    // 466 voice-over started, 467 voice-over finished).
    const s32 CarSelectVehicle::maiEventToObserve[5] = { 21, 82, 244, 466, 467 };
    const s32 CarSelectVehicle::miNumEventsObserved  = 5;

    // @0x82F26D28 -> "CarName".
    const char* CarSelectVehicle::mpacCarSelectorName = "CarName";

    const char CarSelectVehicle::KAC_CARS_AVAILABLE_STRINGID[21]      = "CAR_SELECT_AVAILABLE";
    const char CarSelectVehicle::KAC_SPEED_STATS_BAR_NAME[17]         = "SpeedStatsBar_mc";
    const char CarSelectVehicle::KAC_BOOST_STATS_BAR_NAME[17]         = "BoostStatsBar_mc";
    const char CarSelectVehicle::KAC_STRENGTH_STATS_BAR_NAME[20]      = "StrengthStatsBar_mc";
    const char CarSelectVehicle::KAC_CARS_UNLOCKED_NAME[16]           = "CarsUnlocked_mc";
    const char CarSelectVehicle::KAC_CAR_TYPE[11]                     = "CarType_mc";
    const char CarSelectVehicle::KAC_SLIDER_BAR_NAME[21]              = "CarouselScrollBar_mc";
    const char CarSelectVehicle::KAC_ONLINE_COUNTDOWN_NAME[13]        = "Countdown_mc";
    const char CarSelectVehicle::KAC_ONLINE_PLAYER_LIST[15]           = "PlayerTable_mc";
    const char CarSelectVehicle::macTrackAnimTransitionComponentName[14] = "TrackAnim_mcp";
    const char CarSelectVehicle::macMainAnimComponentName[13]         = "MainAnim_mcp";

    // @0x82066030 (12 bytes, read out of the image): the per-car-type gauge colours.
    const u32 CarSelectVehicle::mauBoostColours[3] =
    {
        0x00FFD200u,   // E_CARTYPE_DANGER
        0x00990000u,   // E_CARTYPE_AGGRESSION
        0x00176A12u,   // E_CARTYPE_STUNTS
    };

    // @0x8206603C..0x8206604C, read out of the image.
    const f32 CarSelectVehicle::KC_CAROUSEL_X_ADVANCE           = 165.0f;
    const f32 CarSelectVehicle::KC_SCREEN_WIDTH                 = 1280.0f;
    const f32 CarSelectVehicle::KC_X_FRAME_CAROUSEL_ADJUST      = 13.0f;
    const f32 CarSelectVehicle::KC_X_FRAME_CAROUSEL_DECAY_ADJUST = 3.25f;
    const f32 CarSelectVehicle::KF_AXIS_DEAD_ZONE               = 0.15f;

    // The console's two file-scope counters (dword_82FB4958 / dword_82FB495C).
    s32 CarSelectVehicle::gsiNumCarouselCars      = 0;
    s32 CarSelectVehicle::gsiNumCarsUnlockedTotal = 0;

    // ================================================================================
    // ctor / Construct
    // ================================================================================

    // ---- CarSelectVehicle (ctor) @ 0x82508670 --------------------------------------
    // The X360 ctor stores this state's own vtable (+0x000, off_82075470) and then a long
    // run of embedded sub-object vtables across the object (the highest store is +0x3F4C),
    // and calls the embedded TextSelection ctor (r3 = this+0x8F0). Every one of those is
    // emitted implicitly in C++ -- by this object's construction, by the CarSelectMain base
    // and by the embedded component members' default ctors -- so the body is empty. It
    // stores no field values (Construct does that).
    CarSelectVehicle::CarSelectVehicle()
    {
    }

    // ---- Construct @ 0x824BEBF0 ----------------------------------------------------
    void CarSelectVehicle::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        // cpp:119 -- the X360 streams this message through StrStream; the call site passes
        // a plain string, so CGS_ASSERT forwards it directly.
        CGS_ASSERT(lpFsm != 0, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid ScriptedFsm ptr", "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCarSelectVehicle.cpp", 119));

        CarSelectMain::Construct(liId, lpFsm);

        gsiNumCarouselCars      = 0;
        gsiNumCarsUnlockedTotal = 0;

        mbVoiceOverPlaying  = false;   // stb 0, 0x414D
        mbTitleTickerPosted = false;   // stb 0, 0x414E

        // Four `std 0` over +0x19B0..+0x19CF -- both 128-bit arrays cleared.
        maSelectedCarsDrivenState.UnSetAll();
        maSelectedCarsWreckedState.UnSetAll();
    }

    // ================================================================================
    // OnEnter / OnLeave
    // ================================================================================

    // ---- OnEnter @ 0x824C9470 ------------------------------------------------------
    void CarSelectVehicle::OnEnter()
    {
        CarSelectMain::OnEnter();

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            // The X360 inlines GetAccessPointers (CgsGuiStateInterface.h:344), GetGuiCache
            // (CgsGuiShared.h:201) and GetCurrentCarSelectType's own assert
            // (BrnGuiCache.h:4378) into this one debug line.
            *CgsDev::Log::gpDebugPrint << "RG :: CSV : Entering Car Select - "
                                       << mpStateInterface->GetAccessPointers()->GetGuiCache()->GetCurrentCarSelectType()
                                       << "\n";
        }

        // off_82F26C8C -> "TitleText". The X360 dispatches through the component's vtable
        // slot 0, i.e. TextField::Construct(name, stateInterface, parentName).
        mTitleText.Construct(mpacTitleTextFieldName, mpStateInterface, 0);

        {
            GuiTickerFlagsWire409 lTickerOn;
            mpStateInterface->OutputGuiEvent(lTickerOn);
        }

        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mCarSelector.Construct(mpacCarSelectorName, mpStateInterface, 0,
                               Selectable::K_INVALID_ID);

        // NOTE the console order: the three gauges, then CarsUnlocked, THEN CarType.
        mSpeedStatsBar.Construct(KAC_SPEED_STATS_BAR_NAME, mpStateInterface, 0);
        mBoostStatsBar.Construct(KAC_BOOST_STATS_BAR_NAME, mpStateInterface, 0);
        mStrengthStatsBar.Construct(KAC_STRENGTH_STATS_BAR_NAME, mpStateInterface, 0);
        mCarsUnlocked.Construct(KAC_CARS_UNLOCKED_NAME, mpStateInterface, 0);
        mCarType.Construct(KAC_CAR_TYPE, mpStateInterface, 0);

        // The pooled empty string @0x820046A7 -- the overview group has no apt clip of its own.
        mCarouselOverviewSelectableGroup.Construct("", mpStateInterface, 0,
                                                   Selectable::K_INVALID_ID);
        mCarouselOverviewSelectableGroup.SetHighlightable(true);   // group vtable slot 1

        for (s32 liIcon = 0; liIcon < KI_NUMBER_VISIBLE_VEHICLE_ICONS; ++liIcon)
        {
            // ⚠️ 1-BASED: the X360 does `addi r26, r11, 1` before the SPrintf, so the clips
            // are Overview_1_mc .. Overview_5_mc. The capacity argument is 31.
            char lacName[32];
            CgsCore::SPrintf(lacName, 31, "Overview_%i_mc", liIcon + 1);

            lacName[31] = '\0';
            maCarouselOverviewSelectable[liIcon].Construct(
                lacName, mpStateInterface, 0, Selectable::K_INVALID_ID);
            maCarouselOverviewSelectable[liIcon].SetHighlightable(true);   // cell slot 1
            mCarouselOverviewSelectableGroup.Add(&maCarouselOverviewSelectable[liIcon]);  // group slot 7
        }

        mCarouselSliderBar.Construct(KAC_SLIDER_BAR_NAME, mpStateInterface, 0);
        mOnlineCountdown.Construct(KAC_ONLINE_COUNTDOWN_NAME, mpStateInterface, 0);
        mOnlinePlayerList.Construct(KAC_ONLINE_PLAYER_LIST, mpStateInterface, 0);

        // Both stores land BEFORE the two animation Constructs on the console.
        mbFirstFrame     = true;
        mpHostStatusData = 0;

        mTrackAnimTransitionComponent.Construct(macTrackAnimTransitionComponentName, mpStateInterface, 0);
        mMainAnimComponent.Construct(macMainAnimComponentName, mpStateInterface, 0);

        mfCarouselXOffset      = 0.0f;
        mfCarouselXOffsetDecay = 0.0f;

        // flt_8206603C * i + flt_82069CB4 - flt_82057930 == i*165 + 345 - 640, i.e. the five
        // icons laid out around screen centre (KC_SCREEN_WIDTH / 2 == 640).
        for (s32 liIcon = 0; liIcon < KI_NUMBER_VISIBLE_VEHICLE_ICONS; ++liIcon)
        {
            mafCarouselOriginalXPos[liIcon] =
                (static_cast<f32>(liIcon) * KC_CAROUSEL_X_ADVANCE) + 345.0f - (KC_SCREEN_WIDTH * 0.5f);
        }

        muCarouselControllerRightPressedRefCount = 0;
        muCarouselControllerLeftPressedRefCount  = 0;
        mbControllerAxisActive = false;
        mbVoiceOverPlaying     = false;

        if (!mbTitleTickerPosted)
        {
            GuiOverlayRequest lTitleTicker;
            lTitleTicker.Construct("DwnldTitleUp");
            mpStateInterface->OutputGuiEvent(lTitleTicker);
            mbTitleTickerPosted = true;
        }
    }

    // ---- OnLeave @ 0x824C9938 ------------------------------------------------------
    void CarSelectVehicle::OnLeave()
    {
        CarSelectMain::OnLeave();
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // ================================================================================
    // Update
    // ================================================================================

    // ---- Update @ 0x824DCBF0 -------------------------------------------------------
    // ⚠️ Hex-Rays renders the queue walk with a spurious `float*` induction variable and
    // drops the SetTimeLeft argument into it; the shapes below come from the asm.
    void CarSelectVehicle::Update()
    {

        const CgsModule::Event* lpEvent;
        s32 liSize;

        for (s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_APT_TRIGGER:
                HandleAptTrigger(reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent));
                break;

            case KI_EVENT_COUNTDOWN_TIME:
                mOnlineCountdown.SetTimeLeft(
                    reinterpret_cast<const GuiCountdownTimePayload*>(lpEvent)->mfTimeLeft);
                break;

            case KI_EVENT_LOBBY_PLAYER_LIST:
                HandleLobbyPlayerList(
                    reinterpret_cast<const GuiEventNetworkLobbyPlayerList*>(lpEvent));
                break;

            case KI_EVENT_VOICE_OVER_STARTED:
                mbVoiceOverPlaying = true;
                break;

            case KI_EVENT_VOICE_OVER_FINISHED:
                mbVoiceOverPlaying = false;
                break;

            default:
                break;
            }
        }

        CarSelectMain::Update();

        if (mbFirstFrame)
        {
            CGS_ASSERT(mpGuiCache != 0,
                       CGS_ASSERT_MESSAGE_AT("mpGuiCache", CGS_ARTIST_FILE, 319));

            const BrnGameState::GameStateModuleIO::ECarSelectType leCarSelectType =
                mpGuiCache->GetCurrentCarSelectType();
            GuiEventActivateCarSelect lCarSelectType;
            lCarSelectType.meState = BrnGameState::GameStateModuleIO::E_CAR_SELECT_STATE_MODEL;
            lCarSelectType.meCarSelectType = leCarSelectType;
            mpStateInterface->OutputGuiEvent(lCarSelectType);

            mbFirstFrame = false;
        }

        if (meCurrentState >= E_CARSELECT_VISIBLE_INTERACTIVE)
            UpdateComponents();
    }

    // ================================================================================
    // resource / apt surface
    // ================================================================================

    // ---- AppendAptComponents @ 0x824B57F8 ------------------------------------------
    // ⚠️ Boost comes BEFORE Speed here and after it in OnEnter -- that is the console order.
    // ⚠️ The last two names are GetName() reads off the two AnimationComponents, not the
    // static literals: they are only valid once OnEnter has Constructed them, which is why
    // this function and OnEnter have to land together.
    void CarSelectVehicle::AppendAptComponents()
    {
        CarSelectMain::AppendAptComponents();

        GuiCache* lpGuiCache = mpGuiCache;
        CGS_ASSERT_AT(lpGuiCache != 0, "lpGuiCache", CGS_ARTIST_FILE, 346);

        lpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpacTitleTextFieldName);
        lpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpacWaitingTextFieldName);
        lpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpacCarSelectorName);
        lpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, KAC_BOOST_STATS_BAR_NAME);
        lpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, KAC_SPEED_STATS_BAR_NAME);
        lpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, KAC_STRENGTH_STATS_BAR_NAME);
        lpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, KAC_SLIDER_BAR_NAME);
        lpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, "HelpItemToggle_mc");
        lpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTrackAnimTransitionComponent.GetName());
        lpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMainAnimComponent.GetName());
    }

    // ---- PlayMovie @ 0x824C9DE8 ----------------------------------------------------
    // The inlined OutputGuiEvent<GuiEventPlayAptMovie> record { 8, 18, 12, <name>, 3 } on
    // channel 41 -- exactly what StateInterface::PlayAptMovie posts. The movie name comes
    // from gGuiResourceIdentifier[E_GUI_RESOURCEID_APT_CARSELECT_MAIN] through off_82F27B34, i.e. the same resource this
    // state's maResourcesToLoad[0] loads.
    void CarSelectVehicle::PlayMovie()
    {
        CgsGui::GuiEventPlayAptMovie lMovie;
        lMovie.mpacMovieName = gGuiResourceIdentifier[E_GUI_RESOURCEID_APT_CARSELECT_MAIN];
        lMovie.miLevelNum = 3;
        mpStateInterface->OutputViewState(lMovie);
    }

    // ---- GetResourcesToLoad @ 0x824B5A08 / GetNumberResourcesToLoad @ 0x824B5A18 ----
    CgsGui::sResourceTuple* CarSelectVehicle::GetResourcesToLoad() const
    {
        return const_cast<CgsGui::sResourceTuple*>(maResourcesToLoad);
    }

    u32 CarSelectVehicle::GetNumberResourcesToLoad() const
    {
        return muNumResourcesToLoad;
    }

    // ================================================================================
    // helpers
    // ================================================================================

    // ---- IsCarSelectable (DWARF cpp:488) -------------------------------------------
    // Inlined at all three X360 call sites (UpdateCarouselTransition x2, SetTicker): a
    // linear scan of the live carousel list. The compare is a full 64-bit `cmpld`.
    bool CarSelectVehicle::IsCarSelectable(CgsID lCarId) const
    {
        const CgsID* lpaSelectedCars = maSelectedCars;
        for (s32 liCar = 0; liCar < gsiNumCarouselCars; ++liCar)
        {
            if (lpaSelectedCars[liCar] == lCarId)
                return true;
        }
        return false;
    }

    // ---- IsLoading @ 0x824C14B8 ----------------------------------------------------
    // ⭐ CORRECTION to the handed-down spec: the id this compares against
    // mCurrentSetupInfo.mCarId is *(this+0x2B0) == CarSelectMain::miMostRecentDropInId
    // (the car the GAME STATE most recently reported the player is on), NOT
    // mDesiredSetupInfo (+0x2A0). The asm is unambiguous -- `ld r4, 0x2B0(r31)` feeds the
    // vehicle lookup and `ld r11, 0x290(r31)` is the committed id -- and the semantics
    // follow: the screen reports "still loading" until the reported car matches the
    // selection (or is that selection's livery parent).
    //
    // ⭐ It also reports "still loading" while a VOICE-OVER is playing: the byte at +0x414D
    // is mbVoiceOverPlaying (Update sets it on event 466 and clears it on 467), not the
    // controller-axis flag the earlier spec named.
    bool CarSelectVehicle::IsLoading() const
    {
        if (mbCarChangeInProgress)
            return true;
        if (mfCarouselXOffset != 0.0f)
            return true;
        if (mbVoiceOverPlaying)
            return true;

        // (The LIST guard that stood here is RETIRED 2026-08-02 with the WorldDataController.
        // ⚠️ HISTORY, keep for the ledger: it was added as a guard on the LOOKED-UP ENTRY and
        // read as if it covered this call -- it did not. `mpVehicleList->GetVehicleData(...)`
        // dereferences the LIST, and `VehicleList::GetVehicleData(CgsID)` opens with
        // `mov edi,[rcx+0x3700]` (GetVehicleCount), so a null list access-violated INSIDE the
        // callee before any result existed to test. Confirmed from WER fault offset 0x596C4 ==
        // GetVehicleData(CgsID)+0x14, reproduced twice; the caller is IsLoading+0x50, which
        // UpdateComponents runs EVERY FRAME. It hid behind the `mbVoiceOverPlaying` early
        // return above -- the screen only died once the 9-second Junkyard car-info VO
        // (INT_SHOWCAR.SNS) drained.)
        const BrnResource::VehicleListEntry* lpVehicleListEntry =
            mpVehicleList->GetVehicleData(miMostRecentDropInId);
        CGS_ASSERT(lpVehicleListEntry != 0, "lpVehicleListEntry");   // cpp:1505

        // ⚠️ ENTRY guard (shape (b) in BrnCarSelectVehicle.h), not in the X360 body. The
        // console dereferences the entry unconditionally on the line below; it can do that
        // because the game-state module always publishes a live drop-in car id (event 406 /
        // 565) before this screen goes interactive. On this build that id comes from
        // BrnGameModule::PublishCarSelectionToGui, a flagged stand-in, and miMostRecentDropInId
        // is still 0 until it lands -- which on the console path would be an immediate null
        // dereference EVERY FRAME (UpdateComponents calls IsLoading through vtable slot
        // +0x2C). Reporting "still loading" is the meaning of an unresolved drop-in id, so the
        // guard is behaviour-preserving for every case the console can actually reach. Remove
        // it when the real event-406/565 producer lands.
        if (lpVehicleListEntry == 0)
            return true;

        const u8 luLiveryType = lpVehicleListEntry->GetLiveryType();
        const CgsID lComparisonId =
            (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
                ? lpVehicleListEntry->GetParentId()
                : miMostRecentDropInId;

        if (miMostRecentDropInId == mCurrentSetupInfo.mCarId)
            return false;

        return lComparisonId != mCurrentSetupInfo.mCarId;
    }

    // ---- HandleAptTrigger @ 0x824B5918 ---------------------------------------------
    // Event 21. Trigger TYPE 1 (a component load notification) is routed to the online
    // player table when the reported clip name contains the table's own component name;
    // trigger TYPE 4 (a transition-complete notification) is offered to the three stats
    // bars in Speed / Boost / Strength order. Every test is a SUBSTRING match (strstr),
    // not an equality compare.
    void CarSelectVehicle::HandleAptTrigger(const CgsGui::GuiEventAptTriggerPayload* lpTrigger)
    {
        switch (lpTrigger->meEventType)
        {
        case CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE:
            if (std::strstr(lpTrigger->mpacComponentName, KAC_SPEED_STATS_BAR_NAME) != 0)
                mSpeedStatsBar.HandleTransitionComplete(lpTrigger->mpacComponentName, lpTrigger->miUniqueId);
            else if (std::strstr(lpTrigger->mpacComponentName, KAC_BOOST_STATS_BAR_NAME) != 0)
                mBoostStatsBar.HandleTransitionComplete(lpTrigger->mpacComponentName, lpTrigger->miUniqueId);
            else if (std::strstr(lpTrigger->mpacComponentName, KAC_STRENGTH_STATS_BAR_NAME) != 0)
                mStrengthStatsBar.HandleTransitionComplete(lpTrigger->mpacComponentName, lpTrigger->miUniqueId);
            break;

        case CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD:
            if (std::strstr(lpTrigger->mpacComponentName, mOnlinePlayerList.GetName()) != 0)
                mOnlinePlayerList.HandleLoadNotification(lpTrigger->mpacComponentName);
            break;

        default:
            break;
        }
    }
}

// FOLDED FROM BrnCarSelectVehicle_Components.cpp
// ===================================================================================
// BrnGui::CarSelectVehicle  -- partfile 02: the screen build + the per-frame pump
//   class:BrnGui::CarSelectVehicle
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm + pseudocode):
//   SetupComponents             @ 0x824C9978   (DWARF cpp:355)
//   SetupMenuComponents         @ 0x824BECD0   (DWARF cpp:519)
//   SetCarSelectorComponent     @ 0x824C0FE8   (DWARF cpp:425)
//   SetupStatsComponent         @ 0x824C1200   (DWARF cpp:1109)
//   SetupCarsUnlockedTextComponent               (DWARF cpp:1155 -- inlined on X360)
//   SetCarouselComponent        @ 0x824BBE90   (DWARF cpp:1176)
//   SetSliderBarComponent                        (DWARF cpp:1264 -- inlined on X360)
//   UpdateComponents            @ 0x824BBD18   (DWARF cpp:971)
//   UpdateCarouselTransition    @ 0x824D7D98   (DWARF cpp:854)
//
// The component vtable slot map these by-name calls stand in for (BrnSelectableGroup.h,
// re-validated against off_82073020):
//   0 SetActive  1 SetHighlightable  2 SetSelectable  3 SetHighlighted  4 Select
//   5 Update     6 Clear             7 Add            8 Enable          9 Disable
//  10 HighlightNext  11 HighlightPrevious  12 HighlightIndex
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"          // BrnGui::Selectable (GetId)
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry

namespace BrnGui
{
    namespace
    {
        // KI_CHANNEL_GUI_OUT is declared above in this translation unit.

        // The pooled empty string @0x820046A7 the X360 passes for "no caption".

        // The three car-type captions SetupStatsComponent builds its "%s" from (a local
        // const array in the source; the X360 stack-builds it at 0x824C148C..0x824C1494).
        const char* const KAPC_CAR_TYPE_STRINGIDS[3] =
        {
            "$CARTYPE_DANGER",      // E_CARTYPE_DANGER
            "$CARTYPE_AGRESSION",   // E_CARTYPE_AGGRESSION (the console's spelling)
            "$CARTYPE_STUNTS",      // E_CARTYPE_STUNTS
        };

        // The two-flag ticker command SetupComponents posts as id 536:
        // { 2, 536, 12, u8 1, u8 0 } on channel 40, 16 bytes.
        struct GuiTickerFlagsWire536 : public CgsGui::GuiEvent<536>
        {
            u8 mbFlagA;   // +0x0C
            u8 mbFlagB;   // +0x0D
            GuiTickerFlagsWire536()
                : mbFlagA(1), mbFlagB(0) {}
        };
    }

    // ================================================================================
    // SetupComponents -- the one-shot screen build
    // ================================================================================

    // ---- SetupComponents @ 0x824C9978 ----------------------------------------------
    void CarSelectVehicle::SetupComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "lpGuiCache");   // cpp:371

        // Blank both help prompts (invisible glyphs) until UpdateComponents decides what
        // the continue prompt should say. Back first, exactly as the console does.
        mHelpItemBack.SetItem("", ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                          ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        mHelpItemContinue.SetItem("", ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);

        SetupMenuComponents();
        SetCarSelectorComponent();
        CarSelectMain::SetupCarNameComponent(mCurrentSetupInfo.mCarId);

        // The manufacturer badge. The console's re-fetch, RESTORED 2026-08-02: the X360 reads
        // mpGuiCache->GetWorldDataController()->GetVehicleList() (its inlined
        // mpWorldDataController assert is BrnGuiCache.h:2324), which is the same pointer
        // CarSelectMain::UpdateGuiCache latches into mpVehicleList.
        mManufacturerLogo.Set(mpGuiCache->GetWorldDataController()->GetVehicleList(),
                              mCurrentSetupInfo.mCarId);

        SetupCarsUnlockedTextComponent();

        mTitleText.SetText("CAR SELECT: SELECT VEHICLE");

        // The stats gauges follow the HIGHLIGHTED car, not the committed one: the X360
        // copies mCurrentSetupInfo (both qwords) into a stack CarSetupInfo and then
        // overwrites only its car id with the highlighted row's.
        CarSetupInfo lHighlightedSetupInfo = mCurrentSetupInfo;

        // ⚠️ PC-BUILD GUARD -- see "PC-BUILD GUARDS" in BrnCarSelectVehicle.h.
        // ROOT-CAUSED 2026-08-02 from the WER fault offset (0xFAE23 == SetupComponents+0x1A3,
        // `mov r15, qword ptr [rax+0x18]`), the same offset in two independent crashes:
        //
        //   SetCarSelectorComponent (called five lines up) bails on the absent VehicleList, so
        //   mCarSelector is EMPTY and miHighlightedIndex is still Construct's -1.
        //   SelectableGroup::GetHighlighted (@0x8240FD60) has NO LOWER BOUND: the X360 asm's
        //   `lbz r11,0xA5(r28) / extsb / cmpwi r11,0 / blt` only SKIPS the ">= 100" assert when
        //   the index is negative -- it still performs `*(4 * (index + 42) + a1)`. So an empty
        //   group returns the bytes that PRECEDE the array, i.e.
        //   {miSelectableCount, miHighlightedIndex, mbWrapped, pad} == 0x0000FF00 in this state.
        //   That value is NON-NULL, so the console's own `!= 0` assert on the next line passes
        //   (which is exactly why no [ASSERT] line ever appeared before the crash) and the
        //   ->GetId() load at +0x18 access-violates on 0xFF18.
        //
        // The console never reaches that state because event 412 always fills the selector
        // first. Guard with the console's OWN "nothing is highlighted" idiom -- the signed
        // `miHighlightedIndex > -1` test that TextSelection::Update @0x824E83A0 uses (X360
        // `extsb / cmpwi -1`) -- and keep the committed car as the highlighted one. Delete the
        // guard and restore the bare assert + deref once the event-412 producer lands.
        CgsID lHighlightedId = mCurrentSetupInfo.mCarId;
        if (mCarSelector.miHighlightedIndex > -1)
        {
            lHighlightedId = mCarSelector.GetHighlightedId();
        }
        lHighlightedSetupInfo.mCarId = lHighlightedId;

        SetupStatsComponent(&lHighlightedSetupInfo);

        for (s32 liIcon = 0; liIcon < KI_NUMBER_VISIBLE_VEHICLE_ICONS; ++liIcon)
            mCarouselOverviewSelectableGroup.Enable(liIcon);        // group slot 8

        mCarouselOverviewSelectableGroup.HighlightIndex(2);         // group slot 12 (centre icon)

        SetCarouselComponent(lHighlightedId);

        // X360: `lis r11,1 / ori r11,r11,0x3B5E / lbzx r11, r27, r11` -- a byte in the
        // GuiCache at +0x13B5E (r27 is mpGuiCache, latched at the top of the function).
        // ⚠️ Hex-Rays renders that indexed load as `*(HIDWORD(v6) + 80734)`, a 64-bit-pair
        // artefact; the base is the cache.
        if (mpGuiCache->GetCarSelectTransitionAlreadyShown())
        {
            GuiTickerFlagsWire536 lTickerFlags;
            mpStateInterface->OutputGuiEvent(lTickerFlags);
        }
        else
        {
            mMainAnimComponent.AddOutputAptViewState("apt_Transition", "transin", false);
        }
    }

    // ---- SetupMenuComponents @ 0x824BECD0 ------------------------------------------
    // ⚠️ THIS BODY COMPUTES A LIST AND DISCARDS IT, in the X360 ARTIST image as shipped.
    // It builds a 128-entry id scratch array (0,1,2,...), then walks the whole VehicleList
    // and appends each entry's display-name pointer for every car that is in maSelectedCars
    // -- and then returns without handing either array to anything. Both arrays are stack
    // locals at the SAME frame offset (r1+0x50 for both the `std` id fill and the `stw`
    // name fill), and the function contains no `bl` other than the two GetVehicleData calls.
    // The work it looks like it should do is done by SetCarSelectorComponent, which
    // SetupComponents calls on the very next line and which re-derives the same list
    // straight from maSelectedCars (and passes NULL for both of SetupTextSelection's array
    // parameters). Reproduced faithfully -- including the discard -- because the observable
    // behaviour of the shipped build is exactly this: the asserts fire, nothing else changes.
    void CarSelectVehicle::SetupMenuComponents()
    {
        // cpp:540 -- the console's assert, RESTORED 2026-08-02 (the LIST guard that stood here
        // is retired with the WorldDataController; see BrnCarSelectVehicle.h).
        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList");   // cpp:540

        CgsID       laCarIds[KI_MAX_SELECTABLE_CARS];
        const char* lapacCarNames[KI_MAX_SELECTABLE_CARS];

        for (s32 liId = 0; liId < KI_MAX_SELECTABLE_CARS; ++liId)
            laCarIds[liId] = static_cast<CgsID>(liId);

        s32 liNumNames = 0;
        for (s32 liVehicle = 0; liVehicle < mpVehicleList->GetVehicleCount(); ++liVehicle)
        {
            const CgsID lCarId = mpVehicleList->GetVehicleData(liVehicle)->GetId();

            for (s32 liSelected = 0; liSelected < gsiNumCarouselCars; ++liSelected)
            {
                if (maSelectedCars[liSelected] == lCarId)
                {
                    lapacCarNames[liNumNames++] =
                        mpVehicleList->GetVehicleData(liVehicle)->GetName();
                    break;
                }
            }
        }

        // The console drops both arrays here. Keep the compiler from warning about it
        // without changing what the function does.
        (void)laCarIds;
        (void)lapacCarNames;
        (void)liNumNames;
    }

    // ---- SetCarSelectorComponent @ 0x824C0FE8 --------------------------------------
    // Rebuild the car-name selector from maSelectedCars, then highlight the committed car
    // (or its livery parent). SetupTextSelection is given NULL for both array parameters:
    // the ids and the row texts are pushed row by row in the loop below.
    void CarSelectVehicle::SetCarSelectorComponent()
    {
        // cpp:441 -- the console's assert, RESTORED 2026-08-02 (LIST guard retired).
        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList");   // cpp:441

        const s32 liNumCars = gsiNumCarouselCars;

        mCarSelector.Clear();                                          // component slot 6
        mCarSelector.SetupTextSelection(liNumCars, liNumCars >= 3, 0, 0);

        for (s32 liCar = 0; liCar < liNumCars; ++liCar)
        {
            Selectable* lpSelectable = mCarSelector.GetSelectable(liCar);
            CGS_ASSERT(lpSelectable != 0, "lpSelectable");             // cpp:457

            const BrnResource::VehicleListEntry* lpVehicleData =
                mpVehicleList->GetVehicleData(maSelectedCars[liCar]);

            // ⚠️ ENTRY guard (shape (b) in BrnCarSelectVehicle.h) -- the console dereferences
            // the lookup result unchecked. STAYS until the real 406/565 producers land: the
            // ids in maSelectedCars come from a stand-in publisher.
            if (lpVehicleData == 0)
                continue;

            lpSelectable->SetId(lpVehicleData->GetId());
            lpSelectable->SetDirty();
            lpSelectable->SetActive(true);                             // row slot 0
            lpSelectable->SetSelectable(true);                         // row slot 2

            mCarSelector.SetItemText(liCar, lpVehicleData->GetName());
        }

        // A livery variant is highlighted under its PARENT car's row.
        CgsID lHighlightId = mCurrentSetupInfo.mCarId;
        const BrnResource::VehicleListEntry* lpCurrentData =
            mpVehicleList->GetVehicleData(mCurrentSetupInfo.mCarId);

        // ⚠️ ENTRY guard (shape (b) in BrnCarSelectVehicle.h). The console dereferences
        // lpCurrentData unconditionally on the next line.
        if (lpCurrentData != 0)
        {
            const u8 luLiveryType = lpCurrentData->GetLiveryType();
            if (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
                lHighlightId = lpCurrentData->GetParentId();
        }

        const s32 liHighlightIndex = mCarSelector.GetIndexFromId(lHighlightId);
        if (liHighlightIndex < 0 || liHighlightIndex >= liNumCars)
            mCarSelector.HighlightIndex(0);                            // component slot 12
        else
            mCarSelector.HighlightId(lHighlightId);
    }

    // ---- SetupStatsComponent @ 0x824C1200 ------------------------------------------
    // Push the three gauges and the car-type caption for lpSetupInfo's car. Every lookup
    // goes through the CACHE's vehicle list (the X360 re-fetches it once and re-resolves
    // the entry four separate times -- reproduced as one lookup by name).
    void CarSelectVehicle::SetupStatsComponent(const CarSetupInfo* lpSetupInfo)
    {
        CGS_ASSERT(mpGuiCache != 0, "lpGuiCache");   // cpp:1125

        // The console's own re-fetch, RESTORED 2026-08-02: the X360 reads
        // mpGuiCache->GetWorldDataController()->GetVehicleList() here (its inlined
        // "mpWorldDataController" assert is BrnGuiCache.h:2324). Now that the controller is
        // populated the substitution that read the latched mpVehicleList is retired.
        const BrnResource::VehicleList* lpVehicleList =
            mpGuiCache->GetWorldDataController()->GetVehicleList();
        CGS_ASSERT(lpVehicleList != 0, "mpVehicleList");   // cpp:1128 (streamed form)

        const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lpSetupInfo->mCarId);
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

        // ⚠️ ENTRY guard (shape (b) in BrnCarSelectVehicle.h). The console dereferences
        // lpVehicleData unconditionally from here on (it has no assert at all on this
        // pointer), which it can afford because the car it is handed is always a live
        // VehicleList id.
        if (lpVehicleData == 0)
            return;

        const u8 luStrengthStat = lpVehicleData->GetStrengthStat();   // entry +0x9B
        const u8 luCarType      = lpVehicleData->GetCarType();        // entry +0xE8

        CGS_ASSERT(luCarType < 3, "Invalid car type");   // cpp:1136 (streamed form)

        const u32 luBarColour = mauBoostColours[luCarType];

        mSpeedStatsBar.SetCar(lpVehicleData->GetSpeedStat(), luBarColour);   // entry +0xEC
        mBoostStatsBar.SetCar(lpVehicleData->GetBoostStat(), luBarColour);   // entry +0xED
        mStrengthStatsBar.SetCar(luStrengthStat, luBarColour);

        char lacCarType[128];
        lacCarType[31] = 0;
        CgsCore::SPrintf(lacCarType, 31, "%s", KAPC_CAR_TYPE_STRINGIDS[luCarType]);
        mCarType.SetText(lacCarType);
    }

    // ---- SetupCarsUnlockedTextComponent (DWARF cpp:1155) ---------------------------
    // ⛔ NOT WIRED UP THIS WAVE, and deliberately NOT a silent {}. The X360 body is one
    // call, inlined at both of its sites (SetupComponents @0x824C9A74 and SetupCar
    // @0x824D81EC):
    //
    //     mCarsUnlocked.SetLocalisedText(KAC_CARS_AVAILABLE_STRINGID,
    //                                    LanguageManager::E_FORMAT_ID_LOOKUP,          // 9
    //                                    gsiNumCarouselCars,      E_FORMAT_INTEGER,    // 11
    //                                    gsiNumCarsUnlockedTotal, E_FORMAT_INTEGER);   // 11
    //
    // -- the two-integer-parameter TextField::SetLocalisedText overload (sub_824E7F78,
    // BrnTextField.cpp:418), which resolves through LanguageManager's two-value
    // FormatTextFromInt (sub_828659E8, CgsLanguageManager.cpp:1259) and thence through
    // LanguageManager::FormatText(char*, u32, s32, ParameterFormatType) for each
    // E_FORMAT_INTEGER parameter.
    //
    // That last leaf is `LanguageManager::FormatIntegerString`, and on this build it is an
    // ARMED __debugbreak() trap-stub (CgsLanguageManager.cpp:686, one of the FLAG trap-stub
    // block). Making the call would break into the debugger on the very screen this wave is
    // bringing up, every time a car is highlighted. Neither the TextField overload nor the
    // LanguageManager overload has a reconstructed body, so wiring it now would ALSO mean
    // inventing two formatters.
    //
    // The only visible consequence is that the "<n> of <m> cars available" caption stays
    // blank; nothing else on the screen reads mCarsUnlocked. Restore the call above verbatim
    // when FormatIntegerString lands.
    void CarSelectVehicle::SetupCarsUnlockedTextComponent()
    {
    }

    // ---- SetCarouselComponent @ 0x824BBE90 -----------------------------------------
    // Rebuild the five overview icons so that lCarId sits in the centre slot. The window
    // is clamped at both ends of the car list, so near the ends fewer than five icons are
    // bound and the rest stay empty.
    void CarSelectVehicle::SetCarouselComponent(CgsID lCarId)
    {
        // cpp:1191 -- THE CONSOLE'S OWN guard: it asserts mpVehicleList AND gates the whole
        // body on it (`lwz 0x28C / beq skip`). Both restored (this one was never a PC guard).
        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList");   // cpp:1191
        if (mpVehicleList == 0)
            return;

        mCarouselOverviewSelectableGroup.SetDirty();

        // Blank every icon first.
        for (s32 liIcon = 0; liIcon < KI_NUMBER_VISIBLE_VEHICLE_ICONS; ++liIcon)
        {
            RivalTableCell& lrCell = maCarouselOverviewSelectable[liIcon];
            lrCell.SetCarID(0);
            lrCell.SetDriven(false);
            lrCell.SetWrecked(false);
            lrCell.SetEmpty(true);
            lrCell.SetDirty();
        }

        const s32 liNumCars = gsiNumCarouselCars;

        // Where is the selected car in the list? (Not found -> index 0.)
        s32 liSelectedIndex = 0;
        for (s32 liCar = 0; liCar < liNumCars; ++liCar)
        {
            if (maSelectedCars[liCar] == lCarId)
            {
                liSelectedIndex = liCar;
                break;
            }
        }

        // The window of icon slots that map onto real cars: slot 2 is the selected car, so
        // slot i shows car (liSelectedIndex + i - 2), clamped to [0, liNumCars).
        s32 liFirstIcon = 2 - liSelectedIndex;
        if (liFirstIcon < 0)
            liFirstIcon = 0;

        s32 liLastIcon = (liNumCars - liSelectedIndex) + 1;
        if (liLastIcon >= 4)
            liLastIcon = 4;

        if (liFirstIcon <= liLastIcon)
        {
            s32 liVehicleIndex = (liFirstIcon + liSelectedIndex) - 2;

            for (s32 liIcon = liFirstIcon; liIcon <= liLastIcon; ++liIcon, ++liVehicleIndex)
            {
                RivalTableCell& lrCell = maCarouselOverviewSelectable[liIcon];

                lrCell.SetEmpty(false);

                CGS_ASSERT(liVehicleIndex >= 0 && liVehicleIndex < liNumCars,
                           "carousel vehicle index out of range ");   // cpp:1239 (streamed form)

                lrCell.SetCarID(maSelectedCars[liVehicleIndex]);

                // Both bit arrays are indexed with the same assert the X360 inlines from
                // CgsBitArray.h:203 (`index < 128`).
                lrCell.SetDriven(maSelectedCarsDrivenState.IsBitSet(static_cast<u32>(liVehicleIndex)));

                lrCell.SetWrecked(maSelectedCarsWreckedState.IsBitSet(static_cast<u32>(liVehicleIndex)));
            }
        }

        SetSliderBarComponent();
    }

    // ---- SetSliderBarComponent (DWARF cpp:1264) ------------------------------------
    // Inlined at SetCarouselComponent's tail (the closing CarouselSliderBar::SetVisible
    // call at 0x824BC404..0x824BC418): the scroll bar only appears once the list is longer
    // than the three cars that fit without scrolling.
    void CarSelectVehicle::SetSliderBarComponent()
    {
        mCarouselSliderBar.SetVisible(gsiNumCarouselCars >= 3);
    }

    // ================================================================================
    // per-frame
    // ================================================================================

    // ---- UpdateComponents @ 0x824BBD18 ---------------------------------------------
    void CarSelectVehicle::UpdateComponents()
    {
        mCarSelector.Update();                          // component slot 5 (this class's override)
        mCarouselOverviewSelectableGroup.Update();      // component slot 5 (the base's)

        // Find the highlighted car's position in the list; when it is not in the list the
        // loop falls out with the index == the count (the X360 uses ONE register for both
        // the loop counter and the result).
        const s32 liNumCars = gsiNumCarouselCars;
        const CgsID* lpaSelectedCars = maSelectedCars;
        s32 liHighlightedIndex = 0;
        while (liHighlightedIndex < liNumCars)
        {
            if (lpaSelectedCars[liHighlightedIndex] == mCarSelector.GetHighlightedId())
                break;
            ++liHighlightedIndex;
        }

        // ⓘ Positional, per the asm (r4 = the car count, r5 = the highlighted index). The
        // committed CarouselSliderBar::Update spells its parameters (liFirstItem,
        // liItemCount) -- the NAMES are transposed relative to every caller, but its own
        // extent maths (`liItemCount / (liFirstItem + 4)`) is right for this argument order.
        mCarouselSliderBar.Update(liNumCars, liHighlightedIndex);

        // The continue prompt disappears while a car change is still resolving.
        if (IsLoading())
        {
            mHelpItemContinue.SetItem("", ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }
        else
        {
            mHelpItemContinue.SetItem("$GENERAL_OPTION_CONTINUE",
                                      ButtonIconComponent::E_PADBUTTON_SELECT,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }

        // Slide the five icons by the live carousel scroll offset.
        for (s32 liIcon = 0; liIcon < KI_NUMBER_VISIBLE_VEHICLE_ICONS; ++liIcon)
        {
            const Vector2 lv2ScreenPosition(
                mfCarouselXOffset + mafCarouselOriginalXPos[liIcon], 0.0f);
            maCarouselOverviewSelectable[liIcon].SetScreenPosition(lv2ScreenPosition);
        }
    }

    // ---- UpdateCarouselTransition @ 0x824D7D98 -------------------------------------
    // Advance the scrolling strip. While a decay is running the highlight is stepped one
    // car per KC_CAROUSEL_X_ADVANCE (165px) of travel; each step re-runs SetupCar for the
    // newly highlighted car. Returns whether the highlight moved.
    bool CarSelectVehicle::UpdateCarouselTransition()
    {
        bool lbStepped = false;

        // The quiet (lbQuiet == true) highlight probes only report whether a step is
        // POSSIBLE in that direction; the offset only accumulates when it is. One add per
        // direction (ARTIST 0x824D7E1C: the two arms' adds are cross-jumped into one tail,
        // which is what gives its `fadds f0, f13, f0` operand order).
        if (mfCarouselXOffsetDecay > 0.0f && mCarSelector.HighlightPrevious(true))
        {
            mfCarouselXOffset += mfCarouselXOffsetDecay;
        }
        else if (mfCarouselXOffsetDecay < 0.0f && mCarSelector.HighlightNext(true))
        {
            mfCarouselXOffset += mfCarouselXOffsetDecay;
        }

        while (mfCarouselXOffset >= KC_CAROUSEL_X_ADVANCE)
        {
            lbStepped = true;

            if (mfCarouselXOffsetDecay != 0.0f)
            {
                // A decay was running: snap to the step and stop.
                mfCarouselXOffset      = 0.0f;
                mfCarouselXOffsetDecay = 0.0f;
                TriggerSound(false);
            }
            else
            {
                mfCarouselXOffset -= KC_CAROUSEL_X_ADVANCE;
                TriggerSound(true);
            }

            mCarSelector.HighlightPrevious(false);

            const bool lbCarChanged = (mCurrentSetupInfo.mCarId != mCarSelector.GetHighlightedId());

            CarSetupInfo lSetupInfo;
            lSetupInfo.mCarId      = mCarSelector.GetHighlightedId();
            lSetupInfo.mbSelectable = IsCarSelectable(lSetupInfo.mCarId);

            SetupCar(&lSetupInfo, lbCarChanged);   // this class's vtable slot +0x64
        }

        while (mfCarouselXOffset <= -KC_CAROUSEL_X_ADVANCE)
        {
            lbStepped = true;

            if (mfCarouselXOffsetDecay != 0.0f)
            {
                mfCarouselXOffsetDecay = 0.0f;
                mfCarouselXOffset      = 0.0f;
                TriggerSound(false);
            }
            else
            {
                mfCarouselXOffset += KC_CAROUSEL_X_ADVANCE;
                TriggerSound(true);
            }

            mCarSelector.HighlightNext(false);

            const bool lbCarChanged = (mCurrentSetupInfo.mCarId != mCarSelector.GetHighlightedId());

            CarSetupInfo lSetupInfo;
            lSetupInfo.mCarId      = mCarSelector.GetHighlightedId();
            lSetupInfo.mbSelectable = IsCarSelectable(lSetupInfo.mCarId);

            SetupCar(&lSetupInfo, lbCarChanged);
        }

        return lbStepped;
    }
}

// FOLDED FROM BrnCarSelectVehicle_Input.cpp
// ===================================================================================
// BrnGui::CarSelectVehicle  -- partfile 03: input, selection and the game-state events
//   class:BrnGui::CarSelectVehicle
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm + pseudocode):
//   HandleControllerInput      @ 0x824DCD80   (DWARF cpp:572)
//   SetupCar                   @ 0x824D8108   (DWARF cpp:1023)
//   SetTicker                  @ 0x824C9BC8   (DWARF cpp:1059)
//   TriggerSound               @ 0x824CA0F8   (DWARF cpp:1520)
//   HandleCarInfoResponseEvent @ 0x824BEDC0   (DWARF cpp:937)
//   HandleLobbyPlayerList      @ 0x824C9E58   (DWARF cpp:1380)
//
// ⓘ CarSelectMain's dispatcher passes the OBSERVED EVENT ID as HandleControllerInput's
// second parameter, and for events 5..8 that id is the input event KIND, not a controller
// port: 5 == GuiEventControllerInputDown, 6 == ...Pressed, 7 == ...Released,
// 8 == GuiEventControllerAxis (CgsGuiEventTypeDefs.h:81/:88/:95/:102). The committed
// BrnCarSelectMain.h comment calls it "the controller selector"; this body proves it is the
// kind -- it switches on it and reads a DIFFERENT payload shape in each arm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h"
#include "GameSource/Input/GameInputActions.h"                       // EGameInputActions (the controller action vocabulary)

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // AddEvent
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::ECarSelectType
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData (the event-244 rows)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData (the gamertag)
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry

#include <cstring>   // std::strncpy / std::memset
#include <cstdlib>   // std::getenv (BRN_CARSEL_TAPSTEP)

namespace BrnGui
{
    namespace
    {
        // KI_CHANNEL_GUI_OUT is declared above in this translation unit.


        // ---- observed input event kinds (CgsGuiEventTypeDefs.h) ---------------------
        const s32 KI_EVENT_CONTROLLER_DOWN     = 5;
        const s32 KI_EVENT_CONTROLLER_PRESSED  = 6;
        const s32 KI_EVENT_CONTROLLER_RELEASED = 7;
        const s32 KI_EVENT_CONTROLLER_AXIS     = 8;

        // ---- the five action ids this screen reacts to ------------------------------
        // EGameInputActions now has a home (GameSource/Input/GameInputActions.h); the names
        // below stay role-named because that is what THIS handler does with them -- the two
        // "_ALT" ids are the dpad family (39/40 GUI_DPAD_LEFT/RIGHT, the DOWN/RELEASED
        // hold-to-scroll arms) and the plain pair are the generic-nav family (43/44 GUI_LEFT/
        // GUI_RIGHT, the PRESSED single-step arms).
        const s32 KI_ACTION_CAROUSEL_PREV_ALT = 39;   // 0x27
        const s32 KI_ACTION_CAROUSEL_NEXT_ALT = 40;   // 0x28
        const s32 KI_ACTION_CAROUSEL_PREV     = 43;   // 0x2B
        const s32 KI_ACTION_CAROUSEL_NEXT     = 44;   // 0x2C
        const s32 KI_ACTION_ACCEPT            = 49;   // 0x31 (EGameInputActions GUI_SELECT)

        // ✅ ROOT CAUSE FIXED ELSEWHERE, COMPENSATING ARM DELETED (input-vocabulary wave,
        // 2026-08-29). `KI_ACTION_ACCEPT_PC = 45` used to sit here because KA_BINDINGS bound
        // the accept key to 45 GUI_START rather than 49 GUI_SELECT. The console body --
        // BrnGui::CarSelectVehicle::HandleControllerInput @0x824DCD80 -- has switch cases
        // { 40, 43, 44 } and one `v11 != 49` test, and NO 45 anywhere, so the arm was pure
        // divergence. KA_BINDINGS now puts Enter/Space/pad-A on 49 and the CONTINUE prompt
        // works through the console's own id.

        // The GuiAudioTriggerEvent action word TriggerSound posts (X360 `li r4, 7`).
        const s32 KI_AUDIO_ACTION_CAROUSEL = 7;

        // The audio trigger record's queued event id. ⓘ The committed
        // BrnGui::GuiAudioTriggerEvent models this SAME 100-byte record under id 201 (its
        // presentation-action producer); this screen posts it under 457. The record the X360
        // builds is { 100, 457, 12, <the 100-byte audio record> }, 112 bytes on channel 40.
        const u32 KU_AUDIO_TRIGGER_EVENT_ID_CAROUSEL = 457;

        // ---- the analogue-axis scale (an inline literal in the source; the X360 pools it
        //      at flt_8206B2B8 == 17.647058f). It maps the live axis range
        //      [KF_AXIS_DEAD_ZONE, 1.0] onto [0, 15] pixels of scroll per sample.
        const f32 KF_AXIS_TO_CAROUSEL_SCALE = 17.647058f;

        // ---- in-queue payload views (the queue delivers the HEADER-STRIPPED payload) ----

        // CgsGui::GuiEventControllerInputDown / ...Pressed / ...Released, minus the header.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // CgsGui::GuiEventControllerAxis, minus the header.
        struct ControllerAxisPayload : public CgsModule::Event
        {
            s32 miAxis;       // +0x00 (this screen only reacts to axis 0)
            f32 mfXAxis;      // +0x04
            f32 mfYAxis;      // +0x08
        };

        // Event 412 (GuiCarSelectionEvent). The transport is a
        // CgsContainers::Array<s64,128> (elements @+0x000, count @+0x400 -- see
        // CgsArrayS64_128.cpp) followed by the two 128-bit state arrays and the
        // "cars unlocked" companion counter. The canonical typed record lives in
        // BrnGuiEventTypeDefs.h; this local view keeps the screen's explicit raw-count sentinel
        // and upper-bound assertions instead of routing them through Array::GetLength().
        struct GuiCarSelectionPayload : public CgsModule::Event
        {
            CgsID maCarIds[128];        // +0x000
            s32   miCount;              // +0x400 (-1 == the array was never Construct/Clear'ed)
            u32   muPad404;             // +0x404
            u64   mau64DrivenBits[2];   // +0x408 (BitArray<128>)
            u64   mau64WreckedBits[2];  // +0x418 (BitArray<128>)
            s32   miNumCarsUnlocked;    // +0x428
        };

        // Event 244 (GuiEventNetworkLobbyPlayerList). The queue delivers the RAW record --
        // AddGuiEvent<GuiEventNetworkLobbyPlayerList> @0x823CF4C8 hands VariableEventQueue::
        // AddEvent the record with its id 244 and length 456 passed out-of-band, so there is
        // no CgsGui::GuiEvent header on this path. The 456 bytes are the eight lobby rows then
        // the live count (X360 `lwz r11, 0x1C0(record)` == 8 * 56). Row fields come from the
        // record's real home, BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h -- no forked
        // layout here. Identical view to the sibling consumers, BrnCarSelectLivery_wJ_01.cpp:31
        // and BrnOnlineGameRoomPlayerInfo_wH_15.cpp:60.
        struct GuiEventNetworkLobbyPlayerListPayload
        {
            BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData
                maPlayers[CarSelectOnlinePlayerList::KI_MAX_PLAYERS];   // +0x000
            s32 miNumPlayers;                                           // X360 +0x1C0 (448)
        };

        // The lobby vehicle-choice mode HandleLobbyPlayerList's host arm runs on
        // (X360 `lwzx r11, mpGuiCache, 0xA9C8 ; cmplwi cr6, r11, 1`): the host picks one car
        // for the whole lobby, so a non-host client never drives the table itself.
        // FLAG consumer-named -- the console has no symbol for the value, only the misspelt
        // "Not a valid vehicle chouce mode : " assert that bounds the word at 1.
        const s32 KI_VEHICLE_CHOICE_HOST_PICKS = 1;

        // ---- out-queue wire records -------------------------------------------------

        // The "activate car select" record HandleLobbyPlayerList's non-host arm posts:
        // { 8, 192, 12, 2, 2 }, channel 40, 20 bytes. Same wire shape (and the same reason for
        // building it by hand rather than through StateInterface::OutputGuiEvent) as
        // BrnCarSelectMain_wG_01.cpp's GuiEventActivateCarSelect20 -- see the call site.
        // The two payload words are (ACTION, TYPE), recovered from the consumer
        // GameStateModule::ProcessGameEvents @0x823A0A18 case 94.
        struct GuiEventActivateCarSelect20 : public CgsGui::GuiEvent<192>
        {
            u32 muWord0;   // +0x0C -- the ACTION
            u32 muWord1;   // +0x10 -- the TYPE (2 == E_CAR_SELECT_TYPE_ONLINE_EVENT_START)

            GuiEventActivateCarSelect20(u32 luWord0, u32 luWord1)
                : muWord0(luWord0), muWord1(luWord1) {}
            GuiEventActivateCarSelect20() {}
        };

        // The "clear the ticker" command: { 2, 536, 12, u8 1, u8 0 }, channel 40, 16 bytes.
        // GuiTickerFlagsWire536 is declared above in this translation unit.







        // The custom ticker message payload (0x818 bytes). Layout recovered store-for-store
        // from BrnGui::GuiEventTickerCustomMessage::AddString @0x823A6940, whose asserts bake
        // "GameSource/Gui/BrnGuiEventTypeDefs.h" lines 390/391/392:
        //   +0x000  s32  maiStringTypes[4]            (`stwx r28, mi8NumStrings*4, this`)
        //   +0x010  char maacStrings[4][512]          (`strncpy(this + (n << 9) + 0x10, s, 512)`)
        //   +0x810  s8   mi8NumStrings                (`lbz/extsb`, bounded < 4)
        //   +0x811..+0x814  four flag bytes SetTicker seeds { 0, 0, 1, 0 }
        // Kept TU-LOCAL rather than promoted into BrnGuiEventTypeDefs.h: the type already
        // has an opaque twin in BrnGuiDemangledEventTypes.h (`GuiEvent<537>` + a 2060-byte
        // blob) and the two headers are mutually exclusive by construction, so a second
        // definition would be a live ODR fork.
        struct GuiTickerCustomMessagePayload
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;     // AddString's bound (h:391)
            static const s32 KI_MAX_STRING_LENGTH = 512;   // AddString's strncpy count

            s32  maiStringTypes[KI_MAX_NUM_STRINGS];                       // +0x000
            char maacStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH];    // +0x010
            s8   mi8NumStrings;                                            // +0x810
            // FLAG: four flag bytes at +0x811..+0x814 whose roles are not recovered; the only
            // observed producer (SetTicker) seeds them { 0, 0, 1, 0 } before AddString.
            u8   maFlags[4];                                               // +0x811
            u8   maPad815[3];                                              // +0x815 (sizeof == 0x818)

            // @0x823A6940 -- copy lpString into the next free 512-byte slot and record its
            // format type. The count is read as a SIGNED byte (X360 `lbz` + `extsb`).
            void AddString(const char* lpString, s32 liType)
            {
                CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");                   // h:390
                CGS_ASSERT(mi8NumStrings < KI_MAX_NUM_STRINGS,
                           "mi8NumStrings < KI_MAX_NUM_STRINGS");                       // h:391
                CGS_ASSERT(lpString != 0, "lpString");                                  // h:392

                std::strncpy(maacStrings[mi8NumStrings], lpString,
                             static_cast<size_t>(KI_MAX_STRING_LENGTH));
                maiStringTypes[mi8NumStrings] = liType;
                ++mi8NumStrings;
            }
        };

        // { 0x818, 537, 12, <the message> }, channel 40, 0x824 bytes.
        struct GuiTickerCustomMessageWire : public CgsGui::GuiEvent<537>
        {
            GuiTickerCustomMessagePayload mMessage;   // +0x0C
            GuiTickerCustomMessageWire()
            {
                std::memset(&mMessage, 0, sizeof(mMessage));
                mMessage.maFlags[2] = 1;   // the one non-zero seed (+0x813)
            }
        };
    }

    // ================================================================================
    // controller input
    // ================================================================================

    // ---- HandleControllerInput @ 0x824DCD80 ----------------------------------------
    // Every arm ends in UpdateCarouselTransition(), which is what actually steps the
    // highlight once enough travel has accumulated.
    void CarSelectVehicle::HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventKind)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");   // cpp:587

        CarSelectMain::HandleControllerInput(lpEvent, liEventKind);


        switch (liEventKind)
        {
        case KI_EVENT_CONTROLLER_DOWN:
        {
            // A held direction nudges the strip by KC_X_FRAME_CAROUSEL_ADJUST per sample,
            // but only after the press has been seen at least twice (the ref count the
            // PRESSED arm raises), so a single tap does not double-step.
            const ControllerButtonPayload* lpInput =
                reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

            if (lpInput->miButtonId == KI_ACTION_CAROUSEL_PREV_ALT)
            {
                if (muCarouselControllerLeftPressedRefCount > 1u)
                {
                    mfCarouselXOffsetDecay = 0.0f;
                    if (mCarSelector.HighlightPrevious(true))
                        mfCarouselXOffset += KC_X_FRAME_CAROUSEL_ADJUST;
                }
            }
            else if (lpInput->miButtonId == KI_ACTION_CAROUSEL_NEXT_ALT)
            {
                if (muCarouselControllerRightPressedRefCount > 1u)
                {
                    mfCarouselXOffsetDecay = 0.0f;
                    if (mCarSelector.HighlightNext(true))
                        mfCarouselXOffset -= KC_X_FRAME_CAROUSEL_ADJUST;
                }
            }
            break;
        }

        case KI_EVENT_CONTROLLER_PRESSED:
        {
            const ControllerButtonPayload* lpInput =
                reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

            if (lpInput->miButtonId == KI_ACTION_CAROUSEL_PREV)
            {
                // A fresh press while the strip is still coasting the other way is flushed
                // out first, one step at a time.
                if (muCarouselControllerLeftPressedRefCount == 0u && mfCarouselXOffsetDecay != 0.0f)
                {
                    while (!UpdateCarouselTransition())
                        ;
                }

                if (mCarSelector.HighlightPrevious(true))
                {
                    const u32 luRefCount = muCarouselControllerLeftPressedRefCount;
                    mfCarouselXOffsetDecay = 0.0f;
                    if (luRefCount == 0u)
                    {
                        mfCarouselXOffset      = 0.0f;
                        mfCarouselXOffsetDecay = KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                    }
                    muCarouselControllerLeftPressedRefCount = luRefCount + 1u;
                }

                mCarouselOverviewSelectableGroup.HighlightIndex(2);   // group slot 12
            }
            else if (lpInput->miButtonId == KI_ACTION_CAROUSEL_NEXT)
            {
                if (muCarouselControllerRightPressedRefCount == 0u && mfCarouselXOffsetDecay != 0.0f)
                {
                    while (!UpdateCarouselTransition())
                        ;
                }

                if (mCarSelector.HighlightNext(true))
                {
                    const u32 luRefCount = muCarouselControllerRightPressedRefCount;
                    mfCarouselXOffsetDecay = 0.0f;
                    if (luRefCount == 0u)
                    {
                        mfCarouselXOffset      = 0.0f;
                        mfCarouselXOffsetDecay = -KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                    }
                    muCarouselControllerRightPressedRefCount = luRefCount + 1u;
                }

                mCarouselOverviewSelectableGroup.HighlightIndex(2);
            }
            else if (lpInput->miButtonId == KI_ACTION_ACCEPT && !IsLoading())
            {
                // ⭐ THE VALIDATE PRESS. IsLoading() is dispatched through this class's own
                // vtable slot +0x2C, so a car change still in flight swallows the press.
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "RG :: CSV : SendStateEvent( \"ADVANCE\" )\n";

                SendStateEvent("ADVANCE");
            }
            break;
        }

        case KI_EVENT_CONTROLLER_RELEASED:
        {
            // A release stops the strip: either it starts a final decay step (when there is
            // travel left to unwind and the highlight can still move) or it snaps to rest.
            const ControllerButtonPayload* lpInput =
                reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

            switch (lpInput->miButtonId)
            {
            case KI_ACTION_CAROUSEL_PREV_ALT:
                if (mfCarouselXOffset != 0.0f && mCarSelector.HighlightPrevious(true))
                    mfCarouselXOffsetDecay = KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                else
                    mfCarouselXOffsetDecay = 0.0f;
                muCarouselControllerLeftPressedRefCount = 0u;
                break;

            case KI_ACTION_CAROUSEL_NEXT_ALT:
                if (mfCarouselXOffset != 0.0f && mCarSelector.HighlightNext(true))
                    mfCarouselXOffsetDecay = -KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                else
                    mfCarouselXOffsetDecay = 0.0f;
                muCarouselControllerRightPressedRefCount = 0u;
                break;

            case KI_ACTION_CAROUSEL_PREV:
                // The stick-driven pair only unwind when the axis actually drove them.
                if (!mbControllerAxisActive)
                    break;
                if (mfCarouselXOffset != 0.0f && mCarSelector.HighlightPrevious(true))
                    mfCarouselXOffsetDecay = KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                else
                    mfCarouselXOffsetDecay = 0.0f;
                muCarouselControllerLeftPressedRefCount = 0u;
                mbControllerAxisActive = false;
                break;

            case KI_ACTION_CAROUSEL_NEXT:
                if (!mbControllerAxisActive)
                    break;
                if (mfCarouselXOffset != 0.0f && mCarSelector.HighlightNext(true))
                    mfCarouselXOffsetDecay = -KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                else
                    mfCarouselXOffsetDecay = 0.0f;
                muCarouselControllerRightPressedRefCount = 0u;
                mbControllerAxisActive = false;
                break;

            default:
                break;
            }
            break;
        }

        case KI_EVENT_CONTROLLER_AXIS:
        {
            const ControllerAxisPayload* lpAxis =
                reinterpret_cast<const ControllerAxisPayload*>(lpEvent);

            // Only axis 0 (the left stick's X) drives the carousel.
            if (lpAxis->miAxis != 0)
                break;

            if (lpAxis->mfXAxis > KF_AXIS_DEAD_ZONE)
            {
                mbControllerAxisActive = true;
                if (muCarouselControllerRightPressedRefCount > 1u && mCarSelector.HighlightNext(true))
                {
                    mfCarouselXOffsetDecay = 0.0f;
                    mfCarouselXOffset +=
                        (KF_AXIS_DEAD_ZONE - lpAxis->mfXAxis) * KF_AXIS_TO_CAROUSEL_SCALE;
                }
            }
            else if (lpAxis->mfXAxis < -KF_AXIS_DEAD_ZONE)
            {
                mbControllerAxisActive = true;
                if (muCarouselControllerLeftPressedRefCount > 1u && mCarSelector.HighlightPrevious(true))
                {
                    mfCarouselXOffsetDecay = 0.0f;
                    mfCarouselXOffset +=
                        (-lpAxis->mfXAxis - KF_AXIS_DEAD_ZONE) * KF_AXIS_TO_CAROUSEL_SCALE;
                }
            }
            break;
        }

        default:
            break;
        }

        UpdateCarouselTransition();
    }

    // ================================================================================
    // selection
    // ================================================================================

    // ---- SetupCar @ 0x824D8108 -- this class's own virtual (X360 vtable +0x64) -----
    void CarSelectVehicle::SetupCar(const CarSetupInfo* lpSetupInfo, bool lbCommit)
    {
        // cpp:1038 -- streamed form on the console.
        CGS_ASSERT(lpSetupInfo != 0, "Invalid CarSetupInfo structure");

        // Only a real selection commits into mDesiredSetupInfo (and raises the
        // car-change-in-progress gate); a carousel scroll just re-skins the screen.
        if (lbCommit)
            CarSelectMain::SetupCar(lpSetupInfo);

        CarSelectMain::SetupCarNameComponent(lpSetupInfo->mCarId);
        SetupCarsUnlockedTextComponent();
        SetupStatsComponent(lpSetupInfo);
        SetCarouselComponent(lpSetupInfo->mCarId);

        // The console's own re-fetch, RESTORED 2026-08-02 (the substitution that read the
        // latched mpVehicleList is retired with the WorldDataController).
        mManufacturerLogo.Set(mpGuiCache->GetWorldDataController()->GetVehicleList(),
                              lpSetupInfo->mCarId);

        SetTicker(lpSetupInfo->mCarId);
    }

    // ---- SetTicker @ 0x824C9BC8 ----------------------------------------------------
    // Clear whatever the ticker is showing, then post the car's blurb line: the unlocked
    // form for a car the player can pick, the "how to win it" form otherwise.
    void CarSelectVehicle::SetTicker(CgsID lCarId)
    {
        // cpp:1074 -- the console's assert, RESTORED 2026-08-02 (LIST guard retired).
        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList");   // cpp:1074

        const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex(lCarId);
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liVehicleIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liVehicleIndex);
        // cpp:1082 -- the console's assert on the ENTRY. Still suppressed: the id it is handed
        // comes from a stand-in producer (shape (b) in BrnCarSelectVehicle.h).

        {
            GuiTickerFlagsWire536 lTickerClear;
            mpStateInterface->OutputGuiEvent(lTickerClear);
        }

        // ⚠️ ENTRY guard (shape (b) in BrnCarSelectVehicle.h). The console dereferences
        // lpVehicleData unconditionally below.
        if (lpVehicleData == 0)
            return;

        GuiTickerCustomMessageWire lTicker;

        // A livery variant advertises its PARENT car's blurb.
        CgsID lBlurbCarId = lCarId;
        const u8 luLiveryType = lpVehicleData->GetLiveryType();
        if (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
            lBlurbCarId = lpVehicleData->GetParentId();

        char lacCarId[16];
        CgsIDConvertToString(lBlurbCarId, lacCarId);

        char lacBlurbKey[32];
        CgsCore::SPrintf(lacBlurbKey, 31,
                         IsCarSelectable(lBlurbCarId) ? "CAR_BLURB_%s" : "CAR_BLURB_TO_WIN_%s",
                         lacCarId);
        lacBlurbKey[31] = 0;

        // Format type 2 == CgsLanguage::LanguageManager::E_FORMAT_MINUTES_SECONDS_HUNDREDTHS
        // in the parameter-format enum, but the ticker consumer uses the same word as its own
        // string-kind selector; the X360 literal is 2 and it is passed straight through.
        lTicker.mMessage.AddString(lacBlurbKey, 2);

        mpStateInterface->OutputGuiEvent(lTicker);
    }

    // ---- TriggerSound @ 0x824CA0F8 -------------------------------------------------
    void CarSelectVehicle::TriggerSound(bool lbClappers)
    {
        GuiAudioTriggerEvent lAudio;
        if (lbClappers)
            lAudio.Construct(KI_AUDIO_ACTION_CAROUSEL, "", "CodeCarChoiceCarouselClappers", "");
        else
            lAudio.Construct(KI_AUDIO_ACTION_CAROUSEL, "", "CodeCarChoiceCarousel", "");

        mpStateInterface->OutputGuiEvent(lAudio);
    }

    // ================================================================================
    // game-state events
    // ================================================================================

    // ---- HandleCarInfoResponseEvent @ 0x824BEDC0 -----------------------------------
    // Event 412 -- the ONLY producer of this screen's car list. The game-state module
    // publishes game action 184 and BrnGameModule::TranslateGameActionsToGuiEvents
    // @0x823EBCA4 turns it into this record.
    void CarSelectVehicle::HandleCarInfoResponseEvent(const CgsModule::Event* lpEvent,
                                                      s32 liEventType)
    {
        // cpp:582 -- the assert's baked file is BrnCarSelectMain.cpp (a copy-paste in the
        // original), so the line number belongs to that file, not this one.
        CGS_ASSERT(lpEvent != 0, "lpEvent");

        if (liEventType != 412)
            return;

        const GuiCarSelectionPayload* lpPayload =
            reinterpret_cast<const GuiCarSelectionPayload*>(lpEvent);

        // The two inlined CgsContainers::Array<s64,128> guards.
        CGS_ASSERT(lpPayload->miCount != -1,
                   "Array used before Construct/Clear was called");   // CgsArray.h:336
        CGS_ASSERT(lpPayload->miCount < KI_MAX_SELECTABLE_CARS,
                   "Too many cars for the selection");                // cpp:959 (streamed form)

        gsiNumCarouselCars = 0;
        for (s32 liCar = 0; liCar < lpPayload->miCount; ++liCar)
        {
            CGS_ASSERT(lpPayload->miCount != -1,
                       "Array used before Construct/Clear was called");   // CgsArray.h:336
            maSelectedCars[gsiNumCarouselCars] = lpPayload->maCarIds[liCar];
            gsiNumCarouselCars = gsiNumCarouselCars + 1;
        }

        // Both 128-bit state arrays are copied wholesale (two `ld`/`std` pairs each).
        std::memcpy(&maSelectedCarsDrivenState, lpPayload->mau64DrivenBits,
                    sizeof(lpPayload->mau64DrivenBits));
        std::memcpy(&maSelectedCarsWreckedState, lpPayload->mau64WreckedBits,
                    sizeof(lpPayload->mau64WreckedBits));

        gsiNumCarsUnlockedTotal = lpPayload->miNumCarsUnlocked;
    }

    // ---- HandleLobbyPlayerList @ 0x824C9E58 ----------------------------------------
    // RECONSTRUCTED 2026-09-07 from this function's OWN X360 export (asm arbitrates); the
    // tripwire that stood here is retired. Both of the things its banner named as missing
    // have landed: CarSelectOnlinePlayerList::Show / Hide / SetPlayerName / SetPlayerCar /
    // SetFinalSelection are bodied in that component's own TU, and the event-244 record is
    // the eight LobbyPlayerStatusData rows + the live count, read through the rows' real
    // home rather than a forked layout.
    //
    // Event 244 is the online lobby roster. The whole body is behind an ONLINE-ONLY entry
    // gate (`lwz 0x288` != 0 and `lwz 0x7C8` == 2), so the offline Junkyard flow -- which
    // observes 244 too -- falls straight out.
    //
    // Past the gate the console splits on the lobby's VEHICLE-CHOICE mode, read out of the
    // GuiCache (`lwzx r11, mpGuiCache, 0xA9C8`, `cmplwi 1`):
    //   * 0  -> straight to the row refresh.
    //   * 1  -> "the host picks the car for everybody": latch the roster's host row once,
    //           and if THIS client is not that row, accept immediately on the client's
    //           behalf instead of showing a table it cannot drive. If it IS the host row,
    //           fall through to the row refresh.
    //   * >1 -> the console's own misspelt assert, then the row refresh anyway.
    //
    // ⓘ NAMING, FLAGGED. The word at cache+0xA9C8 is exposed as GuiCache::GetOnlineHostGameState()
    // (BrnGuiCache.h:381), which is a CONSUMER name minted by the CarSelectOnlineEnd TU; that
    // header already notes the slot is structurally the game-params mirror's meVehicleChoice and
    // marks the naming MERGE RECONCILE PENDING. THIS body's own assert text -- "Not a valid
    // vehicle chouce mode : ", the console's typo -- settles it: the slot is the vehicle-choice
    // mode. The accessor is called by its committed name here so this TU does not fork the cache
    // header; the rename belongs to whoever next owns BrnGuiCache.h.
    void CarSelectVehicle::HandleLobbyPlayerList(const GuiEventNetworkLobbyPlayerList* lpEvent)
    {
        if (mpGuiCache == 0
            || meCarSelectType != BrnGameState::GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START)
        {
            return;
        }

        const GuiEventNetworkLobbyPlayerListPayload* lpPlayerList =
            reinterpret_cast<const GuiEventNetworkLobbyPlayerListPayload*>(lpEvent);

        if (mpGuiCache->GetOnlineHostGameState() == KI_VEHICLE_CHOICE_HOST_PICKS)
        {
            // The host row is latched ONCE: the console skips this whole arm while
            // mpHostStatusData is already set (`lwz 0x4124 ; bne -> the row refresh`).
            if (mpHostStatusData == 0)
            {
                for (s32 liPlayer = 0; liPlayer < lpPlayerList->miNumPlayers; ++liPlayer)
                {
                    // X360 `lbz r8, 0(r10)` walking row+0x31 with a 56-byte stride.
                    if (lpPlayerList->maPlayers[liPlayer].mbIsHost)
                    {
                        // ⚠️ The console stores a pointer INTO THE EVENT RECORD
                        // (`mulli r11, r11, 0x38 ; add r11, r11, <event> ; stw r11, 0x4124`),
                        // not a copy of the row -- so mpHostStatusData is only good for as
                        // long as the queued record is. Reproduced, not "fixed": the member's
                        // only reader is the byte test two statements down, in this same call.
                        mpHostStatusData = &lpPlayerList->maPlayers[liPlayer];
                        break;
                    }
                }

                CGS_ASSERT(mpHostStatusData != 0, "mpHostStatusData");   // cpp:1417

            }
        }
        else
        {
            // cpp:1444, the console's streamed assert (its own spelling of "choice"). It fires
            // only for a mode ABOVE 1 -- mode 0 branches past it -- and it is non-gating, so
            // the row refresh below runs either way.
            CGS_ASSERT(static_cast<u32>(mpGuiCache->GetOnlineHostGameState())
                           <= static_cast<u32>(KI_VEHICLE_CHOICE_HOST_PICKS),
                       "Not a valid vehicle chouce mode : ");
        }

        // ---- the row refresh (X360 loc_824C9F0C) ------------------------------------
        if (meCurrentState != E_CARSELECT_VISIBLE_INTERACTIVE)
        {
            return;
        }

        // liPlayer is carried OUT of the fill loop into the hide sweep (X360 r31), which is
        // why it is declared here and the sweep has no initialiser. The count is a signed
        // compare and bounds the fill loop on its own, exactly as the console does it.
        s32 liPlayer = 0;
        for (; liPlayer < lpPlayerList->miNumPlayers; ++liPlayer)
        {
            const BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData& lrPlayer =
                lpPlayerList->maPlayers[liPlayer];

            // A row that is not up yet is shown and named once; the name is only ever
            // fetched on that transition.
            if (!mOnlinePlayerList.IsShowing(liPlayer))
            {
                mOnlinePlayerList.Show(liPlayer);

                // The console fetches the record twice around the assert -- that is the assert
                // macro expanding, not two reads -- and derefs it unguarded either way.
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                    mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lrPlayer.mPlayerID);

                CGS_ASSERT(lpPlayerInfo != 0,
                           "Trying to show the name of a player who isn't in our game");  // cpp:1464

                // X360 `addi r5, r3, 0x100` -- the record's name field at console +256.
                mOnlinePlayerList.SetPlayerName(liPlayer, lpPlayerInfo->mPlayerName.macName);
            }

            // Driven every refresh, shown or not: the tick and the car can change under a row
            // that is already up. ⚠️ Hex-Rays renders the car fetch as two 32-bit reads
            // (`*(v13-4)`, `*(v13-3)`) and drops the index; the asm is one `ld r5, -0x10(r30)`
            // == row+0x00, the 64-bit CgsID, with the index still in r4.
            mOnlinePlayerList.SetFinalSelection(liPlayer, lrPlayer.mbFinalSelection);  // lbz row+0x32
            mOnlinePlayerList.SetPlayerCar(liPlayer, lrPlayer.mSelectedCarID);         // ld  row+0x00
        }

        // Take down the tail, starting from the first row the roster did not fill. The rows are
        // contiguous, so the sweep stops at the first one already hidden rather than running to
        // the end of the bank. Entered even when the roster is empty (the console's `ble` jumps
        // straight in with the index still zero).
        for (; liPlayer < CarSelectOnlinePlayerList::KI_MAX_PLAYERS
               && mOnlinePlayerList.IsShowing(liPlayer); ++liPlayer)
        {
            mOnlinePlayerList.Hide(liPlayer);
        }
    }
}

#undef CGS_ARTIST_FILE
