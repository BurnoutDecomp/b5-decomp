#ifndef BRN_GUI_FLAPT_TIMER_FIELD_COMPONENT_H
#define BRN_GUI_FLAPT_TIMER_FIELD_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                               // Vector4
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"   // base
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"    // BrnFlapt::TextFieldRef (embedded)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT_STREAM_AT (header-inline asserts)
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptTimerFieldComponent.h
//
// BrnGui::FlaptTimerFieldComponent -- an apt-driven timer/countdown field that
// colours its text by lerping between a "safe" and a "danger" colour according to
// the current time and the configured boundaries. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; members/types from the DecFIGS DWARF
// (BrnGuiFlaptTimerFieldComponent.h), gated on the X360 ledger.
//
// Bodied in BrnGuiFlaptTimerFieldComponent.cpp: Construct @ 0x8241C810, Prepare
// @ 0x82427CB8, SetTime @ 0x82427DE0, CalculateColour @ 0x8241C998. Header-inline (DWARF
// h:170 / h:321 / h:349, their asserts name this header): SetBoundaries (ARTIST COMDAT
// 0x8240FED0), IsTimeSafe (0x824100A8), IsTimeDangerous (0x82410280).
//
// LAYOUT (DWARF h:49: `struct FlaptTimerFieldComponent : public BrnFlaptComponent` with
// its own vptr and virtual Construct/Prepare). The base is not polymorphic, so the vfptr
// leads and the XDK pads it to the class's 16-byte (Vector4) alignment: BrnFlaptComponent
// sits at +0x10 (ARTIST Construct 0x8241C82C `addi r30,r31,0x10` ahead of the inlined
// base Construct), as in FlaptRoadSignIconComponent. RoadRuleComponent::Construct
// 0x8242ADF8 calls Construct through the vptr.
//
//   +0x00  vfptr
//   +0x10  BrnFlaptComponent (mpStateInterface, mAptRef)
//   +0x1C  mTimerTextField   (TextFieldRef; 3 words +0x1C/+0x20/+0x24)
//   +0x30  mv4SafeColour, +0x40 mv4DangerColour, +0x50 mv4CurrentColour
//   +0x60  mfSafeColourBoundary .. +0x6C mfCurrentTime, +0x70 meCountingMode
// ============================================================================

namespace CgsGui
{
    struct StateInterface;   // stored by-pointer only (see BrnGuiFlaptComponent.h)
}

namespace BrnFlapt
{
    struct FileRef;          // by const-reference in Prepare; full type via the .cpp
}

namespace BrnGui
{
    struct FlaptTimerFieldComponent : public BrnFlaptComponent
    {
        // BrnGuiFlaptTimerFieldComponent.h:52 (DWARF) -- the timer's count direction.
        enum ETimerMode
        {
            E_TIMER_MODE_COUNTING_UP           = 0,
            E_TIMER_MODE_COUNTING_DOWN         = 1,
            E_TIMER_MODE_COUNTING_NOT_COUNTING = 2,
            E_TIMER_MODE_COUNTING_COUNT        = 3,
        };

        // Construct @ 0x8241C810 (DWARF cpp:47, virtual) -- base Construct, invalidate the
        // text field, default colours (safe white, danger red), install the counting mode
        // and its default boundaries.
        virtual void Construct(const void* lpUnused1,
                               CgsGui::StateInterface* lpStateInterface,
                               ETimerMode leCountingMode,
                               const void* lpUnused2);

        // Prepare @ 0x82427CB8 (DWARF cpp:89, virtual) -- bind this component's clip, then
        // attach the "TimerText_mc" child's named text field and prime it.
        virtual void Prepare(const char* lacTargetMovieclip,
                             const char* lacName,
                             const BrnFlapt::FileRef& lFile);

        // SetBoundaries (DWARF h:170, below) -- set the safe/danger boundaries and
        // recompute 1/(safe-danger).
        void SetBoundaries(f32 lfSafeColourBoundary, f32 lfDangerColourBoundary);

        // Header-inline (DWARF h:204 / h:225 / h:244): ARTIST has no out-of-line copy and
        // inlines them at every site (RoadRuleComponent::Construct @0x8242AE78 sets the
        // colour lanes, RefreshBestData @0x82422734 / @0x8242274C stores the mode word).
        void SetSafeColours(u8 luRed, u8 luGreen, u8 luBlue)
        {
            mv4SafeColour.SetX(static_cast<f32>(luRed)   * (1.0f / 255.0f));
            mv4SafeColour.SetY(static_cast<f32>(luGreen) * (1.0f / 255.0f));
            mv4SafeColour.SetZ(static_cast<f32>(luBlue)  * (1.0f / 255.0f));
        }
        void SetDangerColours(u8 luRed, u8 luGreen, u8 luBlue)
        {
            mv4DangerColour.SetX(static_cast<f32>(luRed)   * (1.0f / 255.0f));
            mv4DangerColour.SetY(static_cast<f32>(luGreen) * (1.0f / 255.0f));
            mv4DangerColour.SetZ(static_cast<f32>(luBlue)  * (1.0f / 255.0f));
        }
        void SetCountingMode(ETimerMode leNewCountingMode)
        {
            meCountingMode = leNewCountingMode;
        }
        void IncrementTime(f32 lfDelta);
        void DecrementsTime(f32 lfDelta);
        void HideTime();
        f32  GetCurrentTime() const;

