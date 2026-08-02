#pragma once

// ===================================================================================
// BrnGui::CarSelectVehicle  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h
//
// The car-select "vehicle" screen flow state -- the Junkyard car carousel. Constructed by
// BrnGui::BrnScreenFlow::Prepare under the FSM id CS_VEHICLE. It derives from
// BrnGui::CarSelectMain and adds the carousel: a BrnGui::TextSelection of the selectable
// car names, five BrnGui::RivalTableCell overview icons in a scrolling strip, three
// BrnGui::PlayerStatsBar gauges, the car-type / cars-unlocked labels, the slider bar, two
// animation components and the two online-only widgets.
//
// 2026-08-02 -- THIS CLASS WAS A HOLLOW SHELL AND IS NOW RE-HOMED.
// The previous revision declared `struct CarSelectVehicle : public CgsGui::State` with ONE
// member (a TextSelection), NO virtuals and NO methods, and a note claiming the sibling
// sub-objects were "not individually recoverable from this single ctor". Both claims were
// wrong: the base is BrnGui::CarSelectMain (the X360 ctor tail-calls CarSelectMain's ctor
// chain and CarSelectVehicle::Construct/OnEnter/OnLeave/Update all open with
// `bl BrnGui__CarSelectMain__<same>`), and the DecFIGS DWARF for this exact header carries
// all 25 members plus the full method set. Twenty-four X360 bodies were hidden behind the
// shell. This is the sixth instance of the "a class that declares none of its virtuals
// looks perfectly alive" defect class.
//
// LAYOUT PROOF. The X360 offsets in the comments come from the asm (they are documentary:
// the x64 gate widens every embedded pointer, so every access below is BY NAME).
// The chain is closed end to end by the sub-object sizes:
//   mCarSelector @+0x8F0 + sizeof(TextSelection)==0xCC0            -> +0x15B0
//   maSelectedCars @+0x15B0, 0x400 bytes (128 * 8)                 -> +0x19B0
//   the two BitArrays @+0x19B0/+0x19C0 (16 bytes each: 128 bits)   -> +0x19D0
//   ... mTrackAnimTransitionComponent @+0x2638 + 0x8C              -> +0x26C4 (mMainAnimComponent)
//   mOnlinePlayerList @+0x2908 .. mbFirstFrame @+0x4120            -> sizeof == 0x1818
//
// ⭐ maSelectedCars IS 128 ENTRIES, NOT THE DWARF'S 92. Settled this wave, three ways --
// the DecFIGS DWARF (a PS3 INTERNAL build) says `CgsID[92]` / `BitArray<92u>` and
// CarSelectMain::KI_MAX_CAR_COUNT == 92, but the X360 ARTIST image this project reconstructs
// says 128 at every site:
//   1. the offset span +0x15B0..+0x19B0 is exactly 0x400 == 128 * 8;
//   2. HandleCarInfoResponseEvent @0x824BEDC0 guards the copy with
//      `cmpwi r11, 0x80 / blt` and the message "Too many cars for the selection" (cpp:959),
//      i.e. the array holds < 128 entries;
//   3. SetCarouselComponent's inlined BitArray index assert (CgsBitArray.h:203) bounds the
//      index with `cmplwi r25, 0x80` -- 128 bits, matching two 64-bit fields per array.
// Independently, the transport agrees: the event-412 payload the list arrives in is a
// CgsContainers::Array<s64,128> (count word at payload +0x400 -- see CgsArrayS64_128.cpp),
// and SetupMenuComponents' local id scratch array is 128 entries. 128 is also the only
// functionally correct choice: the shipped VehicleList has 104 parentless cars, which a
// 92-entry array could not hold.
//
// X360 VTABLE (off_82075470). CarSelectVehicle overrides, in CarSelectMain's slot order:
//   +0x04 OnEnter  +0x08 OnLeave  +0x0C Update  +0x24 GetResourcesToLoad()
//   +0x28 (base GetResourcesToLoadForCarSelect)  +0x2C IsLoading  +0x30 PlayMovie
//   +0x34 AppendAptComponents  +0x38 SetupComponents  +0x3C HandleControllerInput
//   +0x44 HandleCarInfoResponseEvent  +0x60 GetNumberResourcesToLoad
//   +0x64 SetupCar(const CarSetupInfo*, bool)  -- a NEW virtual, not an override of the
//         base's 1-arg SetupCar (UpdateCarouselTransition dispatches it as slot 25).
// ⚠️ Slots +0x48/+0x4C/+0x50/+0x54 (HandleCarAudioLoadComplete / HandlePlayerInfoResponse /
// HandleUnlockedLiveryResponseEvent / HandlePlayerCarColourResponseEvent) all hold
// 0x8284CB38 -- a bare `blr` with 193 xrefs, i.e. the image-wide ICF fold of an EMPTY body,
// NOT _purecall. They are real, empty overrides in the base and stay non-pure here.
//
// ⚠️ VEHICLE-LIST GUARDS -- HALF OF THEM RETIRED 2026-08-02 (the carousel wave). Read this
// before touching any `== 0` test in this class: the two shapes are NOT interchangeable, and
// mistaking one for the other cost the previous wave a crash it could not explain.
//
//   (a) LIST guards -- `mpVehicleList == 0`. These existed because NOTHING populated
//       GuiCache::mpWorldDataController, so mpVehicleList was NULL for the whole screen.
//       ⭐ THAT GAP IS CLOSED: BrnGui::GuiModule now owns a real WorldDataController,
//       Construct binds it into the cache (GuiCache::SetWorldDataController) and its
//       Prepare acquire machine binds the real 430-entry VehicleList through the GameData
//       request path. ALL of the (a) guards are GONE and the console's asserts are restored.
//
//   (b) ENTRY guards -- `lpVehicleData == 0` after a GetVehicleData/GetVehicleIndex lookup.
//       These are a DIFFERENT problem and they STAY. The console can dereference a lookup
//       result unchecked because the game-state module publishes a live drop-in car id
//       (GUI events 406 / 565) before this screen goes interactive; on this build that id
//       comes from BrnGameModule::PublishCarSelectionToGui, a flagged STAND-IN, and
//       CarSelectMain::Construct still seeds mCurrentSetupInfo.mCarId = (CgsID)-1 -- an id
//       that is in no vehicle list. Remove the (b) guards only when the real
//       ProcessGameEvents/BridgeGameStateToGui producers land.
//
// ⚠️⚠️ ONE OF THE ORIGINAL GUARDS TESTED THE WRONG POINTER, which is how (a) and (b) got
// conflated: IsLoading's guard tested the LOOKED-UP ENTRY, so it read as if it covered
// `mpVehicleList->GetVehicleData(...)` -- but that call dereferences the LIST, and
// VehicleList::GetVehicleData(CgsID) opens with `mov edi,[rcx+0x3700]`, so a null list AV'd
// inside the callee before any result existed to test. UpdateComponents calls IsLoading EVERY
// FRAME; it survived only because the `mbVoiceOverPlaying` early return above it masked the
// call for the nine seconds of the Junkyard car-info VO.
// (Lesson for the ledger: a guard's COMMENT is a claim -- check which pointer it actually
// tests, and against which dereference.)
//
// ⚠️ A THIRD shape, and it is what killed the process on this screen twice.
// SetCarSelectorComponent's bail left mCarSelector EMPTY, and SelectableGroup::GetHighlighted()
// has no lower bound on miHighlightedIndex (see the long HAZARD note on that body), so on an
// empty group it returned the four bytes preceding maSelectables -- 0x0000FF00 -- which is
// NON-NULL. The console's own `!= 0` assert therefore NEVER fired and the ->GetId() that
// followed access-violated. SetupComponents tests `mCarSelector.miHighlightedIndex > -1` (the
// console's own idiom) before dereferencing and falls back to the committed car id. That test
// is now normally redundant (the selector is populated), but it is the group's real emptiness
// contract and it stays until GetHighlighted itself is made safe.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                 // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectMain.h"            // BrnGui::CarSelectMain (base) + CarSetupInfo
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelection.h"        // BrnGui::TextSelection (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnPlayerStatsBar.h"       // BrnGui::PlayerStatsBar (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"   // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"      // BrnGui::SelectableGroup (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnRivalTableCell.h"       // BrnGui::RivalTableCell (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnCarouselSliderBar.h"    // BrnGui::CarouselSliderBar (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlineCountdown.h"  // by value
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerList.h" // by value

