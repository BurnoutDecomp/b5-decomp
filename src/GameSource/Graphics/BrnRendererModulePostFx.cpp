#include "GameSource/Graphics/BrnRendererModulePostFx.h"

#include "GameSource/Graphics/BrnEffectsArbitrator.h"      // BrnGraphics::EffectsArbitrator
#include "SharedClasses/Graphics/BrnEffectsData.h"         // BrnEffectsFrame + the five data blocks
#include "GameSource/Graphics/PostFx/BrnPostFx.h"          // msPostFx + every setter/getter below
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::WriteToLog (the diag line)

#include <cstdio>    // snprintf (the diag line)

// ==================================================================================================
// BrnRendererModule::Render @0x8240BFA8 -- THE EFFECTS-FRAME -> BrnPostFx APPLY BLOCK.
//
// This file holds Render's OWN block (pseudocode lines 964..1260, asm 0x8240C290-0x8240C314 and
// 0x8240D700-0x8240DD4C) verbatim in behaviour, split out of BrnRendererModule.cpp for the single
// translation-unit reason spelled out in BrnRendererModulePostFx.h (the EA::Jobs::Job placeholder).
// Render calls each function below from the point the console executes it.
//
// WHAT THE BLOCK IS. The console's post-fx effects are driven entirely by data: the effects module,
// the world's environment manager and the GUI's fx-event arbitrator each write a BrnEffectsFrame per
// layer; BrnGraphics::EffectsArbitrator blends the layers into one evaluated block per effect; and
// this block turns each evaluated block into (a) an m_enabledFx bit and (b) a state publish into the
// render-engine effect object. Nothing here decides anything -- every branch is a frame byte and
// every constant is read off the image.
//
// THE FRAME IS READ AT THE LAYER-0 *INTERNAL* SLOT, ALWAYS. Every read below is
//   `496 * mu8EffectsFrameInternal + mapaEffectsFrames[0]`
// (`lbz r5, 0xC(r30)` / `lwz r11, 0(r30)` / `mulli r11, r11, 0x1F0` with r30 = this+0x480, repeated
// once per effect at 0x8240D704 / 0x8240D7B0 / 0x8240D8D0 / 0x8240D99C / 0x8240DC98). 0x1F0 == 496 is
// the GUEST stride and is NOT reproduced here: the frame is reached through the arbitrator's
// GetInternalEffectsFrame(layer, slot) accessor, which indexes the host array with the HOST stride.
//
// THE Eval* RETURN VALUES ARE DISCARDED, and that is the asm, not a simplification. At all four call
// sites the sequence is `bl sub_823F9xxx` immediately followed by `li r11, 1` (0x8240D728/D72C,
// 0x8240D7D8/D7E0, 0x8240D8F8/D900, 0x8240D9C4/D9CC): the "did this effect evaluate" flag is set
// UNCONDITIONALLY after the call, so what actually gates the effect is the frame's own bool, tested
// before the call. Consuming the return value here would be a behaviour the binary does not have.
//
// (Hex-Rays renders those calls as `sub_823F9AA8(&out, this + 1152)` -- out in r3, the arbitrator in
// r4 -- which is the large-struct-return convention, not an argument order to copy. The seam this
// file codes against is BrnEffectsArbitrator.h's `bool EvalBloom(BrnEffects::BloomData&) const`, per
// the DWARF; whichever of the two the arbitrator group lands, the CALL SITE is the same and the
// result is unused.)
// ==================================================================================================

// --------------------------------------------------------------------------------------------------
// The two bloom scale globals (X360 gfBloomLuminanceScale @0x82F307E0 == 1.0f,
// gfBloomThresholdScale @0x82F307E4 == 1.0f -- conductor idat dump, scratch/postfx_step4_bloom/
// DATA_NOTE.md section 2).
//
// THEY ARE GLOBALS, NOT CONSTANTS, and they are homed here rather than folded to 1.0f because the
// image proves they are tweakables: BrnWorld::EnvironmentSettings::EnvironmentManager::Construct
// @0x827CA408 takes the ADDRESS of each (`lis r11, gfBloomLuminanceScale@ha` / `addi r29, r11,
// gfBloomLuminanceScale@l` @0x827CA87C-0x827CA884, and the same pair for gfBloomThresholdScale
// @0x827CA8D0-0x827CA8D8 and gfSpecularScale @0x827CA914) and registers it with the debug component
// alongside gfSpecularScale. Those two functions are the ONLY references in the whole export set:
//   $ grep -rl "gfBloomThresholdScale\|gfBloomLuminanceScale" .ida-exports/BURNOUT_X360_ARTIST.XEX/
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x827CA408.json   <- EnvironmentManager::Construct (address-of)
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8240BFA8.json   <- BrnRendererModule::Render     (the reads below)
// so this file is their only READER, no other translation unit in the tree names them, and there is
// no split-brain in homing them here. WHEN the environment manager's debug registration is
// reconstructed, declare `extern f32 gfBloomLuminanceScale;` there rather than minting a second copy.
// --------------------------------------------------------------------------------------------------
f32 gfBloomLuminanceScale = 1.0f;   // X360 0x82F307E0
f32 gfBloomThresholdScale = 1.0f;   // X360 0x82F307E4