        // SetTime @ 0x82427DE0 -- set the current time, recompute the band colour,
        // and (if bound) push the colour + formatted value to the text field.
        void SetTime(f32 lfTime);

        // Band queries (DWARF h:321 / h:349, below).
        bool IsTimeSafe();
        bool IsTimeDangerous();

    private:
        // CalculateColour @ 0x8241C998 -- lerp mv4CurrentColour between the safe and
        // danger colours by the clamped time ratio for the active mode.
        void CalculateColour();

        // DWARF h:134 / cpp:28: the child clip Prepare attaches the text field from.
        static const char macTimerTextFieldName[13];

        BrnFlapt::TextFieldRef mTimerTextField;             // h:136 (+0x1C)
        Vector4                mv4SafeColour;               // h:138 (+0x30)
        Vector4                mv4DangerColour;             // h:140 (+0x40)
        Vector4                mv4CurrentColour;            // h:142 (+0x50)
        f32                    mfSafeColourBoundary;        // h:144 (+0x60)
        f32                    mfDangerColourBoundary;      // h:145
        f32                    mfOneOverBoundaryDifference; // h:146
        f32                    mfCurrentTime;               // h:147 (+0x6C)
        ETimerMode             meCountingMode;              // h:149 (+0x70)
    };

    // ------------------------------------------------------------------------
    // Header-inline definitions. The asserts carry the lines ARTIST bakes into its
    // COMDAT copies (li r5,0xB1/0xB6/0x145/0x14A/0x161/0x166) and stream the members
    // (danger first) through the float operator<<.
    // ------------------------------------------------------------------------
#define BRN_FLAPT_TIMER_FIELD_H_FILE "..\\..\\..\\GameSource\\Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptTimerFieldComponent.h"

    inline void FlaptTimerFieldComponent::SetBoundaries(f32 lfSafeColourBoundary, f32 lfDangerColourBoundary)
    {
        mfSafeColourBoundary   = lfSafeColourBoundary;
        mfDangerColourBoundary = lfDangerColourBoundary;

        if (meCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            CGS_ASSERT_STREAM_AT(lfDangerColourBoundary < lfSafeColourBoundary,
                "Danger colour boundry ( " << mfDangerColourBoundary
                << " ) isn't less than the safe colour boundary ( " << mfSafeColourBoundary << " )\n",
                BRN_FLAPT_TIMER_FIELD_H_FILE, 177);
            mfOneOverBoundaryDifference = 1.0f / (mfSafeColourBoundary - mfDangerColourBoundary);
        }
        else if (meCountingMode == E_TIMER_MODE_COUNTING_UP)
        {
            CGS_ASSERT_STREAM_AT(lfDangerColourBoundary > lfSafeColourBoundary,
                "Danger colour boundry ( " << mfDangerColourBoundary
                << " ) isn't greater than the safe colour boundary ( " << mfSafeColourBoundary << " )\n",
                BRN_FLAPT_TIMER_FIELD_H_FILE, 182);
            mfOneOverBoundaryDifference = 1.0f / (mfDangerColourBoundary - mfSafeColourBoundary);
        }
        else
        {
            mfOneOverBoundaryDifference = 0.0f;
        }
    }

    inline bool FlaptTimerFieldComponent::IsTimeSafe()
    {
        if (meCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            CGS_ASSERT_STREAM_AT(mfDangerColourBoundary < mfSafeColourBoundary,
                "Danger colour boundry ( " << mfDangerColourBoundary
                << " ) isn't less than the safe colour boundary ( " << mfSafeColourBoundary << " )\n",
                BRN_FLAPT_TIMER_FIELD_H_FILE, 325);
            return mfCurrentTime > mfSafeColourBoundary;
        }
        else if (meCountingMode == E_TIMER_MODE_COUNTING_UP)
        {
            CGS_ASSERT_STREAM_AT(mfDangerColourBoundary > mfSafeColourBoundary,
                "Danger colour boundry ( " << mfDangerColourBoundary
                << " ) isn't greater than the safe colour boundary ( " << mfSafeColourBoundary << " )\n",
                BRN_FLAPT_TIMER_FIELD_H_FILE, 330);
            return mfCurrentTime < mfSafeColourBoundary;
        }
        return true;
    }

    inline bool FlaptTimerFieldComponent::IsTimeDangerous()
    {
        if (meCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            CGS_ASSERT_STREAM_AT(mfDangerColourBoundary < mfSafeColourBoundary,
                "Danger colour boundry ( " << mfDangerColourBoundary
                << " ) isn't less than the safe colour boundary ( " << mfSafeColourBoundary << " )\n",
                BRN_FLAPT_TIMER_FIELD_H_FILE, 353);
            return mfCurrentTime < mfDangerColourBoundary;
        }
        else if (meCountingMode == E_TIMER_MODE_COUNTING_UP)
        {
            CGS_ASSERT_STREAM_AT(mfDangerColourBoundary > mfSafeColourBoundary,
                "Danger colour boundry ( " << mfDangerColourBoundary
                << " ) isn't greater than the safe colour boundary ( " << mfSafeColourBoundary << " )\n",
                BRN_FLAPT_TIMER_FIELD_H_FILE, 358);
            return mfCurrentTime > mfDangerColourBoundary;
        }
        return false;
    }

#undef BRN_FLAPT_TIMER_FIELD_H_FILE
}

#endif // BRN_GUI_FLAPT_TIMER_FIELD_COMPONENT_H
