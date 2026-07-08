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

        // ---- FSM / resource virtuals (DWARF order gives the derived vtable shape) ----
        // @ 0x824BBC20 -- base State::Construct then prime this state's members + event queue.
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);
        // @ 0x824C8920 -- register events, post the enter commands, latch the cache. [not in scope]
        virtual void OnEnter();
        // @ 0x824C8B78 -- unregister + post the leave commands. [not in scope]
        virtual void OnLeave();
        // @ 0x824DC9C0 -- the per-frame load/interactive state machine. [not in scope]
        virtual void Update();
        // @ 0x824B55B8 -- this state's static resource list (empty: writes {0,0}).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;
        // @ 0x824B56C0 -- the car-select resource list (two virtual getters). [not in scope]
        virtual void GetResourcesToLoadForCarSelect(const CgsGui::sResourceTuple** lppResourceTuples,
                                                    u32* lpuNumberOfResources) const;
        // @ 0x824B4F.. -- is the state still loading. [not in scope]
        virtual bool IsLoading() const;
        // -- play the state's apt movie. [not in scope]
        virtual void PlayMovie();
        // @ 0x824B5380 -- register the always-present help/logo apt components as expected.
        virtual void AppendAptComponents();
        // -- build the state's apt components. [not in scope]
        virtual void SetupComponents();
        // @ 0x824B5410 -- controller input hook (null-event guard only).
        virtual void HandleControllerInput(const CgsModule::Event* lpEvent, s32 liController);

        // ---- non-virtual helpers ----
        // @ 0x824B54A8 -- latch the GuiCache the cache-ready event carries + its VehicleList.
        void UpdateGuiCache(const CgsModule::Event* lpCacheEvent);
        // @ 0x824B5548 -- record the desired car and flag a car change in progress.
        void SetupCar(const CarSetupInfo& lrSetupInfo);
        // @ 0x824C0EB0 -- push the localised car + manufacturer names for lSelectedCarId.
        void SetupCarNameComponent(CgsID lSelectedCarId);
        // @ 0x824C8E08 -- commit mDesired->mCurrent and post the car-select trigger event.
        void TriggerSetupCar();

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
        s32                 meCarSelectType;      // +0x7C8 (BrnGameState::GameStateModuleIO::ECarSelectType)
        CarSelectState      meCurrentState;       // +0x7CC (X360 miState word)
        const BrnResource::WheelList* mpWheelList; // +0x7D0
        CgsModule::EventReceiverQueue<256, 16> mGameDataEventReceiverQueue;  // +0x7D4
    };
}
