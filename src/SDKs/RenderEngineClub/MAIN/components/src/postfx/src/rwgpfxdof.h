#ifndef RW_GPFX_DOF_H
#define RW_GPFX_DOF_H

#include "types.hpp"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // renderengine::ProgramBufferData / ProgramVariableHandle

// rw::graphics::postfx::DepthOfField -- the post-fx depth-of-field effect. It compiles one pixel
// program (the DoF shader) and looks up two shader-constant handles (g_focalConstants1 /
// g_focalConstants2); SetState resolves a DepthOfField::State (focal planes + projection near/far +
// blur params) into the packed shader constants stored in m_state.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../postfx/include/rwgpfxdof.h), member offsets confirmed against
// the X360 binary (DepthOfField @0x82402CA8 + SetState @0x823F8AE0):
//   m_depthOfFieldProgram   +0x00   (DWARF "ProgramBuffer*"; our runtime object is ProgramBufferData)
//   m_focalConstants1Handle +0x04   (4-byte ProgramVariableHandle)
//   m_focalConstants2Handle +0x08
//   m_state                 +0x0C   (State: 8 floats == 0x20 bytes)
//   m_bParametersDirty      +0x2C
//
// Reconstructed in this TU: the Parameters constructor, SetState, DownSampleAndGaussianBlur and
// Release (the last of which the console INLINED into BrnPostFx::Destruct). The remaining DWARF
// methods (GetResourceDescriptor / Initialize / Apply / PostProcess) have no attested call site in
// this build's post-fx path and are not declared here.
namespace rw
{
namespace graphics
{
namespace postfx
{
    // Forward-declared, not included: DownSampleAndGaussianBlur takes RenderTarget by pointer only,
    // and rwgpfxrendertarget.h transitively pulls in the whole renderengine texture/pixel-buffer
    // surface. This is the documented pointer-only cascade-avoidance exception in AGENTS.md.
    class RenderTarget;

    class DepthOfField
    {
    public:
        // The DoF tuning state (DWARF DepthOfField::State, 8 x float32 == 0x20 bytes).
        struct State
        {
            f32 m_focalPlanes[4];   // +0x00  near-blur / focal-near / focal-far / far-blur depths
            f32 m_projNearPlane;    // +0x10  projection near plane
            f32 m_projFarPlane;     // +0x14  projection far plane
            f32 m_dofAmount;        // +0x18
            f32 m_blurRadius;       // +0x1C
        };

        // The effect's construction parameters (DWARF DepthOfField::Parameters); its leading member
        // is the State, so &params == &params.m_state (the X360 ctor forwards a2 straight to SetState).
        struct Parameters
        {
            State m_state;          // +0x00
        };

        // X360 0x82402CA8 -- compile the DoF pixel program, bind the two focal-constant handles, seed
        // the default state, then resolve the supplied parameters via SetState.
        explicit DepthOfField(const Parameters& lParameters);

        // X360 0x823F8AE0 -- resolve a State into the packed shader constants held in m_state: each
        // focal plane becomes clamp01((plane*zProjScale + wProjOffset) / plane) with the projection /
        // dof / blur params carried through verbatim. (zProjScale = far/(far-near),
        // wProjOffset = -near*zProjScale -- the standard depth-linearisation constants.)
        void SetState(const State& lState);

        // X360: called from BrnPostFx::PrepareDownSampleBuffers @0x82408EFC with r4 = the DoF pool
        // slot's RenderTarget, r5 = the caller's result target, r6 = the WORK slot's RenderTarget --
        // and NO float registers (see this edit's note; the `fmr f2, f1` at entry belongs to the
        // bloom call). Down-sample the source into the DoF chain and gaussian-blur it; returns the
        // target it produced. Declaration-only; the body lives in the DoF TU.
        // DWARF rwgpfxdof.h:136 (dwarfdump line 116) -- three parameters.
        RenderTarget* DownSampleAndGaussianBlur(RenderTarget* lpDestRenderTarget,
                                                RenderTarget* lpSourceRenderTarget,
                                                RenderTarget* lpWorkBufferTarget);

        // Release the compiled DoF pixel program and free the resource it was carved into.
        // INLINED on the X360 (BrnPostFx::Destruct 0x8240819C-0x824081F4); body belongs to the DoF TU.
        // DWARF rwgpfxdof.h:125 (dwarfdump line 108).
        void Release();

    private:
        renderengine::ProgramBufferData*    m_depthOfFieldProgram;    // +0x00 (DWARF: ProgramBuffer*)
        renderengine::ProgramVariableHandle m_focalConstants1Handle;  // +0x04
        renderengine::ProgramVariableHandle m_focalConstants2Handle;  // +0x08
        State                               m_state;                  // +0x0C
        bool                                m_bParametersDirty;       // +0x2C
    };
}
}
}

#endif // RW_GPFX_DOF_H
