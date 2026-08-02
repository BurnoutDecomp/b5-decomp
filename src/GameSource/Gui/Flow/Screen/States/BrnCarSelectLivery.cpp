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

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
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

namespace BrnGui
{
    namespace
    {
        // The AddEvent channel every out-queue record in this class uses (GuiEventOut).
        const s32 KI_CHANNEL_GUI_OUT = 40;
        // PlayAptMovie's channel (the same one CgsGui::StateInterface::PlayAptMovie posts on).
        const s32 KI_CHANNEL_GUI_APT = 41;
        const char KAC_EMPTY[] = "";

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
        struct GuiScreenLeaveWire80 : public CgsGui::GuiEvent<80>
        {
            u8 muPayload;   // +0x0C
            GuiScreenLeaveWire80() : CgsGui::GuiEvent<80>(1, 12), muPayload(0) {}
        };

        // The play-apt-movie record PlayMovie posts: { 8, 18, 12, <name>, 3 }, channel 41,
        // 20 bytes -- exactly what StateInterface::PlayAptMovie builds.
        // (Posted through PlayAptMovie below rather than open-coded.)

        // Update's one-shot activate-car-select record: { 8, 192, 12, u32 action,
        // u32 carSelectType }, channel 40, 20 bytes.
        //
        // ⚠️ TWO EXPLICIT WORDS, NOT ONE u64. The X360 builds the pair with
        // `LODWORD = meCarSelectType; HIDWORD = 1; std` -- one big-endian 64-bit store of a
        // two-word record, so word0 == 1 and word1 == the type. Modelling it as a single u64
        // on a little-endian host SWAPS THE HALVES; that exact bug was found in this screen's
        // sibling (BrnCarSelectVehicle) on 2026-08-02 and it emitted the wrong action with a
        // type the game-state dispatcher asserts on.
        // The consumer is GameStateModule::ProcessGameEvents @0x823A0A18 case 94:
        // *_R25 == the ACTION (0 start / 1 enter-modification / 4 exit) and _R25[1] == the
        // car-select TYPE (1 junkyard / 2 online). This screen's action is 1 -- entering the
        // livery screen IS entering car modification.
        struct GuiActivateCarSelectWire : public CgsGui::GuiEvent<192>
        {
            u32 muAction;           // +0x0C (X360 word0 == 1, enter modification)
            u32 muCarSelectType;    // +0x10 (X360 word1 == GsmIO::ECarSelectType)
            GuiActivateCarSelectWire()
                : CgsGui::GuiEvent<192>(8, 12)
                , muAction(0)
                , muCarSelectType(0) {}
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

    // @0x82F26C50 -- six APT resources. All six bundles are present in build\game\GUIAPT\.
    const CgsGui::sResourceTuple CarSelectLivery::maResourcesToLoad[6] =
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
        CGS_ASSERT(lpFsm != 0, "Invalid ScriptedFsm ptr");

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
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:169

        {
            GuiScreenLeaveWire80 lLeave;
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lLeave, KI_CHANNEL_GUI_OUT,
                                                              static_cast<s32>(sizeof(lLeave)));
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
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:239

            GuiActivateCarSelectWire lCarSelectType;
            lCarSelectType.muAction        = 1;   // X360 HIDWORD == 1 (enter modification)
            lCarSelectType.muCarSelectType =
                static_cast<u32>(mpGuiCache->GetCurrentCarSelectType());
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lCarSelectType, KI_CHANNEL_GUI_OUT,
                                                              static_cast<s32>(sizeof(lCarSelectType)));

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

        CGS_ASSERT(mpGuiCache != 0, "lpGuiCache");   // cpp:447

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
        CGS_ASSERT(lpVehicleListEntry != 0, "lpVehicleListEntryGamePlayData");   // cpp:1225
        CGS_ASSERT(mpGuiCache != 0, "lpGameParams");                             // cpp:1228

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
        lAudio.Construct(leAction, KAC_EMPTY, "CodeLivery", KAC_EMPTY);

        lAudio.muHeader0   = 100u;
        lAudio.muEventType = KU_AUDIO_TRIGGER_EVENT_ID;
        lAudio.muHeader2   = 12u;

        mpStateInterface->GetOutputEventQueue()->AddEvent(&lAudio, KI_CHANNEL_GUI_OUT,
                                                          static_cast<s32>(sizeof(lAudio)));
    }
}
