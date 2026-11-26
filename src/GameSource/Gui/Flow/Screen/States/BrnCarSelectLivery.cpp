// ===================================================================================
// BrnGui::CarSelectLivery  -- partfile 01: statics + the FSM / resource surface
//   class:BrnGui::CarSelectLivery
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm + pseudocode):
//   CarSelectLivery (ctor)   @ 0x82508868
//   Construct                @ 0x824BEB40   (DWARF cpp:105)
//   OnEnter                  @ 0x824D6960   (DWARF cpp:120)
//   OnLeave                  @ 0x824D6C30   (DWARF cpp:167)
//   Update                   @ 0x824DFCD0   (DWARF cpp:191)
//   PlayMovie                @ 0x824C81D0   (DWARF cpp:426)
//   AppendAptComponents      @ 0x824BB970   (DWARF cpp:442)
//   GetResourcesToLoad       @ 0x824B5180   (DWARF cpp:974)
//   GetNumberResourcesToLoad               (DWARF cpp:987 -- inlined / export hole)
//   IsLoading                @ 0x824BBBF0   (DWARF cpp:1221)
//   CanCarBePainted          @ 0x824B52E0   (DWARF cpp:1225)
//   TriggerSound             @ 0x824C8898   (DWARF cpp:1203)
//
// The screen build lives in BrnCarSelectLivery_Components.cpp and the input / event handlers
// in BrnCarSelectLivery_Input.cpp; see BrnCarSelectLivery.h for the layout and the handover
// chain this screen closes.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectLivery.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCarSelectLivery.cpp"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (register / play-apt / out-queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / VariableEventQueue
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (debug print)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"  // GuiEventAptTriggerPayload
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (+ GuiFlow)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry

#include <cstring>   // std::strstr

// includes folded in from the BrnCarSelectLivery_w*.cpp partfiles (2026-09-15)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::ECarSelectType
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerList.h"           // the list component
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData

namespace BrnGui
{
    namespace
    {
        // The AddEvent channel every out-queue record in this class uses (GuiEventOut).
        const s32 KI_CHANNEL_GUI_OUT = 40;
        // PlayAptMovie's channel (the same one CgsGui::StateInterface::PlayAptMovie posts on).
        const s32 KI_CHANNEL_GUI_APT = 41;

        // The state's inbound GUI queue, exactly as the sibling car-select states model it.
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- the three observed event ids Update dispatches itself -------------------
        // (the other eight in maiEventToObserve are handled by CarSelectMain's own
        //  ProcesssIncomingEvents ladder: 6/7/8 controller, 64 cache, 412/406/413/414.)
        const s32 KI_EVENT_APT_TRIGGER       = 21;
        const s32 KI_EVENT_COUNTDOWN_TIME    = 82;
        const s32 KI_EVENT_LOBBY_PLAYER_LIST = 244;

        // The two game modes CanCarBePainted refuses to paint in (X360 `cmpwi 12` / `cmpwi 17`
        // against the params-mirror meGameMode). FLAG: the mode names are not in the recovered
        // GsmIO::EGameModeType slice for these two values, so they stay numeric.
        const s32 KI_GAMEMODE_NO_PAINT_A = 12;
        const s32 KI_GAMEMODE_NO_PAINT_B = 17;

        // The GuiAudioTriggerEvent record's queued event id -- the same 100-byte record
        // BrnGui::GuiAudioTriggerEvent models under id 201, posted here under 457 (identical
        // to BrnCarSelectVehicle's carousel trigger).
        const u32 KU_AUDIO_TRIGGER_EVENT_ID = 457;

        // ---- out-queue wire records --------------------------------------------------

        // OnLeave's screen-leave command: { 1, 80, 12, u8 }, channel 40, 16 bytes.
        // ⚠️ The X360 writes the three header words and NEVER writes the one payload byte
        // (asm @0x824D6C88..0x824D6C98: three `stw`, no `stb`) -- it posts whatever was on the
        // stack. Reconstructed as an explicit zero; there is no defined value to copy.
        // No data: the 1-byte record payload is the empty GuiEvent<80> tag itself;
        // ARTIST copies it without writing it.
        struct GuiScreenLeaveWire80 : public CgsGui::GuiEvent<80>
        {
        };

        // The play-apt-movie record PlayMovie posts: { 8, 18, 12, <name>, 3 }, channel 41,
        // 20 bytes -- exactly what StateInterface::PlayAptMovie builds.
        // (Posted through PlayAptMovie below rather than open-coded.)

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