namespace CgsModule { struct Event; }
namespace CgsFsm    { class  ScriptedFsm; }
namespace CgsGui    { struct GuiEventAptTriggerPayload; }

namespace BrnGui
{
    // Event 244's payload (the online lobby player table). Pointer-only: the record has no
    // reconstructed home and the only consumer is the online-lobby handler below.
    struct GuiEventNetworkLobbyPlayerList;
    struct LobbyPlayerStatusData;          // mpHostStatusData target (online lobby only)

    struct CarSelectVehicle : public CarSelectMain
    {
        // BrnCarSelectVehicle.h:131 (DWARF). The carousel shows five overview icons.
        static const s32 KI_NUMBER_VISIBLE_VEHICLE_ICONS = 5;

        // The carousel car list capacity. See the "⭐ maSelectedCars IS 128 ENTRIES" note
        // above -- this is the X360 ARTIST bound, and it is also
        // CgsContainers::Array<s64,128>'s capacity (the event-412 transport).
        static const s32 KI_MAX_SELECTABLE_CARS = 128;

        // @ 0x82508670 -- the compiler-emitted ctor (vtable installs only).
        CarSelectVehicle();

        // @ 0x824BEBF0 (DWARF cpp:113) -- base Construct, then zero the two file-scope
        // carousel counters, the voice-over flag, the title-ticker latch and both bit arrays.
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);
        // @ 0x824C9470 (DWARF cpp:138) -- Construct every embedded component, register the
        // five observed events, seed the carousel geometry, post the "DwnldTitleUp" ticker.
        virtual void OnEnter();
        // @ 0x824C9938 (DWARF cpp:229) -- base OnLeave + unregister the five events.
        virtual void OnLeave();
        // @ 0x824DCBF0 (DWARF cpp:245) -- drain the five observed events, run the base
        // ladder, post the one-shot car-select-type record, then UpdateComponents().
        virtual void Update();

