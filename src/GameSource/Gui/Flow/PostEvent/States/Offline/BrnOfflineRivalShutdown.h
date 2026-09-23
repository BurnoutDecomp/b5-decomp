#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"                                // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"   // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnManufacturerIcon.h"     // BrnGui::ManufacturersIcon (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnLargeCarComponent.h"    // BrnGui::LargeCarComponent (by value)

namespace BrnProgression { class Profile; }   // mpProfile (pointer only)

// Pointer-only parameters (their homes, CgsAptCommunicator.h / CgsGuiEventTypeDefs.h, are
// included by the .cpp). Same treatment as BrnCrashNavPanel.h.
namespace CgsGui { struct GuiEventAptTriggerPayload; struct GuiEventControllerInputPressed; }

// ==========================================================================================
// BrnGui::OfflineRivalShutdown -- the offline "rival shut down" post-event GUI state
// (screen-flow state POST_RIVAL, lapStates[49]). A free-burn takedown of a rival car posts
// game action 120 -> GUI 373 -> InGame::Update case 373 -> SendStateEvent("TO_RVL_POST"),
// which lands the screen flow here. The state plays the "BrnRivalShutdown" apt movie, shows
// the shut-down rival's car (LargeCarComponent), its manufacturer badge and two caption
// lines, swaps the second caption for the new-car instructions, transitions out and ADVANCEs.
//
// ⭐ crash parity G13-X5 (2026-09-23): this class USED TO BE A HOLLOW SHELL -- only
// GetResourcesToLoad. Every other console body below is now reconstructed in
// BrnOfflineRivalShutdown.cpp; ARTIST addresses beside each declaration.
//
// Layout / enum / method set from the DecFIGS DWARF (BrnOfflineRivalShutdown.h:46..170),
// every member gated on an X360 store or load. The X360 byte offsets are DOCUMENTARY (the
// CgsGui::State base and the components widen on x64, so members are reached BY NAME); what
// they pin is the ORDER, and the component strides match the committed component sizes:
//     +0x038 meOfflineRivalShutdownState  Construct/OnEnter/OnLeave `stw 0x38`
//     +0x03C mpProfile                    (DWARF h:142; no X360 body in this class reads it)
//     +0x040 mpGuiCache                   HandleIncomingEvents case 64 `stw 0x40`
//     +0x044 mScreenAnim           0x8C   OnEnter vtbl-slot-0 Construct on this+0x44
//     +0x0D0 mShutdownCarComponent 0xC8   ReleaseResources / SetCarInfo on this+0xD0
//     +0x198 mManufacturerIcon     0x8C   ManufacturersIcon::Set on this+0x198
//     +0x224 mCongratTextCar       0x128  SetLocalisedText on this+0x224
//     +0x34C mCongratTextDesc      0x128  SetLocalisedText on this+0x34C
//     +0x474 mfScreenStartTime            OnEnter `stfs 0x474`, Update case 3 `lfs 0x474`
//     +0x478 mfNewTextStartTime           Update case 5 `stfs 0x478`, case 6 `lfs 0x478`
// ==========================================================================================
namespace BrnGui
{
    class GuiCache;

    struct OfflineRivalShutdown : public CgsGui::State
    {
        // DWARF BrnOfflineRivalShutdown.h:77 -- the presentation ladder Update switches on.
        enum EOfflineRivalShutdownState
        {
            E_OFFLINERIVALSHUTDOWNSTATE_NONE                     = 0,
            E_OFFLINERIVALSHUTDOWNSTATE_LOADINGRESOURCES         = 1,
            E_OFFLINERIVALSHUTDOWNSTATE_WAITINGFORCOMPONENTS     = 2,
            E_OFFLINERIVALSHUTDOWNSTATE_RUNNING                  = 3,
            E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_FADEOUT_TEXT = 4,
            E_OFFLINERIVALSHUTDOWNSTATE_SET_NEW_TEXT             = 5,
            E_OFFLINERIVALSHUTDOWNSTATE_SHOWING_NEW_TEXT         = 6,
            E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_TRANSOUT     = 7,
            E_OFFLINERIVALSHUTDOWNSTATE_TRANSOUT_COMPLETE        = 8,
            E_OFFLINERIVALSHUTDOWNSTATE_FINISH                   = 9,
            E_OFFLINERIVALSHUTDOWNSTATE_COUNT                    = 10,
        };

        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);   // @0x824B94D0 (cpp:86)
        virtual void OnEnter();                                           // @0x824B9588 (cpp:104)
        virtual void OnLeave();                                           // @0x824D3060 (cpp:135)
        virtual void Update();                                            // @0x824DAFE0 (cpp:168)

        // @ 0x825008D0 - hands the rival-shutdown screen's resource list out.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        void HandleIncomingEvents();                                                 // @0x824C23D0 (cpp:301)
        void AppendExpectedComponents();                                             // @0x824B96A0 (cpp:372)
        void SetupComponents();                                                      // @0x824D3130 (cpp:392)
        void HandleAptTriggers(const CgsGui::GuiEventAptTriggerPayload* lpEvent);    // @0x824B9748 (cpp:475)
        // The console dispatches this one through the vtable (HandleIncomingEvents case 6:
        // `lwz r11,0x24(vtbl) ; bctrl`), so it is the DWARF's `virtual`.
        virtual void HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventType); // @0x824BD100 (cpp:525)
        void HandleControllerInputPressed(
            const CgsGui::GuiEventControllerInputPressed* lpControllerPressedEvent);  // @0x824B98B8 (cpp:556)

        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @ 0x82F27318 (.rdata, 4 entries)
        static const u32                    muNumResourcesToLoad;  // @ 0x82066898 (.rdata, == 4)
        static const s32                    maiEventToObserve[];   // @ 0x8206689C (.rdata, 5 entries)
        static const s32                    miNumEventsObserved;   // @ 0x820668B0 (.rdata, == 5)

        EOfflineRivalShutdownState meOfflineRivalShutdownState;   // +0x038 (h:140)
        BrnProgression::Profile*   mpProfile;                     // +0x03C (h:142)
        GuiCache*                  mpGuiCache;                    // +0x040 (h:143)
        AnimationComponent         mScreenAnim;                   // +0x044 (h:150)
        LargeCarComponent          mShutdownCarComponent;         // +0x0D0 (h:152)
        ManufacturersIcon          mManufacturerIcon;             // +0x198 (h:158)
        TextField                  mCongratTextCar;               // +0x224 (h:161)
        TextField                  mCongratTextDesc;              // +0x34C (h:164)
        f32                        mfScreenStartTime;             // +0x474 (h:167)
        f32                        mfNewTextStartTime;            // +0x478 (h:168)
    };
}