    // @0x82F26C50 -- six APT resources. All six bundles are present in build\game\GUIAPT\.
    CgsGui::sResourceTuple       CarSelectLivery::maResourcesToLoad[6] =
    {
        { 150u, CgsGui::E_GUI_RESOURCETYPE_APT },   // BrnCarSelectLivery
        {  34u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5MenuToggle
        {  35u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5MenuItemColourPicker
        {  30u, CgsGui::E_GUI_RESOURCETYPE_APT },   // ColourField
        {  94u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5OnlineCarSelectComponents
        {  55u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5ManufacturersIcon
    };
    const u32 CarSelectLivery::muNumResourcesToLoad = 6;

    // @0x82065DC0 -- dumped big-endian. Self-corroborating: 6/7/8 (controller), 64 (gui
    // cache), 412/406/413/414 (the four car-select responses) are the base's ladder, and
    // 21/82/244 are exactly the three ids Update dispatches itself.
    const s32 CarSelectLivery::maiEventToObserve[11] =
    {
        6, 7, 8, 21, 64, 412, 406, 413, 414, 82, 244,
    };
    const s32 CarSelectLivery::miNumEventsObserved = 11;

    // @0x82F26C80 -> "CarName".
    const char* CarSelectLivery::mpacCarSelectorName = "CarName";

    // @0x82F26C84 / @0x82F26C88 -- the adjacent pointer pair SetupComponents hands to the two
    // SetupToggle calls as the row TITLES (the DWARF's KAPC_OPTIONS[2], cpp:69).
    const char* const CarSelectLivery::KAPC_OPTIONS[CarSelectLivery::E_OPTIONTOGGLE_COUNT] =
    {
        "$CAR_MOD_LIVERY",         // [0] E_OPTIONTOGGLE_LIVERY
        "$CAR_MOD_PAINT_FINISH",   // [1] E_OPTIONTOGGLE_PAINT_FINISH
    };

    // The component names. See the header banner: the DWARF's char[13] / char[19] sizes fit
    // these exactly, and they are what OnEnter passes to the two toggles' Construct.
    const char CarSelectLivery::KAC_OPTION_NAME[13]              = "optionToggle";
    const char CarSelectLivery::KAC_PAINT_COLOUR_OPTION_NAME[19] = "colourOptionToggle";
    const char CarSelectLivery::KAC_HELPITEM_RESTORE[19]         = "HelpItemRestore_mc";
    const char CarSelectLivery::KAC_ONLINE_COUNTDOWN_NAME[13]    = "Countdown_mc";
    const char CarSelectLivery::KAC_ONLINE_PLAYER_LIST[15]       = "PlayerTable_mc";
    const char CarSelectLivery::macFadeScreenAnimComponentName[13] = "FadeAnim_mcp";
    const char CarSelectLivery::macHelpBarAnimComponentName[16]    = "HelpBarAnim_mcp";

    // ================================================================================
    // ctor / Construct
    // ================================================================================

    // ---- CarSelectLivery (ctor) @ 0x82508868 ---------------------------------------
    // The X360 ctor stores this state's own vtable (+0x000, off_820754D8) plus the long run
    // of embedded sub-object vtables across the object (the highest store is +0x27B4 * 4) and
    // calls the MenuToggleGroupVarSize<2> / ColourMenuToggle / TextSelection sub-object ctors.
    // Every one of those is emitted implicitly in C++ -- by this object's construction, by the
    // CarSelectMain base and by the embedded members' default ctors -- so the body is empty.
    // It stores no field values (Construct / OnEnter do that).
    CarSelectLivery::CarSelectLivery()
    {
    }

    // ---- Construct @ 0x824BEB40 ----------------------------------------------------
    void CarSelectLivery::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        // cpp:107 -- streamed on the console; the call site passes a plain string.
        CGS_ASSERT(lpFsm != 0, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid ScriptedFsm ptr", CGS_ARTIST_FILE, 107));

        CarSelectMain::Construct(liId, lpFsm);
    }

    // ================================================================================
    // OnEnter / OnLeave
    // ================================================================================

    // ---- OnEnter @ 0x824D6960 ------------------------------------------------------
    void CarSelectLivery::OnEnter()
    {
        CarSelectMain::OnEnter();

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            // The X360 inlines GetAccessPointers (CgsGuiStateInterface.h:344), GetGuiCache
            // (CgsGuiShared.h:201) and GetCurrentCarSelectType's own assert
            // (BrnGuiCache.h:4378) into this one debug line.
            *CgsDev::Log::gpDebugPrint << "RG :: CSL : Entering Car Select - "
                                       << mpStateInterface->GetAccessPointers()->GetGuiCache()->GetCurrentCarSelectType()
                                       << "\n";
        }

        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mCarSelector.Construct(mpacCarSelectorName, mpStateInterface, 0,
                               static_cast<u64>(-1));

        mOptionToggles.Construct(KAC_OPTION_NAME, mpStateInterface, E_OPTIONTOGGLE_COUNT, 0,
                                 static_cast<u64>(-1));
        mColourMenuToggle.Construct(KAC_PAINT_COLOUR_OPTION_NAME, mpStateInterface, 0,
                                    static_cast<u64>(-1));
        mColourMenuToggle.Clear();   // toggle slot 6

        mOnlineCountdown.Construct(KAC_ONLINE_COUNTDOWN_NAME, mpStateInterface, 0);
        mOnlinePlayerList.Construct(KAC_ONLINE_PLAYER_LIST, mpStateInterface, 0);

        // NOTE the console order: the group is SIZED between the two online widgets and the
        // two animation components.
        mOptionToggles.SetupGroup(E_OPTIONTOGGLE_COUNT, 0);

        mFadeScreenAnimComponent.Construct(macFadeScreenAnimComponentName, mpStateInterface, 0);
        mHelpBarAnimComponent.Construct(macHelpBarAnimComponentName, mpStateInterface, 0);

        mbFirstFrame                 = true;    // stb 1, 46088
        mbFirstUpdateComponentsFrame = true;    // stb 1, 46089
        mbDisableCarModifyInteraction = false;  // stb 0, 46090
        mbFadeScreenPending          = false;   // stb 0, 42604
        mbExiting                    = false;   // stb 0, 46091
        mHelpBarButtonCount          = -1;      // stw -1, 42748

        mHelpItemRestoreDefaultPaint.Construct(KAC_HELPITEM_RESTORE, mpStateInterface, 0);
    }

    // ---- OnLeave @ 0x824D6C30 ------------------------------------------------------
    void CarSelectLivery::OnLeave()
    {
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 169);   // cpp:169

        {
            GuiScreenLeaveWire80 lLeave;
            mpStateInterface->OutputGuiEvent(lLeave);
        }

        // Re-arm the one-shot transition gate this screen consumes (see SetupComponents and
        // Update's auto-accept arm).
        mpGuiCache->SetCarSelectTransitionAlreadyShown(false);

        mOptionToggles.Clear();      // group slot 6
        mColourMenuToggle.Clear();   // toggle slot 6

        CarSelectMain::OnLeave();
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // ================================================================================
    // Update
    // ================================================================================

    // ---- Update @ 0x824DFCD0 -------------------------------------------------------
    void CarSelectLivery::Update()
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
            {
                // Trigger TYPE 1 (a component load notification) whose reported clip name
                // contains the online player table's own component name is routed to it.
                // A SUBSTRING match (strstr), not an equality compare.
                const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
                    reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
                if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD
                    && std::strstr(lpTrigger->mpacComponentName, mOnlinePlayerList.GetName()) != 0)
                {
                    mOnlinePlayerList.HandleLoadNotification(lpTrigger->mpacComponentName);
                }
                break;
            }

            case KI_EVENT_COUNTDOWN_TIME:
                mOnlineCountdown.SetTimeLeft(
                    reinterpret_cast<const GuiCountdownTimePayload*>(lpEvent)->mfTimeLeft);
                break;

            case KI_EVENT_LOBBY_PLAYER_LIST:
                HandleLobbyPlayerList(
                    reinterpret_cast<const GuiEventNetworkLobbyPlayerList*>(lpEvent));
                break;

            default:
                break;
            }
        }

        CarSelectMain::Update();

        if (mbFirstFrame)
        {
            CGS_ASSERT(mpGuiCache != 0,
                       CGS_ASSERT_MESSAGE_AT("mpGuiCache", CGS_ARTIST_FILE, 239));

            const BrnGameState::GameStateModuleIO::ECarSelectType leCarSelectType =
                mpGuiCache->GetCurrentCarSelectType();
            GuiEventActivateCarSelect lCarSelectType;
            lCarSelectType.meState = BrnGameState::GameStateModuleIO::E_CAR_SELECT_STATE_LIVERY;
            lCarSelectType.meCarSelectType = leCarSelectType;
            mpStateInterface->OutputGuiEvent(lCarSelectType);

            mbFirstFrame = false;
        }

        if (meCurrentState >= E_CARSELECT_VISIBLE_INTERACTIVE)
        {
            UpdateComponents();

            // ⭐ THE AUTO-ACCEPT ARM. When the flow has already shown its car-select
            // transition once, this screen does not wait for a press: the first interactive
            // frame on which nothing is loading accepts the selection itself, through the
            // same vtable +0x5C the A press uses. OnLeave clears the gate again.
            if (mpGuiCache->GetCarSelectTransitionAlreadyShown() && !mbExiting && !IsLoading())
            {
                mbExiting = true;
                ExitCarSelection();
            }
        }
    }

    // ================================================================================
    // resource / apt surface
    // ================================================================================

    // ---- PlayMovie @ 0x824C81D0 ----------------------------------------------------
    // The inlined OutputGuiEvent<GuiEventPlayAptMovie> record { 8, 18, 12, <name>, 3 } on
    // channel 41. The movie name comes from gGuiResourceIdentifier[150] through
    // off_82F27B38, i.e. the same resource maResourcesToLoad[0] loads.
    void CarSelectLivery::PlayMovie()
    {
        mpStateInterface->PlayAptMovie("BrnCarSelectLivery", 3);
    }

    // ---- AppendAptComponents @ 0x824BB970 ------------------------------------------
    // ⚠️ The two animation names are GetName() reads off the two AnimationComponents, not the
    // static literals: they are only valid once OnEnter has Constructed them, which is why
    // this function and OnEnter have to land together.
    void CarSelectLivery::AppendAptComponents()
    {
        CarSelectMain::AppendAptComponents();

        CGS_ASSERT_AT(mpGuiCache != 0, "lpGuiCache", CGS_ARTIST_FILE, 447);   // cpp:447

        // The group forwards each live row's own three names (row / title / item selection).
        mOptionToggles.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mFadeScreenAnimComponent.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mHelpBarAnimComponent.GetName());
    }

    // ---- GetResourcesToLoad @ 0x824B5180 / GetNumberResourcesToLoad (DWARF cpp:987) --
    CgsGui::sResourceTuple* CarSelectLivery::GetResourcesToLoad() const
    {
        return const_cast<CgsGui::sResourceTuple*>(maResourcesToLoad);
    }

    u32 CarSelectLivery::GetNumberResourcesToLoad() const
    {
        return muNumResourcesToLoad;
    }

    // ================================================================================
    // helpers
    // ================================================================================

    // ---- IsLoading @ 0x824BBBF0 ----------------------------------------------------
    // `return *(this + 1988) || *(this + 692) != *(this + 660)` -- Hex-Rays splits the 64-bit
    // id compare into its two dwords. +1988 is mbCarChangeInProgress, +688 is
    // miMostRecentDropInId and +656 is mCurrentSetupInfo.mCarId.
    bool CarSelectLivery::IsLoading() const
    {
        if (mbCarChangeInProgress)
        {
            return true;
        }
        return miMostRecentDropInId != mCurrentSetupInfo.mCarId;
    }

    // ---- CanCarBePainted @ 0x824B52E0 ----------------------------------------------
    // ⭐ The console takes the entry's GAMEPLAY-DATA sub-record (entry + 0x90) and reads
    // `(*(u32*)(data + 4) >> 6) & 1` == bit 6 of the flags word at entry + 0x94. That is the
    // SAME bit BrnResource::VehicleListEntry::CanAutoRepair() already names, recovered
    // independently from DriveThruManager::HandleDriveThru's body-shop pre-check -- the two
    // consumers agree, which is what a body shop does in this game (repair AND repaint). The
    // committed accessor is reused rather than forking a second reader of the same bit; this
    // site is the reason its FLAG can now record a second attested role.
    bool CarSelectLivery::CanCarBePainted(const BrnResource::VehicleListEntry* lpVehicleListEntry) const
    {
        CGS_ASSERT(lpVehicleListEntry != 0, CGS_ASSERT_MESSAGE_AT("lpVehicleListEntryGamePlayData", CGS_ARTIST_FILE, 1225 /* ARTIST 0x824B5318 */));   // cpp:1225
        CGS_ASSERT(mpGuiCache != 0, CGS_ASSERT_MESSAGE_AT("lpGameParams", CGS_ARTIST_FILE, 1228 /* ARTIST 0x824B5348 */));                             // cpp:1228

        // ⚠️ ENTRY guard, not in the X360 body. The console dereferences the entry
        // unconditionally because the game state always publishes a live drop-in car id
        // before this screen goes interactive; on this build that id comes from
        // BrnGameModule::PublishCarSelectionToGui, a flagged stand-in. "Cannot be painted" is
        // the meaning of an unresolved car, so the guard is behaviour-preserving for every
        // case the console can actually reach. Remove it with the stand-in.
        if (lpVehicleListEntry == 0)
        {
            return false;
        }

        const s32 leGameMode = mpGuiCache->GetOnlineGameMode();
        if (leGameMode == KI_GAMEMODE_NO_PAINT_A || leGameMode == KI_GAMEMODE_NO_PAINT_B)
        {
            return false;
        }

        return lpVehicleListEntry->CanAutoRepair();
    }

    // ---- TriggerSound @ 0x824C8898 -------------------------------------------------
    // { 100, 457, 12, <the 100-byte audio record> }, 112 bytes on channel 40 -- the same
    // record shape BrnCarSelectVehicle::TriggerSound posts, with this screen's own sound
    // name and the caller's PresentationAction as the action word.
    void CarSelectLivery::TriggerSound(s32 leAction)
    {
        GuiAudioTriggerEvent lAudio;
        lAudio.Construct(leAction, "", "CodeLivery", "");


        mpStateInterface->OutputGuiEvent(lAudio);
    }
}