    protected:
        // @ 0x824B57F8 (DWARF cpp:328) -- append this screen's ten expected apt components.
        virtual void AppendAptComponents();
        // @ 0x824C9978 (DWARF cpp:355) -- the one-shot screen build once every expected apt
        // component has reported in.
        virtual void SetupComponents();

    private:
        // DWARF cpp:488. Inlined at every X360 call site: is lCarId one of the
        // gsiNumCarouselCars entries currently in maSelectedCars?
        bool IsCarSelectable(CgsID lCarId) const;

        // @ 0x824C9DE8 (DWARF cpp:1338) -- PlayAptMovie("BrnCarSelectMain", 3).
        virtual void PlayMovie();
        // @ 0x824DCD80 (DWARF cpp:572) -- the carousel's controller handling (hold / press /
        // release / analogue axis), always closing with UpdateCarouselTransition().
        virtual void HandleControllerInput(const CgsModule::Event* lpEvent, s32 liController);
        // @ 0x824BEDC0 (DWARF cpp:937) -- event 412: adopt the selectable-car list, the
        // driven/wrecked bit arrays and the "cars unlocked" counter.
        virtual void HandleCarInfoResponseEvent(const CgsModule::Event* lpEvent, s32 liEventType);
        // @ 0x824BBD18 (DWARF cpp:971) -- the per-frame component pump (runs from Update once
        // the screen is VISIBLE_INTERACTIVE).
        void UpdateComponents();
        // @ 0x824D8108 (DWARF cpp:1023) -- a NEW virtual (X360 vtable +0x64), not an override:
        // commit the selection (when lbCommit) and re-push every car-dependent component.
        virtual void SetupCar(const CarSetupInfo* lpSetupInfo, bool lbCommit);
        // @ 0x824BECD0 (DWARF cpp:519) -- gather the selectable cars' display names.
        void SetupMenuComponents();
        // @ 0x824B5918 (DWARF cpp:1287) -- route an apt trigger to the player list (type 1) or
        // to one of the three stats bars (type 4). The DWARF spells the parameter
        // `const GuiEventAptTrigger*`; the queue delivers the header-stripped payload, which
        // is CgsGui::GuiEventAptTriggerPayload (the same five fields).
        void HandleAptTrigger(const CgsGui::GuiEventAptTriggerPayload* lpTrigger);
        // @ 0x824C1200 (DWARF cpp:1109) -- push the speed/boost/strength gauges and the
        // car-type label for lpSetupInfo's car.
        void SetupStatsComponent(const CarSetupInfo* lpSetupInfo);
        // DWARF cpp:1155 -- inlined at both X360 call sites (SetupComponents + SetupCar).
        void SetupCarsUnlockedTextComponent();
        // @ 0x824BBE90 (DWARF cpp:1176) -- rebuild the five overview icons around lCarId.
        void SetCarouselComponent(CgsID lCarId);
        // @ 0x824B5A08 / @ 0x824B5A18 (DWARF cpp:1354 / cpp:1367).
        virtual CgsGui::sResourceTuple* GetResourcesToLoad() const;
        virtual u32 GetNumberResourcesToLoad() const;
        // @ 0x824C9E58 (DWARF cpp:1380) -- event 244, ONLINE-ONLY lobby player table.
        void HandleLobbyPlayerList(const GuiEventNetworkLobbyPlayerList* lpEvent);
        // @ 0x824C0FE8 (DWARF cpp:425) -- (re)populate the car-name selector from
        // maSelectedCars and highlight the committed car.
        void SetCarSelectorComponent();
        // @ 0x824C14B8 (DWARF cpp:1475) -- is a car change still in flight?
        virtual bool IsLoading() const;
        // DWARF cpp:1264 -- inlined at its X360 call site (SetCarouselComponent's tail).
        void SetSliderBarComponent();
        // @ 0x824C9BC8 (DWARF cpp:1059) -- clear the ticker and post the car's blurb line.
        void SetTicker(CgsID lCarId);
        // @ 0x824CA0F8 (DWARF cpp:1520) -- post the carousel click / clapper audio trigger.
        void TriggerSound(bool lbClappers);
        // @ 0x824D7D98 (DWARF cpp:854) -- advance the carousel scroll, stepping the highlight
        // one car per KC_CAROUSEL_X_ADVANCE of travel. Returns whether it stepped.
        bool UpdateCarouselTransition();

