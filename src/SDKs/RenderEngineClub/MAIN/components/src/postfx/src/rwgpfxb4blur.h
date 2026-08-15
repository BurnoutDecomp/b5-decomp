#ifndef RW_GPFX_B4BLUR_H
#define RW_GPFX_B4BLUR_H

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::IResourceAllocator / rw::Resource
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector2
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // ProgramBufferData / ProgramVariableHandle
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"     // BlendMaterialState
#include "pc/gcm/renderengine/VertexDescriptor.h"                            // VertexDescriptorData
#include "pc/gcm/renderengine/renderstates.h"                                // DepthStencilState / RasterizerState

// rw::graphics::postfx::B4Blur::State -- the parameter block for the "B4" (Burnout-4) blur post-fx.
// It carries the blend/blur amounts and centres (each a SIMD Vector2), plus the scalar opacity,
// velocity, sharpness MUL/ADD pair, noise and angle controls that the blur shader samples.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../postfx/include/rwgpfxb4blur.h): member names + order.
// Offsets confirmed against the X360 binary (State::State @0x823F52E0, State::SetBlendSharpness
//   @0x823F53A8): the four Vector2 members each occupy a 16-byte VMX register on X360 (the ctor
//   builds {x, y, 0, 0} vectors and writes them with stvx128 at +0x00/+0x10/+0x20/+0x30), and the
//   six trailing scalars are packed 4-byte at +0x40..+0x54. rw::math::vpu::Vector2 is the 16-byte,
//   16-aligned SIMD register type (float x,y,z,w), so embedding it reproduces the X360 byte layout:
//     m_blendAmount    +0x00  Vector2  (ctor {1.0, 1.0})
//     m_blurAmount     +0x10  Vector2  (ctor {1.0, 1.0})
//     m_blendCenter    +0x20  Vector2  (ctor {0.5, 0.5})
//     m_blurCenter     +0x30  Vector2  (ctor {0.5, 0.5})
//     m_blurOpacity    +0x40  f32      (ctor 1.0)
//     m_blurVelocity   +0x44  f32      (ctor 1.0)
//     m_blendSharpMUL  +0x48  f32      (ctor 0.0; set by SetBlendSharpness)
//     m_blendSharpADD  +0x4C  f32      (ctor 0.0; set by SetBlendSharpness)
//     m_blendNoise     +0x50  f32      (ctor 0.005)
//     m_blendAngle     +0x54  f32      (ctor 0.0)

namespace rw
{
namespace graphics
{
namespace postfx
{
    struct B4Blur
    {
        struct State
        {
            rw::math::vpu::Vector2 m_blendAmount;    // +0x00
            rw::math::vpu::Vector2 m_blurAmount;     // +0x10
            rw::math::vpu::Vector2 m_blendCenter;    // +0x20
            rw::math::vpu::Vector2 m_blurCenter;     // +0x30
            f32                    m_blurOpacity;    // +0x40
            f32                    m_blurVelocity;   // +0x44
            f32                    m_blendSharpMUL;  // +0x48
            f32                    m_blendSharpADD;  // +0x4C
            f32                    m_blendNoise;     // +0x50
            f32                    m_blendAngle;     // +0x54

            // X360 0x823F52E0 -- default-construct the blur state with its baseline controls.
            State();

            // X360 0x823F53A8 -- derive the blend-sharpness MUL/ADD pair from a [-1,1] sharpness
            // value: maps it to [0,1], raises to the 16th power, scales by 500, and stores the
            // additive/multiplicative gradient terms the blur shader applies.
            void SetBlendSharpness(f32 lfSharpness);
        };

        // rwgpfxb4blur.h:126-132 (DWARF) -- the construction parameter block. The X360 constructor
        // reads THREE of its members (`*a2` the allocator, `a2[1]` the blend state, `a2 + 4` the
        // 96-byte state), which is why the constructor takes the block and not its first word.
        struct Parameters
        {
            rw::IResourceAllocator*           m_allocator;          // rwgpfxb4blur.h:128
            // rwgpfxb4blur.h:129. X360 dword_83010F74 == CgsBlendStateFactory::saBlendStates[1] ==
            // E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA (CgsBlendStateFactory.h:151).
            // IDENTIFIED, not unattested -- but the factory's table is a private static reached only
            // through a non-static accessor, so BrnPostFx::Construct cannot name it yet and leaves
            // this member at whatever Parameters::Parameters() seeds. See this edit's note.
            renderengine::BlendMaterialState* m_scatterBlendState;
            State                             m_state;              // rwgpfxb4blur.h:130