// ============================================================================
// FOLDED FROM BrnCarSelectLivery_wJ_01.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::CarSelectLivery -- wave-J partfile 01.
//   HandleLobbyPlayerList @0x824B5190
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm arbitrates; Hex-Rays is
// a hint only). The five CarSelectOnlinePlayerList methods this body calls are declared in
// that component's header and link from its own TU.
// ===================================================================================


namespace BrnGui
{
    namespace
    {
        // ---- in-queue payload view (event 244, GuiEventNetworkLobbyPlayerList) -------
        // The queue delivers the RAW record -- this event carries no CgsGui::GuiEvent
        // header (AddGuiEvent<GuiEventNetworkLobbyPlayerList> @0x823CF4C8 hands
        // VariableEventQueue::AddEvent the record with id 244 and length 456 passed
        // out-of-band). The 456 bytes are the eight lobby rows then the live count.
        // Row fields come from the record's real home,
        // BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h -- no forked layout here.
        // Same view as the wave-H consumer, BrnOnlineGameRoomPlayerInfo_wH_15.cpp:60-70.
        struct GuiEventNetworkLobbyPlayerListPayload
        {
            BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData
                maPlayers[CarSelectOnlinePlayerList::KI_MAX_PLAYERS];   // +0x000
            s32 miNumPlayers;                                           // X360 +0x1C0 (448)
        };
    }

    // -------------------------------------------- HandleLobbyPlayerList @ 0x824B5190
    // The network module republished the lobby roster while this screen is up. Bring the
    // eight-row player table in line with it: show and name any row that is not up yet,
    // re-drive every live row's car and final-selection tick, then take down the tail.
    //
    // The whole body is behind an ONLINE-ONLY guard (X360 `lwz 0x288` / `0x7C8 == 2` /
    // `0x7CC == 2`): a GuiCache must be bound, this must be the online-event-start car
    // select, and the screen must be in its interactive state. The offline Junkyard flow
    // observes event 244 too and falls straight out here.
    void CarSelectLivery::HandleLobbyPlayerList(const GuiEventNetworkLobbyPlayerList* lpEvent)
    {
        if (mpGuiCache == 0
            || meCarSelectType != BrnGameState::GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START
            || meCurrentState != E_CARSELECT_VISIBLE_INTERACTIVE)
        {
            return;
        }

        const GuiEventNetworkLobbyPlayerListPayload* lpPlayerList =
            reinterpret_cast<const GuiEventNetworkLobbyPlayerListPayload*>(lpEvent);

        // liPlayer is carried OUT of this loop into the hide sweep below (X360 r31), which
        // is why it is declared here and the second loop has no initialiser. The count is a
        // signed compare (`cmpw` / `blt`), and the main loop is bounded by the count alone
        // -- exactly as the console does it (the producer clamps the roster to eight).
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

                // The console fetches the record twice around the assert -- that is the
                // assert macro expanding, not two separate reads -- and dereferences it
                // unguarded either way. Fetch once, assert, use.
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                    mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lrPlayer.mPlayerID);

                CGS_ASSERT_AT(lpPlayerInfo != 0,
                           "Trying to show the name of a player who isn't in our game", CGS_ARTIST_FILE, 1177);  // cpp:1177

                // X360 `addi r5, r3, 0x100` -- the record's name field at console +256.
                mOnlinePlayerList.SetPlayerName(liPlayer, lpPlayerInfo->mPlayerName.macName);
            }

            // Driven every refresh, shown or not: the tick and the car can change under a
            // row that is already up.
            mOnlinePlayerList.SetFinalSelection(liPlayer, lrPlayer.mbFinalSelection);  // lbz row+50
            mOnlinePlayerList.SetPlayerCar(liPlayer, lrPlayer.mSelectedCarID);         // ld  row+0
        }

        // Take down the tail, starting from the first row the roster did not fill. The rows
        // are contiguous, so the sweep stops at the first one that is already hidden rather
        // than running to the end of the bank. Entered even when the roster is empty (the
        // console's `ble` jumps straight in with the index still zero).
        for (; liPlayer < CarSelectOnlinePlayerList::KI_MAX_PLAYERS
               && mOnlinePlayerList.IsShowing(liPlayer); ++liPlayer)
        {
            mOnlinePlayerList.Hide(liPlayer);
        }
    }
}

// FOLDED FROM BrnCarSelectLivery_Components.cpp
// ===================================================================================
// BrnGui::CarSelectLivery  -- partfile 02: the screen build + the component pumps
//   class:BrnGui::CarSelectLivery
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm + pseudocode):
//   SetupComponents         @ 0x824C8240   (DWARF cpp:462)
//   UpdateComponents        @ 0x824C7CB0   (DWARF cpp:275)
//   UpdateMenuComponents    @ 0x824C0AA0   (DWARF cpp:384)
//   SetupPaintColourToggle  @ 0x824C0B88   (DWARF cpp:611)
//   SetupCar                @ 0x824C80A8   (DWARF cpp:402)
//   SendPlayerCarColourEvent@ 0x824C87F0   (DWARF cpp:1097)
//
// ⚠️ EMPTY-GROUP CONTRACT. Without a GUI event 413 producer muNumUnlockedLiveryCars is 0,
// mCarSelector ends up empty, and SelectableGroup::GetHighlighted() has no lower bound on
// miHighlightedIndex -- on an empty group it has already been observed returning the
// NON-NULL garbage 0x0000FF00, which DEFEATS the console's own `!= 0` assert and
// access-violates on the ->GetId() that follows (see the long note in BrnCarSelectVehicle.h).
// Every GetHighlighted() site below therefore tests miHighlightedIndex > -1 first -- the
// group's real emptiness contract, and the console's own idiom.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectLivery.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"          // ButtonIconComponent::EPadButton
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h"        // the SHARED carousel counter, see below
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"                // BrnWorld::PlayerCarColourPalette

#include <cstring>   // std::memset / std::strncpy

namespace BrnGui
{
    namespace
    {
        // KI_CHANNEL_GUI_OUT is declared above in this translation unit.


        // The three paint FINISHES the second toggle row offers, and their option ids
        // (the X360 builds both arrays on the stack: three string pointers and the qwords
        // 0 / 1 / 2).
        const s32 KI_NUM_PAINT_FINISHES = 3;