        // ---- statics (DWARF cpp:34..cpp:94; definitions in BrnCarSelectVehicle.cpp) -----
        // @0x82F26CF8 -- the six APT resources this screen loads (read out of the image).
        static const CgsGui::sResourceTuple maResourcesToLoad[6];
        static const u32 muNumResourcesToLoad;
        // @0x82065F40 -- { 21, 82, 244, 466, 467 }.
        static const s32 maiEventToObserve[5];
        static const s32 miNumEventsObserved;
        // @0x82F26D28 -> "CarName" (the selector's apt component name).
        static const char* mpacCarSelectorName;

        static const char KAC_CARS_AVAILABLE_STRINGID[21];
        static const char KAC_SPEED_STATS_BAR_NAME[17];
        static const char KAC_BOOST_STATS_BAR_NAME[17];
        static const char KAC_STRENGTH_STATS_BAR_NAME[20];
        static const char KAC_CARS_UNLOCKED_NAME[16];
        static const char KAC_CAR_TYPE[11];
        static const char KAC_SLIDER_BAR_NAME[21];
        static const char KAC_ONLINE_COUNTDOWN_NAME[13];
        static const char KAC_ONLINE_PLAYER_LIST[15];
        static const char macTrackAnimTransitionComponentName[14];
        static const char macMainAnimComponentName[13];
        // @0x82066030 -- the three per-car-type gauge colours (0x00FFD200 / 0x00990000 /
        // 0x00176A12), indexed by VehicleListEntry::GetCarType().
        static const u32 mauBoostColours[3];
        // @0x8206603C / @0x82066040 / @0x82066044 / @0x82066048 / @0x8206604C.
        static const f32 KC_CAROUSEL_X_ADVANCE;
        static const f32 KC_SCREEN_WIDTH;
        static const f32 KC_X_FRAME_CAROUSEL_ADJUST;
        static const f32 KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
        static const f32 KF_AXIS_DEAD_ZONE;
        // ⓘ The DWARF also lists KAC_OPTION_NAME[13] (cpp:70) and KAPC_OPTIONS[1] (cpp:72).
        // No reconstructed body references either, and neither value is attested in scope, so
        // they are deliberately NOT declared here rather than invented.

