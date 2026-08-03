#pragma once

// ===================================================================================
// BrnGui::CarSelectLivery  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCarSelectLivery.h
//
// The car-select "livery" screen flow state -- the LAST screen of the Junkyard flow, under
// the FSM id CS_LIVERY. It derives from BrnGui::CarSelectMain (so BrnCarSelectVehicle.* is a
// direct sibling template) and adds the car-modification surface: a TextSelection of the
// selected car's unlocked liveries, a two-row MenuToggleGroup (livery / paint finish), a
// five-picker ColourMenuToggle, the "restore default paint" help item, two animation
// components and the two online-only widgets.
//
// 2026-08-02 -- RECONSTRUCTED. Until this wave the class was a THREE-METHOD SHELL in
// BrnScreenStatesLinkStubs (`struct CarSelectLivery : public CgsGui::State` with OnEnter /
// OnLeave / Update and no members), i.e. the seventh instance of this campaign's worst defect
// class: every FSM call resolved to a base default that did nothing, and the screen looked
// alive. All 23 X360 bodies are here now, and the base is BrnGui::CarSelectMain -- proven by
// the ctor's tail (it constructs the same sub-object chain CarSelectMain does) and by
// Construct / OnEnter / OnLeave / Update / AppendAptComponents / SetupCar each opening with a
// `bl BrnGui__CarSelectMain__<same name>`.
//
// ⭐ THIS IS THE ACCEPT PATH OF THE JUNKYARD HANDOVER.
// HandleControllerInput @0x824D6D10 case 0x31 (49, GUI_SELECT) -> IsLoading()? no ->
// mbExiting = 1 -> vtable +0x5C == CarSelectMain::ExitCarSelection, which posts
// GuiEventActivateCarSelect {4,1} (id 192) + GuiEventCarSelectAccept (id 536) and sends the
// "ACCEPT" state event that takes BRNSCREENFSM's 34 CS_LIVERY -> 4 INGAME edge. The game-state
// half of that chain landed in b5-decomp 6e1de0bb.
//
// MEMBER OFFSETS (X360; documentary only -- the x64 gate widens every embedded pointer, so
// every access below is BY NAME):
//   +2288  MenuToggleGroupVarSize<2> mOptionToggles       ("optionToggle")
//   +10336 ColourMenuToggle          mColourMenuToggle    ("colourOptionToggle")
//          Vector4 mDefaultCarColour  (DWARF-declared; no recovered body reads or writes it)
//   +35424 HelpItem                  mHelpItemRestoreDefaultPaint  ("HelpItemRestore_mc")
//   +35852 CarSelectOnlineCountdown  mOnlineCountdown     ("Countdown_mc")
//   +36296 CarSelectOnlinePlayerList mOnlinePlayerList    ("PlayerTable_mc")
//   +42464 AnimationComponent        mFadeScreenAnimComponent ("FadeAnim_mcp")
//   +42604 bool                      mbFadeScreenPending
//   +42608 AnimationComponent        mHelpBarAnimComponent    ("HelpBarAnim_mcp")
//   +42748 s32                       mHelpBarButtonCount  (-1 / 0 / 2 / 3)
//   +42752 CgsID                     maUnlockedLiveryCars[8]
//   +42816 u32                       muNumUnlockedLiveryCars
//   +42824 TextSelection             mCarSelector         ("CarName")
//   +46088 bool mbFirstFrame   +46089 mbFirstUpdateComponentsFrame
//   +46090 bool mbDisableCarModifyInteraction   +46091 bool mbExiting
// sizeof == 46096.
//
// ⭐ THE TWO "$CAR_MOD_*" STRINGS ARE NOT THE COMPONENT NAMES. The handed-down spec named
// KAC_OPTION_NAME "$CAR_MOD_LIVERY" and KAC_PAINT_COLOUR_OPTION_NAME "$CAR_MOD_PAINT_FINISH".
// The DWARF sizes them char[13] and char[19], which those two strings do not fit -- but
// "optionToggle" and "colourOptionToggle" do, exactly, and they are the names OnEnter passes
// to MenuToggleGroupVarSize<2>::Construct / ColourMenuToggle::Construct. The two "$CAR_MOD_*"
// literals are the adjacent pointer pair at off_82F26C84/off_82F26C88 -- i.e. the DWARF's
// `const char* KAPC_OPTIONS[2]` (cpp:69), the two toggle TITLES SetupComponents passes.
//
// ⚠️ WHAT THIS SCREEN NEEDS FROM ELSEWHERE (both real, both live):
//   * A GUI EVENT 413 PRODUCER, or the livery carousel is EMPTY. muNumUnlockedLiveryCars is
//     written only by HandleUnlockedLiveryResponseEvent. With 0 items SetupComponents calls
//     SetupTextSelection(0, ...) and UpdateComponents / UpdateMenuComponents then call
//     SelectableGroup::GetHighlighted(), which has no lower bound on miHighlightedIndex and
//     has already been observed returning the NON-NULL garbage 0x0000FF00 on an empty group --
//     defeating the console's own `!= 0` assert and access-violating downstream. Every such
//     site in this class therefore tests mCarSelector.miHighlightedIndex > -1 first, the same
//     emptiness contract BrnCarSelectVehicle.h documents.
//   * CarSelectManager::UpdateExitStreamingBringUp is a FLAGGED STAND-IN for
//     GameStateModule::ProcessStreamingCompleteEvent @0x82390200 -- the only caller of
//     StreamingFinished, hence the only thing that clears the mbWaitingForStreaming latch
//     ExitJunkyard sets. Retire it when the real one lands or UpdateExitState returns early
//     for ever.
// ===================================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"                                             // rw::math::vpu::Vector4
#include "GameShared/GameClasses/Core/CgsID.h"                             // CgsID
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectMain.h"            // BrnGui::CarSelectMain (base) + CarSetupInfo
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelection.h"        // BrnGui::TextSelection (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"      // BrnGui::MenuToggleGroupVarSize<2> (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnColourMenuToggle.h"     // BrnGui::ColourMenuToggle (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"             // BrnGui::HelpItem (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"   // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlineCountdown.h"  // by value
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerList.h" // by value

