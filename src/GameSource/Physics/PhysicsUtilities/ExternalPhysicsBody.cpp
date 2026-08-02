#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu::{IsValid, operator+, Dot, Cross, Mult, ...}
#include "rw/math/vpu/matrix44affine_operation.h"           // rw::math::vpu::OrthoNormalize3x3

#include <cmath>   // std::pow (models the VMX exp2/log2 pow-curve in the damp funcs)

// BrnPhysics::ExternalPhysicsBody -- the 7 functions owned by the BrnPhysics-bodies group.
//
// The X360 build implements every one of these with VMX128/AltiVec inline assembly that does
// not exist on the x64 host. Per project convention (cf. BrnTagPoint.cpp) the bodies below are
// the DE-SIMD'd scalar/vector equivalents written against the reconstructed members BY NAME --
// no __asm. The four AddWorldSpace* accumulators and the two transform-space details are
// recovered store-for-store; the collision-impulse solver and the two angular-velocity dampers
// are reconstructed from their recovered DATA FLOW (the VMX poly-approximation internals -- the
// exp2/log2 pow curve and the per-lane select machinery -- are MODELLED, see the flags below).

// The console frame of ExternalPhysicsBody. Every offset below was read out of the X360 asm of
// a DIFFERENT function than the one next to it, so they corroborate rather than repeat:
//   +0x70  mLocalInverseInertia   ReadPropertiesFromRenderware @0x825A2280 / CalculateWorldIntertia
//   +0xA0  mWorldInverseInertia   CalculateNewVelocity's `addi r11,this,0xA0` + its three row loads
//   +0xD0  mfMass                 CalculateNewVelocity's `li r11,0xD0` -> the vrefp reciprocal
//   +0xE0  mTotalLinearForce      AddWorldSpaceForce / AddLocalForce / Construct
//   +0xF0  mTotalTorque           AddWorldSpaceTorque / AddLocalForce / Construct
//   +0x100 mTotalLinearImpulse    AddWorldSpaceImpulse / AddLocalImpulse / Construct
//   +0x110 mTotalAngularImpulse   AddWorldSpaceAngularImpulse / AddLocalImpulse / Construct
// This whole base chain is pointer-free (see the ExternallySimulatedBody LAYOUT note), so the
// console offsets reproduce exactly on x64.
// (the members are protected, so the offsets are taken through an empty derived probe, which
// adds nothing and therefore shares the class's frame exactly).
#include <cstddef>   // offsetof

static_assert(sizeof(BrnPhysics::ExternalPhysicsBody) == 0x120,
              "sizeof == 0x120 (the four 16-byte accumulators end the class)");

namespace
{
    struct EpbLayoutProbe : public BrnPhysics::ExternalPhysicsBody
    {
        using BrnPhysics::ExternalPhysicsBody::mLocalInverseInertia;
        using BrnPhysics::ExternalPhysicsBody::mWorldInverseInertia;
        using BrnPhysics::ExternalPhysicsBody::mfMass;
        using BrnPhysics::ExternalPhysicsBody::mTotalLinearForce;
        using BrnPhysics::ExternalPhysicsBody::mTotalTorque;
        using BrnPhysics::ExternalPhysicsBody::mTotalLinearImpulse;
        using BrnPhysics::ExternalPhysicsBody::mTotalAngularImpulse;
    };

    static_assert(sizeof(EpbLayoutProbe) == sizeof(BrnPhysics::ExternalPhysicsBody),
                  "the probe adds nothing, so it shares the class's frame");
    static_assert(offsetof(EpbLayoutProbe, mLocalInverseInertia) == 0x70,
                  "mLocalInverseInertia @ +0x70");
    static_assert(offsetof(EpbLayoutProbe, mWorldInverseInertia) == 0xA0,
                  "mWorldInverseInertia @ +0xA0");
    static_assert(offsetof(EpbLayoutProbe, mfMass)               == 0xD0,
                  "mfMass @ +0xD0");
    static_assert(offsetof(EpbLayoutProbe, mTotalLinearForce)    == 0xE0,
                  "mTotalLinearForce @ +0xE0");
    static_assert(offsetof(EpbLayoutProbe, mTotalTorque)         == 0xF0,
                  "mTotalTorque @ +0xF0");
    static_assert(offsetof(EpbLayoutProbe, mTotalLinearImpulse)  == 0x100,
                  "mTotalLinearImpulse @ +0x100");
    static_assert(offsetof(EpbLayoutProbe, mTotalAngularImpulse) == 0x110,
                  "mTotalAngularImpulse @ +0x110");
}

namespace BrnPhysics
{
    namespace vpu = rw::math::vpu;

