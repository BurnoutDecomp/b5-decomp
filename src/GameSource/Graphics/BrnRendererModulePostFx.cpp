#include "GameSource/Graphics/BrnRendererModulePostFx.h"

#include "GameSource/Graphics/BrnEffectsArbitrator.h"      // BrnGraphics::EffectsArbitrator
#include "SharedClasses/Graphics/BrnEffectsData.h"         // BrnEffectsFrame + the five data blocks
#include "GameSource/Graphics/PostFx/BrnPostFx.h"          // msPostFx + every setter/getter below
#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"  // ParticleRenderData (the Update arguments)
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::WriteToLog (the diag line)
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.h"  // ColourCube::GetSize

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
        bool mbTint3d;   // the COLOUR-CUBE tint (m_enabledFx bit 0x20), set by the tint block below
        f32  mfBloomLuminance;
        f32  mfBloomThreshold;
        f32  mafBloomColour[3];
    };
    DiagState gDiag = { false, false, false, false, false, false,
                        0.0f, 0.0f, { 0.0f, 0.0f, 0.0f } };

    // The motion-blur half of the same sampled diagnostic. mfWvpDelta is the largest absolute
    // element difference between MotionBlurState's current and previous world-view-projection
    // matrices: it is ZERO exactly when the reprojection has nothing to say (the pair is still the
    // identity pair MotionBlurState::Construct left, or the camera has not moved), and non-zero the
    // instant a real Update lands a moving camera. That single number is the cheapest proof the
    // consumer chain is alive, because BlurMatrixX/Y/W are built from precisely that difference.
    struct MotionBlurDiagState
    {
        bool mbActive;
        bool mbUpdateCalled;
        s32  miQuality;
        f32  mfCarsBlurAmount;
        f32  mfWorldBlurAmount;
        f32  mfWvpDelta;
    };
    MotionBlurDiagState gMotionBlurDiag = { false, false, 0, 0.0f, 0.0f, 0.0f };

    // Largest absolute element difference between the two WVP matrices, by named lane.
    f32 MaxAbsoluteDifference(const rw::math::vpu::Matrix44& lLhs, const rw::math::vpu::Matrix44& lRhs)
    {
        const rw::math::vpu::Vector4* const lapLhs[4] =
            { &lLhs.xAxis, &lLhs.yAxis, &lLhs.zAxis, &lLhs.wAxis };
        const rw::math::vpu::Vector4* const lapRhs[4] =
            { &lRhs.xAxis, &lRhs.yAxis, &lRhs.zAxis, &lRhs.wAxis };

        f32 lfWorst = KF_ZERO;
        for (u32 luRow = 0; luRow < 4u; ++luRow)
        {
            const f32 lafDelta[4] = { lapLhs[luRow]->x - lapRhs[luRow]->x,
                                      lapLhs[luRow]->y - lapRhs[luRow]->y,
                                      lapLhs[luRow]->z - lapRhs[luRow]->z,
                                      lapLhs[luRow]->w - lapRhs[luRow]->w };
            for (u32 luLane = 0; luLane < 4u; ++luLane)
            {
                const f32 lfAbs = AbsValue(lafDelta[luLane]);
                if (lfAbs > lfWorst)
                    lfWorst = lfAbs;
            }
        }
        return lfWorst;
    }
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
// Render @0x8240C69C-0x8240C798 -- THE COLOUR-CUBE (3D LUT) TINT BLOCK.
//
// It is NOT part of the apply block below, and its position matters: the console runs it EARLY --
// inside PerfMonCpu monitor `*(this + 51508)` (StartMonitor @0x8240C698 / StopMonitor @0x8240C79C),
// immediately BEFORE shadow::Device::ResetShadowing() and the three global texture binds -- because
// BeginTintBlend SCHEDULES AN EA::Jobs JOB that blends the source cubes into the tint volume
// texture while the shadow map and the world passes draw. BrnPostFx::Render then drains it
// (`if (m_processTint) { m_blendJob.WaitOn(); Tint::EndBlendJob(); }`, BrnPostFx.cpp step 1) before
// the composite samples s3. Moving this to the apply block would serialise the job against nothing
// and shorten the window to zero.
//
// THE SHAPE, off the asm (Hex-Rays pseudocode lines 505-533 renders the same block):
//   0x8240C6A8  lbz    r11, 0(this + 0xC41C)         -> mbRenderPostFX, the block's whole gate
//   0x8240C6C0  addi   r5, r1, var_C70               -> the FIVE-entry WEIGHT array
//   0x8240C6C4  addi   r4, r1, var_CA0               -> the FIVE-entry CUBE array
//   0x8240C6C8  mr     r3, r21 (this + 0x480)        -> &mEffectsArbitrator
//   0x8240C6CC  bl     EffectsArbitrator::EvalTint
//   0x8240C6D0..EC                                   -> active = EvalTint() && lbEffectsAllowed
//   0x8240C6F4..0C  ori/rlwinm 0x20 on m_enabledFx   -> BrnPostFx::SetTint(active), INLINED
//   0x8240C710  beq                                  -> if (!active) skip the publish
//   0x8240C718..90  five times:                         if (cube[i]) m_colourCubes[i] = cube[i];
//                                                       m_tintFactors[i] = weight[i];
//   0x8240C794  bl     BrnPostFx::BeginTintBlend
//
// THE NULL GUARD ON THE CUBE STORE IS REAL AND LOAD-BEARING (`cmplwi cr6, r11, 0` / `beq` before
// each of the five `stw`s, and there is no such test before the five weight `stfs`). It exists
// because the slots are PRE-SEEDED with a default cube at prepare time -- see
// PCBringUpSeedTintBlendSources below -- so a layer that contributes no cube this frame leaves the
// default in place rather than nulling a pointer the blend kernels dereference.
//
// IT IS SPELLED ONCE, IN BrnPostFx::SetColourCube (2026-08-16 fix round). The console has exactly
// one test per slot and the five it has are inlined around the five `stw`s INTO m_colourCubes --
// i.e. inside the setter that got inlined, not around the call to it. The first cut wrote the same
// test in both places: behaviour-identical, but it reads as two different rules and invites the next
// reader to "fix" one of them. The guard therefore lives with the store it guards, and the loop
// below calls the setter for every slot, unconditionally, exactly as the console's source did.
//
// THE TWO STACK ARRAYS ARE RAW, exactly as the bloom/vignette out-blocks are: `addi r4, r1,
// var_CA0` with no prior store. EvalTint writes all five entries of both on its true path (4 WORLD
// + 1 FXEVENTS colour cubes, byte_8203E114 = {0,4,1}) and the arrays are read only inside the
// `if (active)` arm, which cannot be entered on its false path.
//
// ---- BRN_POSTFX_TINT3D_AVAILABLE -- THE ONE-LINE KILL SWITCH, AND WHY IT EXISTS ------------------
// E_FX_TINT (0x20) is the ONLY one of the five effect bits that moves the composite's PERMUTATION
// INDEX (BrnPostFxShader.cpp `leShader = 4*blur | 2*dof | tint3d`), so switching it on switches the
// composite from an even permutation to an ODD one -- and every odd permutation SAMPLES the tint
// volume at s3 (`tex3D(Sampler3dTint, composedColour)`). All twelve permutation program pairs are
// adopted on this build (BrnPostFxShader.cpp's "ALL TWELVE PAIRS NOW EXIST" banner), so the draw
// itself is safe; what is NOT this group's to guarantee is what s3 CONTAINS -- the tint volume
// texture, its Lock/Unlock and the seven CPU blend kernels are group `tintrender`'s half
// (rw::graphics::postfx::Tint::Initialize @0x82403B48 / BeginBlendJob @0x823F8310 / the
// TintBlend variant table @0x82F7238C). This half and that half must land TOGETHER: with the bit on
// and no real volume texture behind s3, the grade is whatever that sampler returns. Set this to 0 to
// revert the tint layer to the step-9 behaviour (bit permanently clear, permutation index unmoved)
// without touching anything else. DELETE the switch once tintrender's half is committed and booted.
#define BRN_POSTFX_TINT3D_AVAILABLE 1
// ==================================================================================================
#if BRN_POSTFX_TINT3D_AVAILABLE
namespace
{
    // ---- [FLAG PC bring-up] ------------------------------------------------------------------
    // STANDS IN FOR: BrnEffects::EffectsModule::PrepareResources @0x8229D8A8, case 2 -- the ONE
    // place in the image that seeds BrnPostFx's tint sources. Once the colour-cube dictionary
    // ("PostFx/colourcubedictionary.bin", pool 10) is resident it writes, and writes nothing else
    // into BrnPostFx (asm @0x8229DAAC and the stores around it; pseudocode lines 76-87):
    //     m_numCubesToBlend = 5                              -> SetTintBlendNumber(5)
    //     m_tintFactors[0..4] = 0.2f                         -> SetTintBlendFactor(i, 0.2f)
    //     m_colourCubes[0..4] = <the dictionary's first cube> -> SetColourCube(i, cube)
    //
    // WHY IT HAS TO BE STOOD IN FOR. That TU is not on the build list:
    //     $ grep -n "EffectsModule" tools/build/build_game_exe.bat
    //     294:  rem   driver (EffectsModule::Update / ::GenerateDispatchLists) are BOTH still off this list. ...
    //     300:  rem   DELETE this line when ParticleModule.cpp + EffectsModule.cpp land.
    //     (two COMMENTS, no `echo` line -- the file is not compiled)
    // so on this build nothing ever publishes the blend COUNT, and BrnPostFx::BeginTintBlend
    // @0x823F8380 copies `m_numCubesToBlend` source pointers into the job parameters -- a count of
    // zero makes rw::graphics::postfx::TintBlend @0x82AD4860 index dword_82F7238C[0], which is the
    // table's NULL entry (DATA_DUMP.md "[ 0] ... 0x00000000 (NO FUNC)"). Nothing seeds the five cube
    // slots either, so the FXEVENTS slot -- whose producer (BrnGui::EffectsArbitrator) is also not
    // live -- would hand the kernels a null source pointer to read.
    //
    // WHAT IT WRITES, AND WHERE EACH VALUE COMES FROM. The count is the console's own 5, and it is
    // the same 5 three independent ways (EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT from
    // byte_8203E114, BrnPostFx::m_colourCubes[5], and BeginTintBlend's own
    // `CGS_ASSERT(m_numCubesToBlend <= 5)` at BrnPostFx.cpp:266). The CUBE is NOT invented: it is
    // the first non-null cube THIS FRAME'S EvalTint produced -- a real, loaded
    // rw::graphics::postfx::ColourCube -- copied into the slots the console would have found
    // holding the dictionary default. The weights are NOT seeded: the console's 0.2f x 5 is a
    // prepare-time value the very next Render overwrites with EvalTint's, so it would be dead code.
    //
    // ⚠ A SUBSTITUTED SLOT IS UNOBSERVABLE **ONLY WHEN ITS WEIGHT IS ZERO** -- and since the
    // 2026-08-16 fix round that is the rule the code enforces, not merely the argument for it.
    // (The first cut claimed the substitution was never observable at all. It is not true.)
    //
    // The blend kernels splat each source's weight and multiply (DATA_DUMP.md Blend2Cubes:
    // TintBlendParameters +0x2C / +0x30 -> v127 / v126 -> VMXBlend), so a ZERO-weight source
    // contributes nothing to the output -- but its pixel POINTER is still dereferenced, which is the
    // whole reason such a slot may not be left null. A slot with a NON-ZERO weight is a different
    // animal, and this build can produce one: EnvironmentManager::GenerateEffects @0x827BE698's
    // DEFAULT arm publishes (mpDefTintData, KF_DEF_EFFECTS_LAYER_TINT_WEIGHT = 0.25f) and
    // mpDefTintData is null until Prepare's E_PREPARE_WF_ACQUIRE_DEPENDENCIES acquire lands, so the
    // WORLD layer can arrive here NULL AND WEIGHTED. Substituting there would blend THIS FRAME'S
    // first cube -- the graded 0x8b7e999a "ENV_CC_Paradise_ingame_junk / TINT_Art_Style.psd" every
    // keyframe imports -- at 0.25 into a slot the console would have filled with its own
    // prepare-time default, the NEUTRAL identity cube (Prepare @0x827D49A8 loads
    // "PostFx/colourcubedictionary.bin" into pool 10 and acquires
    // "gamedb://burnout5/Playground/PostFx/ColourCubeDictionary/rgb_colourcube.tga.ImageFile?ID=217407"
    // out of it -- the same bundle PrepareResources reads its default from). That is a visibly
    // over-graded frame. So: substitute into ZERO-WEIGHT slots only, and if any slot is null with a
    // non-zero weight, log it once and return false -- the tint bit stays CLEAR for that frame,
    // which is the one outcome that cannot be mistaken for art direction.
    //
    // Returns false when there is no cube at all this frame; the caller then leaves the tint bit
    // CLEAR rather than scheduling a blend over null sources. That single extra term is the ONLY
    // deviation from the console in this whole block.
    //
    // DELETE-WHEN GameSource/Effects/EffectsModule.cpp is on the build list -- at that point
    // PrepareResources seeds the slots and the count, this function goes, and the caller's
    // `&& lbSeeded` term goes with it.
    bool PCBringUpSeedTintBlendSources(rw::graphics::postfx::ColourCube** lppColourCubes,
                                       const f32* lpafWeights,
                                       u32 luCount)
    {
        rw::graphics::postfx::ColourCube* lpDefault = 0;
        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        {
            if (lppColourCubes[luIndex] != 0)
            {
                lpDefault = lppColourCubes[luIndex];
                break;
            }
        }

        if (lpDefault == 0)
        {
            static bool sbReported = false;
            if (!sbReported)
            {
                sbReported = true;
                CgsDev::Log::WriteToLog(
                    "[postfx-tint] no colour cube this frame -- every EvalTint slot is null, so the"
                    " tint bit stays CLEAR. The world layer's cube comes from the environment"
                    " keyframe's +0x80 import (or the manager's default when Prepare has acquired"
                    " one); the fx-events layer's producer (BrnGui::EffectsArbitrator) is not live."
                    " [FLAG PC bring-up: BrnEffects::EffectsModule::PrepareResources @0x8229D8A8"
                    " not on the build list]\n");
            }
            return false;
        }

        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        {
            if (lppColourCubes[luIndex] != 0)
                continue;

            // A null cube with a REAL weight is observable -- refuse the frame instead of grading
            // it with a cube the console would not have used. See the banner above.
            if (lpafWeights[luIndex] != 0.0f)
            {
                static bool sbWeightedNullReported = false;
                if (!sbWeightedNullReported)
                {
                    sbWeightedNullReported = true;
                    char lacMsg[288];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                        "[postfx-tint] slot %u has NO colour cube but weight %.3f -- the console"
                        " would blend its prepare-time NEUTRAL default here, so the tint bit stays"
                        " CLEAR this frame rather than substituting a graded cube."
                        " [FLAG PC bring-up: BrnEffects::EffectsModule::PrepareResources"
                        " @0x8229D8A8 not on the build list]\n",
                        static_cast<unsigned>(luIndex),
                        static_cast<double>(lpafWeights[luIndex]));
                    CgsDev::Log::WriteToLog(lacMsg);
                }
                return false;
            }

            lppColourCubes[luIndex] = lpDefault;
        }

