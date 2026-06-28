// ===================================================================================
// BrnGui::FlaptTimerFieldComponent  -- apt timer/countdown field component
//   GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptTimerFieldComponent.cpp
//
//   Construct        @ 0x8241C810
//   Prepare          @ 0x82427CB8
//   SetTime          @ 0x82427DE0
//   CalculateColour  @ 0x8241C998
//   IsTimeSafe       @ 0x824100A8   (kept; previously committed + reviewed)
//   IsTimeDangerous  @ 0x82410280   (kept; previously committed + reviewed)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. All are non-static members (asm:
// r3 = this). Member access is BY NAME throughout (the flat attested layout lives in
// the header). The X360 Begin/StrStream/Fire/End dev-assert sequences fold into
// CGS_ASSERT(cond,"msg") per the module house style (file/line via __FILE__/__LINE__).
//
// CalculateColour de-optimisation: the X360 emitted the colour update as raw VMX
// (lvx/stvx/vmaddfp) plus an fsel-pair clamp. Semantically it computes a per-lane
// linear interpolation of mv4CurrentColour between the danger and safe colours by a
// time ratio clamped to [0,1]; reconstructed here as that clamped lerp over the named
// Vector4 lanes (semantic parity, not byte parity).
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptTimerFieldComponent.h"

#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"             // BrnFlapt::FileRef::FindComponent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"   // MovieClipInstance::ResetTimeline
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h" // AttachToTextFieldComponent
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

namespace BrnGui
{
    // Default mode boundaries the X360 installs in Construct (flt_820047C4 == 15.0f,
    // flt_8204C54C == 5.0f). For a countdown the safe band is the high time and the
    // danger band the low time; counting up reverses that.
    static const f32 KF_DEFAULT_OUTER_BOUNDARY = 15.0f;
    static const f32 KF_DEFAULT_INNER_BOUNDARY = 5.0f;

    // SetLocalisedText format selector the X360 passes when refreshing the timer text
    // (li r5, 2). Documented external-API integer (the ParameterFormatType enum value).
    static const s32 KI_TIMER_TEXT_FORMAT = 2;

    // @ 0x8241C810 -- initialise the timer field.
    void FlaptTimerFieldComponent::Construct(const void* /*lpDEBUGName*/,
                                             CgsGui::StateInterface* lpStateInterface,
                                             ETimerMode leCountingMode,
                                             const void* /*lpcParentName*/)
    {
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");

        // Base init (inlined by the X360): bind the state interface and clear the
        // movie-clip handle + the text-field handle.
        mpStateInterface = lpStateInterface;
        mAptRef.mpMovieClipInst = 0;
        mAptRef.mpTransform     = 0;
        mTimerTextField.mpTextFieldInstance = 0;
        mTimerTextField.mpParentMovie       = 0;
        mTimerTextField.mpTransform         = 0;

        // Default colours: safe = white (1,1,1), danger = red (1,0,0), current = 0
        // (the X360 builds these into the three colour lanes after zeroing them).
        mv4SafeColour.SetZero();
        mv4SafeColour.x = 1.0f;
        mv4SafeColour.y = 1.0f;
        mv4SafeColour.z = 1.0f;

        mv4DangerColour.SetZero();
        mv4DangerColour.x = 1.0f;

        mv4CurrentColour.SetZero();

        meCountingMode = leCountingMode;

        // Pick the default safe/danger boundaries for the active mode. Counting down,
        // the safe band is the high time (15) and danger the low time (5); counting up
        // reverses that (safe=5, danger=15). NOT_COUNTING leaves both at 0.
        f32 lfSafeBoundary;
        f32 lfDangerBoundary;
        if (leCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            lfSafeBoundary   = KF_DEFAULT_OUTER_BOUNDARY;   // 15.0
            lfDangerBoundary = KF_DEFAULT_INNER_BOUNDARY;   // 5.0
        }
        else if (leCountingMode == E_TIMER_MODE_COUNTING_UP)
        {
            lfSafeBoundary   = KF_DEFAULT_INNER_BOUNDARY;   // 5.0
            lfDangerBoundary = KF_DEFAULT_OUTER_BOUNDARY;   // 15.0
        }
        else
        {
            lfSafeBoundary   = 0.0f;
            lfDangerBoundary = 0.0f;
        }
        SetBoundaries(lfSafeBoundary, lfDangerBoundary);

        mfCurrentTime = 0.0f;
    }

    // @ 0x82427CB8 -- resolve, bind and reset the component, then attach the timer
    // text field and prime it.
    void FlaptTimerFieldComponent::Prepare(const char* lpcTextFieldName,
                                           const char* lacName,
                                           const BrnFlapt::FileRef& lFlaptFile)
    {
        CGS_ASSERT(lacName != 0, "lacName");
        CGS_ASSERT(lacName != 0, "lacName != NULL");

        // Bind this component's own movie clip and reset its timeline.
        lFlaptFile.FindComponent(&mAptRef, lacName);
        CGS_ASSERT(mAptRef.mpMovieClipInst != 0, "mpMovieClipInst");
        mAptRef.mpMovieClipInst->ResetTimeline();

        // Dig the "TimerText_mc" sub-component's named text field out of the file.
        AttachToTextFieldComponent(&mTimerTextField, lpcTextFieldName,
                                   "TimerText_mc", lacName, lFlaptFile);

        // Push the current band colour + formatted time to the field.
        mTimerTextField.SetColour(mv4CurrentColour);
        mTimerTextField.SetLocalisedText(mfCurrentTime, KI_TIMER_TEXT_FORMAT);
    }

