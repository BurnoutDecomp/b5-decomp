#ifndef BRN_GUI_FLAPT_INTERPOLATOR_COMPONENT_H
#define BRN_GUI_FLAPT_INTERPOLATOR_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // Vector2, Vector4
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"  // BrnFlaptComponent (base)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                // MovieClipRef (embedded by value)

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptInterpolatorComponent.h
//
// BrnGui::FlaptInterpolatorComponent -- an apt-driven tween component. It owns a
// second "controlled" movie clip (separate from the base mAptRef) plus a start/end
// colour and a start/end scale; SetProportion(t) lerps the controlled clip's colour
// scale and size scale from the start pair to the end pair as t runs 0..1.
//
// Derives from BrnFlaptComponent (the non-polymorphic apt-component base owning
// mpStateInterface + mAptRef). Reconstructed from BURNOUT_X360_ARTIST.XEX; member
// names/types/shape taken from the DecFIGS DWARF (BrnGuiFlaptInterpolatorComponent.h)
// and gated on the X360 ledger.
//
// LAYOUT (X360-attested by the Construct @ 0x8241CFB8 / Prepare @ 0x8241D060 /
// SetInterpValues @ 0x8241D0D0 / SetProportion @ 0x8242DC90 asm). This is a
// NON-polymorphic subclass, so the BrnFlaptComponent base subobject sits at +0x00:
//   +0x00  mpStateInterface       (base; `stw r30, 0(this)` in Construct)
//   +0x04  mAptRef                (base MovieClipRef: +0x04 inst, +0x08 transform)
//   +0x0C  mControlledMovieClip   (MovieClipRef: +0x0C inst, +0x10 transform; the
//                                   SetInterpValues child store is `stw …, 0xC/0x10`)
//   +0x14  (pad to 16-byte SIMD alignment)
//   +0x20  mv4StartColour         (Vector4; `stvx128 …, 0x20`)
//   +0x30  mv4EndColour           (Vector4; `stvx128 …, 0x30`)
//   +0x40  mv2StartScale          (Vector2; `stvx128 …, 0x40`)
//   +0x50  mv2EndScale            (Vector2; `stvx128 …, 0x50`)
//   +0x60  mfCurrentProportion    (f32;  `stfs f0, 0x60` -- init KF_INVALID_PROPORTION)
//   +0x64  mbSetInterpValues      (bool; `stb r11, 0x64`)
// sizeof == 0x68.
//
// All member access is BY NAME; there are no raw-offset hacks.
// ============================================================================

namespace CgsGui
{
    struct StateInterface;   // stored by-pointer in the base; full type elsewhere.
}

namespace BrnFlapt
{
    struct FileRef;          // used by const-reference in Prepare; .cpp pulls the full type.
}

namespace BrnGui
{
    class FlaptInterpolatorComponent : public BrnFlaptComponent
    {
    public:
        // Construct @ 0x8241CFB8 -- bind the state interface through the base, clear
        // the controlled clip handle, zero the start/end colour & scale pairs, set the
        // current proportion to the "invalid" sentinel and clear the values-set flag.
        void Construct(const void* lpDEBUGName,
                       CgsGui::StateInterface* lpStateInterface,
                       const void* lpcParentName);

        // Prepare @ 0x8241D060 -- forward up to BrnFlaptComponent::Prepare to bind the
        // component's own clip, then reset mfCurrentProportion to the invalid sentinel.
        void Prepare(const char* lacName,
                     const BrnFlapt::FileRef& lFile,
                     const char* lacParentName);

        // SetInterpValues @ 0x8241D0D0 -- resolve the named child clip (under mAptRef's
        // parent) as the controlled clip, record the start/end colour & scale endpoints,
        // and mark the interpolation values set.
        void SetInterpValues(const char* lpControlledMovieClipName,
                             Vector4 lv4StartColour,
                             Vector4 lv4EndColour,
                             Vector2 lv2StartScale,
                             Vector2 lv2EndScale);

        // SetProportion @ 0x8242DC90 -- set the tween parameter (0..1) and, when it
        // changes, drive the controlled clip's colour scale and size scale to the
        // lerp(start, end, proportion) of each pair.
        void SetProportion(f32 lfNewProportion);

    protected:
        // The invalid/"unset" proportion sentinel (-1.0f) stored by Construct/Prepare.
        static const f32 KF_INVALID_PROPORTION;

        BrnFlapt::MovieClipRef mControlledMovieClip;   // +0x0C
        Vector4                mv4StartColour;          // +0x20
        Vector4                mv4EndColour;            // +0x30
        Vector2                mv2StartScale;           // +0x40
        Vector2                mv2EndScale;             // +0x50
        f32                    mfCurrentProportion;     // +0x60
        bool                   mbSetInterpValues;       // +0x64
    };
}

#endif // BRN_GUI_FLAPT_INTERPOLATOR_COMPONENT_H
