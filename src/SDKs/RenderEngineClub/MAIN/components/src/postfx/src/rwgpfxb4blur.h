#ifndef RW_GPFX_B4BLUR_H
#define RW_GPFX_B4BLUR_H

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::IResourceAllocator (Parameters::m_allocator)
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector2

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

        // rwgpfxb4blur.h:126-132 (DWARF) -- the construction parameter block. Leading member is the
        // allocator, so &Parameters::m_allocator is the `rw::IResourceAllocator**` the constructor
        // below takes.
        struct Parameters
        {
            rw::IResourceAllocator* m_allocator;          // rwgpfxb4blur.h:128
            // rwgpfxb4blur.h:129. X360 dword_83010F74 == CgsBlendStateFactory::saBlendStates[1] ==
            // E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA (CgsBlendStateFactory.h:151).
            // IDENTIFIED, not unattested -- but the factory's table is a private static reached only
            // through a non-static accessor, so BrnPostFx::Construct cannot name it yet and leaves
            // this member at whatever Parameters::Parameters() seeds. See this edit's note.
            void*                   m_scatterBlendState;
            State                   m_state;              // rwgpfxb4blur.h:130

            Parameters();                                 // rwgpfxb4blur.h:132
        };

        // X360 0x823FE9C8 -- construct the B4 blur into `this` from the parameter block's address.
        explicit B4Blur(rw::IResourceAllocator** lppParameters);
    };
}
}
}

#endif // RW_GPFX_B4BLUR_H