        // ---- the TU's two mutable counters ---------------------------------------------
        // The X360 keeps these as FILE-SCOPE statics in BrnCarSelectVehicle.cpp
        // (dword_82FB4958 / dword_82FB495C, both zeroed by Construct). This reconstruction
        // is split across three partfiles, so they are class statics here -- same storage,
        // same lifetime, same single definition.
        //   gsiNumCarouselCars       -- the live maSelectedCars fill count. Every carousel
        //     loop bound (SetupMenuComponents / SetCarSelectorComponent / UpdateComponents /
        //     SetCarouselComponent / IsCarSelectable) reads it; HandleCarInfoResponseEvent
        //     is the only writer besides Construct.
        //   gsiNumCarsUnlockedTotal  -- the SECOND parameter of the "CAR_SELECT_AVAILABLE"
        //     label; copied from the event-412 payload word at +0x428. FLAG: the name is
        //     from its role (the companion of the available count); the console symbol is
        //     unnamed.
        // ⭐ gsiNumCarouselCars IS SHARED WITH BrnGui::CarSelectLivery (2026-08-02). That
        // screen reads dword_82FB4958 at two sites -- UpdateComponents @0x824C7CB0 (offer the
        // "$GENERAL_OPTION_BACK" prompt only when there is more than one car) and
        // HandleControllerInput @0x824D6D10 case 0x32 (allow the GO_BACK press on the same
        // condition). Two TUs reading one address means the console's counter is
        // linker-visible, not a file-scope static, so this one is public.
    public:
        static s32 gsiNumCarouselCars;
    private:
        static s32 gsiNumCarsUnlockedTotal;

        // ---- members over the CarSelectMain base (X360 offsets are documentary) ---------
        TextSelection             mCarSelector;                        // +0x8F0   h:106
        CgsID                     maSelectedCars[KI_MAX_SELECTABLE_CARS];  // +0x15B0 h:108
        CgsContainers::BitArray<128> maSelectedCarsDrivenState;        // +0x19B0  h:110
        CgsContainers::BitArray<128> maSelectedCarsWreckedState;       // +0x19C0  h:112
        PlayerStatsBar            mSpeedStatsBar;                      // +0x19D0  h:114
        PlayerStatsBar            mBoostStatsBar;                      // +0x1B0C  h:115
        PlayerStatsBar            mStrengthStatsBar;                   // +0x1C48  h:116
        TextField                 mCarType;                            // +0x1D84  h:117
        TextField                 mCarsUnlocked;                       // +0x1EAC  h:125
        RivalTableCell            maCarouselOverviewSelectable[KI_NUMBER_VISIBLE_VEHICLE_ICONS];  // +0x1FD8 h:134
        SelectableGroup           mCarouselOverviewSelectableGroup;    // +0x2370  h:135
        CarouselSliderBar         mCarouselSliderBar;                  // +0x25A8  h:136
        AnimationComponent        mTrackAnimTransitionComponent;       // +0x2638  h:139
        AnimationComponent        mMainAnimComponent;                  // +0x26C4  h:142
        CarSelectOnlineCountdown  mOnlineCountdown;                    // +0x2750  h:147
        CarSelectOnlinePlayerList mOnlinePlayerList;                   // +0x2908  h:150
        bool                      mbFirstFrame;                        // +0x4120  h:152
        const LobbyPlayerStatusData* mpHostStatusData;                 // +0x4124  h:153
        f32                       mfCarouselXOffset;                   // +0x4128  h:158
        f32                       mfCarouselXOffsetDecay;              // +0x412C  h:159
        f32                       mafCarouselOriginalXPos[KI_NUMBER_VISIBLE_VEHICLE_ICONS];  // +0x4130 h:162
        u32                       muCarouselControllerRightPressedRefCount;  // +0x4144 h:167
        u32                       muCarouselControllerLeftPressedRefCount;   // +0x4148 h:168
        bool                      mbControllerAxisActive;              // +0x414C  h:169
        bool                      mbVoiceOverPlaying;                  // +0x414D  h:170
        // ⚠️ A 26th member the DecFIGS DWARF member list does NOT carry (its last entry is
        // h:170). Construct @0x824BEBF0 zeroes it (`stb r31, 0x414E(r30)`) and OnEnter
        // @0x824C9470 reads it, posts the 304-byte "DwnldTitleUp" ticker (type 184, channel
        // 40) when it is clear, and sets it -- a one-shot "the title ticker has been posted"
        // latch. Named for that behaviour and flagged as un-attested by the DWARF.
        bool                      mbTitleTickerPosted;                 // +0x414E  (not in DWARF)
    };
}