        // The livery option strings the first toggle row offers, indexed by livery slot.
        // Entries whose VehicleListEntry livery-type byte reads 3 or 4 are overridden to the
        // chrome / gold names.
        const s32 KI_LIVERY_TYPE_CHROME = 3;
        const s32 KI_LIVERY_TYPE_GOLD   = 4;

        // SetupPaintColourToggle's palette cap (X360 `cmplwi 60 / ble`).
        const s32 KI_MAX_PALETTE_COLOURS = 60;

        // The PresentationAction words TriggerSound is called with from this screen
        // (X360 `li r4, 7 / 8 / 9`). FLAG: named for their call sites -- the
        // AttribSys::Enums::PresentationAction enum slice with these three values is not in
        // the recovered set.
        const s32 KI_AUDIO_ACTION_SCROLL = 7;
        const s32 KI_AUDIO_ACTION_BACK   = 8;
        const s32 KI_AUDIO_ACTION_SELECT = 9;

        // ---- out-queue wire records --------------------------------------------------

        // The car-selection trigger SetupComponents and SetupCar both post:
        // { 8, 411, 16, <pad>, CgsID }, channel 40, 24 bytes.
        struct GuiCarSelectTriggerWire411 : public CgsGui::GuiEvent<411>
        {
            CgsID mCarId;     // +0x10
            GuiCarSelectTriggerWire411()
                : mCarId(0) {}
        };

        // The player-car-colour command SendPlayerCarColourEvent posts:
        // { 8, 416, 12, u32 paintFinishIndex, u32 colourIndex }, channel 40, 20 bytes.
        //
        // ⚠️ TWO EXPLICIT WORDS, NOT ONE u64 -- the same trap the id-192 record carries. The
        // X360 writes the pair into one stack qword and stores it with a single `std`
        // (asm 0x824C8824 stores the COLOUR index at var_40+4 and 0x824C884C the PAINT-FINISH
        // index at var_40, then 0x824C8858/0x824C8860 reload and store the qword), so on the
        // big-endian console word0 is the paint-finish index and word1 the colour index.
        // Modelling it as a u64 on a little-endian host would swap them.
        struct GuiPlayerCarColourWire416 : public CgsGui::GuiEvent<416>
        {
            u32 muPaintFinishIndex;   // +0x0C (X360 word0)
            u32 muColourIndex;        // +0x10 (X360 word1)
            GuiPlayerCarColourWire416()
                : muPaintFinishIndex(0), muColourIndex(0) {}
        };

        // The "clear the ticker" command: { 2, 536, 12, u8 1, u8 0 }, channel 40, 16 bytes.
        // Same record BrnCarSelectVehicle_Input.cpp builds; kept TU-local for the same reason
        // (the type's canonical home is mutually exclusive with BrnGuiEventTypeDefs.h).
        struct GuiTickerFlagsWire536 : public CgsGui::GuiEvent<536>
        {
            u8 mbFlagA;   // +0x0C
            u8 mbFlagB;   // +0x0D
            GuiTickerFlagsWire536()
                : mbFlagA(1), mbFlagB(0) {}
        };

        // The 0x818-byte custom ticker message, layout recovered from
        // BrnGui::GuiEventTickerCustomMessage::AddString @0x823A6940 (its asserts bake
        // BrnGuiEventTypeDefs.h:390/391/392). Same TU-local model as the sibling screen's.
        struct GuiTickerCustomMessagePayload
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;
            static const s32 KI_MAX_STRING_LENGTH = 512;