namespace CgsModule   { struct Event; }
namespace CgsFsm      { class  ScriptedFsm; }
namespace CgsGui      { struct GuiEventAptTriggerPayload; }
namespace BrnResource { struct VehicleListEntry; }

namespace BrnGui
{
    // Event 244's payload (the online lobby player table). Kept incomplete here, but the
    // record IS recovered (wave J): the X360 posts it RAW -- no GuiEvent header --
    // AddGuiEvent<GuiEventNetworkLobbyPlayerList> @0x823CF4C8 calls AddEvent(queue, record,
    // 244, 456), and the 456 bytes decompose as
    // BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData maPlayers[8] (the committed
    // 56-byte rows) then s32 miNumPlayers at +448 (+4 tail pad). Consumers view it through
    // a TU-local payload struct per the wave-H precedent
    // (BrnOnlineGameRoomPlayerInfo_wH_15.cpp); declared here as well as in
    // BrnCarSelectVehicle.h -- same incomplete type, one namespace.
    struct GuiEventNetworkLobbyPlayerList;

    struct CarSelectLivery : public CarSelectMain
    {
        // BrnCarSelectLivery.h:109 (DWARF) / cpp:75 -- the livery carousel's capacity, and the
        // bound HandleUnlockedLiveryResponseEvent checks the event-413 array against.
        static const u32 KI_MAX_LIVERIES_PER_MODEL = 8;

        // The two toggle rows the screen drives, in MenuToggleGroupVarSize<2> index order.
        enum EOptionToggle
        {
            E_OPTIONTOGGLE_LIVERY       = 0,
            E_OPTIONTOGGLE_PAINT_FINISH = 1,
            E_OPTIONTOGGLE_COUNT        = 2,
        };

        // @ 0x82508868 -- the compiler-emitted ctor (vtable installs only).
        CarSelectLivery();

        // @ 0x824BEB40 (DWARF cpp:105) -- assert the FSM pointer, then base Construct.
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);
        // @ 0x824D6960 (DWARF cpp:120) -- register the 11 observed events and Construct every
        // embedded component.
        virtual void OnEnter();
        // @ 0x824D6C30 (DWARF cpp:167) -- post the screen's leave command (id 80), re-arm the
        // GuiCache transition gate, clear both toggles, base OnLeave, unregister.
        virtual void OnLeave();
        // @ 0x824DFCD0 (DWARF cpp:191) -- drain the three self-handled events, run the base
        // ladder, post the one-shot activate-car-select record, then the component pump and
        // the auto-accept arm.
        virtual void Update();

    protected:
        // @ 0x824BB970 (DWARF cpp:442) -- append this screen's expected apt components.
        virtual void AppendAptComponents();
        // @ 0x824C8240 (DWARF cpp:462) -- the one-shot screen build.
        virtual void SetupComponents();