    // ---------------------------------------------------------------------------------------
    // AddWorldSpace{Force,Torque,Impulse,AngularImpulse}  @0x825BE710 / .9D0 / .8F8 / .EAA8
    //
    // Each is the same shape: assert the incoming world-space vector is finite, then add it
    // (vaddfp the stored accumulator by the argument and store back). The asm self-compares
    // each of the x/y/z lanes (`vspltw`+`vcmpeqfp.`) to detect NaN -- i.e. RwMathVPU::IsValid
    // -- and fires the matching "Bad <kind> added " assert on failure, but the add then runs
    // UNCONDITIONALLY (the assert is a non-gating tripwire). Store target confirmed by asm
    // `addi r11,this,0x{E0,F0,100,110}` -> mTotalLinearForce/mTotalTorque/mTotalLinearImpulse/
    // mTotalAngularImpulse. The accumulators carry xyz (force/torque/impulse have no w term).
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::AddWorldSpaceForce(Vector3 lvForce)
    {
        CGS_ASSERT(vpu::IsValid(lvForce), "Bad force added ");
        mTotalLinearForce = vpu::Add(mTotalLinearForce, lvForce);
    }

    void ExternalPhysicsBody::AddWorldSpaceTorque(Vector3 lvTorque)
    {
        CGS_ASSERT(vpu::IsValid(lvTorque), "Bad torque added ");
        mTotalTorque = vpu::Add(mTotalTorque, lvTorque);
    }

    void ExternalPhysicsBody::AddWorldSpaceImpulse(Vector3 lvImpulse)
    {
        CGS_ASSERT(vpu::IsValid(lvImpulse), "Bad impulse added ");
        mTotalLinearImpulse = vpu::Add(mTotalLinearImpulse, lvImpulse);
    }

    void ExternalPhysicsBody::AddWorldSpaceAngularImpulse(Vector3 lvImpulse)
    {
        CGS_ASSERT(vpu::IsValid(lvImpulse), "Bad angular impulse added ");
        mTotalAngularImpulse = vpu::Add(mTotalAngularImpulse, lvImpulse);
    }

    // =======================================================================================
    // ⭐ THE INTEGRATOR
    //
    // Everything below this banner was ABSENT from the tree. `IntegrateTransform` and
    // `CalculateNewVelocity` are the two functions every force leaf in VehiclePhysics
    // ultimately pushes into: the leaves fill the four accumulators, CalculateNewVelocity turns
    // them into a velocity change and clears them, IntegrateTransform advances the pose.
    //
    // ⚠️ Both take the timestep in the incoming VECTOR register v1, not an FPU register --
    // recovered from the ASM, not from a PC declaration (see the header note on the dropped
    // `dt` argument). The de-SIMD convention is the file's existing one: the VMX128 lane
    // machinery is lowered to named scalar/Vector3 math, no __asm, stores in the asm's order.
    // =======================================================================================

    // ---------------------------------------------------------------------------------------
    // CalculateNewVelocity  @0x825A1B10
    //
    //     invMass = 1 / mfMass
    //     mLinearVelocity  += (mTotalLinearForce * dt + mTotalLinearImpulse) * invMass
    //     mAngularVelocity += mWorldInverseInertia * (mTotalTorque * dt + mTotalAngularImpulse)
    //     mTotalLinearImpulse = mTotalAngularImpulse = mTotalLinearForce = mTotalTorque = 0
    //
    // FIDELITY: CLEAN on the data flow, the store set and the store ORDER (the asm writes the
    // new linear velocity to this+0x40 before it touches the angular register at this+0x50, and
    // clears the four accumulators last, in the order +0x100, +0x110, +0xE0, +0xF0).
    //
    // The four dev asserts are reproduced with their console text and in the console's order --
    // they are the same non-gating `rw::math::IsValid` tripwires used elsewhere in this file,
    // fired BEFORE the integrate (ExternalPhysicsBody.cpp:279/280 == asm 0x117/0x118) and again
    // AFTER it (:291/:292 == 0x123/0x124). They pin the two velocity members by NAME: the asm
    // fires "rw::math::IsValid(mAngularVelocity)" on the register at this+0x50 and
    // "rw::math::IsValid(mLinearVelocity)" on this+0x40.
    //
    // FLAG (modelled, not bit-verified): 1/mfMass is the VMX `vrefp` reciprocal estimate plus TWO
    // Newton-Raphson refinement steps (vnmsubfp/vmaddfp/vmaddcfp128 against a vcfsx-materialised
    // 1.0). It is written as a plain divide here, the same modelling the two collision solvers
    // below already use. Deliberately NOT guarded against a zero mass: the console does not guard
    // either, and a guarded 0 would silently freeze a mass-less body instead of tripping the two
    // trailing IsValid asserts that exist precisely to catch it.
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::CalculateNewVelocity(VecFloat lvfDeltaTime)
    {
        const f32 lfDt      = lvfDeltaTime.x;   // broadcast VecFloat -> scalar (de-modelled lane)
        const f32 lfInvMass = 1.0f / mfMass.x;

        CGS_ASSERT(vpu::IsValid(mAngularVelocity), "rw::math::IsValid(mAngularVelocity)");
        CGS_ASSERT(vpu::IsValid(mLinearVelocity),  "rw::math::IsValid(mLinearVelocity)");

        // Linear: the force accumulator becomes an impulse over the step, joins the impulse
        // accumulator, and divides by mass.
        const Vector3 lvLinearImpulse =
            vpu::Add(vpu::Mult(mTotalLinearForce, lfDt), mTotalLinearImpulse);
        mLinearVelocity = vpu::Add(mLinearVelocity, vpu::Mult(lvLinearImpulse, lfInvMass));

        // Angular: same shape, but through the world inverse-inertia tensor rather than 1/m.
        // (asm: splat the three lanes of the angular impulse and combine the three tensor rows.)
        const Vector3 lvAngularImpulse =
            vpu::Add(vpu::Mult(mTotalTorque, lfDt), mTotalAngularImpulse);
        const Vector3 lvDeltaOmega = vpu::Add(
            vpu::Add(vpu::Mult(mWorldInverseInertia.xAxis, lvAngularImpulse.x),
                     vpu::Mult(mWorldInverseInertia.yAxis, lvAngularImpulse.y)),
            vpu::Mult(mWorldInverseInertia.zAxis, lvAngularImpulse.z));
        mAngularVelocity = vpu::Add(mAngularVelocity, lvDeltaOmega);

        CGS_ASSERT(vpu::IsValid(mAngularVelocity), "rw::math::IsValid(mAngularVelocity)");
        CGS_ASSERT(vpu::IsValid(mLinearVelocity),  "rw::math::IsValid(mLinearVelocity)");

        // Consume the accumulators (asm store order: +0x100, +0x110, +0xE0, +0xF0).
        mTotalLinearImpulse.SetZero();
        mTotalAngularImpulse.SetZero();
        mTotalLinearForce.SetZero();
        mTotalTorque.SetZero();
    }

