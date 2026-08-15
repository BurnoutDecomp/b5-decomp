#ifndef RW_GPFX_VIGNETTE_H
#define RW_GPFX_VIGNETTE_H

#include "types.hpp"
#include "rw/rwcore_structs.h"                                              // rw::IResourceAllocator / rw::Resource
#include "rw/math/vpu/types.h"                                             // rw::math::vpu::Vector4
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h" // renderengine::ProgramBufferData / ProgramVariableHandle / Parameters

// rw::graphics::postfx::Vignette -- the post-fx vignette effect (a darkened/rotated screen-border
// gradient drawn as a full-screen quad). The constructor builds the four render states the quad draws
// with (an opaque-with-blend BlendState, a Position+UV VertexDescriptor, a no-depth DepthStencilState,
// and a no-cull RasterizerState) through the renderengine resource pipeline, compiles the vignette
// vertex + pixel programs and binds their shader-constant handles (uvOffset / controlScalars /
// innerColour / outerColour), and seeds the per-frame state. SetState recomputes the shader-constant
// vectors from a State.
//
// ==================================================================================================
// LAYOUT CORRECTED 2026-08-15 (step-3 vignette/dof/raster wave). The previous revision of this header
// was a store-map GUESS ("No DWARF/leak source models the class body") and it was wrong in four ways.
// The DecFIGS DWARF DOES model it --
//   references/DecFIGS/dwarfdump/SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxvignette.h
// -- and every one of its offsets is now double-attested against the X360 ARTIST asm:
//
//   DWARF member (rwgpfxvignette.h:149..176)   X360 offset   attesting instruction
//   ----------------------------------------  -----------   -------------------------------------
//   Vector4 m_gradientScalars                  +0x00         SetState `stvx128 v0, r0, r31`   @0x823FE988
//   Vector4 m_innerColour                      +0x10         SetState `stvx128 v0, r31, r8(0x10)`
//   Vector4 m_outerColour                      +0x20         SetState `stvx128 v0, r31, r9(0x20)`
//   Vector4 m_centerScale                      +0x30         SetState `stvx128 v0, r31, r10(0x30)`
//   float32_t m_angle                          +0x40         SetState `stfs f0, 0x40(r31)`    @0x823FE9A8
//   ProgramVariableHandle m_controlScalarsHandle +0x44       ctor `addi r5, r30, 0x44`        @0x82404168
//   ProgramVariableHandle m_innerColourHandle    +0x48       ctor `addi r5, r30, 0x48`        @0x8240417C
//   ProgramVariableHandle m_outerColourHandle    +0x4C       ctor `addi r5, r30, 0x4C`        @0x82404190
//   ProgramVariableHandle m_uvOffsetHandle       +0x50       ctor `addi r5, r30, 0x50`        @0x82404140
//   Resource m_blendStateResource              +0x54         Release `addi r4, r31, 0x54`     @0x823F9448
//   Resource m_vertexDescriptorResource        +0x68         Release `addi r4, r31, 0x68`     @0x823F9430
//   Resource m_depthStencilResource            +0x7C         Release `addi r4, r31, 0x7C`     @0x823F9410
//   Resource m_rasterizerStateResource         +0x90         Release `addi r4, r31, 0x90`     @0x823F93F8
//   ProgramBuffer* m_vertexProgram             +0xA4         ctor `stw r3, 0xA4(r30)`         @0x82404144
//   ProgramBuffer* m_pixelProgram              +0xA8         ctor `stw r3, 0xA8(r30)`         @0x8240416C
//   ProgramBuffer* m_textureProgram            +0xAC         (never written by the X360 ctor; it is
//                                                             exactly the 0xA8 -> 0xB0 gap)
//   BlendState* m_blendState                   +0xB0         ctor `stw r11, 0xB0(r30)`        @0x82403F24
//   RasterizerState* m_rasterizerState         +0xB4         ctor `stw r10, 0xB4(r30)`        @0x82404134
//   VertexDescriptor* m_vertexDescriptor       +0xB8         ctor `stw r11, 0xB8(r30)`        @0x82403FD4
//   DepthStencilState* m_depthStencilState     +0xBC         ctor `stw r10, 0xBC(r30)`        @0x824040A4
//   rw::IResourceAllocator* m_allocator        +0xC0         ctor `stw r10, 0xC0(r30)`        @0x82403E88
//
// ProgramVariableHandle is 4 bytes (programbuffer.h:61-67), so the four handles fill +0x44..+0x53 and
// the first Resource lands exactly on +0x54 -- which is where Release's LAST DoFree points. The four
// Resources at 0x54/0x68/0x7C/0x90 (0x14 each) then end at 0xA4, which is where the first program
// pointer is stored. Twenty-one members, twenty-one offsets, no pad and no gap: that is the whole
// object, and it is why the old header's 16-byte "mau8Pad44" and its handles parked at
// +0x110/+0x120/+0x130/+0x140 were fabrications.
//
// WHAT THIS FIXES BESIDES NAMES: the State was declared ONE VECTOR LATE. The console's State is
// { gradientScalars, innerColour, outerColour, centerScale } == 0x40 bytes exactly, which is what
// BrnPostFx::Render's `addi r6, r31, 0x390` / `addi r7, r31, 0x3D0` pair measures (0x3D0 - 0x390 =
// 0x40). The old spelling called the first vector "mInnerColour" and bolted a fifth `mfSharpness`
// float on the end, so every consumer read the wrong lane and the block was 0x44. The independent
// proof is BrnPostFxShader.cpp:1203-1217, which had already diagnosed the shift and written its
// vignette constants through the wrong names on purpose while waiting for this header.
// ==================================================================================================

