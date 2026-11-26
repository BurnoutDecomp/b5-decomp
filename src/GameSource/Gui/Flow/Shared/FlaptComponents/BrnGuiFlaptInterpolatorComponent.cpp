// ===================================================================================
// BrnGui::FlaptInterpolatorComponent -- apt-driven colour/scale tween component
//   GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptInterpolatorComponent.cpp
//
//   FlaptInterpolatorComponent::Construct        @ 0x8241CFB8
//   FlaptInterpolatorComponent::Prepare          @ 0x8241D060
//   FlaptInterpolatorComponent::SetInterpValues  @ 0x8241D0D0
//   FlaptInterpolatorComponent::SetProportion    @ 0x8242DC90
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. All are non-static members (asm:
// r3 = this). Member access is BY NAME throughout; the X360 Begin/Fire/End dev-assert
// triplets fold into CGS_ASSERT(cond,"msg") per the module house style.
//
// The component owns a second "controlled" movie clip (mControlledMovieClip, distinct
// from the base mAptRef) plus a start/end colour pair and a start/end scale pair.
// SetInterpValues binds the controlled clip and records those endpoints; SetProportion
// drives the controlled clip's colour scale and size scale to lerp(start, end, t) as
// the proportion t runs 0..1. The X360 emitted the per-lane lerp as a
// vsubfp + broadcast + vmaddfp on the 16-byte Vector4/Vector2 registers; it is
// de-optimised here into ordinary named-component math (start + (end - start) * t).
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptInterpolatorComponent.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptInterpolatorComponent.cpp"
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"   // BrnFlapt::FileRef
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT

namespace BrnGui
{
    // The "unset"/invalid proportion sentinel the X360 loads from flt_820037C8 (-1.0f)
    // and stores into mfCurrentProportion in Construct/Prepare, so the first real
    // SetProportion always differs and drives the clip.
    const f32 FlaptInterpolatorComponent::KF_INVALID_PROPORTION = -1.0f;

    // @ 0x8241CFB8 -- bind the state interface through the base, clear the controlled
    // clip handle, zero both colour and both scale endpoints, set the proportion to the
    // invalid sentinel and clear the values-set flag.
    void FlaptInterpolatorComponent::Construct(const void* /*lpDEBUGName*/,
                                               CgsGui::StateInterface* lpStateInterface,
                                               const void* /*lpcParentName*/)
    {
        // The header-inline base Construct (its assert is BrnGuiFlaptComponent.h:113), then the
        // controlled clip's SetInvalid, the proportion sentinel, the flag and the four zeroed
        // endpoints (ARTIST 0x8241CFDC..0x8241D044).
        BrnFlaptComponent::Construct(lpStateInterface);
        mControlledMovieClip.SetInvalid();
        mfCurrentProportion = KF_INVALID_PROPORTION;
        mbSetInterpValues   = false;
        mv4StartColour.SetZero();
        mv4EndColour.SetZero();
        mv2StartScale.SetZero();
        mv2EndScale.SetZero();
    }

    // @ 0x8241D060 -- bind the component's own clip through the base, then reset the
    // current proportion to the invalid sentinel so the next SetProportion re-drives it.
    void FlaptInterpolatorComponent::Prepare(const char* lacName,
                                             const BrnFlapt::FileRef& lFile,
                                             const char* lacParentName)
    {
        CGS_ASSERT_AT(lacName != 0,
            "lacName",
                CGS_ARTIST_FILE, 76 /* ARTIST 0x8241D08C */);


        BrnFlaptComponent::Prepare(lacName, lFile, lacParentName);
        mfCurrentProportion = KF_INVALID_PROPORTION;
    }

    // @ 0x8241D0D0 -- resolve the named child clip (under the base clip's parent) as the
    // controlled clip, record the start/end colour & scale endpoints, and flag the
    // interpolation values as set.
    void FlaptInterpolatorComponent::SetInterpValues(const char* lpControlledMovieClipName,
                                                     Vector4 lv4StartColour,
                                                     Vector4 lv4EndColour,
                                                     Vector2 lv2StartScale,
                                                     Vector2 lv2EndScale)
    {
        CGS_ASSERT_AT(mAptRef.mpMovieClipInst != 0, "mMovieClipRef.IsValid()", CGS_ARTIST_FILE, 99 /* ARTIST 0x8241D124 */);

        // Locate the named child under the base clip's parent and adopt it as the
        // controlled clip. DWARF cpp:102 local lParent, initialised from GetParent (ARTIST
        // builds it in place at 0x50 and passes that address, 0x8241D130..0x8241D13C).
        BrnFlapt::MovieClipRef lParent = mAptRef.GetParent();
        mControlledMovieClip = lParent.FindChildMovieClip(lpControlledMovieClipName);

        // Record the tween endpoints (X360: stvx128 of the four vector args).
        mv4StartColour = lv4StartColour;
        mv4EndColour   = lv4EndColour;
        mv2StartScale  = lv2StartScale;
        mv2EndScale    = lv2EndScale;

        mbSetInterpValues = true;
    }

    // @ 0x8242DC90 -- set the tween parameter (asserted within [0,1]); when it changes,
    // drive the controlled clip's colour scale and size scale to the per-lane
    // lerp(start, end, proportion) of each endpoint pair.
    void FlaptInterpolatorComponent::SetProportion(f32 lfNewProportion)
    {
        CGS_ASSERT_AT((0.0f <= lfNewProportion) && (1.0f >= lfNewProportion),
                   "( 0.0f <= lfNewProportion ) && ( 1.0f >= lfNewProportion )", CGS_ARTIST_FILE, 127);
        CGS_ASSERT_AT(mbSetInterpValues, "mbSetInterpValues", CGS_ARTIST_FILE, 128);
        CGS_ASSERT_AT(mControlledMovieClip.mpMovieClipInst != 0,
                   "mControlledMovieClip.IsValid()", CGS_ARTIST_FILE, 129);

        if (lfNewProportion != mfCurrentProportion)
        {
            mfCurrentProportion = lfNewProportion;

            __vector4 lScalar = __vspltisw(0);
            lScalar.x = lfNewProportion;
            const __vector4 lT = __vspltw(lScalar, 0);
            Vector4 lv4Colour(__vmaddfp(__vsubfp(mv4EndColour.GetVector(), mv4StartColour.GetVector()), lT, mv4StartColour.GetVector()));
            mControlledMovieClip.SetColourScale(lv4Colour);

            Vector2 lv2Scale;
            lv2Scale.mV = __vmaddfp(__vsubfp(mv2EndScale.mV, mv2StartScale.mV), lT, mv2StartScale.mV);
            mControlledMovieClip.SetSizeScale(lv2Scale);
        }
    }
}

#undef CGS_ARTIST_FILE
