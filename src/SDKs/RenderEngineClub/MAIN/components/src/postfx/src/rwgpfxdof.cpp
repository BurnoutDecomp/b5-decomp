#include "types.hpp"

#include <cstring>   // memset

#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxdof.h"      // DepthOfField
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"   // PfxHelper + gpPfxHelper
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"      // ProgramBuffer
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                       // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"            // shadow::Device::SetState
#include "rw/rwcore_structs.h"                                                   // rw::IResourceAllocator / Resource

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::DepthOfField::DepthOfField             @ 0x82402CA8
//   rw::graphics::postfx::DepthOfField::SetState                 @ 0x823F8AE0
//   rw::graphics::postfx::DepthOfField::DownSampleAndGaussianBlur@ 0x82408270
//   rw::graphics::postfx::DepthOfField::Release                  -- INLINED into
//                                                                   BrnPostFx::Destruct @0x8240819C
//
// DepthOfField compiles the DoF pixel program, binds its two focal-constant handles, then resolves
// the supplied tuning State into the packed shader constants stored in m_state. SetState linearises
// each focal-plane depth through the camera's projection (zProjScale = far/(far-near),
// wProjOffset = -near*zProjScale) and clamps the result to [0,1]; the projection / dof / blur params
// pass through unchanged. (Math + clamp order verified against the SetState asm: the two `fsel`
// instructions are the [0,1] clamp via floating-select; flt_82001CC0 == 0.0, flt_82001C98 == 1.0.)

// --------------------------------------------------------------------------------------------------
// THE DoF PIXEL PROGRAM -- NO LONGER BLOCKED, AND NO LONGER GATED.
//
// The X360 constructor compiles its pixel program from XENOS MICROCODE embedded in the executable
// image: `addi r4, r11, unk_82043FB8@l` @0x82402D08 with `li r5, 0x284` (644) @0x82402CDC and
// `li r3, 1` (pixel) @0x82402CE4, allocator `li r6, 0` @0x82402CD8 (the singleton's own).
//
// Those 644 bytes are dumped now (scratch/postfx_step5_wave/PACKAGES_DUMP.md, from ARTIST_copy.i64 --
// the dump's size matches the `li r5, 0x284` immediate exactly). They were disassembled with
// tools/assets/shaders/xenos.py, re-expressed as HLSL in tools/assets/shaders/brn_postfx_helper.fx
// and published as a PC ShaderProgramBuffer image by the generated leaf
// pc/gcm/renderengine/PostFxHelperProgramsPC.cpp. The console CTAB and the PC program's variable
// table were compared entry by entry -- g_focalConstants1 c0 x1, g_focalConstants2 c1 x1,
// screen_texture s0, low_texture s1, depth_texture s2 -- and are IDENTICAL, so the two
// GetVariableHandleByName lookups below resolve to the same registers the console resolved.
//
// WHAT THE PROGRAM COMPUTES (recovered instruction by instruction; the full listing is in the .fx):
//     near = saturate((g_focalConstants1.y - depth) * g_focalConstants2.y)
//     far  = saturate((depth - g_focalConstants1.z) * g_focalConstants2.z)
//     t    = max(near, far) * g_focalConstants2.x
//     oC0.rgb = lerp(screen_texture, low_texture, t)
// -- i.e. the two INNER focal planes drive the blend and the two outer ones are never read by this
// program (a finding, not an omission). Alpha is the previous-scalar residual on the console
// (`retain_prev export0.___w` with no scalar op defining PS), so the PC program writes 0.
//
// ONE PC DEVIATION, FLAGGED IN THE .fx: the console reconstructs a 24-bit depth from three 8-bit
// channels of a depth surface read through a colour sampler; on PC the post-fx targets' depth is
// bound as a RAW-DEPTH texture (INTZ / DF24 / DF16) whose .r IS the depth, so those three
// instructions collapse to one fetch. Same quantity, different source encoding.
//
// The gate macro BRN_POSTFX_DOF_PROGRAM_AVAILABLE is GONE. It guarded a second, local copy of the
// console microcode route -- and that route belongs to PfxHelper::CreateProgram, which is the single
// funnel the console itself goes through (see the "THIS IS THE MICROCODE WALL, AND IT IS THE ONLY
// PLACE IT NEEDS TO BE" banner in rwgpfxhelper.cpp). CreateProgram tries the PC image first, and its
// own gate still owns the refusal when there is no image. This TU just makes the console's call.
// --------------------------------------------------------------------------------------------------

