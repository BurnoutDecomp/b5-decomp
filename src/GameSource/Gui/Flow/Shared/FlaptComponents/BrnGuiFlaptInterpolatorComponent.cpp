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

#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"   // BrnFlapt::FileRef
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");

        // Base init (the X360 inlined BrnFlaptComponent::Construct + MovieClipRef
        // SetInvalid): bind the state channel and invalidate the base clip handle.
        mpStateInterface        = lpStateInterface;
        mAptRef.mpMovieClipInst = 0;
        mAptRef.mpTransform     = 0;

        // Invalidate the controlled clip handle.
        mControlledMovieClip.mpMovieClipInst = 0;
        mControlledMovieClip.mpTransform     = 0;

        // Zero the colour and scale endpoints (X360: stvx128 of a zero vector).
        mv4StartColour.SetZero();
        mv4EndColour.SetZero();
        mv2StartScale.SetZero();
        mv2EndScale.SetZero();

        mfCurrentProportion = KF_INVALID_PROPORTION;
        mbSetInterpValues   = false;
    }

    // @ 0x8241D060 -- bind the component's own clip through the base, then reset the
    // current proportion to the invalid sentinel so the next SetProportion re-drives it.
    void FlaptInterpolatorComponent::Prepare(const char* lacName,
                                             const BrnFlapt::FileRef& lFile,
                                             const char* lacParentName)
    {
        CGS_ASSERT(lacName != 0, "lacName");

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
        CGS_ASSERT(mAptRef.mpMovieClipInst != 0, "mMovieClipRef.IsValid()");

        // Locate the named child under the base clip's parent and adopt it as the
        // controlled clip.
        BrnFlapt::MovieClipRef lParent;
        mAptRef.GetParent(&lParent);
        lParent.FindChildMovieClip(&mControlledMovieClip, lpControlledMovieClipName);

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
        CGS_ASSERT((0.0f <= lfNewProportion) && (1.0f >= lfNewProportion),
                   "( 0.0f <= lfNewProportion ) && ( 1.0f >= lfNewProportion )");
        CGS_ASSERT(mbSetInterpValues, "mbSetInterpValues");
        CGS_ASSERT(mControlledMovieClip.mpMovieClipInst != 0,
                   "mControlledMovieClip.IsValid()");

        if (lfNewProportion != mfCurrentProportion)
        {
            mfCurrentProportion = lfNewProportion;

            // Colour scale = lerp(start, end, t) per RGBA lane.
            Vector4 lv4Colour;
            lv4Colour.x = mv4StartColour.x + (mv4EndColour.x - mv4StartColour.x) * lfNewProportion;
            lv4Colour.y = mv4StartColour.y + (mv4EndColour.y - mv4StartColour.y) * lfNewProportion;
            lv4Colour.z = mv4StartColour.z + (mv4EndColour.z - mv4StartColour.z) * lfNewProportion;
            lv4Colour.w = mv4StartColour.w + (mv4EndColour.w - mv4StartColour.w) * lfNewProportion;
            mControlledMovieClip.SetColourScale(lv4Colour);

            // Size scale = lerp(start, end, t) per lane.
            Vector2 lv2Scale;
            lv2Scale.x = mv2StartScale.x + (mv2EndScale.x - mv2StartScale.x) * lfNewProportion;
            lv2Scale.y = mv2StartScale.y + (mv2EndScale.y - mv2StartScale.y) * lfNewProportion;
            lv2Scale.z = mv2StartScale.z + (mv2EndScale.z - mv2StartScale.z) * lfNewProportion;
            lv2Scale.w = mv2StartScale.w + (mv2EndScale.w - mv2StartScale.w) * lfNewProportion;
            mControlledMovieClip.SetSizeScale(lv2Scale);
        }
    }
}