    // ---------------------------------------------------------------------------------------
    // IntegrateTransform  @0x825A7930   (63 asm instructions -- the literal integrator)
    //
    //     mTransform.wAxis += mLinearVelocity * dt
    //     for each rotation row r:  r -= cross(r, mAngularVelocity * dt)      [ == r + omega x r ]
    //     mTransform = OrthoNormalize3x3(mTransform)
    //     CalculateWorldIntertia()
    //
    // FIDELITY: CLEAN, recovered instruction by instruction.
    //   * `vmaddfp v0, v0(this+0x40), v13(this+0x30), v1` -> wAxis = linearVelocity*dt + wAxis.
    //   * `vmulfp128 v0, v0(this+0x50), v1` -> the spin vector omega*dt, then, per row, the VMX
    //     shifted-cross idiom `row*permwi(w,0x63) - permwi(row,0x63)*w` followed by one more
    //     permwi -- permwi mask 0x63 is the lane rotation {y,z,x,w}, and the pair of rotations
    //     around the products is exactly cross(row, w). The row order in the asm is zAxis,
    //     yAxis, xAxis; each row is computed from the ORIGINAL rows, so the order is cosmetic.
    //   * `bl rw__math__vpu__OrthoNormalize3x3(&temp, this)` then four `lvx128/stvx128` pairs
    //     copying temp's rows back over this+0x00/0x10/0x20/0x30 -- all FOUR rows, position
    //     included, which is why the position update above has to happen first.
    //   * `bl BrnPhysics__ExternalPhysicsBody__CalculateWorldIntertia` with this in r3.
    //
    // The first-order `r + (omega x r) dt` step is the console's own approximation, not a
    // simplification introduced here -- there is no quaternion, no matrix exponential and no
    // sub-stepping in this function; the re-orthonormalisation is what keeps it stable.
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::IntegrateTransform(VecFloat lvfDeltaTime)
    {
        const f32 lfDt = lvfDeltaTime.x;

        // Position: p += v * dt.
        mTransform.wAxis = vpu::Add(vpu::Mult(mLinearVelocity, lfDt), mTransform.wAxis);

        // Orientation: spin each basis row by omega*dt (first order).
        const Vector3 lvSpin = vpu::Mult(mAngularVelocity, lfDt);
        mTransform.zAxis = vpu::Subtract(mTransform.zAxis, vpu::Cross(mTransform.zAxis, lvSpin));
        mTransform.yAxis = vpu::Subtract(mTransform.yAxis, vpu::Cross(mTransform.yAxis, lvSpin));
        mTransform.xAxis = vpu::Subtract(mTransform.xAxis, vpu::Cross(mTransform.xAxis, lvSpin));

        // Re-orthonormalise the 3x3 (the position row rides through untouched).
        mTransform = vpu::OrthoNormalize3x3(mTransform);

        // The inertia tensor follows the new orientation.
        CalculateWorldIntertia();
    }