// [FLAG PC-platform leaf: shader programs] The PC image of the DoF pixel program, published by the
// generated leaf pc/gcm/renderengine/PostFxHelperProgramsPC.cpp. Declared here as the minimal
// external surface -- the convention BrnPostFxBloom.cpp and BrnPostFxShader.cpp already use for
// their generated leaves; there is no header for these. The SIZE is the PC image's, not the X360
// microcode's 644, and it is read from the leaf's own published constant (whose static_assert ties
// it to sizeof() of the array) rather than hard-coded: ProgramBufferPC_Adopt bounds-checks the blob
// against the size it is handed.
namespace renderengine
{
    extern const u8  gauPostFxDepthOfFieldPixelProgramPC[];
    extern const u32 guPostFxDepthOfFieldPixelProgramPCSize;
}

namespace
{
    // The X360 assembly immediates this file uses.
    const f32 KF_ZERO      = 0.0f;        // flt_82001CC0
    const f32 KF_ONE       = 1.0f;        // flt_82001C98
    const f32 KF_HALF_PI   = 1.5707964f;  // flt_82047B38 -- the second (vertical) blur's angle
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // X360 0x82402CA8.
    DepthOfField::DepthOfField(const Parameters& lParameters)
    {
        // The X360 nulls the program slot first (`stw r10(=0), 0(r31)` @0x82402CE0), then writes the
        // default tuning state, then resolves the supplied parameters over the top of it.
        m_depthOfFieldProgram = nullptr;

        m_state.m_focalPlanes[0] = 0.1f;
        m_state.m_focalPlanes[1] = 10.0f;
        m_state.m_focalPlanes[2] = 100.0f;
        m_state.m_focalPlanes[3] = 1000.0f;
        m_state.m_projNearPlane  = 0.1f;
        m_state.m_projFarPlane   = 1000.0f;
        m_state.m_dofAmount      = 1.0f;
        m_state.m_blurRadius     = 4.5f;
        m_bParametersDirty       = true;

        // ======== THE CONSOLE ROUTE (X360 0x82402D00-0x82402D60) ==================================
        // `li r3, 1` (pixel) / `addi r4, r11, unk_82043FB8@l` / `li r5, 0x284` (644) / `li r6, 0`
        // (no allocator override -> CreateProgram falls back to the singleton's own), then the two
        // GetVariableHandleByName lookups. The pointer passed here is the PC image of that same
        // program; CreateProgram adopts it and returns a live ProgramBufferData.
        m_depthOfFieldProgram = PfxHelper::CreateProgram(
            1, renderengine::gauPostFxDepthOfFieldPixelProgramPC,
            renderengine::guPostFxDepthOfFieldPixelProgramPCSize, nullptr);

        if (m_depthOfFieldProgram != nullptr)
        {
            renderengine::ProgramBuffer::GetVariableHandleByName(
                m_depthOfFieldProgram, reinterpret_cast<const u8*>("g_focalConstants1"),
                &m_focalConstants1Handle);
            renderengine::ProgramBuffer::GetVariableHandleByName(
                m_depthOfFieldProgram, reinterpret_cast<const u8*>("g_focalConstants2"),
                &m_focalConstants2Handle);
        }
        else
        {
            // [FLAG PC bring-up] NOT IN THE CONSOLE: the X360 dereferences the returned program
            // unconditionally because its CreateProgram cannot fail here. On PC the adopt can refuse
            // (a malformed image, or a shader-type mismatch), and the committed lookup dereferences
            // lpData unconditionally (programbuffer.cpp:263), so it would fault. A default
            // ProgramVariableHandle has register COUNT 0, which every uploader in this tree treats
            // as "constant not present", so nothing binds and nothing uploads.
            m_focalConstants1Handle = renderengine::ProgramVariableHandle();
            m_focalConstants2Handle = renderengine::ProgramVariableHandle();

            static bool sbReportedNoDofProgram = false;
            if (!sbReportedNoDofProgram)
            {
                sbReportedNoDofProgram = true;
                CgsDev::Log::WriteToLog(
                    "[postfx-dof] constructed with NO pixel program -- PfxHelper::CreateProgram"
                    " REFUSED the PC image published by PostFxHelperProgramsPC.cpp"
                    " (gauPostFxDepthOfFieldPixelProgramPC). The program slot is null and both"
                    " focal-constant handles have register count 0. [FLAG PC bring-up]\n");
            }
        }

        SetState(lParameters.m_state);
    }