namespace
{
    // Degrees -> radians. flt_8203B6D4, addressed as `(flt_8203B6D4 - 0x8203B710)(r21)` at
    // 0x8240D87C (the vignette angle) and 0x8240DC44 (the blur angle); Hex-Rays prints both uses as
    // the literal 0.017453292 (pseudocode lines 1050 and 1230).
    const f32 KF_DEGREES_TO_RADIANS = 0.017453292f;

    // The depth-of-field PROJECTION planes, which Render supplies rather than the effects frame:
    // flt_82004014 -> DepthOfField::State::m_projNearPlane (`stfs f0, 0x3E0(r20)` @0x8240D988) and
    // flt_82009E10 -> m_projFarPlane (`stfs f0, 0x3E4(r20)` @0x8240D990). Hex-Rays prints the pair
    // as `_R20[248] = 0.1; _R20[249] = 1000.0;` (pseudocode lines 1092-1093).
    const f32 KF_DOF_PROJECTION_NEAR_PLANE = 0.1f;
    const f32 KF_DOF_PROJECTION_FAR_PLANE  = 1000.0f;

    // The B4-blur speed normaliser: mfSpeedMPH * flt_82048914, clamped at 1.0f
    // (`lfs f0, flt_82048914@l(r11)` / `fmuls f12, f13, f0` / `fcmpu`+`fmr f12, f31` @0x8240DA50-
    // 0x8240DA60; Hex-Rays prints it as `v182 = (*(...+464) * 0.0060606059)` with the `> 1.0` clamp,
    // pseudocode lines 1122-1124). 464 == 0x1D0 == BrnEffectsFrame::mfSpeedMPH.
    const f32 KF_BLUR_SPEED_MPH_SCALE = 0.0060606059f;   // flt_82048914  (== 1/165)

    // The two yaw-rate scalars the blur centres and amounts are biased by. Both are splatted from a
    // one-float stack block and multiplied by splat(mAngularVelocity.y):
    //   flt_82009B70 == -0.125f  -> the CENTRE bias   (stored to two stack blocks @0x8240DA88 /
    //                               @0x8240DA90, Hex-Rays `v328[0] = -0.125; v329[0] = -0.125;`)
    //   flt_82003F40 ==  0.25f   -> the AMOUNT bias   (@0x8240DAB8, Hex-Rays `v302 = 0.25;`)
    const f32 KF_BLUR_CENTRE_YAW_SCALE = -0.125f;   // flt_82009B70
    const f32 KF_BLUR_AMOUNT_YAW_SCALE =  0.25f;    // flt_82003F40

    // The motion-blur STENCIL quantiser: both blur amounts are multiplied by flt_82010C20 and
    // truncated to a byte (`fmuls f13, f13, f0` / `fctiwz` / `stfiwx` / `stb` @0x8240C2C0-0x8240C314).
    // ATTESTED at 255.0f -- conductor idat dump, DATA_NOTE.md section 2 -- and corroborated inside
    // the pseudocode, which prints the same multiply as `(*(... + 476) * 255.0)` (line 400).
    const f32 KF_MOTION_BLUR_STENCIL_SCALE = 255.0f;   // flt_82010C20

    const f32 KF_ZERO = 0.0f;
    const f32 KF_ONE  = 1.0f;

    // fsel-pair clamp to [0,1] (`fsel f0, -x, f27(0.0), x` then `fsubs f11, f31(1.0), f0` /
    // `fsel f0, f11, f0, f31` @0x8240DB88-0x8240DBA8). De-optimised to the arithmetic it expresses,
    // per AGENTS.md's strength-reduction rule.
    f32 Clamp01(f32 lfValue)
    {
        if (lfValue < KF_ZERO)
            return KF_ZERO;
        if (lfValue > KF_ONE)
            return KF_ONE;
        return lfValue;
    }

    // `vandc vD, vS, v13` with v13 == splat(0x80000000) (`vspltisw v13, -1` + `vslw v13, v13, v13`
    // @0x8240DA70 / @0x8240DA8C) -- a sign-bit clear, i.e. fabs (asm 0x8240DB48).
    f32 AbsValue(f32 lfValue)
    {
        return (lfValue < KF_ZERO) ? -lfValue : lfValue;
    }