    // ---------------------------------------------------------------------------------------
    // CalculateWorldIntertia  @0x825A1E30
    //
    //     mWorldInverseInertia = R^T * mLocalInverseInertia * R,   R = mTransform's 3x3
    //
    // (row-vector convention: a world vector is taken into body space by R^T, scaled by the
    // local inverse inertia, and returned to world space by R.)
    //
    // FIDELITY: CLEAN on the data flow and on the two-stage structure. The X360 builds R^T with
    // the classic vmrghw/vmrglw lane-merge transpose (against a zero register for the missing
    // fourth row), computes T = R^T * L, STORES T into mWorldInverseInertia (+0xA0/+0xB0/+0xC0),
    // reloads it and computes W = T * R into the same three rows -- the intermediate store is
    // reproduced faithfully below rather than folded away, because it is observable.
    // Two dev asserts bracket it: the transform is checked on entry (ExternalPhysicsBody.cpp:353
    // == asm 0x161) and the produced tensor on exit (:367 == 0x16F).
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::CalculateWorldIntertia()
    {
        const Vector3& lR0 = mTransform.xAxis;
        const Vector3& lR1 = mTransform.yAxis;
        const Vector3& lR2 = mTransform.zAxis;

        CGS_ASSERT(vpu::IsValid(lR0) && vpu::IsValid(lR1) && vpu::IsValid(lR2),
                   "rw::math::IsValid(mTransform)");

        // R^T (the vmrghw/vmrglw transpose; the fourth lane merges against zero).
        const Vector3 lRt0{ lR0.x, lR1.x, lR2.x, 0.0f };
        const Vector3 lRt1{ lR0.y, lR1.y, lR2.y, 0.0f };
        const Vector3 lRt2{ lR0.z, lR1.z, lR2.z, 0.0f };

        const Vector3& lL0 = mLocalInverseInertia.xAxis;
        const Vector3& lL1 = mLocalInverseInertia.yAxis;
        const Vector3& lL2 = mLocalInverseInertia.zAxis;

        // Stage 1: T = R^T * L, written straight into the destination rows (the asm's stores).
        mWorldInverseInertia.xAxis = vpu::Add(vpu::Add(vpu::Mult(lL0, lRt0.x), vpu::Mult(lL1, lRt0.y)),
                                              vpu::Mult(lL2, lRt0.z));
        mWorldInverseInertia.yAxis = vpu::Add(vpu::Add(vpu::Mult(lL0, lRt1.x), vpu::Mult(lL1, lRt1.y)),
                                              vpu::Mult(lL2, lRt1.z));
        mWorldInverseInertia.zAxis = vpu::Add(vpu::Add(vpu::Mult(lL0, lRt2.x), vpu::Mult(lL1, lRt2.y)),
                                              vpu::Mult(lL2, lRt2.z));

        // Stage 2: W = T * R, over the rows just stored (the asm reloads them).
        const Vector3 lT0 = mWorldInverseInertia.xAxis;
        const Vector3 lT1 = mWorldInverseInertia.yAxis;
        const Vector3 lT2 = mWorldInverseInertia.zAxis;

        mWorldInverseInertia.zAxis = vpu::Add(vpu::Add(vpu::Mult(lR0, lT2.x), vpu::Mult(lR1, lT2.y)),
                                              vpu::Mult(lR2, lT2.z));
        mWorldInverseInertia.yAxis = vpu::Add(vpu::Add(vpu::Mult(lR0, lT1.x), vpu::Mult(lR1, lT1.y)),
                                              vpu::Mult(lR2, lT1.z));
        mWorldInverseInertia.xAxis = vpu::Add(vpu::Add(vpu::Mult(lR0, lT0.x), vpu::Mult(lR1, lT0.y)),
                                              vpu::Mult(lR2, lT0.z));

        CGS_ASSERT(vpu::IsValid(mWorldInverseInertia.xAxis) &&
                   vpu::IsValid(mWorldInverseInertia.yAxis) &&
                   vpu::IsValid(mWorldInverseInertia.zAxis),
                   "rw::math::IsValid(mWorldInverseInertia)");
    }

    // ---------------------------------------------------------------------------------------
    // The three space-resolving helpers: AddLocalForce @0x825A1670, AddLocalImpulse @0x825A1878
    // and GetImpulsesFromLocalImpulse @0x825A1A80.
    //
    // All three share ONE body shape, verified against GetImpulsesFromLocalImpulse (whose 36
    // instructions are the shape with no assert noise around it):
    //
    //   if (vectorSpace == BODY_SPACE)      v = R0*v.x + R1*v.y + R2*v.z      // rotate to world
    //   if (positionSpace == WORLD_SPACE)   r = position - mTransform.wAxis   // relative to COM
    //   else                                r = R0*p.x + R1*p.y + R2*p.z      // rotate the offset
    //   linear  += v                        (or *lpLinearImpulseOut  = v)
    //   angular += cross(r, v)              (or *lpAngularImpulseOut = cross(r, v))
    //
    // (rw::physics::InputSpace is DWARF-authoritative: WORLD_SPACE = 0, BODY_SPACE = 1, and the
    // asm's two compares are `cmpwi r4,1` for the vector and `cmpwi r5,0` for the position --
    // i.e. the two tags are tested against OPPOSITE values, which is why a two-argument
    // stand-in that drops them cannot be right for both.)
    //
    // The cross product is the same shifted-permwi idiom decoded in IntegrateTransform.
    // FIDELITY: CLEAN. AddLocalForce's two dev asserts are ExternalPhysicsBody.cpp:134/135
    // ("Bad positioned force added" / "Force applied at a bad position") and AddLocalImpulse's
    // are :184/:185 ("Bad positioned impulse added" / "Impulse applied at a bad position"), both
    // non-gating -- the accumulate runs regardless, exactly as in the four AddWorldSpace* funcs.
    // ---------------------------------------------------------------------------------------
    namespace
    {
        // Rotate a body-space vector into world space by the transform's 3x3 (row-vector form).
        inline Vector3 RotateToWorld(const Matrix44Affine& lrTransform, Vector3 lVector)
        {
            return vpu::Add(vpu::Add(vpu::Mult(lrTransform.xAxis, lVector.x),
                                     vpu::Mult(lrTransform.yAxis, lVector.y)),
                            vpu::Mult(lrTransform.zAxis, lVector.z));
        }
    }