            s32  maiStringTypes[KI_MAX_NUM_STRINGS];                       // +0x000
            char maacStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH];    // +0x010
            s8   mi8NumStrings;                                            // +0x810
            u8   maFlags[4];                                               // +0x811
            u8   maPad815[3];                                              // +0x815

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
                // Two non-zero seeds, both measured at 0x824C7D7C..0x824C7D90:
                //   stb 1 -> +0x811 maFlags[0], stb 1 -> +0x813 maFlags[2].
                // maFlags[0] is what distinguishes the ticker KINDS -- SetTicker seeds
                // {0,0,1,0} where this producer seeds {1,0,1,0} (measured independently
                // in BrnOnlineGameOptions_wI_09.cpp). Dropping it sends the line out as
                // the wrong ticker kind.
                mMessage.maFlags[0] = 1;   // +0x811
                mMessage.maFlags[2] = 1;   // +0x813
            }
        };
    }

    // ================================================================================
    // the one-shot screen build
    // ================================================================================

    // ---- SetupComponents @ 0x824C8240 ----------------------------------------------
    void CarSelectLivery::SetupComponents()
    {
        CGS_ASSERT_AT(mpVehicleList != 0, "mpVehicleList", CGS_ARTIST_FILE, 464);   // cpp:464
        CGS_ASSERT_AT(mpGuiCache != 0, "lpGuiCache", CGS_ARTIST_FILE, 467);         // cpp:467

        // The whole car-modification surface is suppressed when the flow has already shown
        // its car-select transition, or when this car has nothing to modify (one livery and
        // no repaint). In that mode the screen is a pure "press A to continue" confirmation --
        // which is exactly the Junkyard starter-car case.
        if (mpGuiCache->GetCarSelectTransitionAlreadyShown())
        {
            mbDisableCarModifyInteraction = true;
        }

        if (muNumUnlockedLiveryCars <= 1u)
        {
            const s32 liIndex = mpVehicleList->GetVehicleIndex(mCurrentSetupInfo.mCarId);
            const BrnResource::VehicleListEntry* lpEntry =
                (liIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liIndex);
            if (!CanCarBePainted(lpEntry))
            {
                mbDisableCarModifyInteraction = true;
            }
        }

        CarSelectMain::SetupCarNameComponent(mCurrentSetupInfo.mCarId);

        // The eight livery option labels, built on the stack exactly as the console does.
        const char* lapacLiveryOptions[KI_MAX_LIVERIES_PER_MODEL] =
        {
            "$LIVERY_0", "$LIVERY_1", "$LIVERY_2", "$LIVERY_3",
            "$LIVERY_4", "$LIVERY_5", "$LIVERY_6", "$LIVERY_7",
        };

        mCarSelector.SetupTextSelection(static_cast<s32>(muNumUnlockedLiveryCars), true, 0, 0);

        u64 lau64LiveryIds[KI_MAX_LIVERIES_PER_MODEL];
        std::memset(lau64LiveryIds, 0, sizeof(lau64LiveryIds));

        u32 luHighlightSlot = 0;
        for (u32 luLivery = 0; luLivery < muNumUnlockedLiveryCars; ++luLivery)
        {
            const s32 liIndex = mpVehicleList->GetVehicleIndex(maUnlockedLiveryCars[luLivery]);
            const BrnResource::VehicleListEntry* lpVehicleData =
                (liIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liIndex);
            CGS_ASSERT_AT(lpVehicleData != 0, "lpVehicleData", CGS_ARTIST_FILE, 508);   // cpp:508

            lau64LiveryIds[luLivery] = luLivery;

            Selectable* lpSelectable = mCarSelector.GetSelectable(static_cast<s32>(luLivery));
            CGS_ASSERT_AT(lpSelectable != 0, "lpSelectable", CGS_ARTIST_FILE, 514);     // cpp:514

            // ⚠️ ENTRY guard (not in the X360 body -- it dereferences lpVehicleData on the
            // next line). Removed with the stand-in car-id producer; see CanCarBePainted.
            if (lpVehicleData == 0 || lpSelectable == 0)
            {
                continue;
            }

            lpSelectable->SetId(lpVehicleData->GetId());
            lpSelectable->SetDirty();
            lpSelectable->SetActive(true);       // item slot 0
            lpSelectable->SetSelectable(true);   // item slot 2

            mCarSelector.SetItemText(static_cast<s32>(luLivery), lpVehicleData->GetName());

            if (lpVehicleData->GetId() == mCurrentSetupInfo.mCarId)
            {
                luHighlightSlot = luLivery;
            }

            const u8 luLiveryType = lpVehicleData->GetLiveryType();
            if (luLiveryType == KI_LIVERY_TYPE_CHROME)
            {
                lapacLiveryOptions[luLivery] = "$LIVERY_CHROME";
            }
            if (luLiveryType == KI_LIVERY_TYPE_GOLD)
            {
                lapacLiveryOptions[luLivery] = "$LIVERY_GOLD";
            }
        }

        if (!mbDisableCarModifyInteraction)
        {
            mOptionToggles.SetupToggle(E_OPTIONTOGGLE_LIVERY,
                                       static_cast<s32>(muNumUnlockedLiveryCars), true,
                                       KAPC_OPTIONS[E_OPTIONTOGGLE_LIVERY],
                                       lapacLiveryOptions, lau64LiveryIds);
            mOptionToggles.HighlightItem(E_OPTIONTOGGLE_LIVERY,
                                         static_cast<s32>(luHighlightSlot));
        }

        mCarSelector.HighlightId(mCurrentSetupInfo.mCarId);

        // The three paint finishes and their option ids (X360: three stack pointers + the
        // qwords 0 / 1 / 2).
        const char* lapacPaintOptions[KI_NUM_PAINT_FINISHES] =
        {
            "$PAINT_GLOSS", "$PAINT_METALLIC", "$PAINT_PEARLESCENT",
        };
        u64 lau64PaintIds[KI_NUM_PAINT_FINISHES] = { 0u, 1u, 2u };

        if (!mbDisableCarModifyInteraction)
        {
            mOptionToggles.SetupToggle(E_OPTIONTOGGLE_PAINT_FINISH, KI_NUM_PAINT_FINISHES, true,
                                       KAPC_OPTIONS[E_OPTIONTOGGLE_PAINT_FINISH],
                                       lapacPaintOptions, lau64PaintIds);
            mOptionToggles.HighlightItem(E_OPTIONTOGGLE_PAINT_FINISH, 0);
            mOptionToggles.SetHighlightable(true);   // group slot 1
            mOptionToggles.SetHighlighted(true);     // group slot 3
        }

        if (!mbDisableCarModifyInteraction)
        {
            SetupPaintColourToggle(0, false);

            // With a single livery the LIVERY row has nothing to offer, so the highlight
            // starts on the PAINT FINISH row.
            if (muNumUnlockedLiveryCars <= 1u)
            {
                mOptionToggles.HighlightIndex(E_OPTIONTOGGLE_PAINT_FINISH);   // group slot 12
            }
        }

        CGS_ASSERT(mpGuiCache->GetWorldDataController() != 0,
                   "mpWorldDataController");   // BrnGuiCache.h:2324
        mManufacturerLogo.Set(mpGuiCache->GetWorldDataController()->GetVehicleList(),
                              mCurrentSetupInfo.mCarId);

        {
            GuiCarSelectTriggerWire411 lTrigger;
            lTrigger.mCarId = mCurrentSetupInfo.mCarId;
            mpStateInterface->OutputGuiEvent(lTrigger);
        }

        UpdateMenuComponents();
    }

    // ================================================================================
    // per-frame pumps
    // ================================================================================

    // ---- UpdateComponents @ 0x824C7CB0 ---------------------------------------------
    void CarSelectLivery::UpdateComponents()
    {
        if (mbFirstUpdateComponentsFrame)
        {
            mbFirstUpdateComponentsFrame = false;
        }

        if (mbFadeScreenPending)
        {
            mbFadeScreenPending = false;
            mFadeScreenAnimComponent.AddOutputAptViewState("apt_Transition", "fade", false);

            // Clear the ticker, then post the "car needs repair" line.
            {
                GuiTickerFlagsWire536 lTickerClear;
                mpStateInterface->OutputGuiEvent(lTickerClear);
            }
            {
                char lacKey[32];
                CgsCore::SPrintf(lacKey, 31, "%s", "CAR_REPAIR_REQUIRED");
                lacKey[31] = 0;

                GuiTickerCustomMessageWire lTicker;
                lTicker.mMessage.AddString(lacKey, 2);
                mpStateInterface->OutputGuiEvent(lTicker);
            }
        }

        if (!mbDisableCarModifyInteraction)
        {
            mOptionToggles.Update();      // group slot 5
            mColourMenuToggle.Update();   // toggle slot 5
        }

        // The two help items: CONTINUE is offered whenever nothing is loading, BACK only when
        // there is more than one car to go back to.
        if (IsLoading())
        {
            mHelpItemContinue.SetItem("", ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mHelpItemBack.SetItem("", ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }
        else
        {
            mHelpItemContinue.SetItem("$GENERAL_OPTION_CONTINUE",
                                      ButtonIconComponent::E_PADBUTTON_SELECT,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            // ⭐ THE CAROUSEL COUNT IS SHARED WITH THE SIBLING SCREEN. The X360 reads
            // dword_82FB4958 here, and that is the SAME global BrnGui::CarSelectVehicle's
            // HandleCarInfoResponseEvent writes (it is a linker-visible counter, not a
            // file-scope static -- two different TUs read the one address). "Back" is only
            // offered when there is more than one car to go back to.
            if (CarSelectVehicle::gsiNumCarouselCars > 1)
            {
                mHelpItemBack.SetItem("$GENERAL_OPTION_BACK",
                                      ButtonIconComponent::E_PADBUTTON_BACK,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            }
            else
            {
                mHelpItemBack.SetItem("", ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            }
        }

        // ⚠️ EMPTY-GROUP CONTRACT (see the banner). The console asserts on GetHighlighted()
        // and then dereferences it; on an empty selector that assert does NOT fire.
        const SelectableGroup& lrCarSelector = mCarSelector;
        CGS_ASSERT(mCarSelector.miHighlightedIndex > -1 && lrCarSelector.GetHighlighted() != 0,
                   "GetHighlighted()");   // BrnSelectableGroup.h:218
        const BrnResource::VehicleListEntry* lpVehicleData = 0;
        if (mCarSelector.miHighlightedIndex > -1)
        {
            const Selectable* lpHighlighted = lrCarSelector.GetHighlighted();
            const s32 liIndex = mpVehicleList->GetVehicleIndex(lpHighlighted->GetId());
            lpVehicleData = (liIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liIndex);
        }

        // "Restore default paint" is offered only when the car CAN be painted, the modify
        // surface is live, and the current finish/colour pair differs from the car's default.
        const bool lbRestoreAvailable =
            !IsLoading()
            && CanCarBePainted(lpVehicleData)
            && !mbDisableCarModifyInteraction
            && lpVehicleData != 0
            && !(mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)
                     ->mItemText.miHighlightedIndex == lpVehicleData->GetDefaultPaintFinish()
                 && mColourMenuToggle.GetHighlightedColourIndex()
                        == lpVehicleData->GetDefaultPaintColour());

        if (!lbRestoreAvailable)
        {
            mHelpItemRestoreDefaultPaint.SetItem("",
                                                 ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                                 ButtonIconComponent::E_PADBUTTON_INVISIBLE);

            // The help BAR animation follows the number of prompts on screen: zero while
            // loading, two once the screen is live.
            if (IsLoading())
            {
                if (mHelpBarButtonCount != 0)
                {
                    mHelpBarAnimComponent.AddOutputAptViewState("apt_Transition", "zero", false);
                    mHelpBarButtonCount = 0;
                }
            }
            else if (mHelpBarButtonCount != 2)
            {
                mHelpBarAnimComponent.AddOutputAptViewState("apt_Transition", "two", false);
                mHelpBarButtonCount = 2;
            }
        }
        else
        {
            mHelpItemRestoreDefaultPaint.SetItem("$GENERAL_OPTION_RESTORE",
                                                 ButtonIconComponent::E_PADBUTTON_OPTION1,
                                                 ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            if (mHelpBarButtonCount != 3)
            {
                mHelpBarAnimComponent.AddOutputAptViewState("apt_Transition", "three", false);
                mHelpBarButtonCount = 3;
            }
        }
    }

    // ---- UpdateMenuComponents @ 0x824C0AA0 -----------------------------------------
    // Re-enable (or disable) the whole paint surface for whichever livery car is highlighted.
    void CarSelectLivery::UpdateMenuComponents()
    {
        const SelectableGroup& lrCarSelector = mCarSelector;
        CGS_ASSERT(mCarSelector.miHighlightedIndex > -1 && lrCarSelector.GetHighlighted() != 0,
                   "GetHighlighted()");   // BrnSelectableGroup.h:218
        if (mCarSelector.miHighlightedIndex <= -1)
        {
            return;
        }

        const Selectable* lpHighlighted = lrCarSelector.GetHighlighted();
        const s32 liIndex = mpVehicleList->GetVehicleIndex(lpHighlighted->GetId());
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liIndex);

        const bool lbPaintable = CanCarBePainted(lpVehicleData);

        mColourMenuToggle.SetActive(lbPaintable);                                   // toggle slot 0
        mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->SetActive(lbPaintable);  // row slot 0
    }

    // ---- SetupPaintColourToggle @ 0x824C0B88 ---------------------------------------
    // (Re)build the five-picker colour toggle from the palette the CURRENT paint finish
    // selects, then re-seat it on liColourIndex.
    void CarSelectLivery::SetupPaintColourToggle(s32 liColourIndex, bool lbHighlighted)
    {
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 613);   // cpp:613

        const s32 liPaintFinish =
            mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->mItemText.miHighlightedIndex;

        CGS_ASSERT(mpGuiCache->GetWorldDataController() != 0,
                   "mpWorldDataController");   // BrnGuiCache.h:2324

        BrnGui::WorldDataController* lpWorldData = mpGuiCache->GetWorldDataController();

        const BrnWorld::PlayerCarColourPalette* lpPalette =
            lpWorldData->GetColourPaletteFromType(
                static_cast<BrnWorld::EPalettesTypes>(liPaintFinish));

        s32 liNumColours = KI_MAX_PALETTE_COLOURS;
        if (lpPalette->miNumColours <= KI_MAX_PALETTE_COLOURS)
        {
            liNumColours = lpPalette->miNumColours;
        }

        // The three parallel arrays the toggle wants: the top colour, the bottom colour and
        // the option id, one per palette entry.
        const rw::math::vpu::Vector4* lapTopColours[KI_MAX_PALETTE_COLOURS];
        const rw::math::vpu::Vector4* lapBottomColours[KI_MAX_PALETTE_COLOURS];
        u64                           lau64Ids[KI_MAX_PALETTE_COLOURS];

        for (s32 liColour = 0; liColour < liNumColours; ++liColour)
        {
            // The X360 walks both pointer columns by 16 bytes (one Vector4) per entry:
            // `*&v22[i] = *palette + 16*i` / `*&v23[i] = palette[1] + 16*i`. Both columns are
            // 32-bit SERIALISED slots that PlayerCarColoursResourceType::FixUp has rebased;
            // GetPaintColours()/GetPearlColours() are that widening, nothing more.
            lau64Ids[liColour]         = static_cast<u64>(liColour);
            lapTopColours[liColour]    = &lpPalette->GetPaintColours()[liColour];
            lapBottomColours[liColour] = &lpPalette->GetPearlColours()[liColour];
        }

        mColourMenuToggle.Clear();                        // toggle slot 6
        mColourMenuToggle.SetActive(true);                // toggle slot 0
        mColourMenuToggle.SetHighlightable(true);         // toggle slot 1
        mColourMenuToggle.SetHighlighted(lbHighlighted);  // toggle slot 3
        mColourMenuToggle.SetSelectable(true);            // toggle slot 2

        mColourMenuToggle.SetupMenuToggleGradient(liNumColours, true, "$CAR_MOD_PAINT_COLOUR",
                                                  lapTopColours, lapBottomColours, lau64Ids);
        mColourMenuToggle.HighlightIndex(liColourIndex);
        mColourMenuToggle.SetDirty();                     // *(toggle + 0xC) |= 0x10
        mColourMenuToggle.Update();                       // toggle slot 5

        CGS_ASSERT_AT(mpVehicleList != 0, "mpVehicleList", CGS_ARTIST_FILE, 654);   // cpp:654
        const SelectableGroup& lrCarSelector = mCarSelector;
        CGS_ASSERT(mCarSelector.miHighlightedIndex > -1 && lrCarSelector.GetHighlighted() != 0,
                   "GetHighlighted()");                    // BrnSelectableGroup.h:218
        if (mCarSelector.miHighlightedIndex <= -1)
        {
            return;
        }

        const Selectable* lpHighlighted = lrCarSelector.GetHighlighted();
        const s32 liIndex = mpVehicleList->GetVehicleIndex(lpHighlighted->GetId());
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liIndex);

        mColourMenuToggle.SetActive(CanCarBePainted(lpVehicleData));   // toggle slot 0
    }

    // ================================================================================
    // selection
    // ================================================================================

    // ---- SetupCar @ 0x824C80A8 -----------------------------------------------------
    void CarSelectLivery::SetupCar(const CarSetupInfo* lpSetupInfo)
    {
        // cpp:404 -- streamed on the console.
        CGS_ASSERT(lpSetupInfo != 0, "Invalid CarSetupInfo structure");

        mDesiredSetupInfo = *lpSetupInfo;
        // cpp:600 -- the assert's baked file is BrnCarSelectMain.cpp (the base's own copy).
        CGS_ASSERT(mDesiredSetupInfo.mbSelectable, "mDesiredSetupInfo.mbSelectable");

        mbCarChangeInProgress = true;

        CarSelectMain::SetupCarNameComponent(lpSetupInfo->mCarId);

        GuiCarSelectTriggerWire411 lTrigger;
        lTrigger.mCarId = lpSetupInfo->mCarId;
        mpStateInterface->OutputGuiEvent(lTrigger);
    }

    // ---- SendPlayerCarColourEvent @ 0x824C87F0 -------------------------------------
    void CarSelectLivery::SendPlayerCarColourEvent()
    {
        const s32 liColourIndex = mColourMenuToggle.GetHighlightedColourIndex();
        const s32 liPaintFinish =
            mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->mItemText.miHighlightedIndex;

        GuiPlayerCarColourWire416 lColour;
        lColour.muPaintFinishIndex = static_cast<u32>(liPaintFinish);   // X360 word0
        lColour.muColourIndex      = static_cast<u32>(liColourIndex);   // X360 word1
        mpStateInterface->OutputGuiEvent(lColour);

        SetupPaintColourToggle(liColourIndex, mColourMenuToggle.IsHighlighted());
    }
}

// FOLDED FROM BrnCarSelectLivery_Input.cpp
// ===================================================================================
// BrnGui::CarSelectLivery  -- partfile 03: controller input + the game-state events
//   class:BrnGui::CarSelectLivery
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm + pseudocode):
//   HandleControllerInput               @ 0x824D6D10  (DWARF cpp:672)
//   HandleCarInfoResponseEvent          @ 0x824BBA10  (DWARF cpp:927)
//   HandlePlayerInfoResponse            @ 0x824C8758  (DWARF cpp:1002)
//   HandleUnlockedLiveryResponseEvent   @ 0x824BBA80  (DWARF cpp:1031)
//   HandlePlayerCarColourResponseEvent  @ 0x824C0E18  (DWARF cpp:1066)
//
// ⭐⭐ THIS FILE CLOSES THE JUNKYARD HANDOVER. HandleControllerInput's case 0x31 is the
// accept: IsLoading() (state vtable +0x2C) gates it, mbExiting latches it once, and
// ExitCarSelection (state vtable +0x5C, CarSelectMain's) posts the {4,1} activate record,
// the accept event and the "ACCEPT" state event that takes BRNSCREENFSM's
// 34 CS_LIVERY -> 4 INGAME edge.
//
// ⓘ CarSelectMain's dispatcher passes the OBSERVED EVENT ID as the second parameter, and for
// events 5..8 that id is the input event KIND, not a controller port (5 Down / 6 Pressed /
// 7 Released / 8 Axis). This screen only reacts to kind 6 -- PRESSED.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectLivery.h"
#include "GameSource/Input/GameInputActions.h"                       // EGameInputActions (the controller action vocabulary)

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // AddEvent
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h"        // the SHARED carousel counter
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry

namespace BrnGui
{
    namespace
    {
        // KI_CHANNEL_GUI_OUT is declared above in this translation unit.

        // ---- observed input event kinds (CgsGuiEventTypeDefs.h) ---------------------
        const s32 KI_EVENT_CONTROLLER_PRESSED = 6;

        // ---- the seven action ids this screen reacts to -----------------------------
        // EGameInputActions now has a home (GameSource/Input/GameInputActions.h); the four
        // cursor ids stay role-named for what THIS handler does with them.
        const s32 KI_ACTION_TOGGLE_PREV  = 41;   // 0x29 -- step to the previous toggle ROW
        const s32 KI_ACTION_TOGGLE_NEXT  = 42;   // 0x2A -- step to the next toggle ROW / enter it
        const s32 KI_ACTION_OPTION_PREV  = 43;   // 0x2B -- previous option within the row
        const s32 KI_ACTION_OPTION_NEXT  = 44;   // 0x2C -- next option within the row
        const s32 KI_ACTION_ACCEPT       = 49;   // 0x31 -- ⭐ EGameInputActions GUI_SELECT
        const s32 KI_ACTION_BACK         = 50;   // 0x32 -- GUI_CANCEL
        const s32 KI_ACTION_RESTORE      = 52;   // 0x34 -- restore the factory paint