            Parameters();                                           // rwgpfxb4blur.h:132
        };

        // X360 0x823FE9C8 -- construct the B4 blur into `this` from its parameter block.
        // ⚠ WAS `B4Blur(rw::IResourceAllocator**)`. The DWARF (rwgpfxb4blur.h:196) and the asm agree
        // on `const Parameters&`: BrnPostFx::Construct passes the block's base @0x8240A3C0 and the
        // callee reads three different members out of it. `&params.m_allocator` spelt the same
        // address by accident; it could not name the other two.
        explicit B4Blur(const Parameters& lrParameters);
        // The console publishes a freshly built State into the effect with a raw 0x60-byte copy over
        // m_state (BrnRendererModule::Render @0x8240BFA8, `li r5, 0x60` / `addi r4, r20, 0x3F0` /
        // `lwz r3, 0x5C8(r20)` / `bl memcpy` @0x8240DC30-0x8240DC50). m_state is member 0, so the
        // memcpy IS an assignment; expressed as one so no caller pokes a private member.
        void SetState(const State& lrState);

        // --- SHAPE (DWARF rwgpfxb4blur.h:254-308). Every offset in the comments is CONFIRMED by the
        // X360 constructor's store map, and the total (0x124 -> 0x130 at the class's 16-byte
        // alignment) is exactly the carve size BrnPostFx::Construct requests (`li r11, 0x130`).
        //
        // ⚠ THIS IS ALSO A LIVE FIX. Until now B4Blur was an EMPTY class, so `sizeof(B4Blur)` was 1
        // and BrnPostFx::Construct -- which sizes its carve from `sizeof` -- asked the allocator for
        // ONE BYTE before placement-new'ing the object into it. Modelling the members repairs the
        // carve automatically.
    private:
        State                               m_state;                            // +0x00  :254
        renderengine::ProgramBufferData*    m_blurProgram;                      // +0x60  :256
        renderengine::ProgramBufferData*    m_downsampleProgram;                // +0x64  :257
        rw::Resource                        m_rasterizerStateResource;          // +0x68  :259
        renderengine::RasterizerState*      m_rasterizerState;                  // +0x7C  :260
        renderengine::ProgramBufferData*    m_textureProgram;                   // +0x80  :262
        renderengine::ProgramVariableHandle m_blendFactorTextureHandle;         // +0x84  :263
        renderengine::ProgramVariableHandle m_sampleOffsetHandle;               // +0x88  :265
        renderengine::ProgramVariableHandle m_controlScalarsHandle;             // +0x8C  :266
        rw::Resource                        m_quadVertexDescriptorResource;     // +0x90  :271
        renderengine::VertexDescriptorData* m_quadVertexDescriptor;             // +0xA4  :272
        rw::Resource                        m_quadDepthStencilStateResource;    // +0xA8  :274
        renderengine::DepthStencilState*    m_quadDepthStencilState;            // +0xBC  :275
        renderengine::ProgramBufferData*    m_quadVertexProgram;                // +0xC0  :277
        rw::Resource                        m_scatterVertexDescriptorResource;  // +0xC4  :282
        renderengine::VertexDescriptorData* m_scatterVertexDescriptor;          // +0xD8  :283
        rw::Resource                        m_scatterDepthStencilStateResource; // +0xDC  :285
        renderengine::DepthStencilState*    m_scatterDepthStencilState;         // +0xF0  :286
        renderengine::ProgramBufferData*    m_scatterVertexProgram;             // +0xF4  :288
        renderengine::ProgramVariableHandle m_scatterUVOffsetHandle;            // +0xF8  :289
        renderengine::ProgramBufferData*    m_scatterPixelProgram;              // +0xFC  :291
        renderengine::ProgramVariableHandle m_scatterScalars1Handle;            // +0x100 :292
        renderengine::ProgramVariableHandle m_scatterScalars2Handle;            // +0x104 :293
        renderengine::BlendMaterialState*   m_scatterBlendState;                // +0x108 :295
        renderengine::ProgramBufferData*    m_radialVertexProgram;              // +0x10C :299
        renderengine::ProgramVariableHandle m_radialUVOffsetHandle;             // +0x110 :300
        renderengine::ProgramBufferData*    m_radialPixelProgram;               // +0x114 :302
        renderengine::ProgramVariableHandle m_radialScalars1Handle;             // +0x118 :303
        renderengine::ProgramVariableHandle m_radialScalars2Handle;             // +0x11C :304
        rw::IResourceAllocator*             m_allocator;                        // +0x120 :308
    };
}
}
}

#endif // RW_GPFX_B4BLUR_H