    void ExternalPhysicsBody::AddLocalForce(Vector3 lForce, rw::physics::InputSpace leForceSpace,
                                            Vector3 lPosition, rw::physics::InputSpace lePositionSpace)
    {
        CGS_ASSERT(vpu::IsValid(lForce),    "Bad positioned force added");
        CGS_ASSERT(vpu::IsValid(lPosition), "Force applied at a bad position");

        const Vector3 lvForce = (leForceSpace == rw::physics::BODY_SPACE)
                              ? RotateToWorld(mTransform, lForce) : lForce;
        const Vector3 lvArm   = (lePositionSpace == rw::physics::WORLD_SPACE)
                              ? vpu::Subtract(lPosition, mTransform.wAxis)
                              : RotateToWorld(mTransform, lPosition);

        mTotalLinearForce = vpu::Add(mTotalLinearForce, lvForce);
        mTotalTorque      = vpu::Add(mTotalTorque, vpu::Cross(lvArm, lvForce));
    }

    void ExternalPhysicsBody::AddLocalImpulse(Vector3 lImpulse, rw::physics::InputSpace leImpulseSpace,
                                              Vector3 lPosition, rw::physics::InputSpace lePositionSpace)
    {
        CGS_ASSERT(vpu::IsValid(lImpulse),  "Bad positioned impulse added");
        CGS_ASSERT(vpu::IsValid(lPosition), "Impulse applied at a bad position");

        const Vector3 lvImpulse = (leImpulseSpace == rw::physics::BODY_SPACE)
                                ? RotateToWorld(mTransform, lImpulse) : lImpulse;
        const Vector3 lvArm     = (lePositionSpace == rw::physics::WORLD_SPACE)
                                ? vpu::Subtract(lPosition, mTransform.wAxis)
                                : RotateToWorld(mTransform, lPosition);

        mTotalLinearImpulse  = vpu::Add(mTotalLinearImpulse, lvImpulse);
        mTotalAngularImpulse = vpu::Add(mTotalAngularImpulse, vpu::Cross(lvArm, lvImpulse));
    }

    void ExternalPhysicsBody::GetImpulsesFromLocalImpulse(
        Vector3 lImpulse, rw::physics::InputSpace leImpulseSpace,
        Vector3 lPosition, rw::physics::InputSpace lePositionSpace,
        Vector3* lpLinearImpulseOut, Vector3* lpAngularImpulseOut) const
    {
        const Vector3 lvImpulse = (leImpulseSpace == rw::physics::BODY_SPACE)
                                ? RotateToWorld(mTransform, lImpulse) : lImpulse;
        const Vector3 lvArm     = (lePositionSpace == rw::physics::WORLD_SPACE)
                                ? vpu::Subtract(lPosition, mTransform.wAxis)
                                : RotateToWorld(mTransform, lPosition);

        // (the X360 stores the linear result BEFORE it computes the cross product)
        *lpLinearImpulseOut  = lvImpulse;
        *lpAngularImpulseOut = vpu::Cross(lvArm, lvImpulse);
    }

    // ---------------------------------------------------------------------------------------
    // The lifecycle quartet -- Construct @0x825A1598, Destruct @0x825A15E0, Release @0x825A1628,
    // Prepare @0x825A78D0.
    //
    // FIDELITY: CLEAN, store-for-store. Each is 17 instructions (Prepare 23): a `bl` to the
    // ExternallySimulatedBody function of the same name, then `stvx128 zero` into the four
    // accumulators (asm offset order: +0x100, +0x110, +0xE0, +0xF0). Prepare additionally calls
    // CalculateWorldIntertia and returns 1.
    //
    // Note what these do NOT do: none of them touches mLocalInverseInertia, mWorldInverseInertia
    // or mfMass. A freshly Constructed body has a zero mass and a zero inertia tensor until
    // SetMass / ReadPropertiesFromRenderware / ReadPropertiesFromChangeInertiaEvent fills them
    // in -- which is exactly why Prepare's CalculateWorldIntertia is the last step of bring-up
    // and not the first.
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::Construct()
    {
        ExternallySimulatedBody::Construct();
        mTotalLinearImpulse.SetZero();
        mTotalAngularImpulse.SetZero();
        mTotalLinearForce.SetZero();
        mTotalTorque.SetZero();
    }

    void ExternalPhysicsBody::Destruct()
    {
        ExternallySimulatedBody::Destruct();
        mTotalLinearImpulse.SetZero();
        mTotalAngularImpulse.SetZero();
        mTotalLinearForce.SetZero();
        mTotalTorque.SetZero();
    }