    // X360 0x823F8AE0.
    void DepthOfField::SetState(const State& lState)
    {
        // Standard depth-linearisation constants from the camera projection near/far planes.
        const f32 lfZProjScale  = lState.m_projFarPlane / (lState.m_projFarPlane - lState.m_projNearPlane);
        const f32 lfWProjOffset = -(lState.m_projNearPlane * lfZProjScale);

        // Resolve each focal-plane depth into a [0,1] blend factor. A zero plane is the explicit
        // divide-by-zero guard the asm's leading fcmpu encodes (-> 0).
        for (s32 liPlane = 0; liPlane < 4; ++liPlane)
        {
            const f32 lfPlane = lState.m_focalPlanes[liPlane];
            if (lfPlane == KF_ZERO)
            {
                m_state.m_focalPlanes[liPlane] = KF_ZERO;
            }
            else
            {
                f32 lfBlend = (lfPlane * lfZProjScale + lfWProjOffset) / lfPlane;
                lfBlend = lfBlend < KF_ZERO ? KF_ZERO : lfBlend;   // fsel low clamp
                lfBlend = lfBlend > KF_ONE  ? KF_ONE  : lfBlend;   // fsel high clamp
                m_state.m_focalPlanes[liPlane] = lfBlend;
            }
        }

        // The projection / dof / blur parameters carry through unchanged.
        m_state.m_projNearPlane = lState.m_projNearPlane;
        m_state.m_projFarPlane  = lState.m_projFarPlane;
        m_state.m_dofAmount     = lState.m_dofAmount;
        m_state.m_blurRadius    = lState.m_blurRadius;
    }

    // ============================================================================================
    // DepthOfField::DownSampleAndGaussianBlur  @ 0x82408270      (DWARF rwgpfxdof.h:136)
    //
    // Twenty-nine instructions, and every one of them is one of five things.
    //
    // 1. THE HELPER SINGLETON. `lwz r31, off_82FAEE80@l(r11)` @0x82408294 -- the shared PfxHelper.
    //    Nothing in this function uses the DoF's OWN program; the two blur passes and the down-sample
    //    are all the helper's, which is why the function survives with the DoF program slot empty.
    //
    // 2. THE BLEND-STATE PUSH (0x82408298-0x824082D4). Lock-gate byte -> compare the cached pointer ->
    //    Xbox2SetStateLowLevelShadowed(state, cached == 0) -> write the cache back. That is
    //    shadow::Device::SetState(const renderengine::BlendMaterialState*) @0x82276A68 INLINED,
    //    instruction for instruction (its own body is shadowingdevice.cpp:936, and byte_83010907 /
    //    dword_83010964 are already homed there as mbBlendStateLocked / mpBlendState). AGENTS.md's
    //    inlining-reversal rule says restore the call, and restoring it is also what keeps this TU off
    //    two private statics of shadow::Device. The state it pushes is `lwz r29, 0x4C(r31)` -- the
    //    helper's m_opaqueBlendState.
    //
    // 3. THE DOWN-SAMPLE. `li r6, 1` @0x824082D8 -> METHOD_16TAP_WITH_BILINEAR, dest = r4 = the DoF
    //    pool slot, source = r5 = the caller's result target.
    //
    // 4. THE SEPARABLE GAUSSIAN. Two Blur9 passes, ping-ponging through the work buffer:
    //       Blur9(work, dest, angle 0.0,        radius, 1.0)     horizontal
    //       Blur9(dest, work, angle 1.5707964,  radius, 1.0)     vertical (flt_82047B38 = pi/2)
    //    The radius is `lfs f2, 0x28(r28)` -- this + 0x28, which is m_state (+0x0C) + m_blurRadius
    //    (+0x1C). The weight factor is flt_82001C98 == 1.0, held in f31 across both calls.
    //
    // 5. THE RETURN. `mr r3, r30` @0x82408334 -- the DESTINATION target, i.e. the blurred DoF buffer.
    //
    // ⚠ THERE ARE NO FLOAT PARAMETERS. The `fmr f2, f1` at the CALLER's entry (@0x82408E98) belongs
    // to BrnPostFxBloom::Render, not to this call; the DoF branch @0x82408EE4-0x82408EFC sets r3/r4/
    // r5/r6 and touches no FPR. See BrnPostFx.cpp's PrepareDownSampleBuffers banner. DWARF
    // rwgpfxdof.h:136 gives the same three parameters.
    // ============================================================================================
    RenderTarget* DepthOfField::DownSampleAndGaussianBlur(RenderTarget* lpDestRenderTarget,
                                                          RenderTarget* lpSourceRenderTarget,
                                                          RenderTarget* lpWorkBufferTarget)
    {
        PfxHelper* const lpHelper = gpPfxHelper;

        // [FLAG PC bring-up] NOT IN THE CONSOLE. The X360 dereferences off_82FAEE80 unconditionally
        // because PfxHelper::PfxHelper has always run by this point. In this tree the helper's carve
        // can legitimately come back null (BrnPostFx::Construct tests every one of its five
        // allocations), and blurring through a null singleton is a crash rather than a missing
        // effect. Return the SOURCE -- the un-blurred scene, which is the visually safe degradation;
        // returning the destination would hand the caller a target nothing has written this frame.
        if (lpHelper == nullptr)
        {
            static bool sbReportedNoHelper = false;
            if (!sbReportedNoHelper)
            {
                sbReportedNoHelper = true;
                CgsDev::Log::WriteToLog(
                    "[postfx-dof] DownSampleAndGaussianBlur called with no PfxHelper singleton"
                    " (rw::graphics::postfx::gpPfxHelper == null); returning the source target"
                    " unblurred. [FLAG PC bring-up]\n");
            }
            return lpSourceRenderTarget;
        }

        // Push the shared opaque blend state through the shadow cache (inlined on the console).
        shadow::Device::SetState(lpHelper->GetOpaqueBlendState());

        // Half-resolution down-sample of the scene into the DoF buffer.
        lpHelper->DownSample(lpDestRenderTarget, lpSourceRenderTarget,
                             PfxHelper::METHOD_16TAP_WITH_BILINEAR);

        // Separable 9-tap gaussian: horizontal into the work buffer, vertical back into the DoF buffer.
        lpHelper->Blur9(lpWorkBufferTarget, lpDestRenderTarget, KF_ZERO,    m_state.m_blurRadius, KF_ONE);
        lpHelper->Blur9(lpDestRenderTarget, lpWorkBufferTarget, KF_HALF_PI, m_state.m_blurRadius, KF_ONE);

        return lpDestRenderTarget;
    }