namespace rw
{
namespace graphics
{
namespace postfx
{
    class Vignette
    {
    public:
        // The per-frame vignette controls (DWARF rwgpfxvignette.h:38-45). EXACTLY 0x40 bytes.
        //   m_gradientScalars.x  the [-1,1] sharpness SetState maps through pow(x*0.5+0.5, 16)
        //   m_gradientScalars.y  the rotation angle (SetState copies it straight to Vignette::m_angle,
        //                        and the composite writes it to the VignetteAngle constant)
        //   m_innerColour        the colour at the centre of the gradient
        //   m_outerColour        the colour at the border
        //   m_centerScale        centre.xy / scale.xy -- the constant the composite's own interned
        //                        name calls "VignetteCentreXyScaleXy"
        struct State
        {
            rw::math::vpu::Vector4 m_gradientScalars;   // +0x00  rwgpfxvignette.h:40
            rw::math::vpu::Vector4 m_innerColour;       // +0x10  rwgpfxvignette.h:41
            rw::math::vpu::Vector4 m_outerColour;       // +0x20  rwgpfxvignette.h:42
            rw::math::vpu::Vector4 m_centerScale;       // +0x30  rwgpfxvignette.h:43
        };

        // rwgpfxvignette.h:87-92 (DWARF) -- the construction parameter block. X360 Parameters is
        // { allocator @+0x00, State @+0x10 }: the ctor reads the allocator at +0x00
        // (`lwz r10, 0(r22)`-equivalent -> `stw r10, 0xC0(r30)`) and hands SetState `addi r4, r22, 0x10`
        // @0x824041A0 -- i.e. &m_state, at +0x10 because Vector4 is 16-aligned.
        struct Parameters
        {
            rw::IResourceAllocator* m_allocator;   // rwgpfxvignette.h:89
            State                   m_state;       // rwgpfxvignette.h:90

            // rwgpfxvignette.h:92 -- X360 rw::graphics::postfx::Vignette::Parameters::Parameters
            // @0x823F5420. Seeds the block; BrnPostFx::Construct then overwrites m_allocator only.
            Parameters();
        };

        // X360 0x82403DB8 -- construct the vignette into `this`.
        //
        // SIGNATURE CORRECTED: DWARF rwgpfxvignette.h:139 is `Vignette(const Parameters&)`, and the asm
        // agrees (r4 is the block base; SetState is handed r4+0x10). The previous spelling took
        // `rw::IResourceAllocator**` and recovered the State as `*(const State*)(lppParameters + 1)`,
        // which is a HOST CORRUPTION and not merely a style problem: on x64 `lppParameters + 1` is
        // base+8, while Parameters::m_state is at base+16 (Vector4 is alignas(16)), so the seed read
        // eight bytes of padding plus the first three lanes of the real state.
        explicit Vignette(const Parameters& lrParameters);

        // X360 0x823FE8B0 -- recompute the vignette shader constants from `lrState`.
        // DWARF rwgpfxvignette.h:131.
        void SetState(const State& lrState);

        // X360 0x823F93D0 -- release every renderengine resource the vignette owns.
        // DWARF rwgpfxvignette.h:125. Called out-of-line by BrnPostFx::Destruct @0x824081FC.
        void Release();

    protected:
        // --- the shader-constant block SetState writes (DWARF rwgpfxvignette.h:149-153) -------------
        rw::math::vpu::Vector4              m_gradientScalars;           // +0x00
        rw::math::vpu::Vector4              m_innerColour;               // +0x10
        rw::math::vpu::Vector4              m_outerColour;               // +0x20
        rw::math::vpu::Vector4              m_centerScale;               // +0x30
        f32                                 m_angle;                     // +0x40

        // --- the four shader-constant handles (DWARF rwgpfxvignette.h:155-158) ----------------------
        renderengine::ProgramVariableHandle m_controlScalarsHandle;      // +0x44
        renderengine::ProgramVariableHandle m_innerColourHandle;         // +0x48
        renderengine::ProgramVariableHandle m_outerColourHandle;         // +0x4C
        renderengine::ProgramVariableHandle m_uvOffsetHandle;            // +0x50

        // --- the four render-state resource backing blocks (DWARF rwgpfxvignette.h:160-163) ---------
        rw::Resource                        m_blendStateResource;        // +0x54
        rw::Resource                        m_vertexDescriptorResource;  // +0x68
        rw::Resource                        m_depthStencilResource;      // +0x7C
        rw::Resource                        m_rasterizerStateResource;   // +0x90

        // --- the compiled programs and the state objects (DWARF rwgpfxvignette.h:165-176) -----------
        renderengine::ProgramBufferData*    m_vertexProgram;             // +0xA4
        renderengine::ProgramBufferData*    m_pixelProgram;              // +0xA8
        // DWARF rwgpfxvignette.h:167. The X360 ctor never writes it (it is exactly the 0xA8->0xB0
        // gap); kept so the member set matches the DWARF and the gap is named rather than padded.
        renderengine::ProgramBufferData*    m_textureProgram;            // +0xAC
        void*                               m_blendState;                // +0xB0
        void*                               m_rasterizerState;           // +0xB4
        void*                               m_vertexDescriptor;          // +0xB8
        void*                               m_depthStencilState;         // +0xBC
        rw::IResourceAllocator*             m_allocator;                 // +0xC0
    };
}
}
}

#endif // RW_GPFX_VIGNETTE_H
