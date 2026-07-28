#pragma once

// ===================================================================================
// BrnGui::CarSelectMain  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCarSelectMain.h
//
// The shared base of the car-select screen flow states (CarSelectVehicle / CarSelectLivery
// derive from it and forward their FSM virtuals here). A CgsGui::State leaf that latches a
// GuiCache + its VehicleList, drives the manufacturer/car-name text fields, the manufacturer
// badge and the two help items, and posts the car-selection GUI events.
//
// Class shape / member NAMES + declaration order are from the DecFIGS DWARF for this exact
// path (BrnCarSelectMain.h, X360-attested per the ledger). The byte offsets in the comments
// are the X360 32-bit-ABI offsets proven by BURNOUT_X360_ARTIST.XEX (the members are laid out
// contiguously: mManufacturerName@0x38 .. mGameDataEventReceiverQueue@0x7D4); the gate compiles
// 64-bit, so the embedded widgets/queue widen and the offsets are documentary only -- every
// member is accessed BY NAME.
//
// X360 VTABLE SLOT MAP (wave G; derived from the virtual dispatch displacements in
// ProcesssIncomingEvents @0x824D73D8, Update @0x824DC9C0 and GetResourcesToLoadForCarSelect
// @0x824B56C0, cross-checked against the DWARF declaration order). Base CgsGui::State
// hierarchy fills slots 0-8 (OnEnter..GetResourcesToLoad(2-arg)); CarSelectMain's new
// virtuals land, in slot order:
//   +0x24 GetResourcesToLoad() [0-arg overload -- MSVC groups it adjacent to its base
//         name-mate]   +0x28 GetResourcesToLoadForCarSelect   +0x2C IsLoading
//   +0x30 PlayMovie    +0x34 AppendAptComponents   +0x38 SetupComponents
//   +0x3C HandleControllerInput      +0x40 UpdateGuiCache
//   +0x44 HandleCarInfoResponseEvent +0x48 HandleCarAudioLoadComplete
//   +0x4C HandlePlayerInfoResponse   +0x50 HandleUnlockedLiveryResponseEvent
//   +0x54 HandlePlayerCarColourResponseEvent  +0x58 SetupCar  +0x5C ExitCarSelection
//   +0x60 GetNumberResourcesToLoad
// The x64 build lays out its own vtable; the map documents WHY each dispatch site in the
// reconstruction is a plain by-name call.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                           // CgsID (typedef u64)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"          // CgsGui::State (base)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"     // CgsModule::EventReceiverQueue<256,16>
#include "GameSource/Gui/BrnGuiTextField.h"                              // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"           // BrnGui::HelpItem (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnManufacturerIcon.h"   // BrnGui::ManufacturersIcon (by value)

namespace CgsModule { struct Event; }
namespace CgsFsm    { class  ScriptedFsm; }
namespace BrnResource { struct VehicleList; struct WheelList; }

namespace BrnGui
{
    class GuiCache;

    // BrnCarSelectMain.h:47 (DWARF). The selected-car descriptor the state tracks: a car id
    // plus whether that car is currently selectable. Copied wholesale (mCurrent<=mDesired) when
    // a selection is triggered.
    struct CarSetupInfo
    {
        CgsID mCarId;        // +0x00
        bool  mbSelectable;  // +0x08 (X360 stores/reads the whole qword slot)

        void Construct();    // DWARF BrnCarSelectMain.h:51 (id = invalid, selectable = true)
    };

    struct CarSelectMain : public CgsGui::State
    {
        // BrnCarSelectMain.h:70 (DWARF).
        static const s32 KI_MAX_CAR_COUNT = 92;

        // BrnCarSelectMain.h:72 (DWARF). The load state machine (meCurrentState).
        enum CarSelectState
        {
            E_CARSELECT_INVALID            = -1,
            E_CARSELECT_UNLOADED           = 0,
            E_CARSELECT_LOADING_COMPONENTS = 1,
            E_CARSELECT_VISIBLE_INTERACTIVE = 2,
            E_CARSELECT_COUNT              = 3,
        };