    void ExternalPhysicsBody::Release()
    {
        ExternallySimulatedBody::Release();
        mTotalLinearImpulse.SetZero();
        mTotalAngularImpulse.SetZero();
        mTotalLinearForce.SetZero();
        mTotalTorque.SetZero();
    }

    bool ExternalPhysicsBody::Prepare()
    {
        ExternallySimulatedBody::Prepare();
        mTotalLinearImpulse.SetZero();
        mTotalAngularImpulse.SetZero();
        mTotalLinearForce.SetZero();
        mTotalTorque.SetZero();
        CalculateWorldIntertia();
        return true;   // asm: `li r3, 1`
    }

    // ---------------------------------------------------------------------------------------
    // CalculateCollisionImpulseWithInanimateObject  @0x8259C978
    //
    // The textbook impulse magnitude for a body striking an immovable object at contact point
    // r (relative to the centre of mass) with surface normal n, relative velocity vRel and
    // restitution e:
    //
    //     j = -(1 + e) (vRel . n) / ( 1/m + n . ( (I^-1 (r x n)) x r ) )
    //
    // The recovered asm computes exactly this shape: a vmsum3fp dot of the normal with a
    // velocity vector, the cross/inverse-inertia/cross chain assembled through the vpermwi +
    // vnmsubfp sequence, a vrefp reciprocal of the effective mass, and three stores:
    //   * the inverse effective-mass scalar  -> *lpvfInvInertiaOut   (first store, asm r6)
    //   * the impulse magnitude / scaled-n    -> the VecFloat return  (asm r3)
    //   * the impulse vector  j*n             -> *lpvImpulseOut        (asm r5)
    // and asserts `lpvfInvInertiaOut != NULL` before writing it.
    //
    // FLAG (modelled, not bit-verified): the X360 Hex-Rays dropped the argument list (rendered
    // `int(...)`), so the arg->register threading was recovered from the PS3 DecFIGS DWARF
    // prototype (PS3 0x68B130): (Vector3 lPoint, Vector3 lPointVel, Vector3 lCollisionNormal,
    // VecFloat lvfRestitution, Vector3* lpImpulseOut, VecFloat* lpvfInvInertiaOut). The PS3 asm
    // shows lPointVel is the relative velocity (it subtracts the body velocity at this+0x30:
    // `vsubfp v2,v2,v0; lvx v0,this,0x30`), lPoint is the cross-product `r`, lCollisionNormal is
    // the surface normal `n`. The inverse inertia used is this body's mWorldInverseInertia. The
    // data flow, output set and assert match the asm; the precise VMX tensor multiply / reciprocal
    // refinement is the modelled I^-1 row-combination, not proven lane-for-lane.
    // ---------------------------------------------------------------------------------------
    VecFloat ExternalPhysicsBody::CalculateCollisionImpulseWithInanimateObject(
        Vector3 lPoint, Vector3 lPointVel, Vector3 lCollisionNormal,
        VecFloat lvfRestitution, Vector3* lpImpulseOut, VecFloat* lpvfInvInertiaOut)
    {
        CGS_ASSERT(lpvfInvInertiaOut != nullptr, "lpvfInvInertiaOut != NULL");

        const f32 lfRestitution = lvfRestitution.x;   // broadcast VecFloat -> scalar (de-modelled lane)

        // Angular term: I^-1 ( r x n ), then ( I^-1(rxn) ) x r, projected onto n.
        const Vector3 lvRxN = vpu::Cross(lPoint, lCollisionNormal);
        const Vector3 lvAngular = vpu::Add(
            vpu::Add(vpu::Mult(mWorldInverseInertia.xAxis, lvRxN.x),
                     vpu::Mult(mWorldInverseInertia.yAxis, lvRxN.y)),
            vpu::Mult(mWorldInverseInertia.zAxis, lvRxN.z));
        const Vector3 lvAngularAtContact = vpu::Cross(lvAngular, lPoint);

        // Effective inverse mass: 1/m + n . (angular term). m is stored as VecFloat (broadcast).
        const f32 lfInvMass = (mfMass.x != 0.0f) ? (1.0f / mfMass.x) : 0.0f;
        const f32 lfDenominator = lfInvMass + vpu::Dot(lCollisionNormal, lvAngularAtContact);
        const f32 lfInvDenominator = (lfDenominator != 0.0f) ? (1.0f / lfDenominator) : 0.0f;

        // Impulse magnitude and the impulse vector j*n.
        const f32 lfRelativeNormalSpeed = vpu::Dot(lPointVel, lCollisionNormal);
        const f32 lfImpulse = -(1.0f + lfRestitution) * lfRelativeNormalSpeed * lfInvDenominator;
        const Vector3 lvImpulse = vpu::Mult(lCollisionNormal, lfImpulse);

        // Stores (asm order): inverse-effective-mass scalar, then the impulse vector out.
        VecFloat lvfInvInertia; lvfInvInertia.x = lvfInvInertia.y = lvfInvInertia.z = lvfInvInertia.w = lfInvDenominator;
        *lpvfInvInertiaOut = lvfInvInertia;
        if (lpImpulseOut != nullptr)
            *lpImpulseOut = lvImpulse;

        VecFloat lvfResult; lvfResult.x = lvfResult.y = lvfResult.z = lvfResult.w = lfImpulse;
        return lvfResult;
    }

