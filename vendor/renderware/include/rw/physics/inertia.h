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
// is by NAMED MEMBER, not by sizeof. The 48 IS however the X360-attested ARRAY STRIDE
// (CgsPhysics::RigidBodyData::GetInertia @0x8289D0C0 indexes maInertias at 48*(idx+50)),
// which is why _rw_physics_Inertia_AssertLayout below pins sizeof as well as the members.
//
// ⭐ SOLE OWNER OF THIS RECORD SINCE 2026-08-04 (task #141). `CgsPhysics::Inertia` used to
// be a SECOND, independent definition of these same seven fields, cast across at
// PhysicsSimulationModule::ProcessAddRigidBodyQueue. It is now a plain alias onto this
// class -- see the block at CgsPhysicsSimulationModule.h. ⛔ Do not re-introduce a local
// copy of this layout anywhere: `Inertia` appears in no mangled name that encodes its
// definition, so a body compiled against one copy links CLEANLY against a call site
// compiled against the other. The link succeeding is not evidence of anything.
// =====================================================================================

#include "types.hpp"             // f32
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector3
#include <cfloat>                // FLT_MAX -- the ctor's two velocity clamps
#include <cstddef>               // offsetof (the layout pin below)

namespace rw
{
namespace physics
{

    class Inertia
    {
    public:
        // DWARF inertia.h:80. NO STANDALONE X360 SYMBOL -- the console inlines it, and the
        // one place it is inlined is also where its constants were read:
        // CgsPhysics::RigidBodyData::RigidBodyData @0x827DB728 (0x70 bytes), a 200-pass
        // loop at stride 0x30 over &maInertias[0]. The three float pool constants were read
        // out of the image as raw bytes, not inferred from role:
        //   flt_82001C98 = 3f800000 = 1.0f    (f0)
        //   flt_820CD79C = 7f7fffff = FLT_MAX (f12)
        //   flt_82001CC0 = 00000000 = 0.0f    (f13)
        // The 16-byte stack vector the loop lvx128/stvx128's into mInvTens is built from
        // three `stfs f0` plus one `stw 0`, i.e. {1.0f, 1.0f, 1.0f, 0.0f} -- lane 3 is the
        // Vector3 pad and the console does write it.
        //
        // ⚠️ THIS CTOR IS WHY THERE IS NO `SetSphericalInertia`. The DWARF declares SEVEN
        // getters and only SIX setters; mSpherical is the one with no setter. A previous
        // note (and the task brief) claimed the de-fork "needs SetSphericalInertia added" --
        // it does not, and adding it would have been a fabricated SDK entry point. mSpherical
        // is written HERE and read by DynamicUpdate's sleep-energy term; nothing else in the
        // tree ever writes it. Checked by grep over the whole repo, not assumed.
        Inertia()
            : mInvMass(1.0f)
            , mSpherical(1.0f)
            , mMaxVelocity(FLT_MAX)
            , mMaxOmega(FLT_MAX)
            , mLinearDrag(0.0f)
            , mAngularDrag(0.0f)
        {
            mInvTens.x = 1.0f;      // stack vector lane 0 -> stvx128 to +0x00
            mInvTens.y = 1.0f;      // lane 1
            mInvTens.z = 1.0f;      // lane 2
            mInvTens.w = 0.0f;      // lane 3 (the Vector3 pad; `stw 0`)
        }

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
        // Never-called layout pin, so any drift in a member offset is a compile error.
        // (The `friend` is what lets offsetof reach these private members from outside --
        // the CHistogram.h / CArrayLookahead.h / CSeqDissolveDetector.h precedent.)
        friend void _rw_physics_Inertia_AssertLayout();

        rw::math::vpu::Vector3 mInvTens;      // :176  +0x00  local inverse inertia diagonal
        f32                    mInvMass;      // :177  +0x10
        f32                    mSpherical;    // :178  +0x14  sleep-energy angular scale
        f32                    mMaxVelocity;  // :179  +0x18  linear speed clamp
        f32                    mMaxOmega;     // :180  +0x1C  angular speed clamp
        f32                    mLinearDrag;   // :181  +0x20
        f32                    mAngularDrag;  // :182  +0x24
    };

