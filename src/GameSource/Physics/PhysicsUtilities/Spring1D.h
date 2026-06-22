#pragma once

// BrnPhysics::Spring1D -- a scalar (1-dimensional) damped-spring solver. Used by the camera
// behaviour rig (BrnDirector::Camera::BehaviourRig) to spring a single length toward a desired
// value with stiffness/dampening and min/max travel clamps.
//
// MINIMAL OWNING reconstruction at the DWARF home (PhysicsUtilities/Spring1D.h). The member
// SEQUENCE + names are verbatim from the DecFIGS DWARF (Spring1D.h:293-301); the layout is a
// flat run of nine f32 scalars (no vptr -- the struct is non-polymorphic in the DWARF and the
// Construct/Prepare/Update asm index it as `result[0..8]` with a 4-byte stride). The three
// ledger funcs (Construct/Prepare/Update) are bodied in Spring1D.cpp against these members BY
// NAME; the remaining API (Release/Destruct/getters/setters/Reset) is declared-only and owned
// by a future TU -- the trivial inline accessors needed by no caller in this group are left as
// declarations so the home stays honest.
//
// SuspensionSpring (the SIMD sibling that the DWARF also homes here) is a DISTINCT type owned by
// its own future TU and is intentionally NOT reconstructed here.

#include "types.hpp"   // f32

namespace BrnPhysics
{
    struct Spring1D
    {
        // --- the 3 ledger funcs bodied in Spring1D.cpp ---

        // @0x8259CE48: zero every parameter except mfMass (set to 1.0 so Prepare's divide by
        // mass is safe before Prepare runs).
        void Construct();

        // @0x8259CE80: seed the spring's desired/current length, mass, stiffness, dampening and
        // the min/max stretch travel; zero the running velocity + accumulated force. Returns
        // true (the X360 always returns 1).
        bool Prepare(f32 lfDesiredLength, f32 lfCurrentLength, f32 lfMass, f32 lfStiffness,
                     f32 lfDampening, f32 lfMinStretch, f32 lfMaxStretch);

        // @0x8259CEB8: advance the spring one timestep. Integrates Hooke's law with viscous
        // dampening, then clamps the current length into [mfMinStretch, mfMaxStretch].
        void Update(f32 lfTimeStep);

        // --- declared-only (owned by a future Spring1D TU) ---
        void Release();
        void Destruct();
        f32  GetLength() const;
        f32  GetOffset() const;
        f32  GetVelocity() const;
        f32  GetOffsetRatio() const;
        f32  GetMaxStretch() const;
        void SetStiffness(f32);
        void SetDampening(f32);
        void SetVelocity(f32);
        void SetDesiredLength(f32);
        void SetMaxStretch(f32);
        void SetMinStretch(f32);
        void AddForce(f32);
        void Reset();

    private:
        f32 mfDesiredLength;       // :293  result[0] @ +0x00
        f32 mfCurrentLength;       // :294  result[1] @ +0x04
        f32 mfMass;                // :295  result[2] @ +0x08
        f32 mfVelocity;            // :296  result[3] @ +0x0C
        f32 mfStiffness;           // :297  result[4] @ +0x10
        f32 mfTotalForce;          // :298  result[5] @ +0x14
        f32 mfDampeningConstant;   // :299  result[6] @ +0x18
        f32 mfMaxStretch;          // :300  result[7] @ +0x1C
        f32 mfMinStretch;          // :301  result[8] @ +0x20
    };
}
