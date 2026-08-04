#pragma once

// =====================================================================================
// rw::physics::Inertia -- the shared mass/inertia property block a RenderWare physics
// RigidBody points at. One Inertia can be shared by many bodies (it is a *description*,
// not per-body state): the local inverse inertia diagonal, the inverse mass, and the
// per-body velocity clamps and drag coefficients the integrator applies every tick.
//
// PROVENANCE
//   NAMES / TYPES / ORDER : DecFIGS DWARF --
//     references/DecFIGS/dwarfdump/SDKs/EATech/include/cmn/rw/physics/inertia.h:176..182.
//   OFFSETS + WHICH FIELD DOES WHAT : BURNOUT_X360_ARTIST.XEX
//     rw::physics::RigidBody::DynamicUpdate @0x82BC2B78, read this wave. It loads the
//     Inertia through RigidBody+0x5C and touches exactly five fields:
//       lfs  0x14(inertia)  -> the sleep-energy angular scale   (mSpherical)
//       lfs  0x18(inertia)  -> squared, the LINEAR speed clamp  (mMaxVelocity)
//       lfs  0x1C(inertia)  -> squared, the ANGULAR speed clamp (mMaxOmega)
//       lvlx +0x20          -> 1 - x, the LINEAR damping factor (mLinearDrag)
//       lvlx +0x24          -> 1 - x, the ANGULAR damping factor(mAngularDrag)
//     (0x82BC2E38 clamps the ANGULAR magnitude first, 0x82BC2EA4 the linear one second.)
//
// ⚠️ THE FIRST MEMBER IS A 16-BYTE REGISTER, NOT A PACKED 12-BYTE TRIPLE. A prior draft
//    (#124) read `Vector3 mInvTens` as 12 bytes and shifted every following field down by
//    4, which put the angular clamp on `mLinearDrag` and the linear damping on
//    `mMaxOmega` -- i.e. it would have compiled, linked, run and damped the wrong axis
//    with nothing asserting. The five X360 offsets above settle it: mInvTens occupies
//    +0x00..+0x0F and mInvMass starts at +0x10.
//
// LAYOUT NOTE (x64): the console block is 0x28 = 40 bytes. rw::math::vpu::Vector3 is
// `alignas(16)`, so the PC type rounds up to 48. Nothing embeds an Inertia by value in a
// serialised record (RigidBody holds a POINTER), so the tail padding is inert; parity here
// is by NAMED MEMBER, not by sizeof.
// =====================================================================================

#include "types.hpp"             // f32
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector3

namespace rw
{
namespace physics
{

    class Inertia
    {
    public:
        // DWARF accessors (inertia.h:110..134). All of these are inlined away on the
        // console -- no standalone X360 symbol exists for any of them.
        const rw::math::vpu::Vector3& GetInverseInertia() const   { return mInvTens; }
        const f32& GetInverseMass() const                         { return mInvMass; }
        const f32& GetSphericalInertia() const                    { return mSpherical; }
        const f32& GetMaxLinearVelocity() const                   { return mMaxVelocity; }
        const f32& GetMaxAngularVelocity() const                  { return mMaxOmega; }
        const f32& GetLinearDrag() const                          { return mLinearDrag; }
        const f32& GetAngularDrag() const                         { return mAngularDrag; }

        void SetInverseInertia(const rw::math::vpu::Vector3& lrV) { mInvTens = lrV; }
        void SetInverseMass(f32 lfV)                              { mInvMass = lfV; }
        void SetMaxLinearVelocity(f32 lfV)                        { mMaxVelocity = lfV; }
        void SetMaxAngularVelocity(f32 lfV)                       { mMaxOmega = lfV; }
        void SetLinearDrag(f32 lfV)                               { mLinearDrag = lfV; }
        void SetAngularDrag(f32 lfV)                              { mAngularDrag = lfV; }

    private:
        rw::math::vpu::Vector3 mInvTens;      // :176  +0x00  local inverse inertia diagonal
        f32                    mInvMass;      // :177  +0x10
        f32                    mSpherical;    // :178  +0x14  sleep-energy angular scale
        f32                    mMaxVelocity;  // :179  +0x18  linear speed clamp
        f32                    mMaxOmega;     // :180  +0x1C  angular speed clamp
        f32                    mLinearDrag;   // :181  +0x20
        f32                    mAngularDrag;  // :182  +0x24
    };

} // namespace physics
} // namespace rw