        // + ROOT CAUSE FIXED ELSEWHERE, COMPENSATING ARM DELETED (input-vocabulary wave,
        // 2026-08-29). The local `KI_ACTION_ACCEPT_PC` (45) used to sit here because
        // KA_BINDINGS bound the accept key to 45 GUI_START rather than 49 GUI_SELECT. The
        // console body -- BrnGui::CarSelectLivery::HandleControllerInput @0x824D6D10 --
        // switches on exactly { 41, 42, 43, 44, 49, 50, 52 } and has NO 45 arm, so the extra
        // case was pure divergence. KA_BINDINGS now puts Enter/Space/pad-A on 49.

        // The PresentationAction words TriggerSound is called with (X360 `li r4, 7 / 8 / 9`).
        // KI_AUDIO_ACTION_SCROLL is declared above in this translation unit.
        // KI_AUDIO_ACTION_BACK is declared above in this translation unit.
        // KI_AUDIO_ACTION_SELECT is declared above in this translation unit.

        // ---- in-queue payload views (the queue delivers the header-stripped payload) ----

        // CgsGui::GuiEventControllerInputPressed, minus the header.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // Event 406 (GuiPlayerInfoResponse). This screen reads the "car needs repair" BYTE at
        // +0x38 (X360 `lwz r11, 0x38(r4)`); the base's dispatcher has already taken the car id
        // at +0x20 out of the same record.
        struct GuiPlayerInfoResponsePayload : public CgsModule::Event
        {
            u8  maHead[0x38];        // +0x00 (the car id at +0x20 -- taken by the base)
            u8  mbCarNeedsRepair;    // +0x38 -- BYTE: the X360 uses lbz r11, 0x38(r30)
                                     // @0x824C87B0 and stores it back with stbx. On the
                                     // big-endian console an lbz here would read the HIGH
                                     // byte of an s32, so the producer field really is a
                                     // byte. Modelling it as s32 on the little-endian host
                                     // makes != 0 true for any non-zero byte in +0x38..3B.
        };

        // Event 413 (the unlocked-livery list). The transport is a
        // CgsContainers::Array<s64,8>: elements at +0x00, count at +0x40 (see CgsArrayS64_8.cpp).
        struct GuiUnlockedLiveryPayload : public CgsModule::Event
        {
            CgsID maCarIds[8];   // +0x00
            s32   miCount;       // +0x40 (-1 == the array was never Construct/Clear'ed)
        };

        // Event 414 (the player's current car colour). The X360 reads one word at +0x04 and
        // hands it to SetupPaintColourToggle as the colour index.
        struct GuiPlayerCarColourPayload : public CgsModule::Event
        {
            s32 miPaintFinishIndex;   // +0x00
            s32 miColourIndex;        // +0x04
        };

        // ---- out-queue wire records --------------------------------------------------

        // The player-info REQUEST HandlePlayerInfoResponse posts back:
        // { 8, 410, 16, <pad>, CgsID }, channel 40, 24 bytes.
        struct GuiPlayerCarColourRequestWire410 : public CgsGui::GuiEvent<410>
        {
            CgsID mCarId;    // +0x10
            GuiPlayerCarColourRequestWire410()
                : mCarId(0) {}
        };
    }

    // ================================================================================
    // controller input
    // ================================================================================