        // ---- FSM / resource virtuals (DWARF declaration order) -----------------------
        // @ 0x824BBC20 -- base State::Construct then prime this state's members + event queue.
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);
        // @ 0x824C8920 -- register the 21 observed events, hide the HUD (event 148 @ch42),
        // zero the setup ids, latch the GuiCache off the access pointers, post command 405
        // (@ch40), Construct the five apt components, latch meCarSelectType.
        virtual void OnEnter();
        // @ 0x824C8B78 -- unregister, invalidate the state, clear the level-3 apt movie
        // (PlayAptMovie("", 3)), drop the expected-component list, post command 533 (@ch40).
        virtual void OnLeave();
        // @ 0x824DC9C0 -- the per-frame load/interactive state machine; always ends in
        // ProcesssIncomingEvents().
        virtual void Update();
        // @ 0x824B55B8 -- this state's static resource list (empty: writes {0,0}).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    protected:
        // @ 0x824B56C0 -- the car-select resource list: forwards to the two 0-arg virtual
        // getters (GetResourcesToLoad() / GetNumberResourcesToLoad()).
        virtual void GetResourcesToLoadForCarSelect(const CgsGui::sResourceTuple** lppResourceTuples,
                                                    u32* lpuNumberOfResources) const;
        // @ 0x824D73D8 -- drain mpInGuiEventQueue and dispatch the 21 observed events. The
        // triple-s spelling is the ORIGINAL name (DWARF cpp:301).
        void ProcesssIncomingEvents();
        // DWARF cpp:954. No base body is emitted in the X360 image (dead-stripped: only the
        // derived states are instantiated); the derived TUs own their overrides.
        virtual bool IsLoading() const;
        // DWARF h:136. No base body in the X360 image (see IsLoading note).
        virtual void PlayMovie();
        // @ 0x824B5380 -- register the always-present help/logo apt components as expected.
        virtual void AppendAptComponents();
        // DWARF cpp:511. No base body in the X360 image (see IsLoading note).
        virtual void SetupComponents();
        // @ 0x824B5410 -- controller input hook (null-event guard only). liController is the
        // observed event id (5..8 select the controller; ProcesssIncomingEvents forwards it).
        virtual void HandleControllerInput(const CgsModule::Event* lpEvent, s32 liController);
        // @ 0x824B54A8 -- latch the GuiCache the cache-ready event (64) carries + its
        // VehicleList. VIRTUAL with the trailing event-id s32 per the DWARF (cpp:544) -- the
        // dispatcher passes (event, event id) like the other handlers; this body ignores the id.
        virtual void UpdateGuiCache(const CgsModule::Event* lpCacheEvent, s32 liEventType);
        // DWARF cpp:563 -- event 412. No base body in the X360 image (see IsLoading note).
        virtual void HandleCarInfoResponseEvent(const CgsModule::Event* lpEvent, s32 liEventType);
        // DWARF cpp:789 -- event 564. No base body in the X360 image.
        virtual void HandleCarAudioLoadComplete();
        // DWARF cpp:803 -- event 406 (after the setup-id latch). No base body in the X360 image.
        virtual void HandlePlayerInfoResponse(const CgsModule::Event* lpEvent, s32 liEventType);
        // DWARF cpp:819 -- event 413. No base body in the X360 image.
        virtual void HandleUnlockedLiveryResponseEvent(const CgsModule::Event* lpEvent, s32 liEventType);
        // DWARF cpp:834 -- event 414. No base body in the X360 image.
        virtual void HandlePlayerCarColourResponseEvent(const CgsModule::Event* lpEvent, s32 liEventType);
        // @ 0x824B5548 -- record the desired car and flag a car change in progress. VIRTUAL,
        // POINTER param per the DWARF (cpp:577; CarSelectVehicle overloads it with an extra
        // bool as a separate new virtual).
        virtual void SetupCar(const CarSetupInfo* lpSetupInfo);
        // @ 0x824C8CB8 -- accept the selection: junkyard flows post GuiEventActivateCarSelect
        // {4,1}, then ticker-clear {true,false}, then the "ACCEPT" state event.
        virtual void ExitCarSelection();
        // @ 0x824C0EB0 -- push the localised car + manufacturer names for lSelectedCarId.
        void SetupCarNameComponent(CgsID lSelectedCarId);