    // ============================================================================================
    // DepthOfField::Release                                      (DWARF rwgpfxdof.h:125)
    //
    // INLINED on the console -- there is no standalone rw::graphics::postfx::DepthOfField::Release in
    // the export set. The body is recovered from its ONE caller, BrnPostFx::Destruct, at
    // 0x8240819C-0x824081F4 (r30 = this->m_pfxDof, r29 = gpPfxHelper):
    //
    //     lwz  r3, 0(r30)                       m_depthOfFieldProgram
    //     cmplwi cr6, r3, 0 ; beq  ...           if it is null, do nothing
    //     bl   renderengine__ProgramBuffer__Release
    //     lwz  r3, 0(r29)                       gpPfxHelper->m_allocator   <-- the HELPER's, not ours
    //     lwz  r10, 0(r30)                      the program pointer again
    //     stw  r31(=0), 0x00(r11) .. 0x10(r11)  a zeroed five-word rw::Resource on the stack
    //     stw  r10, var_50(r1)                  ... with slot 0 = the program
    //     bctrl on vtbl+0x14                    allocator->DoFree(that Resource)
    //     stw  r31(=0), 0(r30)                  m_depthOfFieldProgram = 0
    //
    // This is the DWARF's own SafeRelease<renderengine::ProgramBuffer> (rwgpfxhelper.h:176, whose
    // recorded body is exactly `IResourceAllocator::Free` + `Resource::Resource`) expanded in place.
    //
    // WHY THE HELPER'S ALLOCATOR AND NOT THE DoF's: DepthOfField has no allocator member (the DWARF
    // member list is program / two handles / State / dirty flag, and the ctor passes 0 to
    // CreateProgram, which makes CreateProgram fall back to the singleton's own). The block therefore
    // has to go back to the allocator that issued it, which is the helper's. That asymmetry with
    // Vignette::Release (which frees through its OWN m_allocator) is real, not a transcription slip.
    //
    // vtbl+0x14 is the established DoFree slot (rwcore_structs.h: IResourceAllocator_vtbl::DoFree).
    // Per the release conventions already verified in this campaign, DoFree takes the Resource by
    // const reference; the five zero words plus slot 0 are exactly a default-constructed rw::Resource
    // with its first base resource set.
    // ============================================================================================
    void DepthOfField::Release()
    {
        if (m_depthOfFieldProgram == nullptr)
        {
            return;
        }

        renderengine::ProgramBuffer::Release(m_depthOfFieldProgram);

        // [FLAG PC bring-up] The console reads gpPfxHelper unconditionally; see the note in
        // DownSampleAndGaussianBlur. If the singleton is gone there is no allocator to hand the block
        // back to, so the program pointer is still cleared and the block is leaked rather than freed
        // through a null vtable.
        PfxHelper* const lpHelper = gpPfxHelper;
        if (lpHelper != nullptr)
        {
            rw::Resource lProgramResource;
            std::memset(&lProgramResource, 0, sizeof(lProgramResource));
            lProgramResource.m_baseResources[0] = m_depthOfFieldProgram;
            lpHelper->GetAllocator()->DoFree(lProgramResource);
        }

        m_depthOfFieldProgram = nullptr;
    }
}
}
}
