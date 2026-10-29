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

namespace
{
    // ARTIST 0x82043FB8, 644 bytes; SHA-256 a32f73bf3e199dc8bc5bb32499b7e3acaa00ed010033df5a14e4b3b16e4c4fa5.
    __declspec(align(4)) const u8 kDofPixelProgram[] = {
        0x10, 0x2A, 0x11, 0x00, 0x00, 0x00, 0x01, 0x9C, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x01, 0x50, 0x00, 0x00, 0x01, 0x78, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x28, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x01, 0x1A,
        0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x08,
        0x00, 0x00, 0x01, 0x13, 0x00, 0x00, 0x00, 0x80, 0x00, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x00, 0x02, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x00, 0x00, 0x00, 0xC4, 0x00, 0x00, 0x00, 0xD4,
        0x00, 0x02, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x00, 0x00, 0x00, 0xE8,
        0x00, 0x00, 0x00, 0xF8, 0x00, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x64, 0x65, 0x70, 0x74, 0x68, 0x5F, 0x74, 0x65,
        0x78, 0x74, 0x75, 0x72, 0x65, 0x00, 0xAB, 0xAB, 0x00, 0x04, 0x00, 0x0C, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x5F, 0x66, 0x6F, 0x63, 0x61, 0x6C, 0x43,
        0x6F, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x74, 0x73, 0x31, 0x00, 0xAB, 0xAB, 0x00, 0x01, 0x00, 0x03,
        0x00, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3D, 0xCC, 0xCC, 0xCD,
        0x3F, 0x80, 0x00, 0x00, 0x42, 0xC8, 0x00, 0x00, 0x44, 0x7A, 0x00, 0x00, 0x67, 0x5F, 0x66, 0x6F,
        0x63, 0x61, 0x6C, 0x43, 0x6F, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x74, 0x73, 0x32, 0x00, 0xAB, 0xAB,
        0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00,
        0x6C, 0x6F, 0x77, 0x5F, 0x74, 0x65, 0x78, 0x74, 0x75, 0x72, 0x65, 0x00, 0x73, 0x63, 0x72, 0x65,
        0x65, 0x6E, 0x5F, 0x74, 0x65, 0x78, 0x74, 0x75, 0x72, 0x65, 0x00, 0x70, 0x73, 0x5F, 0x33, 0x5F,
        0x30, 0x00, 0x32, 0x2E, 0x30, 0x2E, 0x36, 0x35, 0x33, 0x34, 0x2E, 0x31, 0x00, 0xAB, 0xAB, 0xAB,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x14, 0x01, 0xFC, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0xA8,
        0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x21,
        0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x30, 0x50, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x80, 0x00, 0x01,
        0x3B, 0x80, 0x00, 0x01, 0x37, 0x80, 0x00, 0x01, 0x3F, 0x00, 0x00, 0x00, 0x43, 0x7F, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x60, 0x02, 0xC4, 0x00, 0x12, 0x00, 0x00, 0x95, 0x00, 0x00, 0x50, 0x08, 0x00, 0x00, 0x22, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x18, 0x20, 0x01, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00,
        0x10, 0x08, 0x10, 0x01, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x28, 0x00, 0x01,
        0x1F, 0x1F, 0xFE, 0xC8, 0x00, 0x00, 0x40, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xBE, 0x6C, 0x1B,
        0x8B, 0x00, 0xFF, 0xFE, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0xEA, 0x00, 0x00, 0x00,
        0xC8, 0x01, 0x00, 0x00, 0x00, 0xBE, 0xC0, 0x00, 0xB0, 0x00, 0xFE, 0x00, 0xB8, 0x24, 0x00, 0x00,
        0x02, 0x6C, 0xC6, 0x80, 0x80, 0x00, 0x00, 0x00, 0xC9, 0x03, 0x00, 0x00, 0x00, 0xC5, 0xC5, 0x00,
        0xA1, 0x00, 0x01, 0x00, 0x14, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0xE2, 0x00, 0x00, 0x00,
        0xA8, 0x2D, 0x00, 0x00, 0x02, 0xF0, 0xF0, 0x41, 0xC0, 0x02, 0x01, 0x01, 0xC8, 0x8F, 0xC0, 0x00,
        0x00, 0xB1, 0x14, 0xC0, 0xEB, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

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
        m_depthOfFieldProgram = 0;

        m_state.m_focalPlanes[0] = 0.1f;
        m_state.m_focalPlanes[1] = 10.0f;
        m_state.m_focalPlanes[2] = 100.0f;
        m_state.m_focalPlanes[3] = 1000.0f;
        m_state.m_projNearPlane  = 0.1f;
        m_state.m_projFarPlane   = 1000.0f;
        m_state.m_dofAmount      = 1.0f;
        m_state.m_blurRadius     = 4.5f;
        m_bParametersDirty       = true;

        m_depthOfFieldProgram = PfxHelper::CreateProgram(1, kDofPixelProgram, sizeof(kDofPixelProgram), 0);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            m_depthOfFieldProgram, reinterpret_cast<const u8*>("g_focalConstants1"), &m_focalConstants1Handle);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            m_depthOfFieldProgram, reinterpret_cast<const u8*>("g_focalConstants2"), &m_focalConstants2Handle);

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
        if (m_depthOfFieldProgram == 0)
        {
            return;
        }

        renderengine::ProgramBuffer::Release(m_depthOfFieldProgram);

        PfxHelper* const lpHelper = gpPfxHelper;
        if (lpHelper != 0)
        {
            rw::Resource lProgramResource;
            std::memset(&lProgramResource, 0, sizeof(lProgramResource));
            lProgramResource.m_baseResources[0] = m_depthOfFieldProgram;
            lpHelper->GetAllocator()->DoFree(lProgramResource);
        }

        m_depthOfFieldProgram = 0;
    }
}
}
}