    // ---- HandleControllerInput @ 0x824D6D10 ----------------------------------------
    void CarSelectLivery::HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventKind)
    {
        CGS_ASSERT(lpEvent != 0, CGS_ASSERT_MESSAGE_AT("lpEvent", CGS_ARTIST_FILE, 674 /* ARTIST 0x824D6D48 */));   // cpp:674

        CarSelectMain::HandleControllerInput(lpEvent, liEventKind);

        // Only PRESSED reaches this screen's own ladder.
        if (liEventKind != KI_EVENT_CONTROLLER_PRESSED)
        {
            return;
        }

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_TOGGLE_PREV:
        {
            if (mbDisableCarModifyInteraction)
                break;

            if (mColourMenuToggle.IsHighlighted())
            {
                // Leave the colour picker: hand the highlight back to the toggle rows, with
                // the PAINT FINISH row selected.
                mColourMenuToggle.SetHighlighted(false);                                       // toggle slot 3
                mOptionToggles.SetHighlighted(true);                                           // group slot 3
                mOptionToggles.GetSelectable(E_OPTIONTOGGLE_LIVERY)->SetHighlighted(false);    // row slot 3
                mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->SetHighlighted(true);
                SendPlayerCarColourEvent();
                TriggerSound(KI_AUDIO_ACTION_BACK);
            }
            else if (muNumUnlockedLiveryCars > 1u)
            {
                // Step up through the toggle rows (only meaningful when the livery row has
                // something to offer).
                if (mOptionToggles.HighlightPrevious(false))   // group slot 11
                {
                    TriggerSound(KI_AUDIO_ACTION_BACK);
                }
            }
            break;
        }

        case KI_ACTION_TOGGLE_NEXT:
        {
            if (mbDisableCarModifyInteraction || IsLoading() || !mOptionToggles.IsHighlighted())
                break;

            if (mOptionToggles.GetSelectable(E_OPTIONTOGGLE_LIVERY)->IsHighlighted())
            {
                // On the LIVERY row: step down onto the PAINT FINISH row, but only when that
                // row is live and this car can actually be painted.
                MenuToggle* lpPaintRow = mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH);
                if (lpPaintRow->IsActive())
                {
                    const CgsID lHighlightedId = mCarSelector.GetHighlightedId();
                    const BrnResource::VehicleListEntry* lpVehicleData =
                        mpVehicleList->GetVehicleData(lHighlightedId);
                    if (CanCarBePainted(lpVehicleData))
                    {
                        mOptionToggles.HighlightNext(false);   // group slot 10
                        TriggerSound(KI_AUDIO_ACTION_SELECT);
                    }
                }
            }
            else
            {
                // On the PAINT FINISH row: drop INTO the colour picker.
                mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->SetHighlighted(false);
                mOptionToggles.SetHighlighted(false);          // group slot 3
                mColourMenuToggle.SetHighlighted(true);        // toggle slot 3
                SendPlayerCarColourEvent();
                TriggerSound(KI_AUDIO_ACTION_SELECT);
            }
            break;
        }

        case KI_ACTION_OPTION_PREV:
        {
            if (mbDisableCarModifyInteraction)
                break;

            if (!mOptionToggles.IsHighlighted())
            {
                // The colour picker owns the cursor.
                if (mColourMenuToggle.HighlightPrevious())
                {
                    TriggerSound(KI_AUDIO_ACTION_SCROLL);
                    SendPlayerCarColourEvent();
                }
                break;
            }

            mOptionToggles.HighlightPreviousItem();   // group slot 14 (+0x38)

            if (mOptionToggles.miHighlightedIndex == E_OPTIONTOGGLE_PAINT_FINISH)
            {
                // A paint-finish change only needs the colour re-published.
                SendPlayerCarColourEvent();
                TriggerSound(KI_AUDIO_ACTION_SCROLL);
            }
            else if (mOptionToggles.miHighlightedIndex == E_OPTIONTOGGLE_LIVERY)
            {
                // A livery change is a CAR change: step the selector and commit it.
                mCarSelector.HighlightPrevious(false);   // selector slot 11

                CarSetupInfo lSetupInfo = mCurrentSetupInfo;
                lSetupInfo.mCarId       = mCarSelector.GetHighlightedId();
                lSetupInfo.mbSelectable = true;
                SetupCar(&lSetupInfo);                   // state vtable +0x58

                UpdateMenuComponents();
                TriggerSound(KI_AUDIO_ACTION_SCROLL);
            }
            break;
        }

        case KI_ACTION_OPTION_NEXT:
        {
            if (mbDisableCarModifyInteraction)
                break;

            if (!mOptionToggles.IsHighlighted())
            {
                if (mColourMenuToggle.HighlightNext())
                {
                    TriggerSound(KI_AUDIO_ACTION_SCROLL);
                    SendPlayerCarColourEvent();
                }
                break;
            }

            mOptionToggles.HighlightNextItem();   // group slot 13 (+0x34)

            if (mOptionToggles.miHighlightedIndex == E_OPTIONTOGGLE_PAINT_FINISH)
            {
                SendPlayerCarColourEvent();
                TriggerSound(KI_AUDIO_ACTION_SCROLL);
            }
            else if (mOptionToggles.miHighlightedIndex == E_OPTIONTOGGLE_LIVERY)
            {
                mCarSelector.HighlightNext(false);       // selector slot 10

                CarSetupInfo lSetupInfo = mCurrentSetupInfo;
                lSetupInfo.mCarId       = mCarSelector.GetHighlightedId();
                lSetupInfo.mbSelectable = true;
                SetupCar(&lSetupInfo);

                UpdateMenuComponents();
                TriggerSound(KI_AUDIO_ACTION_SCROLL);
            }
            break;
        }

        case KI_ACTION_ACCEPT:
        {
            // ⭐⭐ THE VALIDATE PRESS -- the last user action of the Junkyard flow.
            // mbExiting latches it so a second press cannot re-enter ExitCarSelection, and
            // IsLoading() (this class's own vtable slot +0x2C) swallows it while a car change
            // is still in flight. ExitCarSelection is CarSelectMain's, reached through vtable
            // slot +0x5C: it posts GuiEventActivateCarSelect {4,1} (id 192) and
            // GuiEventCarSelectAccept (id 536), then sends the "ACCEPT" state event.
            if (!mbExiting && !IsLoading())
            {
                mbExiting = true;
                ExitCarSelection();
            }
            break;
        }

        case KI_ACTION_BACK:
        {
            // Back is only offered when there is more than one car to go back to (the same
            // shared carousel counter UpdateComponents gates the prompt on).
            if (!mbExiting && CarSelectVehicle::gsiNumCarouselCars > 1 && !IsLoading())
            {
                mbExiting = true;
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "RG :: CSL : SendStateEvent( \"GO_BACK\" )\n";
                }
                SendStateEvent("GO_BACK");
            }
            break;
        }

        case KI_ACTION_RESTORE:
        {
            // Put the factory paint back: re-seat whichever of the two toggles differs from
            // the car's default pair, then re-publish.
            if (IsLoading() || mbDisableCarModifyInteraction)
                break;

            const CgsID lHighlightedId = mCarSelector.GetHighlightedId();
            const BrnResource::VehicleListEntry* lpVehicleData =
                mpVehicleList->GetVehicleData(lHighlightedId);
            if (!CanCarBePainted(lpVehicleData))
                break;

            bool lbChanged = false;

            if (mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->mItemText.miHighlightedIndex
                != lpVehicleData->GetDefaultPaintFinish())
            {
                lbChanged = true;
                mOptionToggles.HighlightItem(E_OPTIONTOGGLE_PAINT_FINISH,
                                             lpVehicleData->GetDefaultPaintFinish());
            }

            if (mColourMenuToggle.GetHighlightedColourIndex()
                != lpVehicleData->GetDefaultPaintColour())
            {
                lbChanged = true;
                mColourMenuToggle.HighlightIndex(lpVehicleData->GetDefaultPaintColour());
            }

            if (lbChanged)
            {
                SendPlayerCarColourEvent();
            }
            break;
        }

        default:
            break;
        }
    }

    // ================================================================================
    // game-state events
    // ================================================================================

    // ---- HandleCarInfoResponseEvent @ 0x824BBA10 -----------------------------------
    // Event 412. This screen overrides the handler purely to keep the base's two asserts and
    // then do NOTHING with the car list -- the livery screen's list arrives on 413 instead.
    // The X360 body is exactly the two asserts and a return.
    void CarSelectLivery::HandleCarInfoResponseEvent(const CgsModule::Event* lpEvent,
                                                     s32 /*liEventType*/)
    {
        CGS_ASSERT_AT(lpEvent != 0, "lpEvent", CGS_ARTIST_FILE, 929);   // cpp:929
        // The second assert's baked file is BrnCarSelectMain.cpp:582 (a copy-paste in the
        // original); it tests the same pointer.
        CGS_ASSERT(lpEvent != 0, "lpEvent");
    }

    // ---- HandlePlayerInfoResponse @ 0x824C8758 -------------------------------------
    // Event 406. Ask the game state for this car's colour (id 410), then latch the
    // "car needs repair" word: it BOTH disables the whole modify surface and arms the
    // fade-screen + "CAR_REPAIR_REQUIRED" ticker UpdateComponents posts.
    void CarSelectLivery::HandlePlayerInfoResponse(const CgsModule::Event* lpEvent,
                                                   s32 /*liEventType*/)
    {
        {
            GuiPlayerCarColourRequestWire410 lRequest;
            lRequest.mCarId = mCurrentSetupInfo.mCarId;
            mpStateInterface->OutputGuiEvent(lRequest);
        }

        const GuiPlayerInfoResponsePayload* lpPayload =
            reinterpret_cast<const GuiPlayerInfoResponsePayload*>(lpEvent);

        mbDisableCarModifyInteraction = (lpPayload->mbCarNeedsRepair != 0);
        if (mbDisableCarModifyInteraction)
        {
            mbFadeScreenPending = true;
        }
    }

    // ---- HandleUnlockedLiveryResponseEvent @ 0x824BBA80 ----------------------------
    // Event 413 -- the ONLY producer of this screen's livery list, and therefore the only
    // thing that makes mCarSelector non-empty. See the empty-group contract in
    // BrnCarSelectLivery_Components.cpp.
    void CarSelectLivery::HandleUnlockedLiveryResponseEvent(const CgsModule::Event* lpEvent,
                                                            s32 liEventType)
    {
        CGS_ASSERT_AT(lpEvent != 0, "lpEvent", CGS_ARTIST_FILE, 1033);   // cpp:1033

        if (liEventType != 413)
        {
            return;
        }

        const GuiUnlockedLiveryPayload* lpPayload =
            reinterpret_cast<const GuiUnlockedLiveryPayload*>(lpEvent);

        // The two inlined CgsContainers::Array<s64,8> guards.
        CGS_ASSERT(lpPayload->miCount != -1,
                   "Array used before Construct/Clear was called");   // CgsArray.h:336
        CGS_ASSERT(lpPayload->miCount < static_cast<s32>(KI_MAX_LIVERIES_PER_MODEL),
                   "Too many cars for the selection");                // cpp:1042 (streamed)

        muNumUnlockedLiveryCars = 0u;
        for (s32 liCar = 0; liCar < lpPayload->miCount; ++liCar)
        {
            CGS_ASSERT(lpPayload->miCount != -1,
                       "Array used before Construct/Clear was called");   // CgsArray.h:336
            maUnlockedLiveryCars[muNumUnlockedLiveryCars] = lpPayload->maCarIds[liCar];
            muNumUnlockedLiveryCars = muNumUnlockedLiveryCars + 1u;
        }
    }

    // ---- HandlePlayerCarColourResponseEvent @ 0x824C0E18 ---------------------------
    // Event 414. The game state reports the car's live colour; re-seat the paint-finish row
    // and the picker on it (unless the modify surface is off, in which case there is nothing
    // to re-seat).
    void CarSelectLivery::HandlePlayerCarColourResponseEvent(const CgsModule::Event* lpEvent,
                                                             s32 liEventType)
    {
        CGS_ASSERT(lpEvent != 0,
            CGS_ASSERT_MESSAGE_AT(
                "lpEvent",
                CGS_ARTIST_FILE, 1068 /* ARTIST 0x824C0E40 */));   // cpp:1068

        if (liEventType != 414 || mbDisableCarModifyInteraction)
        {
            return;
        }

        const GuiPlayerCarColourPayload* lpPayload =
            reinterpret_cast<const GuiPlayerCarColourPayload*>(lpEvent);

        // Both index arguments ARE explicitly loaded from the payload by the X360, measured
        // at 0x824C0E74..0x824C0E8C:  li r4,1 / lwz r5,0(r30) -> HighlightItem(liItem =
        // payload+0x00), then addi r3,r31,0x2860 / lwz r4,4(r30) -> HighlightIndex(liIndex =
        // payload+0x04). (An earlier comment here claimed both were left unset -- it was
        // wrong, and the code below was already right.)
        mOptionToggles.HighlightItem(E_OPTIONTOGGLE_PAINT_FINISH, lpPayload->miPaintFinishIndex);
        mColourMenuToggle.HighlightIndex(lpPayload->miColourIndex);
        SetupPaintColourToggle(lpPayload->miColourIndex, mColourMenuToggle.IsHighlighted());
    }

}

#undef CGS_ARTIST_FILE
