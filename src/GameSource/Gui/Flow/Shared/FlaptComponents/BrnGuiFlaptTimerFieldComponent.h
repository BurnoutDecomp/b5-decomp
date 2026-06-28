#ifndef BRN_GUI_FLAPT_TIMER_FIELD_COMPONENT_H
#define BRN_GUI_FLAPT_TIMER_FIELD_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                               // Vector4
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"    // BrnFlapt::MovieClipRef (embedded)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"    // BrnFlapt::TextFieldRef (embedded)

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptTimerFieldComponent.h
//
// BrnGui::FlaptTimerFieldComponent -- an apt-driven timer/countdown field that
// colours its text by lerping between a "safe" and a "danger" colour according to
// the current time and the configured boundaries. Derives from BrnFlaptComponent.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (members/types pinned from the DecFIGS
// DWARF at references/DecFIGS/dwarfdump/.../BrnGuiFlaptTimerFieldComponent.h, gated
// on the X360 ledger).
//
// This TU bodies, in BrnGuiFlaptTimerFieldComponent.cpp:
//   Construct        @ 0x8241C810    Prepare          @ 0x82427CB8
//   SetTime          @ 0x82427DE0    CalculateColour  @ 0x8241C998
//   IsTimeSafe       @ 0x824100A8    IsTimeDangerous  @ 0x82410280   (already done)
//
// LAYOUT NOTE (X360-attested offsets, from the Construct / Prepare / SetTime /
// CalculateColour asm). The DWARF declares this as
//   struct FlaptTimerFieldComponent : public BrnFlaptComponent { vptr; ... }
// but the X360 build inserts a 12-byte gap between the leading vptr (+0x00) and the
// BrnFlaptComponent base subobject, which the PS3 DWARF does not show (a build-delta
// the layout-recovery rule resolves: trust the X360 asm offsets, model the
// unexplained lead as opaque padding). The base subobject therefore begins at +0x10
// (mpStateInterface @ +0x10, mAptRef @ +0x14) -- so this class is modelled flat
// (named members at their attested displacements) rather than via C++ inheritance,
// which would misplace the base at +0x04. All member access is BY NAME.
//
//   +0x00  vptr + 12-byte X360 lead gap (opaque)
//   +0x10  mpStateInterface  (stw SI, 0x10(this))
//   +0x14  mAptRef           (MovieClipRef; mpMovieClipInst @ +0x14)
//   +0x1C  mTimerTextField   (TextFieldRef; 3 words +0x1C/+0x20/+0x24)
//   +0x30  mv4SafeColour     (Vector4; lvx/stvx r31,0x30)
//   +0x40  mv4DangerColour   (Vector4; lvx/stvx r31,0x40)
//   +0x50  mv4CurrentColour  (Vector4; stvx r31,0x50)
//   +0x60  mfSafeColourBoundary
//   +0x64  mfDangerColourBoundary
//   +0x68  mfOneOverBoundaryDifference
//   +0x6C  mfCurrentTime
//   +0x70  meCountingMode
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
    struct FlaptTimerFieldComponent
    {
        // BrnGuiFlaptTimerFieldComponent.h:52 (DWARF) -- the timer's count
        // direction. The asm selects on this: COUNTING_DOWN(1) and COUNTING_UP(0)
        // each take a thresholded branch; NOT_COUNTING(2) short-circuits.
        enum ETimerMode
        {
            E_TIMER_MODE_COUNTING_UP           = 0,
            E_TIMER_MODE_COUNTING_DOWN         = 1,
            E_TIMER_MODE_COUNTING_NOT_COUNTING = 2,
            E_TIMER_MODE_COUNTING_COUNT        = 3,
        };

        // Construct @ 0x8241C810 -- store the state interface, zero the refs and
        // colours, install the counting mode, and pick default boundaries for that
        // mode (UP: safe=15,danger=5; DOWN: safe=5,danger=15; NOT_COUNTING: 0/0).
        // DWARF shape Construct(const void*, StateInterface*, ETimerMode, const void*);
        // the X360 body reads only the state interface (r5) and counting mode (r6) --
        // the two const void* params (a debug name and a parent prefix) are passed in
        // the convention but unused by this body.
        void Construct(const void* lpDEBUGName,
                       CgsGui::StateInterface* lpStateInterface,
                       ETimerMode leCountingMode,
                       const void* lpcParentName);

        // Prepare @ 0x82427CB8 -- resolve the named component out of lFlaptFile,
        // bind+reset its movie clip, attach the "TimerText_mc" sub-component's named
        // text field into mTimerTextField, then push the current colour + time to it.
        void Prepare(const char* lpcTextFieldName,
                     const char* lacName,
                     const BrnFlapt::FileRef& lFlaptFile);

        // SetBoundaries @ X360 (ledger-attested) -- set the safe/danger boundaries
        // and recompute 1/(safe-danger). Bodied in its own (sibling) TU; declared
        // here so Construct can call it.
        void SetBoundaries(f32 lfSafeColourBoundary, f32 lfDangerColourBoundary);

        // The following setters are X360-attested for this class but bodied in their
        // own sibling TUs; declared here so they resolve against this single home.
        void SetSafeColours(u8 luRed, u8 luGreen, u8 luBlue);
        void SetDangerColours(u8 luRed, u8 luGreen, u8 luBlue);
        void SetCountingMode(ETimerMode leCountingMode);
        void IncrementTime(f32 lfDelta);
        void DecrementsTime(f32 lfDelta);
        void HideTime();
        f32  GetCurrentTime() const;

        // SetTime @ 0x82427DE0 -- set the current time, recompute the band colour,
        // and (if bound) push the colour + formatted value to the text field.
        void SetTime(f32 lfTime);

        // 0x824100A8 / 0x82410280 -- band queries (bodied; see the .cpp).
        bool IsTimeSafe();
        bool IsTimeDangerous();

    private:
        // CalculateColour @ 0x8241C998 -- lerp mv4CurrentColour between the safe and
        // danger colours by the clamped time ratio for the active mode.
        void CalculateColour();

        // ---- layout (see LAYOUT NOTE; flat by attested offset) -----------------
        u8                     mauPadHead[0x10];            // +0x00 vptr + X360 lead gap
        CgsGui::StateInterface* mpStateInterface;            // +0x10
        BrnFlapt::MovieClipRef  mAptRef;                     // +0x14
        BrnFlapt::TextFieldRef  mTimerTextField;             // +0x1C
        u8                     mauPad28[0x08];              // +0x28 align Vector4 to +0x30
        Vector4                mv4SafeColour;               // +0x30
        Vector4                mv4DangerColour;             // +0x40
        Vector4                mv4CurrentColour;            // +0x50
        f32                    mfSafeColourBoundary;        // +0x60
        f32                    mfDangerColourBoundary;      // +0x64
        f32                    mfOneOverBoundaryDifference; // +0x68
        f32                    mfCurrentTime;               // +0x6C
        ETimerMode             meCountingMode;              // +0x70
    };
}

#endif // BRN_GUI_FLAPT_TIMER_FIELD_COMPONENT_H
