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
// SuspensionSpring (the SIMD sibling that the DWARF also homes here) is a DISTINCT type. Its
// state-setter API (Set*) is bodied in SuspensionSpring.cpp; the remaining declared-only methods
// are owned by a future TU.

#include "types.hpp"          // f32
#include "BrnCommonTypes.h"   // Vector4

namespace BrnPhysics
{
    // Forward declaration only, so SuspensionSpring can name its friend (see the ACCESS NOTE on
    // that class). `struct` matches the DecFIGS DWARF spelling
    // (dwarfdump/.../VehiclePhysics.h: `struct BrnPhysics::Vehicle::VehiclePhysics`) and the
    // committed VehiclePhysics.h:160. No layout is pulled in.
    namespace Vehicle { struct VehiclePhysics; }

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

    // ------------------------------------------------------------------------------------------
    // BrnPhysics::SuspensionSpring -- the SIMD (VMX) sibling of Spring1D. Where Spring1D keeps its
    // state as a flat run of FPU scalars, SuspensionSpring packs its state into three 16-byte
    // 4-lane vector registers (the console `VectorIntrinsicUnion::VectorIntrinsic`; on PC that is
    // one rw::math::vpu::Vector4 each -- same layout/offset, lanes named by .x/.y/.z/.w). Member
    // SEQUENCE + names are verbatim from the DecFIGS DWARF (Spring1D.h:147-149); the public/
    // protected method set + signatures are the DWARF declarations (Spring1D.h:42-191).
    //
    // The eight Set* state-setters are bodied in SuspensionSpring.cpp. Each console body broadcasts
    // the scalar across a VMX register and `vrlimi128`-inserts it into exactly one lane, guarded by
    // a `vcmpeqfp128. v0,v127,v127` NaN check (x==x is false only for NaN) that fires an assert on
    // an invalid (NaN) input. The lane each setter writes is pinned by the asm's store base offset
    // + vrlimi mask and matches the packed lane name in the member identifier. The rest of the API
    // (Construct/Destruct/Prepare/Reset/Integrate/ApplyForce + getters) is declared-only and owned
    // by a future TU.
    struct SuspensionSpring
    {
        // The suspension model's owner. VERIFIED, not assumed: every X360 caller of every
        // PROTECTED member of this class is a VehiclePhysics member function, and there is no
        // other caller anywhere in the image --
        //   SetExternalForce @0x825BFCA8  xrefs_to = { VehiclePhysics::UpdateSuspensionSprings
        //                                              @0x825F7AF0,
        //                                              VehiclePhysics::CalculateWeightTransfer
        //                                              @0x825F9DD0 }
        //   SetAcceleration  @0x825BF9C0  xrefs_to = { VehiclePhysics::UpdateSuspensionSprings }
        //   SetSpringForce   @0x825BFBB0  xrefs_to = { VehiclePhysics::UpdateSuspensionSprings }
        // and CalculateWeightTransfer issues `bl SuspensionSpring::SetExternalForce` OUT OF LINE
        // (0x825FA22C), so the access is a real cross-class call, not an inlined member fold.
        // A non-member, non-derived class can only make that call as a friend, so the console
        // header must have carried this declaration. It is deliberately the tightest form the
        // evidence supports (one named class), NOT a blanket friend and NOT a widening of the
        // members to public -- the DecFIGS DWARF is explicit that the three registers and the
        // four force/acceleration setters are `protected` (Spring1D.h:147-191).
        friend struct BrnPhysics::Vehicle::VehiclePhysics;

        void     Construct();
        void     Destruct();
        void     Prepare(VecFloat lvStiffness, VecFloat lvDamping, VecFloat lvMass);
        void     Reset();
        void     Integrate(VecFloat lvTimeStep);
        void     ApplyForce(VecFloat lvForce);
        VecFloat GetResultantForce() const;
        VecFloat GetStiffness() const;
        VecFloat GetDamping() const;
        VecFloat GetMass() const;
        VecFloat GetPosition() const;
        VecFloat GetVelocity() const;

        // @0x8259F318: base+0, vrlimi mask 8 -> lane 0 (.x) of the stiffness/damping/mass/position vec.
        void SetStiffness(f32 lfStiffness);
        // @0x8259F410: base+0, vrlimi mask 4 -> lane 1 (.y).
        void SetDamping(f32 lfDamping);
        // @0x8259F508: base+0, vrlimi mask 2 -> lane 2 (.z).
        void SetMass(f32 lfMass);
        // @0x8259F600: base+0, vrlimi mask 1 -> lane 3 (.w).
        void SetPosition(f32 lfPosition);
        // @0x825BF8C8: base+16, vrlimi mask 8 -> lane 0 (.x) of the velocity/accel/dampingF/springF vec.
        void SetVelocity(f32 lfVelocity);

        VecFloat GetAcceleration() const;

    protected:
        VecFloat GetDampingForce() const;
        VecFloat GetSpringForce() const;
        VecFloat GetExternalForce() const;

        // @0x825BF9C0: base+16, vrlimi mask 4 -> lane 1 (.y).
        void SetAcceleration(f32 lfAcceleration);
        // @0x825BFAB8: base+16, vrlimi mask 2 -> lane 2 (.z). ⭐ BODIED 2026-08-11 (the
        // suspension-springs wave) -- it stopped being "declared-only" the moment
        // UpdateSuspensionSprings, its ONLY caller in the image, got a body. 0x825BFAB8 is missing
        // from the IDA export set, so the lane was read out of the image bytes and calibrated
        // against the three siblings in this register; see the SuspensionSpring.cpp banner.
        void SetDampingForce(f32 lfDampingForce);
        // @0x825BFBB0: base+16, vrlimi mask 1 -> lane 3 (.w).
        void SetSpringForce(f32 lfSpringForce);
        // @0x825BFCA8: base+32, vrlimi mask 8 -> lane 0 (.x) of the external-force vec.
        void SetExternalForce(f32 lfExternalForce);

        // ACCESS NOTE -- CORRECTED (2026-08-02). The previous note here claimed "the three registers
        // are PUBLIC ... `protected` here would only force a fabricated accessor". That claim was
        // FALSE and it was never evidence: the DecFIGS DWARF records the accessibility straight out
        // of the original declaration and puts all three registers (and SetAcceleration /
        // SetDampingForce / SetSpringForce / SetExternalForce / the three force getters) under
        // `protected`. The note was also self-defeating -- it justified `public` by pointing at
        // VehiclePhysics::ApplySuspensionForces @0x825D1EE8 reading reg0.z * reg1.y from outside the
        // class, which is exactly what the `friend struct Vehicle::VehiclePhysics` above licenses.
        // The members stay protected; nothing is fabricated; no accessor was invented.
        // :147  +0x00  {x=stiffness, y=damping, z=mass, w=position}
        Vector4 mvStiffness_Damping_Mass_Position;
        // :148  +0x10  {x=velocity, y=acceleration, z=damping force, w=spring force}
        Vector4 mvVelocity_Acceleration_DampingForce_SpringForce;
        // :149  +0x20  {x=external force}
        Vector4 mvExternalForce;
    };
}