    // The [FLAG PC bring-up] gate every function in this file shares: on the console the arbitrator
    // is Constructed by BrnRendererModule::Construct @0x8240A778 line 126 and can never be absent,
    // so the console body has no such test. On PC it is Constructed lazily (see
    // EnsureEffectsArbitratorBringUp in BrnRendererModule.cpp) and is null until then. Keeping the
    // test HERE, at the seam, is the same choice EnsurePostFxSceneTargets makes for the render-target
    // pool: the reconstructed body stays untestful and the PC failure degrades instead of crashing.
    // DELETE with the bring-up.
    const BrnEffectsFrame* GetBaseInternalFrame(const BrnGraphics::EffectsArbitrator* lpArbitrator)
    {
        if (lpArbitrator == 0)
            return 0;
        return &lpArbitrator->GetInternalEffectsFrame(
            BrnGraphics::EffectsArbitrator::KU_EFFECTS_LAYER_BASE, 0u);
    }

    // The last frame's evaluated state, for the sampled diagnostic below only.
    struct DiagState
    {
        bool mbBloom;
        bool mbVignette;
        bool mbDepthOfField;
        bool mbBlur;
        bool mbTint2d;
        f32  mfBloomLuminance;
        f32  mfBloomThreshold;
        f32  mafBloomColour[3];
    };
    DiagState gDiag = { false, false, false, false, false, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f } };
}

// ==================================================================================================
// Render @0x8240C290-0x8240C314 -- the three bytes read off the layer-0 internal frame once per frame.
//
// The console reloads mapaEffectsFrames[0] and mu8EffectsFrameInternal THREE times here (0x8240C2A0,
// 0x8240C2AC, 0x8240C2C8) and recomputes 496*internal three times, which is register-allocation
// noise: all three address the same frame. One reference is the reconstruction of it.
// ==================================================================================================
void BrnRendererReadPostFxFrameBytes(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                     BrnRendererPostFxFrameBytes* lpOut)
{
    if (lpOut == 0)
        return;

    lpOut->mbMotionBlurActive   = false;
    lpOut->mu8WorldBlurStencil  = 0u;
    lpOut->mu8CarsBlurStencil   = 0u;

    const BrnEffectsFrame* const lpFrame = GetBaseInternalFrame(lpArbitrator);
    if (lpFrame == 0)
    {
        // [FLAG PC bring-up] no arbitrator yet. The zeros above are NOT a chosen floor: they are what
        // a Constructed frame yields -- BrnEffectsFrame::Construct calls mMotionBlurData.Construct(),
        // and MotionBlurData::Construct @0x821F84E8 sets mfCarsBlurAmount = mfWorldBlurAmount = 0.0f
        // and mbIsActive = false -- which is what the console reads on any frame with no motion-blur
        // event posted.
        return;
    }

    const BrnDirector::Camera::MotionBlurData& lrMotionBlur = lpFrame->GetMotionBlurData();

    lpOut->mbMotionBlurActive = lrMotionBlur.mbIsActive;
    lpOut->mu8WorldBlurStencil = static_cast<u8>(
        static_cast<s32>(lrMotionBlur.mfWorldBlurAmount * KF_MOTION_BLUR_STENCIL_SCALE));
    lpOut->mu8CarsBlurStencil = static_cast<u8>(
        static_cast<s32>(lrMotionBlur.mfCarsBlurAmount * KF_MOTION_BLUR_STENCIL_SCALE));
}

