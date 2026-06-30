#include "GameSource/Graphics/PostFx/BrnPostFx.h"

// BrnPostFx -- the Burnout post-effects driver.
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm (BrnPostFx ctor @0x82407FC8) is the spine for the
// construction-time member values reconstructed here; the DecFIGS DWARF supplies the member shape.
//
// Only the constructor is reconstructed with a body: it is the boot-trace-executed function whose
// every member write maps to a named member and whose every call (BrnPostFxShader's ctor, the
// embedded B4Blur::State ctor, and EA::Jobs::Job's name ctor) is satisfied by an exposed dependency
// API. The remaining five methods (Construct / Destruct / Render / PrepareDownSampleBuffers /
// BeginTintBlend / EndTintBlend) drive a wide surface of render-engine post-fx entry points
// (Tint::BeginBlendJob/EndBlendJob, RenderTarget::Begin/Resolve, DepthOfField::DownSampleAnd...,
// PfxHelper::Release, BrnPostFxShader::Construct/Render, ...) that the committed dependency headers
// do not yet declare. Per the reconstruction rules those bodies are NOT fabricated here; they are
// left declaration-only (in the header) until their dependency surface is reconstructed.

BrnPostFx msPostFx;

BrnPostFx::BrnPostFx()
    : m_blendJob(nullptr) // EA::Jobs::Job::Job(&m_blendJob, 0): the unnamed tint-blend job
{
    // mPostFxShader, m_b4blurState and m_blendJob are constructed by their own constructors:
    //   BrnPostFxShader::BrnPostFxShader(this)           -- the embedded shader sub-object
    //   rw::graphics::postfx::B4Blur::State::State(...)  -- the embedded blur state block
    //   EA::Jobs::Job::Job(&m_blendJob, 0)               -- the tint-blend job, unnamed (null name)
    // matching the X360 ctor's three sub-object construction calls.

    m_enabledFx = 0;

    // The vignette state constant block: inner colour {0,0,0,0}, outer colour {1,1,1,1},
    // controlA {1,1,1,1}, controlB {0.5,0.5,1,1} (the v11..v14 stores into the vignette gap).
    m_vignetteState.mInnerColour.x = 0.0f;
    m_vignetteState.mInnerColour.y = 0.0f;
    m_vignetteState.mInnerColour.z = 0.0f;
    m_vignetteState.mInnerColour.w = 0.0f;
    m_vignetteState.mOuterColour.x = 1.0f;
    m_vignetteState.mOuterColour.y = 1.0f;
    m_vignetteState.mOuterColour.z = 1.0f;
    m_vignetteState.mOuterColour.w = 1.0f;
    m_vignetteState.mControlA.x = 1.0f;
    m_vignetteState.mControlA.y = 1.0f;
    m_vignetteState.mControlA.z = 1.0f;
    m_vignetteState.mControlA.w = 1.0f;
    m_vignetteState.mControlB.x = 0.5f;
    m_vignetteState.mControlB.y = 0.5f;
    m_vignetteState.mControlB.z = 1.0f;
    m_vignetteState.mControlB.w = 1.0f;
    m_vignetteState.mfSharpness = 0.0f;

    // The depth-of-field state seeds (float3D0..float3EC immediates): the four focal planes, the
    // projection near/far planes, the dof amount and the blur radius.
    m_dofState.m_focalPlanes[0] = 0.1f;
    m_dofState.m_focalPlanes[1] = 10.0f;
    m_dofState.m_focalPlanes[2] = 100.0f;
    m_dofState.m_focalPlanes[3] = 1000.0f;
    m_dofState.m_projNearPlane = 0.1f;
    m_dofState.m_projFarPlane = 1000.0f;
    m_dofState.m_dofAmount = 1.0f;
    m_dofState.m_blurRadius = 4.5f;

    // The render-target / effect pointers and the tint-blend counters the ctor zeroes
    // (dword5A0/5A8/5B8/5BC/5C8, lpTint, dword5C4, byte5F4 and the tint-buffer index).
    m_deviceRt = nullptr;
    m_mainRt = nullptr;
    m_renderTarget = nullptr;
    m_frameRt = nullptr;
    m_bloomDsRt = nullptr;
    m_depthOfFieldRt = nullptr;

    m_pfxDof = nullptr;
    m_pfxVignette = nullptr;
    m_pfxTint[0] = nullptr;
    m_pfxTint[1] = nullptr;
    m_pfxB4Blur = nullptr;

    m_processTint = false;
    m_currentTintBuffer = 0;
}
