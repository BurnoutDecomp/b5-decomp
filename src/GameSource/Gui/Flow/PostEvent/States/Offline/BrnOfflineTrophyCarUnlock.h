#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"                                // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"   // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnManufacturerIcon.h"     // BrnGui::ManufacturersIcon (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnLargeCarComponent.h"    // BrnGui::LargeCarComponent (by value)

namespace BrnProgression { class Profile; }   // mpProfile (pointer only)

// Pointer-only parameters (their homes, CgsAptCommunicator.h / CgsGuiEventTypeDefs.h, are
// included by the .cpp). Same treatment as BrnOfflineRivalShutdown.h.
namespace CgsGui { struct GuiEventAptTriggerPayload; struct GuiEventControllerInputPressed; }

// BrnGui::OfflineTrophyCarUnlock -- the offline "trophy car unlocked" post-event GUI state
// (screen-flow state TRPHY_UNLOCK). InGame sends "TO_TRPHY_UNL" a short while after GUI 375
// (GuiEventTrophyCarUnlock) arrives outside a game mode, which lands the screen flow here. The
// state plays the "BrnTrophyCarUnlock" apt movie, shows the awarded carbon car
// (LargeCarComponent), its manufacturer badge and two caption lines, swaps the second caption
// for the new-car instructions, transitions out and ADVANCEs. OnLeave marks the unlock
// sequence as seen on the profile, so the free-roam HUD's progression scan does not raise the
// same trophy again.
//
// The console class is the rival-shutdown screen's twin: the same member order and component
// strides (+0x38 state word, +0x3C profile, +0x40 cache, +0x44 the animation, +0xD0 the car,
// +0x198 the badge, +0x224/+0x34C the captions, +0x474/+0x478 the two clocks). Offsets are
// documentary; members are reached by name.
namespace BrnGui
{
    class GuiCache;

    struct OfflineTrophyCarUnlock : public CgsGui::State
    {
        // The presentation ladder Update switches on.
        enum EOfflineTrophyCarUnlockState
        {
            E_OFFLINETROPHYCARUNLOCKSTATE_NONE                     = 0,
            E_OFFLINETROPHYCARUNLOCKSTATE_LOADINGRESOURCES         = 1,
            E_OFFLINETROPHYCARUNLOCKSTATE_WAITINGFORCOMPONENTS     = 2,
            E_OFFLINETROPHYCARUNLOCKSTATE_RUNNING                  = 3,
            E_OFFLINETROPHYCARUNLOCKSTATE_WAITING_FOR_FADEOUT_TEXT = 4,
            E_OFFLINETROPHYCARUNLOCKSTATE_SET_NEW_TEXT             = 5,
            E_OFFLINETROPHYCARUNLOCKSTATE_SHOWING_NEW_TEXT         = 6,
            E_OFFLINETROPHYCARUNLOCKSTATE_WAITING_FOR_TRANSOUT     = 7,
            E_OFFLINETROPHYCARUNLOCKSTATE_TRANSOUT_COMPLETE        = 8,
            E_OFFLINETROPHYCARUNLOCKSTATE_FINISH                   = 9,
            E_OFFLINETROPHYCARUNLOCKSTATE_COUNT                    = 10,
        };

        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // Hands the trophy-car-unlock screen's resource list out.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        void HandleIncomingEvents();
        void AppendExpectedComponents();
        void SetupComponents();
        void HandleAptTriggers(const CgsGui::GuiEventAptTriggerPayload* lpEvent);
        // Dispatched through the vtable by HandleIncomingEvents (event 6), so virtual.
        virtual void HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventType);
        void HandleControllerInputPressed(
            const CgsGui::GuiEventControllerInputPressed* lpControllerPressedEvent);

        static const CgsGui::sResourceTuple maResourcesToLoad[];   // 4 entries
        static const u32                    muNumResourcesToLoad;  // == 4
        static const s32                    maiEventToObserve[];   // 6 entries
        static const s32                    miNumEventsObserved;   // == 6

        EOfflineTrophyCarUnlockState meOfflineTrophyCarUnlockState;   // +0x038
        BrnProgression::Profile*     mpProfile;                       // +0x03C
        GuiCache*                    mpGuiCache;                      // +0x040
        AnimationComponent           mScreenAnim;                     // +0x044
        LargeCarComponent            mTrophyCarComponent;             // +0x0D0
        ManufacturersIcon            mManufacturerIcon;               // +0x198
        TextField                    mCongratTextCar;                 // +0x224
        TextField                    mCongratTextDesc;                // +0x34C
        f32                          mfScreenStartTime;               // +0x474
        f32                          mfNewTextStartTime;              // +0x478
    };
}