// ==================================================================================================
// Render @0x8240D700-0x8240DC50 -- THE APPLY BLOCK.
// ==================================================================================================
void BrnRendererApplyEffectsFrameToPostFx(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                          bool lbEffectsAllowed)
{
    const BrnEffectsFrame* const lpFrame = GetBaseInternalFrame(lpArbitrator);
    if (lpFrame == 0)
        return;   // [FLAG PC bring-up] see GetBaseInternalFrame.

    const BrnEffectsFrame& lrFrame = *lpFrame;

    // ---- BLOOM (asm 0x8240D704-0x8240D7AC) --------------------------------------------------------
    {
        bool lbEvaluated = false;
        // The out-block is a RAW STACK SLOT on the console (`addi r3, r1, var_C70` with no prior
        // store), and it is left raw here too: it is only READ inside the `if (lbBloomActive)` arm,
        // which cannot be entered unless the Eval above ran and filled it. Seeding it with
        // BloomData::Construct() would be behaviour the binary does not have -- and would drag
        // BloomData::kv4DefScale (SharedClasses/Graphics/BrnEffectsData.cpp) into this TU's link.
        BrnEffects::BloomData lBloom;
        if (lrFrame.GetUseBloom())
        {
            (void)lpArbitrator->EvalBloom(lBloom);   // return DISCARDED -- see the banner
            lbEvaluated = true;
        }

        // `ori r9, r11, 2` / `rlwinm r9, r11, 0,31,29` on m_enabledFx (this+0x384) @0x8240D758-
        // 0x8240D76C -- i.e. the console INLINED BrnPostFx::SetBloom(const bool&) here. AGENTS.md's
        // inlining-reversal rule puts the call back; the bit is never poked directly.
        const bool lbBloomActive = lbEvaluated && lbEffectsAllowed;
        msPostFx.SetBloom(lbBloomActive);

        if (lbBloomActive)
        {
            // The evaluated block's three members land in BrnPostFxBloomData (BrnPostFx's mBloomData,
            // guest this+0x950):
            //   mv4Scale    -> mColour      (`lvx128 v0, r0, &out+0x10` / `stvx128 v0, r20, 0x950`)
            //   mfThreshold -> mfThreshold * gfBloomThresholdScale  (`stfs f0, 0x960(r20)`)
            //   mfLuminance -> mfLuminance * gfBloomLuminanceScale  (`stfs f0, 0x964(r20)`)
            // Note the ORDER the two scalars are read in: threshold (out+4) first, luminance (out+0)
            // second -- the asm loads var_C6C before var_C70.
            BrnPostFxBloomData* const lpBloomData = msPostFx.GetBloom();
            lpBloomData->mColour     = lBloom.mv4Scale;
            lpBloomData->mfThreshold = lBloom.mfThreshold * gfBloomThresholdScale;
            lpBloomData->mfLuminance = lBloom.mfLuminance * gfBloomLuminanceScale;

            gDiag.mfBloomThreshold  = lpBloomData->mfThreshold;
            gDiag.mfBloomLuminance  = lpBloomData->mfLuminance;
            gDiag.mafBloomColour[0] = lBloom.mv4Scale.x;
            gDiag.mafBloomColour[1] = lBloom.mv4Scale.y;
            gDiag.mafBloomColour[2] = lBloom.mv4Scale.z;
        }
        gDiag.mbBloom = lbBloomActive;
    }

    // ---- VIGNETTE (asm 0x8240D7B0-0x8240D8CC) -----------------------------------------------------
    {
        bool lbEvaluated = false;
        BrnEffects::VignetteData lVignette;   // raw stack slot -- see the bloom arm's note
        if (lrFrame.GetUseVignette())
        {
            (void)lpArbitrator->EvalVignette(lVignette);
            lbEvaluated = true;
        }

        const bool lbVignetteActive = lbEvaluated && lbEffectsAllowed;
        msPostFx.SetVignette(lbVignetteActive);   // `ori r11, r9, 0x10` / `rlwinm r11, r9, 0,28,26`

        if (lbVignetteActive)
        {
            // BrnPostFx's Vignette::State (guest this+0x390, four Vector4s):
            //   m_gradientScalars (+0x390) -- READ-MODIFY-WRITE: the console loads the CURRENT
            //     state vector (`li r11, 0x390` / `lvx128 v11, r20, r11` @0x8240D854-0x8240D85C),
            //     copies it to a stack block, overwrites LANE 0 with mfSharpness (`stfs f0,
            //     var_CC0` @0x8240D870) and LANE 1 with mfAngle * DEG2RAD (`stfs f0, var_CBC`
            //     @0x8240D890), then stores the block back (@0x8240D8BC). Lanes 2 and 3 are
            //     DELIBERATELY PRESERVED -- rw::graphics::postfx::Vignette::SetState reads them, and
            //     zeroing them here would be behaviour the binary does not have.
            //   m_innerColour (+0x3A0) <- mv4InnerColour   (`lvx128 v11, var_BC0` / `stvx128 .. 0x3A0`)
            //   m_outerColour (+0x3B0) <- mv4OuterColour   (`lvx128 v11, var_BB0` / `stvx128 .. 0x3B0`)
            //   m_centerScale (+0x3C0) <- (centre.x, centre.y, amount.x, amount.y), built with four
            //     vrlimi128 inserts: masks 8 and 4 at shift 0 take mv2Centre's lanes 0/1, masks 2 and
            //     1 at shift 2 take mv2Amount's lanes 0/1 (@0x8240D84C-0x8240D86C). All four lanes
            //     are written, so the `lvx128 v0, r20, 0x3C0` that seeds v0 is dead.
            rw::graphics::postfx::Vignette::State* const lpState = msPostFx.GetVignetteState();

            lpState->m_gradientScalars.x = lVignette.mfSharpness;
            lpState->m_gradientScalars.y = lVignette.mfAngle * KF_DEGREES_TO_RADIANS;
            // .z / .w untouched -- see above.

            lpState->m_innerColour = lVignette.mv4InnerColour;
            lpState->m_outerColour = lVignette.mv4OuterColour;

            lpState->m_centerScale.x = lVignette.mv2Centre.x;
            lpState->m_centerScale.y = lVignette.mv2Centre.y;
            lpState->m_centerScale.z = lVignette.mv2Amount.x;
            lpState->m_centerScale.w = lVignette.mv2Amount.y;

            // `lwz r3, 0x5BC(r20)` (m_pfxVignette) / `addi r4, r20, 0x390` (&m_vignetteState) /
            // `bl Vignette__SetState` @0x8240D830-0x8240D8C8. TWO arguments: r5/r6/r7 at the call are
            // Hex-Rays noise -- r5 was last written at 0x8240D7B0 and r6/r7 at 0x8240D6EC/0x8240D678,
            // all BEFORE the intervening `bl sub_823F9DE0`, which clobbers every volatile GPR. The
            // committed declaration agrees: rwgpfxvignette.h `void SetState(const State&)`.
            msPostFx.GetVignette()->SetState(*lpState);
        }
        gDiag.mbVignette = lbVignetteActive;
    }

    // ---- DEPTH OF FIELD (asm 0x8240D8D0-0x8240D998) -----------------------------------------------
    {
        bool lbEvaluated = false;
        BrnEffects::DepthOfFieldData lDepthOfField;   // raw stack slot -- see the bloom arm's note
        if (lrFrame.GetUseDepthOfField())
        {
            (void)lpArbitrator->EvalDepthOfField(lDepthOfField);
            lbEvaluated = true;
        }

        const bool lbDofActive = lbEvaluated && lbEffectsAllowed;
        msPostFx.SetDepthOfField(lbDofActive);   // `ori r11, r11, 1` / `clrrwi r11, r11, 1`

        if (lbDofActive)
        {
            // DepthOfField::State (guest this+0x3D0): the four focal planes come straight off the
            // evaluated block (`stfs` x4 into 0x3D0/0x3D4/0x3D8/0x3DC), the two PROJECTION planes are
            // Render's own constants, and m_dofAmount is the block's fifth float (out+0x10 ->
            // `stfs f0, 0x3E8(r20)`). m_blurRadius (+0x3EC) IS NOT WRITTEN by the console here --
            // reproduced as untouched.
            rw::graphics::postfx::DepthOfField::State* const lpState = msPostFx.GetDofState();
            lpState->m_focalPlanes[0] = lDepthOfField.mfNearPlane;
            lpState->m_focalPlanes[1] = lDepthOfField.mfFocalPlane;
            lpState->m_focalPlanes[2] = lDepthOfField.mfFocalPlane2;
            lpState->m_focalPlanes[3] = lDepthOfField.mfFarPlane;
            lpState->m_projNearPlane  = KF_DOF_PROJECTION_NEAR_PLANE;
            lpState->m_projFarPlane   = KF_DOF_PROJECTION_FAR_PLANE;
            lpState->m_dofAmount      = lDepthOfField.mfDofAmount;

            // `lwz r3, 0x5B8(r20)` (m_pfxDof) / `addi r4, r20, 0x3D0` / `bl DepthOfField__SetState`.
            msPostFx.GetDepthOfField()->SetState(*lpState);
        }
        gDiag.mbDepthOfField = lbDofActive;
    }

    // ---- B4 BLUR (asm 0x8240D99C-0x8240DC50) ------------------------------------------------------
    //
    // The one VMX-heavy arm. Every vector op here is a splat-multiply on ONE scalar --
    // mAngularVelocity.y, the camera's yaw rate (`lvx128 v0, r9, 0x1C0` then `vspltw v0, v0, 1`
    // @0x8240DA38 / @0x8240DA68; frame +0x1C0 is mAngularVelocity and word element 1 is its .y lane)
    // -- so the reconstruction is scalar arithmetic, not a fake vector library.
    {
        bool lbEvaluated = false;
        BrnEffects::BlurData lBlur;   // raw stack slot -- see the bloom arm's note
        if (lrFrame.GetUseBlur())
        {
            (void)lpArbitrator->EvalBlur(lBlur);
            lbEvaluated = true;
        }

        const bool lbBlurActive = lbEvaluated && lbEffectsAllowed;
        msPostFx.SetB4Blur(lbBlurActive);   // `ori r11, r11, 0x40` / `rlwinm r11, r11, 0,26,24`

        if (lbBlurActive)
        {
            const f32 lfYawRate = lrFrame.GetAngularVelocity().y;

            // The normalised speed factor, clamped at 1.0 (see KF_BLUR_SPEED_MPH_SCALE).
            f32 lfSpeedFactor = lrFrame.GetSpeedMPH() * KF_BLUR_SPEED_MPH_SCALE;
            if (lfSpeedFactor > KF_ONE)
                lfSpeedFactor = KF_ONE;

            // The yaw biases. The centres are pushed by yaw * -0.125 and clamped to [0,1]; the blend
            // AMOUNT's x lane is reduced by |yaw * 0.25| (`vandc` then `vsubfp v0, v12, v0`
            // @0x8240DB48-0x8240DB4C, v12 == splat(mv2BlendAmount.x)).
            const f32 lfCentreBias  = lfYawRate * KF_BLUR_CENTRE_YAW_SCALE;
            const f32 lfAmountBias  = AbsValue(lfYawRate * KF_BLUR_AMOUNT_YAW_SCALE);

            rw::graphics::postfx::B4Blur::State* const lpState = msPostFx.GetB4BlurState();

            // +0x3F0 m_blendAmount = { blendAmount.x - |yaw*0.25|, blendAmount.y, 0, 0 }.
            // Lanes 2/3 are EXPLICITLY zeroed (`std r18, 0(&var_CA8)` @0x8240DBEC, r18 == 0).
            lpState->m_blendAmount.x = lBlur.mv2BlendAmount.x - lfAmountBias;
            lpState->m_blendAmount.y = lBlur.mv2BlendAmount.y;
            lpState->m_blendAmount.z = KF_ZERO;
            lpState->m_blendAmount.w = KF_ZERO;

            // +0x400 m_blurAmount = the evaluated mv2BlurAmount, copied as ONE 16-byte vector
            // (`lvx128 v0, r0, &out+0x30` / `stvx128 v0, r20, 0x400` @0x8240DBC0/@0x8240DBE0) -- i.e.
            // all four lanes, including whatever BlurData::SetToBlend left in lanes 2/3. Reproduced
            // as a whole-vector copy rather than two lanes, because that is what the store does.
            lpState->m_blurAmount = lBlur.mv2BlurAmount;

            // +0x410 m_blendCenter = { clamp01(blendCentre.x + yaw*-0.125), blendCentre.y, 0, 0 }
            // (lanes 2/3 zeroed by `std r18, 0(&var_C38)` @0x8240DBAC).
            lpState->m_blendCenter.x = Clamp01(lBlur.mv2BlendCentre.x + lfCentreBias);
            lpState->m_blendCenter.y = lBlur.mv2BlendCentre.y;
            lpState->m_blendCenter.z = KF_ZERO;
            lpState->m_blendCenter.w = KF_ZERO;

            // +0x420 m_blurCenter = { clamp01(blurCentre.x + yaw*-0.125), blurCentre.y, 0, 0 }
            // (lanes 2/3 zeroed by `std r18, 0(&var_CB8)` @0x8240DBB4).
            lpState->m_blurCenter.x = Clamp01(lBlur.mv2BlurCentre.x + lfCentreBias);
            lpState->m_blurCenter.y = lBlur.mv2BlurCentre.y;
            lpState->m_blurCenter.z = KF_ZERO;
            lpState->m_blurCenter.w = KF_ZERO;

            // +0x430 m_blurOpacity  <- mfOpacity                       (`stfs f0, 0x430(r20)`)
            // +0x434 m_blurVelocity <- mfVelocity * speedFactor^2      (`fmuls f13, f0, f12` then
            //                          `fmuls f0, f13, f12` @0x8240DBB8 / @0x8240DBF8 -- the factor
            //                          is applied TWICE, which is the asm and not a typo)
            lpState->m_blurOpacity  = lBlur.mfOpacity;
            lpState->m_blurVelocity = lBlur.mfVelocity * lfSpeedFactor * lfSpeedFactor;

            // +0x438/+0x43C m_blendSharpMUL / m_blendSharpADD, derived from the [-1,1] sharpness.
            // Called HERE, between the velocity store and the noise store, exactly as the asm orders
            // it (`bl B4Blur__State__SetBlendSharpness` @0x8240DC28, with r3 = this+0x3F0 and
            // f1 = mfSharpness loaded @0x8240DBF4).
            lpState->SetBlendSharpness(lBlur.mfSharpness);

            // +0x440 m_blendNoise <- mfNoise                            (`stfs f0, 0x440(r20)`)
            // +0x444 m_blendAngle <- mfAngle * DEG2RAD                   (`stfs f0, 0x444(r20)`)
            lpState->m_blendNoise = lBlur.mfNoise;
            lpState->m_blendAngle = lBlur.mfAngle * KF_DEGREES_TO_RADIANS;

            // [FLAG BLOCKED: rw::graphics::postfx::B4Blur has no way to receive a State]
            // The console finishes the arm with
            //     memcpy(*(this + 0x5C8), this + 0x3F0, 0x60)      (asm 0x8240DC30-0x8240DC50)
            // i.e. it copies the 0x60-byte State it just built straight over B4Blur::m_state, which
            // is that class's FIRST member (rwgpfxb4blur.h:254, `State m_state; // +0x00`). That
            // member is PRIVATE and rwgpfxb4blur.h declares no B4Blur::SetState, so the publish
            // cannot be written here without either a raw-offset poke into a live C++ object (an
            // AGENTS.md audit failure) or an edit to a header this wave does not own.
            // MISSING ITEM, exactly: `void B4Blur::SetState(const State& lrState);` in
            // SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxb4blur.h, defined in the
            // sibling .cpp as `m_state = lrState;`. The exact text is in this pass's REPORT under
            // CROSS-GROUP REQUEST 4. Restore the line below when it lands:
            //     msPostFx.GetB4Blur()->SetState(*lpState);
            //
            // INERT ON THIS BUILD, and that is measured rather than hoped: the B4-blur arm only runs
            // when the layer-0 internal frame's mbUseBlur is set, and the PC bring-up producer of that
            // frame (BrnRendererModule::PCBringUpProduceBaseEffectsFrame) writes mbUseBlur = false,
            // because the console's own producer derives it from a camera flag and two debug-component
            // bytes that are all clear on a no-event frame (DATA_NOTE.md section 3). So this whole
            // block is reconstructed-and-unreached today; it must not be reached until the publish
            // above exists.
        }
        gDiag.mbBlur = lbBlurActive;
    }
}