    // ---------------------------------------------------------------------------------------
    // CalculateCollisionImpulseWithBody  @0x8259CAE8
    //
    // The two-body collision impulse (the car-on-car shunt solver). Identical in shape to the
    // inanimate version above, but the effective inverse mass in the denominator sums BOTH
    // bodies' linear (1/m) and angular (n.((I^-1(r x n)) x r)) terms. The recovered asm computes,
    // per body, the I^-1 row-combination of (r x n), crosses it back with r, dots with n, adds
    // the two reciprocal masses, takes the reciprocal of the sum (vrefp + a Newton refinement)
    // and multiplies by the numerator -(1+e)(vRel.n). It stores each body's inverse
    // effective-mass term separately (asm: `1/mA + (lApart.n)` -> lpfInvInertiaAOut at r31,
    // `1/mB + (lBpart.n)` -> lpfInvInertiaBOut at r29) so the caller (ApplyCarCarImpulse) can
    // split the impulse application between the two cars; j*n -> lpImpulseOut (r27); and returns
    // the impulse magnitude (r28). The DecFIGS body hint (ExternalPhysicsBody.cpp:585-618) names
    // lvfNumerator/lvfDenominator/lvfMassA/lvfMassB/lvfOneOverMassA/lvfOneOverMassB/lApart/lBpart.
    //
    // FLAG (modelled, not bit-verified): the VMX `vrefp` reciprocal + its Newton-Raphson
    // refinement (vnmsubfp/vmaddfp) is reproduced as a plain `1.0f / x` (same modelling as the
    // inanimate solver above); the per-lane permute/select machinery is de-SIMD'd to scalar/
    // Vector3 arithmetic. The data flow, output set and the two asserts match the asm.
    // ---------------------------------------------------------------------------------------
    VecFloat ExternalPhysicsBody::CalculateCollisionImpulseWithBody(
        const ExternalPhysicsBody& lBody2, Vector3 lPoint1, Vector3 lPoint2,
        Vector3 lImpactVel, Vector3 lCollisionNormal, VecFloat lvfRestitution,
        Vector3* lpImpulseOut, VecFloat* lpfInvInertiaAOut, VecFloat* lpfInvInertiaBOut) const
    {
        CGS_ASSERT(lpfInvInertiaAOut != nullptr, "lpfInvInertiaAOut != NULL");
        CGS_ASSERT(lpfInvInertiaBOut != nullptr, "lpfInvInertiaBOut != NULL");

        const f32 lfRestitution = lvfRestitution.x;   // broadcast VecFloat -> scalar

        // Numerator: -(1 + e)(vRel . n).
        const f32 lfClosingSpeed = vpu::Dot(lImpactVel, lCollisionNormal);
        const f32 lfNumerator    = -(1.0f + lfRestitution) * lfClosingSpeed;

        // Body A angular coupling: n . ( (Ia^-1 (rA x n)) x rA ),  rA = lPoint1, Ia^-1 = this body.
        const Vector3 lvRAxN = vpu::Cross(lPoint1, lCollisionNormal);
        const Vector3 lvAngularA = vpu::Add(
            vpu::Add(vpu::Mult(mWorldInverseInertia.xAxis, lvRAxN.x),
                     vpu::Mult(mWorldInverseInertia.yAxis, lvRAxN.y)),
            vpu::Mult(mWorldInverseInertia.zAxis, lvRAxN.z));
        const Vector3 lApart = vpu::Cross(lvAngularA, lPoint1);
        const f32 lfAngularA = vpu::Dot(lCollisionNormal, lApart);

        // Body B angular coupling: rB = lPoint2, Ib^-1 = lBody2's world inverse inertia.
        const Vector3 lvRBxN = vpu::Cross(lPoint2, lCollisionNormal);
        const Vector3 lvAngularB = vpu::Add(
            vpu::Add(vpu::Mult(lBody2.mWorldInverseInertia.xAxis, lvRBxN.x),
                     vpu::Mult(lBody2.mWorldInverseInertia.yAxis, lvRBxN.y)),
            vpu::Mult(lBody2.mWorldInverseInertia.zAxis, lvRBxN.z));
        const Vector3 lBpart = vpu::Cross(lvAngularB, lPoint2);
        const f32 lfAngularB = vpu::Dot(lCollisionNormal, lBpart);

        // Inverse masses (m stored as VecFloat, broadcast).
        const f32 lfOneOverMassA = (mfMass.x != 0.0f)        ? (1.0f / mfMass.x)        : 0.0f;
        const f32 lfOneOverMassB = (lBody2.mfMass.x != 0.0f) ? (1.0f / lBody2.mfMass.x) : 0.0f;

        // Effective inverse mass (both bodies) -> impulse magnitude.
        const f32 lfDenominator    = lfOneOverMassA + lfOneOverMassB + lfAngularA + lfAngularB;
        const f32 lfInvDenominator = (lfDenominator != 0.0f) ? (1.0f / lfDenominator) : 0.0f;
        const f32 lfImpVal         = lfNumerator * lfInvDenominator;

        // Outputs: each body's inverse effective-mass term, then the impulse vector j*n.
        const f32 lfInvInertiaA = lfOneOverMassA + lfAngularA;
        const f32 lfInvInertiaB = lfOneOverMassB + lfAngularB;
        VecFloat lvfInvInertiaA; lvfInvInertiaA.x = lvfInvInertiaA.y = lvfInvInertiaA.z = lvfInvInertiaA.w = lfInvInertiaA;
        VecFloat lvfInvInertiaB; lvfInvInertiaB.x = lvfInvInertiaB.y = lvfInvInertiaB.z = lvfInvInertiaB.w = lfInvInertiaB;
        *lpfInvInertiaAOut = lvfInvInertiaA;
        *lpfInvInertiaBOut = lvfInvInertiaB;
        if (lpImpulseOut != nullptr)
            *lpImpulseOut = vpu::Mult(lCollisionNormal, lfImpVal);

        VecFloat lvfResult; lvfResult.x = lvfResult.y = lvfResult.z = lvfResult.w = lfImpVal;
        return lvfResult;
    }