        // SetTintBlendNumber takes `const int&` (DWARF BrnPostFx.h:82), so the value needs a name.
        const int liSourceCount = static_cast<int>(luCount);
        msPostFx.SetTintBlendNumber(liSourceCount);
        return true;
    }

    // The one-shot proof line the step-10 boot check greps for. Printed the first time the tint
    // layer actually schedules a blend.
    //
    // BUILT IN A LOOP OVER luCount (2026-08-16 fix round). The first cut took a count and then
    // hard-indexed [0..4] in the format string and the argument list: correct only while
    // EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT is 5, and that constant is BUILT from the console's
    // byte_8203E114 {0,4,1} rather than written as a literal, so a hard five would over-read the
    // caller's stack arrays the day it moves. The cube's EDGE is in the line now that the tree's
    // ColourCube carries the DWARF's GetSize() -- it is the "edge=N" half of this wave's boot proof.
    void LogTintBlendOnce(rw::graphics::postfx::ColourCube* const* lppColourCubes,
                          const f32* lpfWeights, u32 luCount)
    {
        static bool sbReported = false;
        if (sbReported)
            return;
        sbReported = true;

        char lacMsg[384];
        int liWritten = std::snprintf(lacMsg, sizeof(lacMsg),
                                      "[postfx-tint] tint3d ON: %u sources",
                                      static_cast<unsigned>(luCount));
        size_t luUsed = (liWritten > 0) ? static_cast<size_t>(liWritten) : 0u;
        if (luUsed > sizeof(lacMsg) - 1u)
            luUsed = sizeof(lacMsg) - 1u;

        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        {
            const rw::graphics::postfx::ColourCube* const lpCube = lppColourCubes[luIndex];
            liWritten = std::snprintf(lacMsg + luUsed, sizeof(lacMsg) - luUsed,
                                      " [%u]=%p edge=%u w=%.3f",
                                      static_cast<unsigned>(luIndex),
                                      static_cast<const void*>(lpCube),
                                      (lpCube != 0) ? static_cast<unsigned>(lpCube->GetSize()) : 0u,
                                      static_cast<double>(lpfWeights[luIndex]));
            if (liWritten <= 0)
                break;
            luUsed += static_cast<size_t>(liWritten);
            if (luUsed > sizeof(lacMsg) - 1u)
            {
                luUsed = sizeof(lacMsg) - 1u;   // snprintf truncated; stop appending
                break;
            }
        }

        // The newline goes on through snprintf as well, so this function performs NO index
        // arithmetic of its own into lacMsg -- every write is bounded by the runtime, and the
        // compiler needs no /GS range check (i.e. no fresh CRT external) to see it.
        std::snprintf(lacMsg + luUsed, sizeof(lacMsg) - luUsed, "\n");
        CgsDev::Log::WriteToLog(lacMsg);
    }
}
#endif  // BRN_POSTFX_TINT3D_AVAILABLE