// ==================================================================================================
// Render @0x8240DC80-0x8240DCBC -- the 2D tint colour.
// ==================================================================================================
void BrnRendererEvalPostFxTint2dColour(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                       bool lbEffectsAllowed,
                                       f32* lpafColourXYZW)
{
    if (lpafColourXYZW == 0)
        return;

    // The unconditional zero first (`vspltisw v0, 0` / `stvx128 v0, r0, &var_CA0`). This is why the
    // composite's tint is a pass-through on a no-event frame -- it is the console's own value, not a
    // fallback.
    lpafColourXYZW[0] = KF_ZERO;
    lpafColourXYZW[1] = KF_ZERO;
    lpafColourXYZW[2] = KF_ZERO;
    lpafColourXYZW[3] = KF_ZERO;

    gDiag.mbTint2d = false;

    // NOTE THE ORDER OF THE TWO CONDITIONS: the console tests `v296` (effects allowed) FIRST
    // (`cmplwi cr6, r25, 0` / `beq` @0x8240DC88-0x8240DC90) and only then loads the frame byte. Every
    // OTHER effect above tests the frame byte first and ANDs the allowed flag afterwards. Preserved.
    if (!lbEffectsAllowed)
        return;

    const BrnEffectsFrame* const lpFrame = GetBaseInternalFrame(lpArbitrator);
    if (lpFrame == 0)
        return;   // [FLAG PC bring-up] see GetBaseInternalFrame.

    if (!lpFrame->GetUseTint2d())
        return;

    // The console evaluates INTO the slot it just zeroed (r3 == &var_CA0 at 0x8240DCB8 is the same
    // address the `stvx128 v0` at 0x8240DC8C wrote), so the zero is the seed, not a separate value.
    BrnEffects::TintData2d lTint2d;
    lTint2d.mv4Colour.SetZero();
    (void)lpArbitrator->EvalTint2d(lTint2d);   // return DISCARDED, as at the four sites above

    lpafColourXYZW[0] = lTint2d.mv4Colour.x;
    lpafColourXYZW[1] = lTint2d.mv4Colour.y;
    lpafColourXYZW[2] = lTint2d.mv4Colour.z;
    lpafColourXYZW[3] = lTint2d.mv4Colour.w;

    gDiag.mbTint2d = true;
}