    // ---------------------------------------------------------------------------------------
    // DampenAngularVelocity  @0x825B2CD8   /   DampPitchYawRoll  @0x825BE210
    //
    // Both scale mAngularVelocity in place (asm `addi r11,this,0x50` -> mAngularVelocity load,
    // poly chain, store back). The poly chain is the EARenderWare VMX pow(base, exp) lane
    // approximation: vlogefp (log2) + a Chebyshev-style polynomial (coefficient tables at
    // 0x82014AC0..0x82014AF0) + vexptefp (exp2), combined per axis. The net effect is a
    // frame-rate-correct exponential decay of the angular velocity:
    //
    //     omega *= pow(dampingPerSecond, dt)
    //
    // DampenAngularVelocity uses one isotropic damping coefficient for all three axes;
    // DampPitchYawRoll uses a separate coefficient per body axis (pitch=x, yaw=y, roll=z).
    //
    // FLAG (modelled, not bit-verified): the exact per-lane select machinery and the polynomial
    // coefficients (unk_82014A* / unk_82FB9AF0) are NOT reproduced -- they are the SDK's
    // internal pow() approximation, with no project home. The recovered DATA FLOW (load
    // mAngularVelocity, raise a damping base to the dt power per axis, multiply, store back) is
    // reproduced with std::pow. Faithful in behaviour and store target; the bit pattern of the
    // approximation is intentionally NOT fabricated.
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::DampenAngularVelocity(VecFloat lvfDampingPerSecond, VecFloat lvfDeltaTime)
    {
        const f32 lfFactor = std::pow(lvfDampingPerSecond.x, lvfDeltaTime.x);
        mAngularVelocity = vpu::Mult(mAngularVelocity, lfFactor);
    }

    void ExternalPhysicsBody::DampPitchYawRoll(VecFloat lvfPitchDamping, VecFloat lvfYawDamping,
                                               VecFloat lvfRollDamping, VecFloat lvfDeltaTime)
    {
        const f32 lfDt = lvfDeltaTime.x;
        mAngularVelocity.x *= std::pow(lvfPitchDamping.x, lfDt);   // pitch about body x
        mAngularVelocity.y *= std::pow(lvfYawDamping.x,   lfDt);   // yaw   about body y
        mAngularVelocity.z *= std::pow(lvfRollDamping.x,  lfDt);   // roll  about body z
    }

    // ---------------------------------------------------------------------------------------
    // ReadPropertiesFromRenderware @0x825A2280 lives in its own TU,
    // ExternalPhysicsBody_ReadPropertiesFromRenderware.cpp -- SPLIT 2026-08-02 (physics wave 3).
    //
    // WHY THE SPLIT: it is the only function in this class that calls
    // rw::physics::RigidBody::GetLocalInvInertiaDiagonal(), whose console storage is a POINTER
    // packed into the rigid body's mUp.w float lane and therefore cannot be bodied against the
    // committed 64-bit rigidbody.h layout (see that header and RigidBody.cpp). Leaving it here
    // made the whole of ExternalPhysicsBody -- the four accumulators, AddLocalForce/Impulse,
    // GetImpulsesFromLocalImpulse, CalculateWorldIntertia, IntegrateTransform,
    // CalculateNewVelocity, the lifecycle quartet, i.e. THE INTEGRATOR -- unlinkable for the sake
    // of one function that is not on the vehicle path at all (its only caller is
    // Deformation::PhysicalBodyPart::Update). The split is a BUILD-MECHANICS split only: the code
    // is byte-identical and its declared home is unchanged. Re-merge when the packed lane gets its
    // PC-side storage answer in the SDK reconstruction. (Same idiom as ICEFileClose.cpp.)
    // ---------------------------------------------------------------------------------------
}