    private:
        // @ 0x824C8E08 -- commit mDesired->mCurrent and post the car-select trigger event.
        void TriggerSetupCar();
        // @ 0x824C91C8 -- event 273: lobby-deleted / leave-question / kick overlays, then
        // "DISCONNECT" (+ expected-component drop while still loading).
        void HandleLeftGameEvent(const CgsModule::Event* lpLeftGameEvent);
        // @ 0x824C9008 -- event 57 (launching): host/join overlay + the mode-string message
        // params. NOTE: the body is NOT part of this TU's ledger scope (identity attributes
        // it to another TU) and is not yet reconstructed anywhere -- declaration only; do NOT
        // define it in the BrnCarSelectMain partfiles.
        void HandleLaunchingEvent(const CgsModule::Event* lpLaunchingEvent);
        // @ 0x824C8EF0 -- event 58 (launched): on failure, "CNOnlLchFail" overlay carrying
        // KPC_LAUNCH_FAILED_STRINGIDS[result] as message param 2.
        void HandleLaunchedEvent(const CgsModule::Event* lpLaunchedEvent);
        // DWARF h:292/h:296 -- the two 0-arg resource getters GetResourcesToLoadForCarSelect
        // forwards to (X360 slots +0x24/+0x60). No base bodies in the X360 image (derived
        // states own the real lists).
        virtual CgsGui::sResourceTuple* GetResourcesToLoad() const;
        virtual u32 GetNumberResourcesToLoad() const;

        // ---- statics (DWARF h:254/h:255 + cpp:79/cpp:90; definitions + dumped values in
        //      BrnCarSelectMain_wG_01.cpp) --------------------------------------------------
        // @ 0x82065E80 (.data, read from the image): the 21 observed event ids OnEnter
        // registers / OnLeave unregisters, in table order.
        static const s32 maiEventToObserve[21];
        // @ 0x82065ED4 (immediately after the table) == 21.
        static const s32 miNumEventsObserved;
        // @ 0x82F26C94 (7 string pointers): launched-result -> overlay message string id
        // (entry 0 is the pooled empty string; results 1..6 are the ONLINE_LAUNCHED_FAILED_*
        // ids). HandleLaunchedEvent indexes it with the non-zero result word.
        static const char* const KPC_LAUNCH_FAILED_STRINGIDS[7];
        // @ 0x82F26CB0 (17 string pointers, EGameModeType-indexed; only the online modes
        // 10..14 are populated). Consumed by HandleLaunchingEvent (out of this TU's scope).
        static const char* const KPAC_MODE_STRINGS[18];

    protected:
        // ---- members over the CgsGui::State base (X360 offsets are documentary) ----
        TextField           mManufacturerName;   // +0x38  ("CarMan_mc")
        TextField           mCarName;            // +0x160 ("CarName_mc")
        GuiCache*           mpGuiCache;          // +0x288
        const BrnResource::VehicleList* mpVehicleList;  // +0x28C
        CarSetupInfo        mCurrentSetupInfo;   // +0x290
        CarSetupInfo        mDesiredSetupInfo;   // +0x2A0
        CgsID               miMostRecentDropInId; // +0x2B0
        TextField           mTitleText;          // +0x2B8
        ManufacturersIcon   mManufacturerLogo;   // +0x3E0 ("ManufacturerLogo_mc")
        HelpItem            mHelpItemContinue;    // +0x46C ("HelpItemContinue_mc")
        HelpItem            mHelpItemBack;        // +0x618 ("HelpItemBack_mc")
        bool                mbCarChangeInProgress; // +0x7C4
        s32                 meCarSelectType;      // +0x7C8 (GsmIO::ECarSelectType value; enum
                                                  //         home BrnGameStateSharedIO.h)
        CarSelectState      meCurrentState;       // +0x7CC (X360 miState word)
        const BrnResource::WheelList* mpWheelList; // +0x7D0
        CgsModule::EventReceiverQueue<256, 16> mGameDataEventReceiverQueue;  // +0x7D4
    };
}
