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

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (register / play-apt / out-queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / VariableEventQueue
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (debug print)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"  // GuiEventAptTriggerPayload
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (+ GuiFlow)
#include "GameSource/Gui/BrnGuiOverlaysDirector.h"                        // BrnGui::GuiOverlayRequest (the ticker wire)
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
                : CgsGui::GuiEvent<409>(static_cast<u32>(sizeof(GuiTickerFlagsPayload)), 12)
            {
                mPayload.mbFlagA = 1;
                mPayload.mbFlagB = 0;
            }
        };

        // The 304-byte overlay/ticker request OnEnter posts for the "DwnldTitleUp" title
        // ticker: { 288, 184, 16, <pad>, the 288-byte request }, channel 40. Identical
        // shape to BrnCarSelectMain_wG_02.cpp's GuiOverlayRequestWire.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C (payload is 16-aligned past the header)
            GuiOverlayRequest mRequest;   // +0x10
            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0) {}
        };

        // The one-shot car-select-type record Update posts on its first frame:
        // { 8, 192, 12, u64 carSelectType }, channel 40, 20 bytes.
        struct GuiCarSelectTypeWire : public CgsGui::GuiEvent<192>
        {
            u64 mu64CarSelectType;   // +0x0C (X360 std of the 64-bit widened enum word)
            GuiCarSelectTypeWire()
                : CgsGui::GuiEvent<192>(8, 12)
                , mu64CarSelectType(0) {}
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
    const CgsGui::sResourceTuple CarSelectVehicle::maResourcesToLoad[6] =
    {
        { 149u, CgsGui::E_GUI_RESOURCETYPE_APT },
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
        CGS_ASSERT(lpFsm != 0, "Invalid ScriptedFsm ptr");

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
        mTitleText.Construct("TitleText", mpStateInterface, 0);

        {
            GuiTickerFlagsWire409 lTickerOn;
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lTickerOn, KI_CHANNEL_GUI_OUT,
                                                             static_cast<s32>(sizeof(lTickerOn)));
        }

        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mCarSelector.Construct(mpacCarSelectorName, mpStateInterface, 0,
                               static_cast<u64>(-1));

        // NOTE the console order: the three gauges, then CarsUnlocked, THEN CarType.
        mSpeedStatsBar.Construct(KAC_SPEED_STATS_BAR_NAME, mpStateInterface, 0);
        mBoostStatsBar.Construct(KAC_BOOST_STATS_BAR_NAME, mpStateInterface, 0);
        mStrengthStatsBar.Construct(KAC_STRENGTH_STATS_BAR_NAME, mpStateInterface, 0);
        mCarsUnlocked.Construct(KAC_CARS_UNLOCKED_NAME, mpStateInterface, 0);
        mCarType.Construct(KAC_CAR_TYPE, mpStateInterface, 0);

        // The pooled empty string @0x820046A7 -- the overview group has no apt clip of its own.
        mCarouselOverviewSelectableGroup.Construct("", mpStateInterface, 0,
                                                   static_cast<u64>(-1));
        mCarouselOverviewSelectableGroup.SetHighlightable(true);   // group vtable slot 1

        for (s32 liIcon = 0; liIcon < KI_NUMBER_VISIBLE_VEHICLE_ICONS; ++liIcon)
        {
            // ⚠️ 1-BASED: the X360 does `addi r26, r11, 1` before the SPrintf, so the clips
            // are Overview_1_mc .. Overview_5_mc. The capacity argument is 31.
            char lacName[32];
            CgsCore::SPrintf(lacName, 31, "Overview_%i_mc", liIcon + 1);

            maCarouselOverviewSelectable[liIcon].Construct(lacName, mpStateInterface, 0);
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
            GuiOverlayRequestWire lTitleTicker;
            // The X360 zero-fills the 288-byte payload and writes only its leading id word.
            std::memset(&lTitleTicker.mRequest, 0, sizeof(lTitleTicker.mRequest));
            *reinterpret_cast<CgsID*>(&lTitleTicker.mRequest) = CgsIDCompress("DwnldTitleUp");

            mpStateInterface->GetOutputEventQueue()->AddEvent(&lTitleTicker, KI_CHANNEL_GUI_OUT,
                                                              static_cast<s32>(sizeof(lTitleTicker)));
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
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
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
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:319

            GuiCarSelectTypeWire lCarSelectType;
            lCarSelectType.mu64CarSelectType =
                static_cast<u64>(mpGuiCache->GetCurrentCarSelectType());
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lCarSelectType, KI_CHANNEL_GUI_OUT,
                                                              static_cast<s32>(sizeof(lCarSelectType)));

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

        CGS_ASSERT(mpGuiCache != 0, "lpGuiCache");   // cpp:346

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, "TitleText");
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, "Waiting");
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpacCarSelectorName);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, KAC_BOOST_STATS_BAR_NAME);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, KAC_SPEED_STATS_BAR_NAME);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, KAC_STRENGTH_STATS_BAR_NAME);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, KAC_SLIDER_BAR_NAME);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, "HelpItemToggle_mc");
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTrackAnimTransitionComponent.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMainAnimComponent.GetName());
    }

    // ---- PlayMovie @ 0x824C9DE8 ----------------------------------------------------
    // The inlined OutputGuiEvent<GuiEventPlayAptMovie> record { 8, 18, 12, <name>, 3 } on
    // channel 41 -- exactly what StateInterface::PlayAptMovie posts. The movie name comes
    // from gGuiResourceIdentifier[149] through off_82F27B34, i.e. the same resource this
    // state's maResourcesToLoad[0] loads.
    void CarSelectVehicle::PlayMovie()
    {
        mpStateInterface->PlayAptMovie("BrnCarSelectMain", 3);
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
        for (s32 liCar = 0; liCar < gsiNumCarouselCars; ++liCar)
        {
            if (maSelectedCars[liCar] == lCarId)
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
        const char* lpacName = lpTrigger->mpacComponentName;

        if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
        {
            if (std::strstr(lpacName, mOnlinePlayerList.GetName()) != 0)
                mOnlinePlayerList.HandleLoadNotification(lpacName);
        }
        else if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
        {
            if (std::strstr(lpacName, KAC_SPEED_STATS_BAR_NAME) != 0)
            {
                mSpeedStatsBar.HandleTransitionComplete(lpacName, lpTrigger->miUniqueId);
            }
            else if (std::strstr(lpacName, KAC_BOOST_STATS_BAR_NAME) != 0)
            {
                mBoostStatsBar.HandleTransitionComplete(lpacName, lpTrigger->miUniqueId);
            }
            else if (std::strstr(lpacName, KAC_STRENGTH_STATS_BAR_NAME) != 0)
            {
                mStrengthStatsBar.HandleTransitionComplete(lpacName, lpTrigger->miUniqueId);
            }
        }
    }
}