void BrnRendererBeginPostFxTintBlend(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                     bool lbEffectsAllowed)
{
#if !BRN_POSTFX_TINT3D_AVAILABLE
    // Kill switch OFF: the step-9 behaviour, spelled out rather than left implicit. m_enabledFx's
    // tint bit is never set, so the composite keeps selecting an EVEN permutation and nothing
    // samples s3. Nothing else in the frame changes.
    (void)lpArbitrator;
    (void)lbEffectsAllowed;
    static bool sbReported = false;
    if (!sbReported)
    {
        sbReported = true;
        CgsDev::Log::WriteToLog(
            "[postfx-tint] tint3d compiled OUT."
            " [FLAG PC bring-up: BRN_POSTFX_TINT3D_AVAILABLE]\n");
    }
#else
    // [FLAG PC bring-up] the arbitrator is Constructed lazily on this build; the console's is a
    // by-value member that always exists. Same seam gate as every other function in this file.
    if (lpArbitrator == 0)
        return;

    // The two raw out-arrays, sized by the arbitrator's own contract (4 WORLD + 1 FXEVENTS).
    rw::graphics::postfx::ColourCube*
        lapColourCubes[BrnGraphics::EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT];
    f32 lafWeights[BrnGraphics::EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT];

    // 0x8240C6CC. The return value IS consumed here (unlike the five Eval* in the apply block):
    // `clrlwi r11, r3, 24` / `cmplwi` / `beq` is the first half of the active flag.
    const bool lbEvaluated = lpArbitrator->EvalTint(lapColourCubes, lafWeights);

    bool lbTintActive = lbEvaluated && lbEffectsAllowed;

    // [FLAG PC bring-up] the ONE extra term -- see PCBringUpSeedTintBlendSources' banner. It also
    // publishes the blend COUNT the console publishes at effects-module prepare time.
    if (lbTintActive
        && !PCBringUpSeedTintBlendSources(lapColourCubes, lafWeights,
                                          BrnGraphics::EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT))
    {
        lbTintActive = false;
    }

    // 0x8240C6F4-0x8240C70C: `ori r11, r11, 0x20` / `rlwinm r11, r11, 0,27,25` on m_enabledFx --
    // BrnPostFx::SetTint(const bool&) inlined. AGENTS.md's inlining-reversal rule puts the call
    // back; the bit is never poked directly. This is the bool that reaches
    // BrnPostFxShader::Render as the composite's TINT3D permutation lane (BrnPostFx::Render
    // step 10, `lbTint = lbEffectsAllowed && (m_enabledFx & E_FX_TINT)`).
    msPostFx.SetTint(lbTintActive);
    gDiag.mbTint3d = lbTintActive;

    if (!lbTintActive)
        return;

    // 0x8240C718-0x8240C790, unrolled five times on the console (the count is a compile-time 5),
    // re-rolled here. The console's cube store is null-GUARDED and its weight store is not -- and
    // that guard lives inside BrnPostFx::SetColourCube, the setter the console inlined around that
    // very `stw` (see the banner). Both calls are therefore unconditional here.
    for (u32 luIndex = 0;
         luIndex < BrnGraphics::EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT;
         ++luIndex)
    {
        msPostFx.SetColourCube(static_cast<int>(luIndex), lapColourCubes[luIndex]);
        msPostFx.SetTintBlendFactor(static_cast<int>(luIndex), lafWeights[luIndex]);
    }

    LogTintBlendOnce(lapColourCubes, lafWeights,
                     BrnGraphics::EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT);

    // 0x8240C794. Schedules the EA::Jobs blend of the (cube, weight) sources into the current
    // tint volume texture and sets m_processTint; BrnPostFx::Render drains it.
    msPostFx.BeginTintBlend();
#endif  // BRN_POSTFX_TINT3D_AVAILABLE
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
            // [FLAG PC bring-up] null test: m_pfxDof can be null on this build (the effect carves are
            // PC bring-up); the console never tests it. Unreached today (mbUseDepthOfField is false).
            if (msPostFx.GetDepthOfField() != 0)
            {
                msPostFx.GetDepthOfField()->SetState(*lpState);
            }
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

            // The console finishes the arm with
            //     memcpy(*(this + 0x5C8), this + 0x3F0, 0x60)      (asm 0x8240DC30-0x8240DC50)
            // i.e. it copies the 0x60-byte State it just built straight over B4Blur::m_state, that
            // class's FIRST member (rwgpfxb4blur.h, `State m_state; // +0x00`) -- an assignment, spelled
            // as one through B4Blur::SetState (rwgpfxb4blur.{h,cpp}, landed with this wave) so nothing
            // pokes a private member. [FLAG PC bring-up] the null test: m_pfxB4Blur can be null on this
            // build (BrnPostFx::Construct's effect carves are PC bring-up); the console never tests it.
            if (msPostFx.GetB4Blur() != 0)
            {
                msPostFx.GetB4Blur()->SetState(*lpState);
            }
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
// ⭐ BOTH BLOCKERS CLOSED 2026-08-15 (post-fx step-6 producers wave), and the gate is now 1:
//   1. MotionBlurState::Update IS BODIED, in BrnPostFxShader.cpp, where the DWARF puts it. (The
//      old note here said it needed the rw::math::fpu double-precision matrix family. That was
//      wrong: Update is pure single-precision VMX with no call in it at all. The fpu double family
//      is what BrnPostFxShader::Render's REPROJECTION block needs, and that now exists too.)
//   2. DispatchThreadInputBuffer::ParticleRenderData HAS ITS REAL LAYOUT (DWARF ParticleModule.h:
//      589-606), so the three arguments are reached BY NAME -- mfCurrentTimeStep, mCgsCamera's view
//      and mCgsCamera's projection -- and the console's +0x0C / +0x60 / +0xA0 are three independent
//      confirmations of that layout rather than three offsets anybody has to cast to.
//
// ⭐ THE THIRD BLOCKER -- THE PRODUCER -- CLOSED 2026-08-16 (post-fx step 9). On the console the
// render data is filled every frame by BrnParticle::ParticleModule::GenerateRenderRequests
// @0x82281BD8 -- the ONLY caller of the write-locked accessor:
//   $ python -c "import json;print(json.load(open('.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8227F6E8.json'))['xrefs_to'])"
//   [{'address': '0x82281BD8', 'name': 'BrnParticle::ParticleModule::GenerateRenderRequests'}]
// which in turn is only reached from BrnEffects::EffectsModule::GenerateDispatchLists @0x82296668,
// after BrnParticle::ParticleModule::Update @0x822817D8 (the vtable+68 virtual EffectsModule::Update
// @0x8229EC28 drives) has refreshed the module's own record at ParticleModule+0x8E00. NEITHER module
// is on this build's list -- `grep -n "Particle" tools/build/build_game_exe.bat` finds only the LION
// vendor waveform TU -- so the record was never written and this function was handed a NULL.
//
// It is now written by the named PC bring-up stand-in for that pair,
//     BrnParticle::PCBringUpProduceParticleRenderData  (ParticleModuleBringUp.cpp)
// called from BrnGameModule::DoDispatch with the DIRECTOR's published camera and the SIM timer's
// step / time / multiplier -- the same three floats the console's EffectsModule::Update reads off
// the published TimerStatusInterface. BrnRendererModule.cpp now passes the READ-LOCKED
// `lpDispatchThreadInputBuffer->GetParticleRenderData()`, gated on that producer having run.
//
// ⚠ THE OLD WARNING IS KEPT AS HISTORY, BECAUSE THE HAZARD IT NAMES IS STILL REAL: DO NOT WIRE THE
// ACCESSOR UP WITHOUT A PRODUCER. Passing `lpDispatchThreadInputBuffer->GetParticleRenderData()`
// unconditionally would hand this function UNINITIALISED memory before the first producer run:
// neither DispatchThreadInputBuffer::Construct (faithfully -- the console does not clear that
// payload either) nor CreateIOBuffer<T> (since the 2026-08-15 perf wave) zeroes it. A garbage view
// matrix is almost surely singular or non-finite, and MotionBlurState::Update's zero-time-step arm
// INVERTS it four times -- the resulting NaN would propagate straight into BlurMatrixX/Y/W and, from
// there, into a tex2Dgrad gradient. That is exactly why the call site tests
// BrnParticle::PCBringUpParticleRenderDataProduced() and not just the buffer pointer. The null arm
// below stays for the frames before the first DoDispatch, and for any build where the producer is
// unmounted.
//
// ⚠ STENCIL-MASK DEVIATION, LIVE FROM THIS CHANGE ON. The console's composite blurs CARS and WORLD
// separately behind the stencil mask BrnPostFx writes; this tree's composite has no stencil mask, so
// a real (non-zero) velocity now blurs the WHOLE frame uniformly instead of only what the mask
// selects. That is a known, deliberate deviation -- the fix is the stencil mask, not a weakened
// velocity. Until it lands, expect directional streaking of the world (and of the car with it) in
// the intro / menu cameras, which is precisely the boot proof this rung is checked against.
// ==================================================================================================
#define BRN_POSTFX_MOTION_BLUR_UPDATE_AVAILABLE 1

bool BrnRendererUpdatePostFxMotionBlur(
    const BrnGraphics::EffectsArbitrator* lpArbitrator,
    const BrnParticle::ParticleModule::ParticleRenderData* lpParticleRenderData)
{
    gMotionBlurDiag.mbUpdateCalled = false;

    const BrnEffectsFrame* const lpFrame = GetBaseInternalFrame(lpArbitrator);
    if (lpFrame == 0)
        return false;   // [FLAG PC bring-up] see GetBaseInternalFrame.

    // r7 -- `lbz r11, 0x1E1(r11)` / cntlzw / extrwi / xori @0x8240DD38-0x8240DD48, i.e. plain != 0.
    const BrnDirector::Camera::MotionBlurData& lrMotionBlur = lpFrame->GetMotionBlurData();
    const MotionBlurState::EQuality leQuality =
        lrMotionBlur.mbIsExpensiveMotionBlur ? MotionBlurState::E_QUALITY_EXPENSIVE
                                             : MotionBlurState::E_QUALITY_CHEAP;

    gMotionBlurDiag.mbActive          = lrMotionBlur.mbIsActive;
    gMotionBlurDiag.miQuality         = static_cast<s32>(leQuality);
    gMotionBlurDiag.mfCarsBlurAmount  = lrMotionBlur.mfCarsBlurAmount;
    gMotionBlurDiag.mfWorldBlurAmount = lrMotionBlur.mfWorldBlurAmount;

#if BRN_POSTFX_MOTION_BLUR_UPDATE_AVAILABLE
    if (lpParticleRenderData == 0)
    {
        // [FLAG PC bring-up] the record has not been stamped YET -- see the banner. Since post-fx
        // step 9 this is no longer "there is no producer at all": the producer is
        // BrnParticle::PCBringUpProduceParticleRenderData (ParticleModuleBringUp.cpp), driven from
        // BrnGameModule::DoDispatch, and the call site here gates on
        // BrnParticle::PCBringUpParticleRenderDataProduced(). So this arm now means one of exactly
        // three things: (a) the frames before the first DoDispatch (boot / loading, where DoDispatch
        // is only reached in the IN_GAME flow state), (b) the director output buffer was null so the
        // producer returned early, or (c) ParticleModuleBringUp.cpp is not on the build list.
        // DELETE WHEN the real BrnParticle::ParticleModule + BrnEffects::EffectsModule fill
        // DispatchThreadInputBuffer::mParticleRenderData (then the record is always written before
        // any reader, as on the console, and no latch is needed).
        static bool sbReportedNoRenderData = false;
        if (!sbReportedNoRenderData)
        {
            sbReportedNoRenderData = true;
            CgsDev::Log::WriteToLog(
                "[postfx-mb] MotionBlurState::Update NOT called: no ParticleRenderData has been"
                " stamped yet (the console's producer is BrnParticle::ParticleModule::"
                "GenerateRenderRequests @0x82281BD8, not reconstructed; the PC stand-in is"
                " BrnParticle::PCBringUpProduceParticleRenderData, driven from"
                " BrnGameModule::DoDispatch). The WVP pair stays at Construct's identity/identity,"
                " so the composite's reprojection rows are exactly zero -- correct and finite, just"
                " motionless. [FLAG PC bring-up: ParticleRenderData not produced yet]\n");
        }
        return false;
    }

    const CgsGraphics::Camera& lrCamera = lpParticleRenderData->mCgsCamera;

    // [FLAG PC type bridge] The DWARF types the camera's view member `Matrix44Affine`
    // (dwarfdump/GameShared/GameClasses/Graphics/CgsCamera.h:202) and MotionBlurState::Update's
    // first parameter is a Matrix44Affine; the committed CgsGraphics::Camera models the identical
    // four 16-byte rows as a Matrix44 (mView @+0x00, pinned by that header's _AssertLayout). Same
    // bytes, different SDK spelling. Copied lane by lane rather than reinterpret_cast so nothing
    // silently depends on the two aggregates keeping the same host layout. DELETE-WHEN
    // CgsGraphics::Camera adopts the DWARF's Matrix44Affine for its view member.
    rw::math::vpu::Matrix44Affine lCameraView;
    lCameraView.xAxis = rw::math::vpu::Vector3{ lrCamera.mView.xAxis.x, lrCamera.mView.xAxis.y,
                                                lrCamera.mView.xAxis.z, lrCamera.mView.xAxis.w };
    lCameraView.yAxis = rw::math::vpu::Vector3{ lrCamera.mView.yAxis.x, lrCamera.mView.yAxis.y,
                                                lrCamera.mView.yAxis.z, lrCamera.mView.yAxis.w };
    lCameraView.zAxis = rw::math::vpu::Vector3{ lrCamera.mView.zAxis.x, lrCamera.mView.zAxis.y,
                                                lrCamera.mView.zAxis.z, lrCamera.mView.zAxis.w };
    lCameraView.wAxis = rw::math::vpu::Vector3{ lrCamera.mView.wAxis.x, lrCamera.mView.wAxis.y,
                                                lrCamera.mView.wAxis.z, lrCamera.mView.wAxis.w };

    // The console's five-argument call, argument for argument (asm 0x8240DD04-0x8240DD4C):
    //   r3 = &BrnPostFx::mMotionBlurState        r4 = particleRenderData + 0x60  (the view)
    //   r5 = particleRenderData + 0xA0 (proj)    f1 = *(f32*)(particleRenderData + 0x0C)
    //   r7 = the quality (the float in f1 CONSUMES its GPR slot -- AGENTS.md's PPC rule 4 -- which
    //        is why the fifth argument lands in r7 and Hex-Rays prints only four).
    msPostFx.GetMotionBlurState().Update(lCameraView,
                                         lrCamera.mProjection,
                                         lpParticleRenderData->mfCurrentTimeStep,
                                         leQuality);
    gMotionBlurDiag.mbUpdateCalled = true;
    gMotionBlurDiag.mfWvpDelta = MaxAbsoluteDifference(msPostFx.GetMotionBlurState().mCurrentWVP,
                                                       msPostFx.GetMotionBlurState().mPreviousWVP);
    return true;
#else
    (void)lpParticleRenderData;
    static bool sbReported = false;
    if (!sbReported)
    {
        sbReported = true;
        CgsDev::Log::WriteToLog(
            "[postfx-mb] MotionBlurState::Update compiled OUT."
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
                  "[postfx-fx] apply-call %u: bloom=%d(lum %.3f thr %.3f col %.3f %.3f %.3f)"
                  " vig=%d dof=%d blur=%d tint2d=%d tint3d=%d\n",
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
                  gDiag.mbTint2d ? 1 : 0,
                  gDiag.mbTint3d ? 1 : 0);
    CgsDev::Log::WriteToLog(lacMsg);

    // The motion-blur consumer line. `updated` is 1 only when the console's five-argument
    // MotionBlurState::Update actually ran; `wvpDelta` is the largest element difference between
    // the current and previous world-view-projection matrices, which is what the composite's
    // reprojection differences into BlurMatrixX/Y/W. With no producer wired, the honest reading is
    // active=0 updated=0 wvpDelta=0.000 -- and any NON-zero wvpDelta with updated=0 would be a bug.
    char lacMotionBlurMsg[192];
    std::snprintf(lacMotionBlurMsg, sizeof(lacMotionBlurMsg),
                  "[postfx-mb] apply-call %u: active=%d updated=%d quality=%d cars=%.2f world=%.2f"
                  " wvpDelta=%.4f\n",
                  static_cast<unsigned>(luFrame),
                  gMotionBlurDiag.mbActive ? 1 : 0,
                  gMotionBlurDiag.mbUpdateCalled ? 1 : 0,
                  static_cast<int>(gMotionBlurDiag.miQuality),
                  static_cast<double>(gMotionBlurDiag.mfCarsBlurAmount),
                  static_cast<double>(gMotionBlurDiag.mfWorldBlurAmount),
                  static_cast<double>(gMotionBlurDiag.mfWvpDelta));
    CgsDev::Log::WriteToLog(lacMotionBlurMsg);
}