    // RELOCATED HERE 2026-08-04 (task #141) from CgsPhysicsSimulationModule.cpp:225..231,
    // where they pinned the now-retired `CgsPhysics::Inertia` copy. They belong next to the
    // definition they describe: pins that live in a different TU from the layout they gate
    // are pins that stop gating the moment someone edits the other copy.
    //
    // Every offset is X360-attested TWICE over, by two functions that never see each other:
    //   * the stores in CgsPhysics::RigidBodyData::RigidBodyData @0x827DB728 (the ctor above)
    //   * the loads in rw::physics::RigidBody::DynamicUpdate  @0x82BC2B78 (see the header
    //     comment: 0x14/0x18/0x1C scalar, 0x20/0x24 via lvlx)
    // The 0x00 and 0x10 seats come from the ctor's `stvx128 v0,r0,r11` + `stfs f0,0x10(r11)`.
    //
    // ⚠️ ADJACENCY, NOT JUST A TOTAL SIZE. The last two pins gate mAngularDrag against
    // `mLinearDrag + sizeof(f32)`, because a total-`sizeof` assert on this type is absorbed
    // by the 8 bytes of tail padding the leading alignas(16) Vector3 forces -- i.e. a total
    // check would sit there GREEN through a 2-field defect. Tamper-tested: moving any single
    // member fails at least one pin.
    inline void _rw_physics_Inertia_AssertLayout()
    {
        static_assert(offsetof(Inertia, mInvTens)     == 0x00, "mInvTens @+0x00 (ctor stvx128 v0,r0,r11)");
        static_assert(offsetof(Inertia, mInvMass)     == 0x10, "mInvMass @+0x10 (ctor stfs f0(1.0f),0x10(r11))");
        static_assert(offsetof(Inertia, mSpherical)   == 0x14, "mSpherical @+0x14 (ctor stfs f0(1.0f),0x14; DynamicUpdate lfs 0x14)");
        static_assert(offsetof(Inertia, mMaxVelocity) == 0x18, "mMaxVelocity @+0x18 (ctor stfs f12(FLT_MAX); DynamicUpdate lfs 0x18)");
        static_assert(offsetof(Inertia, mMaxOmega)    == 0x1C, "mMaxOmega @+0x1C (ctor stfs f12(FLT_MAX); DynamicUpdate lfs 0x1C)");
        static_assert(offsetof(Inertia, mLinearDrag)  == 0x20, "mLinearDrag @+0x20 (ctor stfs f13(0.0f); DynamicUpdate lvlx +0x20)");
        static_assert(offsetof(Inertia, mAngularDrag) == 0x24, "mAngularDrag @+0x24 (ctor stfs f13(0.0f); DynamicUpdate lvlx +0x24)");

        // Adjacency form of the same six scalars -- survives a member WIDENING, which the
        // absolute offsets above would too, but which a total-size check would not.
        static_assert(offsetof(Inertia, mInvMass)     == offsetof(Inertia, mInvTens)     + sizeof(rw::math::vpu::Vector3), "mInvMass follows mInvTens");
        static_assert(offsetof(Inertia, mSpherical)   == offsetof(Inertia, mInvMass)     + sizeof(f32), "mSpherical follows mInvMass");
        static_assert(offsetof(Inertia, mMaxVelocity) == offsetof(Inertia, mSpherical)   + sizeof(f32), "mMaxVelocity follows mSpherical");
        static_assert(offsetof(Inertia, mMaxOmega)    == offsetof(Inertia, mMaxVelocity) + sizeof(f32), "mMaxOmega follows mMaxVelocity");
        static_assert(offsetof(Inertia, mLinearDrag)  == offsetof(Inertia, mMaxOmega)    + sizeof(f32), "mLinearDrag follows mMaxOmega");
        static_assert(offsetof(Inertia, mAngularDrag) == offsetof(Inertia, mLinearDrag)  + sizeof(f32), "mAngularDrag follows mLinearDrag");

        // The X360-attested ARRAY STRIDE (GetInertia @0x8289D0C0: 48*(idx+50)). This one IS
        // a total size, and it is legitimate here because it is a stride claim about
        // CgsPhysics::RigidBodyData::maInertias[200], not a field-coverage claim.
        static_assert(sizeof(Inertia) == 48, "Inertia array stride 48 (GetInertia 48*(idx+50))");
    }

} // namespace physics
} // namespace rw