    private:
        // @ 0x824C81D0 (DWARF cpp:426) -- PlayAptMovie("BrnCarSelectLivery", 3).
        virtual void PlayMovie();
        // @ 0x824D6D10 (DWARF cpp:672) -- ⭐ THE ACCEPT PATH. See the banner.
        virtual void HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventKind);
        // @ 0x824BBA10 (DWARF cpp:927) -- event 412: assert-only on this screen.
        virtual void HandleCarInfoResponseEvent(const CgsModule::Event* lpEvent, s32 liEventType);
        // @ 0x824C8758 (DWARF cpp:1002) -- event 406: request the player's car colour (id 410)
        // and latch the "car needs repair" gate.
        virtual void HandlePlayerInfoResponse(const CgsModule::Event* lpEvent, s32 liEventType);
        // @ 0x824BBA80 (DWARF cpp:1031) -- event 413: adopt the unlocked-livery car list.
        virtual void HandleUnlockedLiveryResponseEvent(const CgsModule::Event* lpEvent, s32 liEventType);
        // @ 0x824C0E18 (DWARF cpp:1066) -- event 414: re-seat the paint toggles on the
        // colour the game state reports.
        virtual void HandlePlayerCarColourResponseEvent(const CgsModule::Event* lpEvent, s32 liEventType);
        // @ 0x824C80A8 (DWARF cpp:402) -- commit the desired car and post the selection
        // trigger (id 411).
        virtual void SetupCar(const CarSetupInfo* lpSetupInfo);
        // @ 0x824BBBF0 (DWARF cpp:1221) -- a car change is still in flight, or the drop-in id
        // the game state last reported has not caught up with the committed one.
        virtual bool IsLoading() const;
        // @ 0x824B5180 / DWARF cpp:987.
        virtual CgsGui::sResourceTuple* GetResourcesToLoad() const;
        virtual u32 GetNumberResourcesToLoad() const;

        // @ 0x824C7CB0 (DWARF cpp:275) -- the per-frame component pump.
        void UpdateComponents();
        // @ 0x824C0AA0 (DWARF cpp:384) -- re-enable / disable the paint surface for the
        // currently highlighted livery car.
        void UpdateMenuComponents();
        // @ 0x824C0B88 (DWARF cpp:611) -- (re)build the colour picker from the palette the
        // current paint finish selects.
        void SetupPaintColourToggle(s32 liColourIndex, bool lbHighlighted);
        // @ 0x824B52E0 (DWARF cpp:1225 asserts) -- can this car be repainted at all?
        bool CanCarBePainted(const BrnResource::VehicleListEntry* lpVehicleListEntry) const;
        // @ 0x824C87F0 (DWARF cpp:1097) -- post the player-car-colour command (id 416) and
        // re-seat the colour picker.
        void SendPlayerCarColourEvent();
        // @ 0x824C8898 (DWARF cpp:1203) -- post the screen's audio trigger (id 457).
        void TriggerSound(s32 leAction);
        // @ 0x824B5190 (DWARF cpp:1155) -- event 244, ONLINE-ONLY lobby player table.
        void HandleLobbyPlayerList(const GuiEventNetworkLobbyPlayerList* lpEvent);

        // ---- statics (DWARF cpp:33..cpp:87; definitions in BrnCarSelectLivery.cpp) --------
        // @0x82F26C50 -- the six APT resources this screen loads (read out of the image):
        // 150 BrnCarSelectLivery, 34 B5MenuToggle, 35 B5MenuItemColourPicker, 30 ColourField,
        // 94 B5OnlineCarSelectComponents, 55 B5ManufacturersIcon.
        static const CgsGui::sResourceTuple maResourcesToLoad[6];
        static const u32 muNumResourcesToLoad;
        // @0x82065DC0 -- { 6, 7, 8, 21, 64, 412, 406, 413, 414, 82, 244 }.
        static const s32 maiEventToObserve[11];
        static const s32 miNumEventsObserved;
        // @0x82F26C80 -> "CarName" (the livery selector's apt component name).
        static const char* mpacCarSelectorName;
        // @0x82F26C84 (two adjacent pointers) -- the two toggle TITLES.
        static const char* const KAPC_OPTIONS[E_OPTIONTOGGLE_COUNT];

        static const char KAC_OPTION_NAME[13];               // "optionToggle"
        static const char KAC_PAINT_COLOUR_OPTION_NAME[19];  // "colourOptionToggle"
        static const char KAC_HELPITEM_RESTORE[19];          // "HelpItemRestore_mc"
        static const char KAC_ONLINE_COUNTDOWN_NAME[13];     // "Countdown_mc"
        static const char KAC_ONLINE_PLAYER_LIST[15];        // "PlayerTable_mc"
        static const char macFadeScreenAnimComponentName[13];  // "FadeAnim_mcp"
        static const char macHelpBarAnimComponentName[16];     // "HelpBarAnim_mcp"

        // ---- members over the CarSelectMain base (X360 offsets documentary) -------------
        MenuToggleGroupVarSize<E_OPTIONTOGGLE_COUNT> mOptionToggles;  // +2288
        ColourMenuToggle          mColourMenuToggle;                  // +10336
        // DWARF BrnCarSelectLivery.h:104. No recovered body reads or writes it; declared
        // because the DWARF carries it in the member run between the toggle and the help item.
        rw::math::vpu::Vector4    mDefaultCarColour;
        HelpItem                  mHelpItemRestoreDefaultPaint;       // +35424
        CarSelectOnlineCountdown  mOnlineCountdown;                   // +35852
        CarSelectOnlinePlayerList mOnlinePlayerList;                  // +36296
        AnimationComponent        mFadeScreenAnimComponent;           // +42464
        bool                      mbFadeScreenPending;                // +42604
        AnimationComponent        mHelpBarAnimComponent;              // +42608
        s32                       mHelpBarButtonCount;                // +42748
        CgsID                     maUnlockedLiveryCars[KI_MAX_LIVERIES_PER_MODEL];  // +42752
        u32                       muNumUnlockedLiveryCars;            // +42816
        TextSelection             mCarSelector;                       // +42824
        bool                      mbFirstFrame;                       // +46088
        bool                      mbFirstUpdateComponentsFrame;       // +46089
        bool                      mbDisableCarModifyInteraction;      // +46090
        bool                      mbExiting;                          // +46091
    };
}
