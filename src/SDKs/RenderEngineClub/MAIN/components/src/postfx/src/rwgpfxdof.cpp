#include "types.hpp"

#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxdof.h"      // DepthOfField
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"   // PfxHelper::CreateProgram
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"      // ProgramBuffer::GetVariableHandleByName

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::DepthOfField::DepthOfField @ 0x82402CA8
//   rw::graphics::postfx::DepthOfField::SetState     @ 0x823F8AE0
//
// DepthOfField compiles the DoF pixel program, binds its two focal-constant handles, then resolves
// the supplied tuning State into the packed shader constants stored in m_state. SetState linearises
// each focal-plane depth through the camera's projection (zProjScale = far/(far-near),
// wProjOffset = -near*zProjScale) and clamps the result to [0,1]; the projection / dof / blur params
// pass through unchanged. (Math + clamp order verified against the SetState asm: the two `fsel`
// instructions are the [0,1] clamp via floating-select; flt_82001CC0 == 0.0, flt_82001C98 == 1.0.)

namespace rw
{
namespace graphics
{
namespace postfx
{
    // The DoF pixel-shader microcode (644 bytes) embedded in the X360 image at unk_82043FB8.
    // HONEST PLACEHOLDER: compiled Xenos shader bytecode -- platform data with no recoverable bytes
    // from the function-only exports; modelled as an opaque blob the program factory consumes.
    extern const u8 gauDepthOfFieldProgramMicrocode[];

    // X360 0x82402CA8.
    DepthOfField::DepthOfField(const Parameters& lParameters)
    {
        // Seed the default tuning state (the X360 ctor writes these defaults before SetState resolves
        // the supplied parameters over the top of them).
        m_state.m_focalPlanes[0] = 0.1f;
        m_state.m_focalPlanes[1] = 10.0f;
        m_state.m_focalPlanes[2] = 100.0f;
        m_state.m_focalPlanes[3] = 1000.0f;
        m_state.m_projNearPlane  = 0.1f;
        m_state.m_projFarPlane   = 1000.0f;
        m_state.m_dofAmount      = 1.0f;
        m_state.m_blurRadius     = 4.5f;
        m_bParametersDirty       = true;

        // Compile the DoF pixel program (type 1) and bind the two focal-constant handles.
        m_depthOfFieldProgram = PfxHelper::CreateProgram(1, gauDepthOfFieldProgramMicrocode, 644, nullptr);
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
            if (lfPlane == 0.0f)
            {
                m_state.m_focalPlanes[liPlane] = 0.0f;
            }
            else
            {
                f32 lfBlend = (lfPlane * lfZProjScale + lfWProjOffset) / lfPlane;
                lfBlend = lfBlend < 0.0f ? 0.0f : lfBlend;   // fsel low clamp
                lfBlend = lfBlend > 1.0f ? 1.0f : lfBlend;   // fsel high clamp
                m_state.m_focalPlanes[liPlane] = lfBlend;
            }
        }

        // The projection / dof / blur parameters carry through unchanged.
        m_state.m_projNearPlane = lState.m_projNearPlane;
        m_state.m_projFarPlane  = lState.m_projFarPlane;
        m_state.m_dofAmount     = lState.m_dofAmount;
        m_state.m_blurRadius    = lState.m_blurRadius;
    }
}
}
}