// ==================================================================================================
// Render @0x8240DD04-0x8240DD4C -- MotionBlurState::Update.
//
// THE CALL, decoded off the asm rather than off Hex-Rays (which drops the fifth argument):
//     r3 = this + 0x450                     -> BrnPostFx::mMotionBlurState
//     r4 = particleRenderData + 0x60        (`addi r4, r24, 0x60`  @0x8240DD0C)
//     r5 = particleRenderData + 0xA0        (`addi r5, r24, 0xA0`  @0x8240DD08)
//     f1 = *(f32*)(particleRenderData + 0xC)(`lfs f1, 0xC(r24)`    @0x8240DD04)
//     r7 = (frame.mMotionBlurData.mbIsExpensiveMotionBlur != 0)
//          (`lbz r11, 0x1E1(r11)` / `cntlzw` / `extrwi r11,r11,1,26` / `xori r7, r11, 1`
//           @0x8240DD38-0x8240DD48 -- a byte-to-bool with the sense INVERTED TWICE, i.e. plain != 0)
// The float rides f1 and CONSUMES its GPR slot, which is why the fifth argument lands in r7 and not
// r6 -- the PPC float-argument rule AGENTS.md rule 4 names. Hex-Rays prints only four arguments and
// is wrong; the committed declaration (BrnPostFxShader.h:108) has five, ending in
// `MotionBlurState::EQuality leQuality`, and 0/1 are E_QUALITY_CHEAP / E_QUALITY_EXPENSIVE.
//
// r24 == the value BrnGame::DispatchThreadInputBuffer's read-locked ParticleRenderData accessor
// returns (`bl sub_8227F640` @0x8240C45C; that function asserts "Not locked for reading" and returns
// `a1 + 14496` == this + 0x38A0 == &mParticleRenderData, BrnDispatchThreadInputBuffer.h:182).
//
// ⚠ BLOCKED, TWICE OVER, AND NEITHER HALF IS THIS GROUP'S TO FIX:
//   1. MotionBlurState::Update HAS NO BODY IN THIS TREE. It is declaration-only at
//      BrnPostFxShader.h:108 and its own banner there says why (it needs the rw::math::fpu
//      double-precision matrix family, the same blocker as
//      BRN_POSTFX_MOTION_BLUR_REPROJECTION_AVAILABLE in BrnPostFxShader.cpp):
//        $ grep -rn "MotionBlurState::Update" --include=*.cpp b5-decomp/src
//        b5-decomp/src/GameSource/Graphics/PostFx/BrnPostFxShader.cpp:398    <- a comment
//        b5-decomp/src/GameSource/Graphics/PostFx/BrnPostFxShader.cpp:402    <- a comment
//      Two hits, both inside comments; no definition. Calling it is an LNK2019 for the whole exe.
//   2. THE TWO MATRIX ARGUMENTS CANNOT BE NAMED. ParticleRenderData is still an ad-hoc opaque in
//      BrnDispatchThreadInputBuffer.h:55 (`struct ParticleRenderData { u8 maStorage[1]; }`), so
//      +0x0C / +0x60 / +0xA0 could only be reached by casting raw offsets into a live C++ object --
//      the offset-hack AGENTS.md forbids, and on a 1-byte type it is also out-of-bounds.
//
// So the call is written, in the console's position and with the console's arguments, behind a gate
// that is 0. TO OPEN IT: body MotionBlurState::Update, recover ParticleRenderData's layout (its
// +0x0C time step and the two 64-byte matrices at +0x60 / +0xA0), then set the macro to 1.
// ==================================================================================================
#define BRN_POSTFX_MOTION_BLUR_UPDATE_AVAILABLE 0

