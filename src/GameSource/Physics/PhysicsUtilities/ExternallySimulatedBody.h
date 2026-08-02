#pragma once

// BrnPhysics::ExternallySimulatedBody -- the base of the physics-body hierarchy: a body whose
// transform/velocity are integrated externally (by the RenderWare physics sim) and mirrored
// here. Minimal OWNING reconstruction: the member SEQUENCE is verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../PhysicsUtilities/ExternallySimulatedBody.h:131-134), and
// only the members the BrnPhysics-bodies group's functions touch are bodied; the trivial
// accessors are declared (and the few touched ones inlined) so derived classes
// (ExternalPhysicsBody / SimpleVehiclePhysics) compile against them BY NAME.
//
// LAYOUT -- CORRECTED (2026-08-02). The note that stood here claimed a `vptr @ +0` with
// mTransform @ +16 / mLinearVelocity @ +80 / mAngularVelocity @ +96, and then had to write
// "`this+0x50`==+80? NO -- see note" because its own model contradicted the asm. It was
// describing the DERIVED (SimpleVehiclePhysics) frame, not this class's own.
//
// This class is NOT polymorphic on the console. VERIFIED, four independent ways:
//   1. ExternallySimulatedBody::Construct @0x8259CF28 stores the identity rows straight to
//      `r3+0 / +0x10 / +0x20 / +0x30`, the two zero velocity registers to `r3+0x40 / +0x50`,
//      and the zero byte to `r3+0x60`. There is no vptr slot and no vptr store.
//   2. ExternalPhysicsBody's own members continue at exactly +0x70 (mLocalInverseInertia,
//      confirmed by ReadPropertiesFromRenderware @0x825A2280), +0xA0, +0xD0 and +0xE0..+0x110
//      (the four accumulators, confirmed by the four AddWorldSpace* funcs AND by
//      ExternalPhysicsBody::Construct @0x825A1598). Those only close if this base is exactly
//      0x70 bytes and starts at offset 0.
//   3. CalculateNewVelocity @0x825A1B10 fires "rw::math::IsValid(mAngularVelocity)" on the
//      register at `this+0x50` and "rw::math::IsValid(mLinearVelocity)" on `this+0x40`.
//   4. IntegrateTransform @0x825A7930 does `pos(this+0x30) += linVel(this+0x40) * dt` and
//      spins the three rotation rows (this+0x00/0x10/0x20) by omega(this+0x50).
//
//   THIS class's own frame:            in a SimpleVehiclePhysics/VehiclePhysics object:
//     mTransform       @ +0x00           +0x10   (the leaf's vptr occupies +0x00)
//     mLinearVelocity  @ +0x40           +0x50
//     mAngularVelocity @ +0x50           +0x60
//     mbFrozen         @ +0x60           +0x70
//     sizeof           == 0x70
//
// The vptr belongs to SimpleVehiclePhysics, which is where the first virtuals are introduced
// (ClearCrashing/SetCrashing) -- which is why VehiclePhysics.cpp sees the body subobject as
// `addi r3, this, 0x10` when it calls ExternalPhysicsBody::AddLocalForce, and why
// VehiclePhysics.h's own annotations say "mLinearVelocity, base +0x50" and "mAngularVelocity
// is the +0x60 register". Both views are consistent; only the old comment was not.
//
// The destructor is therefore NOT virtual here (it was, which added a host vptr this class
// does not have and put an unwritten vtable head on a non-polymorphic type). With that gone
// the whole base chain is pointer-free and the console offsets reproduce EXACTLY on x64 --
// asserted in ExternallySimulatedBody_embed_check.cpp. Members are still pinned BY NAME +
// SEQUENCE; the offset asserts are a bonus that this particular chain happens to allow, not
// the parity rule.

#include "types.hpp"
#include "BrnCommonTypes.h"        // Vector3, Matrix44Affine
#include "rw/physics/rigidbody.h"  // rw::physics::RigidBody (ReadFromRenderware source body)

namespace BrnPhysics
{
    // The base body. Reconstructed only as far as the body funcs in this group need; the full
    // set of Construct/Release/RenderWare-IO/accessor methods is a separate future TU and is
    // declared-only here (no bodies) so this header can serve as the shared owning home.
    class ExternallySimulatedBody
    {
    public:
        // Host-side default init only. The console never relies on a constructor -- it calls
        // Construct() @0x8259CF28 explicitly -- but a default-constructed body must not carry
        // garbage velocities, so this mirrors exactly what Construct() stores (identity rows,
        // zero linear/angular velocity, not frozen). Kept INLINE and self-contained so a
        // translation unit can hold one of these without dragging in the .cpp.
        ExternallySimulatedBody() : mbFrozen(false)
        {
            mTransform.SetIdentity();
            mLinearVelocity.SetZero();
            mAngularVelocity.SetZero();
        }
        // NOT virtual -- see the LAYOUT note above (the console class has no vptr).
        ~ExternallySimulatedBody() {}

        // --- accessors touched/needed by callers; trivial inline bodies (DWARF :87-127) ---
        Vector3        GetPosition() const            { return mTransform.wAxis; }
        Matrix44Affine GetTransform() const           { return mTransform; }
        Vector3        GetLinearVelocity() const       { return mLinearVelocity; }
        Vector3        GetAngularVelocity() const       { return mAngularVelocity; }
        bool           IsFrozen() const               { return mbFrozen; }
        void           SetTransform(Matrix44Affine lTransform) { mTransform = lTransform; }
        void           SetLinearVelocity(Vector3 lV)  { mLinearVelocity = lV; }
        void           SetAngularVelocity(Vector3 lV) { mAngularVelocity = lV; }
        void           SetFrozen(bool lb)             { mbFrozen = lb; }

        // --- bodied by the Physics-IO-util group (ExternallySimulatedBody.cpp) ---
        // @0x825A2E88: mirror the RenderWare physics body's pose+velocity into this body. When
        // mbFrozen, leave everything untouched. ADDITIVE GROW (flagged): this RenderWare-IO
        // method was declared-only's sibling-set before; only ReadFromRenderware is added here
        // for the group's ledger func. The remaining Write*/Read*RenderWare methods stay owned
        // by a future TU and are still NOT declared.
        void ReadFromRenderware(const rw::physics::RigidBody* lpRigidBody);

        // --- the four lifecycle funcs, bodied in ExternallySimulatedBody.cpp ---
        // @0x8259CF28 / @0x8259CFF8 / @0x8259D0C0 / @0x8259D190. All four are the SAME body on
        // the console (instruction-for-instruction; only Prepare additionally returns 1): reset
        // the transform to identity, zero both velocity registers and clear mbFrozen.
        void Construct();
        void Destruct();
        bool Prepare();
        void Release();

        // --- declared-only (owned by a future ExternallySimulatedBody TU) ---
        void Translate(Vector3);
        void GetInverseTransform(Matrix44Affine&) const;

    protected:
        Matrix44Affine mTransform;          // :131
        Vector3        mLinearVelocity;     // :132
        Vector3        mAngularVelocity;    // :133
        bool           mbFrozen;            // :134
    };
}
