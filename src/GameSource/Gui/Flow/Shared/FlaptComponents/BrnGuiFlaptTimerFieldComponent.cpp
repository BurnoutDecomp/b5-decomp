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
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptTimerFieldComponent.cpp"
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"             // BrnFlapt::FileRef::FindComponent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"   // MovieClipInstance::ResetTimeline
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h" // AttachToTextFieldComponent
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

namespace BrnGui
{
    // DWARF cpp:28 (decl h:134): the child clip holding the timer's text field.
    const char FlaptTimerFieldComponent::macTimerTextFieldName[13] = "TimerText_mc";

    // SetLocalisedText format selector the X360 passes when refreshing the timer text
    // (li r5, 2). Documented external-API integer (the ParameterFormatType enum value).
    static const s32 KI_TIMER_TEXT_FORMAT = 2;

    // @ 0x8241C810 -- initialise the timer field. Default boundaries 15/5 are ARTIST's
    // literal-pool floats (0x820047C4 == 15.0f, 0x8204C54C == 5.0f).
    void FlaptTimerFieldComponent::Construct(const void* /*lpUnused1*/,
                                             CgsGui::StateInterface* lpStateInterface,
                                             ETimerMode leCountingMode,
                                             const void* /*lpUnused2*/)
    {
        BrnFlaptComponent::Construct(lpStateInterface);
        mTimerTextField.SetInvalid();

        mv4SafeColour.SetZero();
        meCountingMode = leCountingMode;
        mv4DangerColour.SetZero();
        mv4CurrentColour.SetZero();

        SetSafeColours(255, 255, 255);
        SetDangerColours(255, 0, 0);

        if (meCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            SetBoundaries(15.0f, 5.0f);
        }
        else if (meCountingMode == E_TIMER_MODE_COUNTING_UP)
        {
            SetBoundaries(5.0f, 15.0f);
        }
        else
        {
            SetBoundaries(0.0f, 0.0f);
        }

        mfCurrentTime = 0.0f;
    }

    // @ 0x82427CB8 -- bind this component's clip (the base Prepare, no parent prefix:
    // its h:133 assert and the MovieClipRef::ResetTimeline h:272 assert are inlined),
    // then attach the timer's text field and prime it.
    void FlaptTimerFieldComponent::Prepare(const char* lacTargetMovieclip,
                                           const char* lacName,
                                           const BrnFlapt::FileRef& lFile)
    {
        CGS_ASSERT_AT(lacName != 0, "lacName", CGS_ARTIST_FILE, 91);

        BrnFlaptComponent::Prepare(lacName, lFile, NULL);

        mTimerTextField = AttachToTextFieldComponent(lacTargetMovieclip,
                                   macTimerTextFieldName, lacName, lFile);

        mTimerTextField.SetColour(mv4CurrentColour);
        mTimerTextField.SetLocalisedText(mfCurrentTime, KI_TIMER_TEXT_FORMAT);
    }

    // @ 0x82427DE0 -- set the current time, recompute the band colour, and refresh
    // the bound text field.
    void FlaptTimerFieldComponent::SetTime(f32 lfTime)
    {
        mfCurrentTime = lfTime;
        CalculateColour();

        if (mTimerTextField.IsValid())
        {
            mTimerTextField.SetColour(mv4CurrentColour);
            mTimerTextField.SetLocalisedText(lfTime, KI_TIMER_TEXT_FORMAT);   // the parameter (fmr f1,f31 @0x82427E2C)
        }
    }

    // @ 0x8241C998 -- lerp mv4CurrentColour between the danger and safe colours by the
    // clamped time ratio for the active mode.
    void FlaptTimerFieldComponent::CalculateColour()
    {
        if (meCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            CGS_ASSERT_AT(mfOneOverBoundaryDifference != 0.0f,
                       "mfOneOverBoundaryDifference != 0.0f", CGS_ARTIST_FILE, 145);

            // t = 1 at the safe boundary, 0 at the danger boundary.
            f32 lfRatio = (mfCurrentTime - mfDangerColourBoundary)
                        * mfOneOverBoundaryDifference;
            if (lfRatio < 0.0f) { lfRatio = 0.0f; }
            if (lfRatio > 1.0f) { lfRatio = 1.0f; }

            mv4CurrentColour.mV.x = mv4DangerColour.mV.x + (mv4SafeColour.mV.x - mv4DangerColour.mV.x) * lfRatio;
            mv4CurrentColour.mV.y = mv4DangerColour.mV.y + (mv4SafeColour.mV.y - mv4DangerColour.mV.y) * lfRatio;
            mv4CurrentColour.mV.z = mv4DangerColour.mV.z + (mv4SafeColour.mV.z - mv4DangerColour.mV.z) * lfRatio;
            mv4CurrentColour.mV.w = mv4DangerColour.mV.w + (mv4SafeColour.mV.w - mv4DangerColour.mV.w) * lfRatio;
        }
        else if (meCountingMode == E_TIMER_MODE_COUNTING_UP)
        {
            CGS_ASSERT_AT(mfOneOverBoundaryDifference != 0.0f,
                       "mfOneOverBoundaryDifference != 0.0f", CGS_ARTIST_FILE, 155);

            // t = 1 at the danger boundary, 0 at the safe boundary.
            f32 lfRatio = (mfCurrentTime - mfSafeColourBoundary)
                        * mfOneOverBoundaryDifference;
            if (lfRatio < 0.0f) { lfRatio = 0.0f; }
            if (lfRatio > 1.0f) { lfRatio = 1.0f; }

            mv4CurrentColour.mV.x = mv4SafeColour.mV.x + (mv4DangerColour.mV.x - mv4SafeColour.mV.x) * lfRatio;
            mv4CurrentColour.mV.y = mv4SafeColour.mV.y + (mv4DangerColour.mV.y - mv4SafeColour.mV.y) * lfRatio;
            mv4CurrentColour.mV.z = mv4SafeColour.mV.z + (mv4DangerColour.mV.z - mv4SafeColour.mV.z) * lfRatio;
            mv4CurrentColour.mV.w = mv4SafeColour.mV.w + (mv4DangerColour.mV.w - mv4SafeColour.mV.w) * lfRatio;
        }
        else
        {
            // Not actively counting -> hold the safe colour.
            mv4CurrentColour = mv4SafeColour;
        }
    }

    // SetBoundaries, IsTimeSafe and IsTimeDangerous are header-inline (DWARF h:170/321/349).
    // SetSafeColours / SetDangerColours / SetCountingMode are header-inline (DWARF h:204/225/244).
}