bool BrnRendererUpdatePostFxMotionBlur(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                       const void* lpParticleRenderData)
{
#if BRN_POSTFX_MOTION_BLUR_UPDATE_AVAILABLE
    const BrnEffectsFrame* const lpFrame = GetBaseInternalFrame(lpArbitrator);
    if (lpFrame == 0 || lpParticleRenderData == 0)
        return false;

    const MotionBlurState::EQuality leQuality =
        lpFrame->GetMotionBlurData().mbIsExpensiveMotionBlur ? MotionBlurState::E_QUALITY_EXPENSIVE
                                                             : MotionBlurState::E_QUALITY_CHEAP;
    // The three ParticleRenderData reads go here once that type has a layout:
    //     r4 -> the Matrix44Affine at +0x60, r5 -> the Matrix44 at +0xA0, f1 -> the f32 at +0x0C.
    // msPostFx.GetMotionBlurState().Update(lrView, lrProjection, lfTimeStep, leQuality);
    return true;
#else
    (void)lpArbitrator;
    (void)lpParticleRenderData;
    static bool sbReported = false;
    if (!sbReported)
    {
        sbReported = true;
        CgsDev::Log::WriteToLog(
            "[postfx-fx] MotionBlurState::Update NOT called: its body does not exist in this tree and"
            " DispatchThreadInputBuffer::ParticleRenderData is still opaque."
            " [FLAG PC bring-up: BRN_POSTFX_MOTION_BLUR_UPDATE_AVAILABLE]\n");
    }
    return false;
#endif
}

