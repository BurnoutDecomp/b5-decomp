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
// innerColour / outerColour), and seeds the per-frame State. SetState recomputes the shader-constant
// vectors from a State (the inner/outer colours and the gradient sharpness MUL/ADD pair).
//
// SHAPE: the member offsets are X360-asm authoritative (Vignette::Vignette @0x82403DB8 stores into r30
// at the offsets noted; SetState @0x823FE8B0 writes the State block + the controlScalars float). No
// DWARF/leak source models the class body, so the layout is the constructor's store map. The four
// 5-word handle gaps (+0x54/+0x68/+0x7C/+0x90) hold each render state's rw::Resource backing memory the
// constructor allocates then Initialize's.

namespace rw
{
namespace graphics
{
namespace postfx
{
    class Vignette
    {
    public:
        // The per-frame vignette controls SetState packs into the shader constants. The X360 SetState
        // reads the inner/outer colour vectors (each a Vector4) and a leading sharpness scalar; the
        // four leading Vector4 lanes mirror the constant block written into the object's State at +0x00.
        struct State
        {
            rw::math::vpu::Vector4 mInnerColour;   // +0x00
            rw::math::vpu::Vector4 mOuterColour;   // +0x10
            rw::math::vpu::Vector4 mControlA;      // +0x20
            rw::math::vpu::Vector4 mControlB;      // +0x30
            f32                    mfSharpness;    // +0x40 (the [-1,1] sharpness, SetState's mul/add src)
        };

        // X360 0x82403DB8 -- construct the vignette into `this`. lppParameters[0] is the rw resource
        // allocator the effect keeps and hands to every renderengine resource it builds;
        // lppParameters[1..] is the initial State SetState seeds from.
        // rwgpfxvignette.h:87-92 (DWARF) -- the construction parameter block. Its LEADING member is
        // the allocator, which is why the constructor above can take the block's address spelled as
        // `rw::IResourceAllocator**`: &Parameters::m_allocator is exactly that pointer.
        struct Parameters
        {
            rw::IResourceAllocator* m_allocator;   // rwgpfxvignette.h:89
            State                   m_state;       // rwgpfxvignette.h:90

            // rwgpfxvignette.h:92 -- X360 rw__graphics__postfx__Vignette__Parameters__Parameters
            // @0x823F5420. Seeds the block; BrnPostFx::Construct then overwrites m_allocator only.
            Parameters();
        };

        explicit Vignette(rw::IResourceAllocator** lppParameters);

        // X360 0x823FE8B0 -- recompute the vignette shader-constant vectors from `lrState`: the inner
        // and outer colour vectors and the gradient sharpness gradient (mul = sharpness*500+1, add =
        // -sharpness*500) the pixel program applies.
        void SetState(const State& lrState);

        // X360 0x823F93D0 -- release every renderengine resource the vignette owns.
        // rwgpfxvignette.h:158 (DWARF). Called out-of-line by BrnPostFx::Destruct @0x824081FC.
        void Release();

    private:
        // The vignette State constant block (the four Vector4 lanes + the sharpness scalar SetState
        // writes at +0x00..+0x40).
        State                               m_state;                     // +0x00

        u8                                  mau8Pad44[16];               // +0x44 (align to the +0x54 handles)

        // The four render-state resource backing blocks (each the rw::Resource the allocator carves;
        // the constructor copies the allocation result in, then Initialize fills it). X360 +0x54/+0x68/
        // +0x7C/+0x90, 5 words each.
        rw::Resource                        m_blendStateResource;        // +0x54
        rw::Resource                        m_vertexDescriptorResource;  // +0x68
        rw::Resource                        m_depthStencilStateResource; // +0x7C
        rw::Resource                        m_rasterizerStateResource;   // +0x90

        renderengine::ProgramBufferData*    mpVertexProgram;             // +0xA4 (this[41])
        renderengine::ProgramBufferData*    mpPixelProgram;              // +0xA8 (this[42])
        void*                               mpBlendState;                // +0xB0 (this[44])
        void*                               mpRasterizerState;           // +0xB4 (this[45])
        void*                               mpVertexDescriptor;          // +0xB8 (this[46])
        void*                               mpDepthStencilState;         // +0xBC (this[47])
        rw::IResourceAllocator*             mpAllocator;                 // +0xC0 (this[48])

        renderengine::ProgramVariableHandle mControlScalarsHandle;       // +0x110 (this[68])
        renderengine::ProgramVariableHandle mInnerColourHandle;          // +0x120 (this[72])
        renderengine::ProgramVariableHandle mOuterColourHandle;          // +0x130 (this[76])
        renderengine::ProgramVariableHandle mUvOffsetHandle;             // +0x140 (this[80])
    };
}
}
}

#endif // RW_GPFX_VIGNETTE_H
