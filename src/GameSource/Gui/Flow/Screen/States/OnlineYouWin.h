#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"                                  // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"                   // BrnGui::IconComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"     // BrnGui::AnimationComponent (by value)

// BrnGui::OnlineYouWin - the online "you win" flow state. It checks the local player's row in
// the cached online post-event record (which can advance it at once), then shows the photo
// screen (the camera count-down, then "Click") and holds the result. A player timing out ends
// the screen early, and the online stunt-run modes skip it altogether.
//
// Base derivation, member order/names and method shapes follow the original declarations
// (OnlineYouWin.h): mpGuiCache (console +0x38), meInternalState (+0x3C), mfCurrentTime (+0x40),
// mTimeUntillClickTextfield (+0x44), mPhotoComponent (+0x16C), mModeAnimator (+0x200),
// mTickAnimator (+0x28C), mfTickTime (+0x318), mbUseCamera (+0x31C).
namespace BrnGui
{
    // Pointer-only here; the full GuiCache header is pulled in by the .cpp (it is very large
    // and every screen-flow includer would otherwise inherit it).
    class GuiCache;

    struct OnlineYouWin : public CgsGui::State
    {
        enum InternalState
        {
            E_INTERNALSTATE_GETCACHE      = 0,
            E_INTERNALSTATE_WF_WIN_RESULT = 1,
            E_INTERNALSTATE_LOADRESOURCES = 2,
            E_INTERNALSTATE_WFINIT        = 3,
            E_INTERNALSTATE_TAKING        = 4,
            E_INTERNALSTATE_SHOWING       = 5,
            E_INTERNALSTATE_LEFT          = 6,
            E_INTERNALSTATE_COUNT         = 7,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // Hands this screen's static resource list to the loader (count == 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        void UpdateGetCache();
        bool UpdateWFWinResult();
        bool UpdateLoadResources();
        bool UpdateWFInit();
        bool UpdateTaking();
        void UpdateShowing();
        void UpdatePermanent();
        bool HasAnyoneTimedOut();

        GuiCache*          mpGuiCache;                  // console +0x38
        InternalState      meInternalState;             // console +0x3C
        f32                mfCurrentTime;               // console +0x40 (per-sub-state timer)
        TextField          mTimeUntillClickTextfield;   // console +0x44  "PhotoTime_mc"
        IconComponent      mPhotoComponent;             // console +0x16C "PhotoAnimation_mc"
        AnimationComponent mModeAnimator;               // console +0x200 "Pages_anim"
        AnimationComponent mTickAnimator;               // console +0x28C "PhotoTime_anim"
        f32                mfTickTime;                  // console +0x318 (last whole second shown)
        bool               mbUseCamera;                 // console +0x31C

        static const s32                    maiEventToObserve[2];
        static const s32                    miNumEventsObserved;
        static const CgsGui::sResourceTuple maResourcesToLoad[1];
        static const u32                    muNumResourcesToLoad;
        static const f32                    KF_WIN_CHECK_DURATION;
        static const f32                    KF_POSING_DURATION;
        static const f32                    KF_SHOWING_DURATION;
        static const char                   KAC_TIME_UNTILL_CLICK_TEXTFIELD_NAME[13];
        static const char                   KAC_PHOTO_COMPONENT_NAME[18];
        static const char                   KAC_MODE_ANIMATOR[11];
        static const char                   KAC_TICK_ANIMATOR_NAME[15];
    };
}
