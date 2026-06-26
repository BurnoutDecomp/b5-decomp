#ifndef RW_GPFX_B4BLUR_H
#define RW_GPFX_B4BLUR_H

#include "types.hpp"
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
    };
}
}
}

#endif // RW_GPFX_B4BLUR_H
