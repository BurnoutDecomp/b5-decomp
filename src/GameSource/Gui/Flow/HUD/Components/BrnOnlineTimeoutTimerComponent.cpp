// ===================================================================================
// BrnGui::OnlineTimeoutComponent -- implementation
//   b5-decomp/src/GameSource/Gui/Flow/HUD/Components/BrnOnlineTimeoutTimerComponent.cpp
//
// Seven functions, store for store from the console bodies. The state strings go through
// the icon's virtual SetState (vtable slot 3 on the console).
// ===================================================================================
#include "GameSource/Gui/Flow/HUD/Components/BrnOnlineTimeoutTimerComponent.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SnPrintf (the timer's child name)

namespace BrnGui
{
    const f32  OnlineTimeoutComponent::KF_SAFE_BOUNDARY_TIME   = 15.0f;
    const f32  OnlineTimeoutComponent::KF_DANGER_BOUNDARY_TIME = 5.0f;
    const char OnlineTimeoutComponent::KAC_TIMER_NAME[9]       = "Timer_mc";

    // Construct: the icon base (state interface, clip and state hash cleared), then the
    // counting-down timer field named "Timer_mc" under this component, its safe / danger
    // colours and the 15 s / 5 s colour boundaries, and no time pending.
    void OnlineTimeoutComponent::Construct(const char* lacName,
                                           CgsGui::StateInterface* lpStateInterface,
                                           const char* lacParentName)
    {
        FlaptIconComponent::Construct(lacName, lpStateInterface, lacParentName);

        mTimerField.Construct(KAC_TIMER_NAME, mpStateInterface,
                              FlaptTimerFieldComponent::E_TIMER_MODE_COUNTING_DOWN, lacName);
        mTimerField.SetSafeColours(KU_SAFERED, KU_SAFEGREEN, KU_SAFEBLUE);
        mTimerField.SetDangerColours(KU_DANGERRED, KU_DANGERGREEN, KU_DANGERBLUE);
        mTimerField.SetBoundaries(KF_SAFE_BOUNDARY_TIME, KF_DANGER_BOUNDARY_TIME);

        mbActive     = false;
        mbNewTimeSet = false;
        mfNewTime    = -1.0f;
    }

    // Prepare: bind the icon clip (no parent prefix), then the timer field's
    // "TimerText_txt" text field inside "<name>_Timer_mc".
    void OnlineTimeoutComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile,
                                         const char* /*lacParentName*/)
    {
        FlaptIconComponent::Prepare(lacName, lFile, 0);

        char lacTimerName[128];
        CgsCore::SnPrintf(lacTimerName, sizeof(lacTimerName), "%s_%s", lacName, KAC_TIMER_NAME);
        mTimerField.Prepare("TimerText_txt", lacTimerName, lFile);
    }

    // SetTime: keep the smallest time posted since the last Update (`fcmpu` + `bgelr`, so a
    // NaN never replaces a pending time).
    void OnlineTimeoutComponent::SetTime(f32 lfTime)
    {
        if (!mbNewTimeSet || lfTime < mfNewTime)
        {
            mfNewTime    = lfTime;
            mbNewTimeSet = true;
        }
    }

    // Update: consume the pending time. A non-negative (or NaN) time is shown and brings the
    // icon in; a negative time zeroes the field and takes a shown icon out.
    void OnlineTimeoutComponent::Update()
    {
        if (!mbNewTimeSet)
            return;

        if (!(mfNewTime < 0.0f))
        {
            mTimerField.SetTime(mfNewTime);
            if (!mbActive)
                Transin();
        }
        else if (mbActive)
        {
            mTimerField.SetTime(0.0f);
            Transout();
        }
        mbNewTimeSet = false;
    }

    // Show: publish "visible", then mark the component shown.
    void OnlineTimeoutComponent::Show()
    {
        SetState("visible");
        mbActive = true;
    }

    // Transin: publish "transin", then mark the component shown.
    void OnlineTimeoutComponent::Transin()
    {
        SetState("transin");
        mbActive = true;
    }

    // Transout: publish "invisible", then clear the shown flag.
    void OnlineTimeoutComponent::Transout()
    {
        SetState("invisible");
        mbActive = false;
    }
}
