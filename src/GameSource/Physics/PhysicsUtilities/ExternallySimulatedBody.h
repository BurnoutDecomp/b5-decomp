#pragma once

// BrnPhysics::ExternallySimulatedBody -- the base of the physics-body hierarchy: a body whose
// transform/velocity are integrated externally (by the RenderWare physics sim) and mirrored
// here. Minimal OWNING reconstruction: the member SEQUENCE is verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../PhysicsUtilities/ExternallySimulatedBody.h:131-134), and
// only the members the BrnPhysics-bodies group's functions touch are bodied; the trivial
// accessors are declared (and the few touched ones inlined) so derived classes
// (ExternalPhysicsBody / SimpleVehiclePhysics) compile against them BY NAME.
//
// LAYOUT (X360, confirmed against the asm of the body funcs that index this base):
//   vptr            @ +0   (polymorphic base: GetSteeringAngle/ClearCrashing/SetCrashing are
//                           virtual in the SimpleVehiclePhysics leaf)
//   mTransform      @ +16  (Matrix44Affine, 64 bytes; rows xAxis/yAxis/zAxis/wAxis at
//                           +16/+32/+48/+64) -- the graphics-transform get/set and the
//                           SimpleVehiclePhysics transform funcs index exactly these rows.
//   mLinearVelocity @ +80
//   mAngularVelocity@ +96  (DampPitchYawRoll / DampenAngularVelocity load+store this at
//                           `this+0x50`==+80? NO -- see note) ...
//
// POINTER-WIDTH DIVERGENCE (flagged): the X360 vptr is 4 bytes, the host vptr is 8 bytes, so
// the absolute member offsets above are the *console* offsets and are NOT reproduced on the
// 64-bit host. Per project rule members are pinned BY NAME + SEQUENCE; no absolute-offset
// static_assert is placed across the vptr. The asm `addi rN, this, 0x50` resolves to
// mAngularVelocity in the console layout (the damp funcs operate on the angular velocity).

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
        ExternallySimulatedBody() : mbFrozen(false) { mTransform.SetIdentity(); }
        virtual ~ExternallySimulatedBody() {}

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

        // --- declared-only (owned by a future ExternallySimulatedBody TU) ---
        void Construct();
        void Destruct();
        bool Prepare();
        void Release();
        void Translate(Vector3);
        void GetInverseTransform(Matrix44Affine&) const;

    protected:
        Matrix44Affine mTransform;          // :131
        Vector3        mLinearVelocity;     // :132
        Vector3        mAngularVelocity;    // :133
        bool           mbFrozen;            // :134
    };
}
