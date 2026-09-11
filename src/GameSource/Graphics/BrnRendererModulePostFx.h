#ifndef BRN_RENDERER_MODULE_POST_FX_H
#define BRN_RENDERER_MODULE_POST_FX_H

#include "types.hpp"

// ==================================================================================================
// [FLAG PC bring-up] THE APPLY BLOCK'S SEAM -- the declarations BrnRendererModule.cpp needs to reach
// BrnRendererModule::Render @0x8240BFA8's own effects-frame -> BrnPostFx apply block (pseudocode
// lines 964..1260, asm 0x8240D700-0x8240DD4C).
//
// WHY THE BLOCK IS NOT IN BrnRendererModule.cpp, WHERE THE CONSOLE PUT IT. The apply block writes
// BrnPostFx's bloom / vignette / depth-of-field / B4-blur state, so its translation unit must
// include GameSource/Graphics/PostFx/BrnPostFx.h -- which BrnRendererModule.cpp could not, because
// BrnRendererModule.h used to define a placeholder `class EA::Jobs::Job` that redefined the real one
// BrnPostFx.h needs for m_blendJob. The placeholder is gone: the renderer module includes the real
// job.h and constructs its thirty-two embedded jobs itself.
//
// So the split is now VESTIGIAL, and it was PHYSICAL ONLY to begin with: every statement in
// BrnRendererModulePostFx.cpp is a reconstruction of Render's own block, at Render's own position,
// and Render calls each function from exactly the point the console executes it.
//
// DELETE-WHEN: the block moves back into Render, as the console's single translation unit had it --
// a code move, at which point this header, its .cpp and BrnPostFxPCComposite.h retire together.
// ==================================================================================================

namespace BrnGraphics { class EffectsArbitrator; }

// The three per-frame values Render reads off the LAYER-0 INTERNAL BrnEffectsFrame early
// (asm 0x8240C290-0x8240C314) and carries in stack slots for the rest of the function.
struct BrnRendererPostFxFrameBytes
{
    // var_CD0 -- mMotionBlurData.mbIsActive (frame +0x1E0, `lbz r9, 0x1E0(r9)` @0x8240C2DC).
    // Consumed by BeginRenderAntiAliased's lbClearStencil (r5 @0x8240CDB0) and by
    // BrnPostFx::Render's lbMotionBlurEnabled (r7 @0x8240DE18).
    bool mbMotionBlurActive;

    // var_CCE -- (u8)(mMotionBlurData.mfWorldBlurAmount * 255.0f) (frame +0x1DC, `lfs f13,
    // 0x1DC(r8)` * flt_82010C20, `fctiwz` then the low byte @0x8240C2C0-0x8240C300).
    // Consumed by BeginRenderAntiAliased (r6 @0x8240CDAC) and ResolveMSAA (r5 @0x8240D5B4).
    u8 mu8WorldBlurStencil;

    // var_CCF -- the same quantisation of mfCarsBlurAmount (frame +0x1D8, @0x8240C2F0-0x8240C314).
    // Consumed by the CAR passes' stencil reference (r3 @0x8240CED0 / @0x8240D344), which this
    // build does not reconstruct yet; carried so the read is complete and greppable.
    u8 mu8CarsBlurStencil;
};

// Render @0x8240C290-0x8240C314 -- the three effects-frame bytes above, read ONCE per frame right
// after SortDispatchLists and reused by every later consumer. Writes all-zero (the values a
// Constructed frame yields: MotionBlurData::Construct @0x821F84E8 sets both amounts 0.0f and
// mbIsActive false) when the arbitrator has not been Constructed yet on this PC build.
void BrnRendererReadPostFxFrameBytes(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                     BrnRendererPostFxFrameBytes* lpOut);

// Render @0x8240C69C-0x8240C798 (pseudocode 505..533) -- THE COLOUR-CUBE (3D LUT) TINT BLOCK.
// Evaluates the arbitrator's five (colour cube, weight) tint sources, sets or clears BrnPostFx's
// E_FX_TINT bit -- the bool that becomes the composite's TINT3D permutation lane -- publishes the
// sources into m_colourCubes / m_tintFactors and schedules the EA::Jobs blend with
// BrnPostFx::BeginTintBlend.
//
// ⚠ CALL IT AT THE CONSOLE'S POSITION, WHICH IS NOT THE APPLY BLOCK'S. The console runs this EARLY
// in Render -- inside the PerfMonCpu bracket at 0x8240C698/0x8240C79C, immediately before
// shadow::Device::ResetShadowing() and the three global texture binds -- so that the blend job runs
// concurrently with the shadow map and the world passes. BrnPostFx::Render drains it (m_processTint
// -> Job::WaitOn -> Tint::EndBlendJob) before the composite samples the tint volume at s3.
//   lbEffectsAllowed == the same v296 the apply block takes.
void BrnRendererBeginPostFxTintBlend(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                     bool lbEffectsAllowed);

// Render @0x8240D700-0x8240DC50 (pseudocode 964..1232) -- THE APPLY BLOCK. For each of bloom /
// vignette / depth-of-field / B4-blur: test the layer-0 internal frame's bool, call the arbitrator's
// evaluator, set or clear the effect's m_enabledFx bit through BrnPostFx's own mutator, and -- when
// active -- publish the evaluated data into BrnPostFx's state block and hand it to the effect.
// Called from inside Render's `if (mbRenderPostFX)` gate, at the console's position.
//   lbEffectsAllowed == v296 == DispatchThreadInputBuffer::GetCalibrationUnfriendlyEnablePostFx().
void BrnRendererApplyEffectsFrameToPostFx(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                          bool lbEffectsAllowed);

// Render @0x8240DC80-0x8240DCBC (pseudocode 1237..1243) -- the 2D tint colour handed to
// BrnPostFx::Render in v1. ALWAYS writes four floats: the console zeroes the vector first
// (`vspltisw v0, 0` / `stvx128` @0x8240DC80-0x8240DC8C) and only overwrites it when effects are
// allowed AND the layer-0 internal frame's mbUseTint2d is set.
void BrnRendererEvalPostFxTint2dColour(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                       bool lbEffectsAllowed,
                                       f32* lpafColourXYZW);

// Render @0x8240DD04-0x8240DD4C -- MotionBlurState::Update on BrnPostFx's own motion-blur state.
// Returns true when the console's five-argument call was actually made.
//
// THE PARAMETER IS NOW TYPED (it was `const void*` while ParticleRenderData had no layout). It is
// a NESTED type -- BrnParticle::ParticleModule::ParticleRenderData -- and C++ cannot
// forward-declare one of those, so this header includes its home. Passing a null pointer -- which
// BrnRendererModule.cpp still does, because nothing in this tree PRODUCES the render data -- is the
// honest "no producer" signal, handled by the body with one reported line and not a crash.
#include "GameSource/Effects/Particles/ParticleModule.h"   // BrnParticle::ParticleModule::ParticleRenderData

bool BrnRendererUpdatePostFxMotionBlur(
    const BrnGraphics::EffectsArbitrator* lpArbitrator,
    const BrnParticle::ParticleModule::ParticleRenderData* lpParticleRenderData);

// [FLAG PC bring-up diagnostic] one sampled line proving the chain base frame -> Eval* -> BrnPostFx.
// Emits at most six lines, one every 500th call. DELETE with the bring-up.
void BrnRendererLogPostFxEffectState();

#endif // BRN_RENDERER_MODULE_POST_FX_H
