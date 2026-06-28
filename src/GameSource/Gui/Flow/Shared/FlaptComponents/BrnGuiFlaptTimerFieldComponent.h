#ifndef BRN_GUI_FLAPT_TIMER_FIELD_COMPONENT_H
#define BRN_GUI_FLAPT_TIMER_FIELD_COMPONENT_H

#include "types.hpp"

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptTimerFieldComponent.h
//
// BrnGui::FlaptTimerFieldComponent -- an apt-driven timer/countdown field that
// colours its text between a "safe" and a "danger" colour according to the
// current time and the configured boundaries. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (members/types pinned from the DecFIGS DWARF at
// references/DecFIGS/dwarfdump/.../BrnGuiFlaptTimerFieldComponent.h, gated on the
// X360 ledger).
//
// This TU (class:BrnGui::FlaptTimerFieldComponent) bodies the two X360-emitted
// query accessors in BrnGuiFlaptTimerFieldComponent.cpp:
//
//   IsTimeSafe      @ 0x824100A8
//   IsTimeDangerous @ 0x82410280
//
// LAYOUT NOTE: the DWARF declares this class as
//   struct FlaptTimerFieldComponent : public BrnFlaptComponent { ... }
// with, after the vptr + base, a TextFieldRef mTimerTextField and three Vector4
// colours (mv4SafeColour / mv4DangerColour / mv4CurrentColour) before the scalar
// members below. BrnFlaptComponent has no reconstructed header yet (and is not in
// the X360 func ledger), so -- per the project's padding-recovery rule (and the
// BrnFlaptFileInstance.h precedent) -- that whole leading region (vptr + base +
// mTimerTextField + the three Vector4 colours) is modeled as one opaque sized
// padding buffer so the named scalar members below land at their X360-attested
// displacements. All member access is BY NAME; there are no raw-offset hacks.
// Grow this header additively (real base, TextFieldRef/Vector4 colours, the
// remaining setters) as those member TUs and the BrnFlaptComponent base land.
//
// Attested offsets (this group, from the asm of the two accessors):
//   +0x60  mfSafeColourBoundary        (lfs ...,0x60(this))
//   +0x64  mfDangerColourBoundary      (lfs ...,0x64(this))
//   +0x68  mfOneOverBoundaryDifference (DWARF; not read here -- placed so the
//                                       following members keep their offsets)
//   +0x6C  mfCurrentTime               (lfs ...,0x6C(this))
//   +0x70  meCountingMode              (lwz r11,0x70(this))
// ============================================================================

namespace BrnGui
{
    struct FlaptTimerFieldComponent
    {
        // BrnGuiFlaptTimerFieldComponent.h:52 (DWARF) -- the timer's count
        // direction. The asm selects on this: COUNTING_DOWN(1) and COUNTING_UP(0)
        // each take a thresholded branch; any other value short-circuits.
        enum ETimerMode
        {
            E_TIMER_MODE_COUNTING_UP           = 0,
            E_TIMER_MODE_COUNTING_DOWN         = 1,
            E_TIMER_MODE_COUNTING_NOT_COUNTING = 2,
            E_TIMER_MODE_COUNTING_COUNT        = 3,
        };

        // 0x824100A8 -- is the current time in the "safe" colour band?
        //   COUNTING_DOWN: mfCurrentTime >  mfSafeColourBoundary
        //   COUNTING_UP  : mfCurrentTime <  mfSafeColourBoundary
        //   not counting : always true
        // The X360 dev-asserts the boundary ordering (danger vs safe) for the
        // active mode first.
        bool IsTimeSafe();

        // 0x82410280 -- is the current time in the "danger" colour band?
        //   COUNTING_DOWN: mfCurrentTime <  mfDangerColourBoundary
        //   COUNTING_UP  : mfCurrentTime >  mfDangerColourBoundary
        //   not counting : always false
        // Same boundary-ordering dev-assert as IsTimeSafe.
        bool IsTimeDangerous();

        // +0x00 : vptr + BrnFlaptComponent base + TextFieldRef mTimerTextField +
        // Vector4 mv4SafeColour/mv4DangerColour/mv4CurrentColour. Honest opaque
        // padding (see LAYOUT NOTE) so the named members below sit at their
        // attested offsets.
        u8 mauPadBase[0x60];                  // +0x00..0x5F

        f32 mfSafeColourBoundary;             // +0x60
        f32 mfDangerColourBoundary;           // +0x64
        f32 mfOneOverBoundaryDifference;      // +0x68
        f32 mfCurrentTime;                    // +0x6C
        ETimerMode meCountingMode;            // +0x70
    };
}

#endif // BRN_GUI_FLAPT_TIMER_FIELD_COMPONENT_H