    // @ 0x82427DE0 -- set the current time, recompute the band colour, and refresh
    // the bound text field.
    void FlaptTimerFieldComponent::SetTime(f32 lfTime)
    {
        mfCurrentTime = lfTime;
        CalculateColour();

        if (mTimerTextField.mpTextFieldInstance != 0)
        {
            mTimerTextField.SetColour(mv4CurrentColour);
            mTimerTextField.SetLocalisedText(mfCurrentTime, KI_TIMER_TEXT_FORMAT);
        }
    }

    // @ 0x8241C998 -- lerp mv4CurrentColour between the danger and safe colours by the
    // clamped time ratio for the active mode.
    void FlaptTimerFieldComponent::CalculateColour()
    {
        if (meCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            CGS_ASSERT(mfOneOverBoundaryDifference != 0.0f,
                       "mfOneOverBoundaryDifference != 0.0f");

            // t = 1 at the safe boundary, 0 at the danger boundary.
            f32 lfRatio = (mfCurrentTime - mfDangerColourBoundary)
                        * mfOneOverBoundaryDifference;
            if (lfRatio < 0.0f) { lfRatio = 0.0f; }
            if (lfRatio > 1.0f) { lfRatio = 1.0f; }

            mv4CurrentColour.x = mv4DangerColour.x + (mv4SafeColour.x - mv4DangerColour.x) * lfRatio;
            mv4CurrentColour.y = mv4DangerColour.y + (mv4SafeColour.y - mv4DangerColour.y) * lfRatio;
            mv4CurrentColour.z = mv4DangerColour.z + (mv4SafeColour.z - mv4DangerColour.z) * lfRatio;
            mv4CurrentColour.w = mv4DangerColour.w + (mv4SafeColour.w - mv4DangerColour.w) * lfRatio;
        }
        else if (meCountingMode == E_TIMER_MODE_COUNTING_UP)
        {
            CGS_ASSERT(mfOneOverBoundaryDifference != 0.0f,
                       "mfOneOverBoundaryDifference != 0.0f");

            // t = 1 at the danger boundary, 0 at the safe boundary.
            f32 lfRatio = (mfCurrentTime - mfSafeColourBoundary)
                        * mfOneOverBoundaryDifference;
            if (lfRatio < 0.0f) { lfRatio = 0.0f; }
            if (lfRatio > 1.0f) { lfRatio = 1.0f; }

            mv4CurrentColour.x = mv4SafeColour.x + (mv4DangerColour.x - mv4SafeColour.x) * lfRatio;
            mv4CurrentColour.y = mv4SafeColour.y + (mv4DangerColour.y - mv4SafeColour.y) * lfRatio;
            mv4CurrentColour.z = mv4SafeColour.z + (mv4DangerColour.z - mv4SafeColour.z) * lfRatio;
            mv4CurrentColour.w = mv4SafeColour.w + (mv4DangerColour.w - mv4SafeColour.w) * lfRatio;
        }
        else
        {
            // Not actively counting -> hold the safe colour.
            mv4CurrentColour = mv4SafeColour;
        }
    }

    // @ 0x824100A8 -- true while the current time sits in the safe colour band.
    bool FlaptTimerFieldComponent::IsTimeSafe()
    {
        if (meCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            // Counting down: safe boundary is reached before the danger boundary,
            // so the danger boundary must be the lower value.
            CGS_ASSERT(mfDangerColourBoundary < mfSafeColourBoundary,
                       "Danger colour boundry isn't less than the safe colour boundary\n");
            return mfCurrentTime > mfSafeColourBoundary;
        }

        if (meCountingMode != E_TIMER_MODE_COUNTING_UP)
        {
            // Not actively counting -> always considered safe.
            return true;
        }

        // Counting up: the safe boundary is the lower value (danger above it).
        CGS_ASSERT(mfDangerColourBoundary > mfSafeColourBoundary,
                   "Danger colour boundry isn't greater than the safe colour boundary\n");
        return mfCurrentTime < mfSafeColourBoundary;
    }

    // @ 0x82410280 -- true while the current time sits in the danger colour band.
    bool FlaptTimerFieldComponent::IsTimeDangerous()
    {
        if (meCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            CGS_ASSERT(mfDangerColourBoundary < mfSafeColourBoundary,
                       "Danger colour boundry isn't less than the safe colour boundary\n");
            return mfCurrentTime < mfDangerColourBoundary;
        }

        if (meCountingMode != E_TIMER_MODE_COUNTING_UP)
        {
            // Not actively counting -> never considered dangerous.
            return false;
        }

        CGS_ASSERT(mfDangerColourBoundary > mfSafeColourBoundary,
                   "Danger colour boundry isn't greater than the safe colour boundary\n");
        return mfCurrentTime > mfDangerColourBoundary;
    }
}