// ==================================================================================================
// [FLAG PC bring-up diagnostic] the one line that proves the chain.
//
// LATCHED ON A COUNTER, capped at six lines: base frame -> Eval* -> BrnPostFx is a per-frame steady
// state, so a value latch would print once and a per-frame line would flood the log. Six samples 500
// frames apart cover the loading screen, the menu and the first in-world frames.
// DELETE with the bring-up.
// ==================================================================================================
void BrnRendererLogPostFxEffectState()
{
    static u32 suCalls  = 0u;
    static u32 suPrints = 0u;
    const u32 luFrame = suCalls++;
    if ((luFrame % 500u) != 0u || suPrints >= 6u)
        return;
    ++suPrints;

    char lacMsg[224];
    std::snprintf(lacMsg, sizeof(lacMsg),
                  "[postfx-fx] frame %u: bloom=%d(lum %.3f thr %.3f col %.3f %.3f %.3f)"
                  " vig=%d dof=%d blur=%d tint2d=%d\n",
                  static_cast<unsigned>(luFrame),
                  gDiag.mbBloom ? 1 : 0,
                  static_cast<double>(gDiag.mfBloomLuminance),
                  static_cast<double>(gDiag.mfBloomThreshold),
                  static_cast<double>(gDiag.mafBloomColour[0]),
                  static_cast<double>(gDiag.mafBloomColour[1]),
                  static_cast<double>(gDiag.mafBloomColour[2]),
                  gDiag.mbVignette ? 1 : 0,
                  gDiag.mbDepthOfField ? 1 : 0,
                  gDiag.mbBlur ? 1 : 0,
                  gDiag.mbTint2d ? 1 : 0);
    CgsDev::Log::WriteToLog(lacMsg);
}
