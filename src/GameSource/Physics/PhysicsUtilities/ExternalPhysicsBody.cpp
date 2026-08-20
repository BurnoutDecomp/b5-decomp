#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu::{IsValid, operator+, Dot, Cross, Mult, ...}
#include "rw/math/vpu/matrix44affine_operation.h"           // rw::math::vpu::OrthoNormalize3x3
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"  // InChangeRigidBodyInertia (ReadPropertiesFromChangeInertiaEvent)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags (CheckState's failure print)

#include <cmath>   // std::pow (models the VMX exp2/log2 pow-curve in the damp funcs)
#include <cstdlib> // getenv -- the opt-in [bank] bring-up probe only

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

        // ---- [bank] PC bring-up instrument -- DELETE WHEN the wall test is banked ---------------
        // OPT-IN (BRN_BANK_PROBE=1, a probe of its own so it can be armed WITHOUT the deformation
        // probes and vice versa). This is the momentum bank itself: every world-space linear impulse
        // any subsystem hands the body, with the running total it accumulates into.
        // ⚠️ Read it TOGETHER with [chainarrive] -- this function has several callers (the sensor
        // apply's own block (5), the wheels, the wall handler), so a line here on its own attributes
        // nothing. The pair does: a [chainarrive] route WALL immediately followed by a [bank] delta
        // is the impulse-passing chain reaching the momentum bank, in one run, in order.
        {
            static s32 siBankProbe = -1;
            if ( siBankProbe < 0 )
            {
                const char* lpcEnv = getenv( "BRN_BANK_PROBE" );
                siBankProbe = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
            }
            // ⚠️ COUNT ONLY THE NON-ZERO BANKS. The first cut counted every call and capped at 4000:
            // the wheels call this every frame with a zero impulse, so the cap was exhausted during
            // the boot menus and the probe printed NOTHING for a whole 275 s run while the chain was
            // demonstrably delivering. A cap has to be on the events you are counting.
            static u32 suBanks      = 0;
            static u32 suBigBanks   = 0;
            const f32 lfMagSq = lvImpulse.x * lvImpulse.x + lvImpulse.y * lvImpulse.y
                              + lvImpulse.z * lvImpulse.z;
            const f32 lfHorizontalSq = lvImpulse.x * lvImpulse.x + lvImpulse.z * lvImpulse.z;
            // Same two-window shape as [chainarrive]: a short opening window that proves the bank is
            // being fed at all, then only HORIZONTAL-dominant banks, which is what a wall response
            // looks like and what the ground's +Y support does not.
            const bool lbInteresting = ( lfMagSq > 0.0f && ++suBanks <= 30u )
                                     || ( lfHorizontalSq > 1.0f && ++suBigBanks <= 600u );
            if ( siBankProbe == 1 && CgsDev::Log::gpDebugPrint != 0 && lbInteresting )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[bank] n " << static_cast<s32>(suBanks)
                    << " J " << lvImpulse.x << " " << lvImpulse.y << " " << lvImpulse.z
                    << " total " << mTotalLinearImpulse.x << " " << mTotalLinearImpulse.y
                    << " " << mTotalLinearImpulse.z << "\n";
            }
        }
    }

    void ExternalPhysicsBody::AddWorldSpaceAngularImpulse(Vector3 lvImpulse)
    {
        CGS_ASSERT(vpu::IsValid(lvImpulse), "Bad angular impulse added ");
        mTotalAngularImpulse = vpu::Add(mTotalAngularImpulse, lvImpulse);
    }

    // ---------------------------------------------------------------------------------------
    // AddLocalSpaceForce  @0x825BE7E8  (67 insns)
    // ⭐ BODIED 2026-08-07 (wheel-cluster wave; was DECLARE-ONLY, and its .ida-exports JSON is
    // a hole -- pulled fresh from the .i64). Same tripwire shape as the world-space adders
    // (the NaN sweep fires "Bad force added " @ExternalPhysicsBody.h:457 and the add runs
    // unconditionally), but the force arrives in the body's LOCAL frame and is rotated out
    // first: world = xAxis*F.x + yAxis*F.y + zAxis*F.z (the two-vmaddfp cascade over the
    // mTransform rows at this+0x00/0x10/0x20), then mTotalLinearForce (+0xE0) += world.
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::AddLocalSpaceForce(Vector3 lvForce)
    {
        CGS_ASSERT(vpu::IsValid(lvForce), "Bad force added ");

        const Vector3 lvWorld{
            mTransform.xAxis.x * lvForce.x + mTransform.yAxis.x * lvForce.y
                + mTransform.zAxis.x * lvForce.z,
            mTransform.xAxis.y * lvForce.x + mTransform.yAxis.y * lvForce.y
                + mTransform.zAxis.y * lvForce.z,
            mTransform.xAxis.z * lvForce.x + mTransform.yAxis.z * lvForce.y
                + mTransform.zAxis.z * lvForce.z,
            0.0f };

        mTotalLinearForce = vpu::Add(mTotalLinearForce, lvWorld);
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

    // ---------------------------------------------------------------------------------------
    // SetMass -- the mass setter the header has carried as DECLARE-ONLY ("ADDITIVE GROW",
    // ExternalPhysicsBody.h:178) since the deformation wave. Bodied here 2026-08-03 (task #116),
    // in the authoritative home the header names, because it was the ONE symbol on
    // PhysicalBodyPart::Construct's link path that had no definition anywhere in the tree -- and
    // PhysicalBodyPart::Construct is reached from PhysicsModule::Construct @0x825AE308, which had
    // been a live empty stub.
    //
    // SHAPE FROM THE ASM, not invented. The only call site in either build is
    // PhysicalBodyPart::Construct @0x825B4178, which materialises 5.0f on the stack, loads it with
    // `lvlx`, BROADCASTS it with `vspltw`, and stores the whole 16-byte vector to the body's +0xD0
    // (mfMass). So the setter splats its scalar across all four lanes rather than writing .x and
    // leaving the other three lanes stale -- which matters, because every consumer in this file
    // reads mfMass.x while CalculateWorldIntertia-style vector maths would see the rest.
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::SetMass(f32 lfMass)
    {
        mfMass.x = lfMass;
        mfMass.y = lfMass;
        mfMass.z = lfMass;
        mfMass.w = lfMass;
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
    // The textbook impulse magnitude for a body striking an immovable object at world contact
    // point p (with r = p - centre-of-mass), surface normal n, point velocity vRel and
    // restitution e:
    //
    //     j = -(1 + e) (vRel . n) / ( 1/m + n . ( (I^-1 (r x n)) x r ) )
    //
    // The recovered asm computes exactly this shape: a vmsum3fp dot of the normal with a
    // velocity vector, the cross/inverse-inertia/cross chain assembled through the vpermwi +
    // vnmsubfp sequence, a vrefp reciprocal of the effective mass, and three stores:
    //   * the effective inverse-mass denominator -> *lpvfInvInertiaOut (first store, asm r6)
    //   * the signed impulse magnitude           -> the VecFloat return (hidden result r3)
    //   * normal * abs(magnitude)                 -> *lpvImpulseOut      (asm r5)
    // and asserts `lpvfInvInertiaOut != NULL` before writing it.
    //
    // FLAG (modelled, not bit-verified): the X360 Hex-Rays dropped the argument list (rendered
    // `int(...)`), so the arg->register threading was recovered from the PS3 DecFIGS DWARF
    // prototype (PS3 0x68B130): (Vector3 lPoint, Vector3 lPointVel, Vector3 lCollisionNormal,
    // VecFloat lvfRestitution, Vector3* lpImpulseOut, VecFloat* lpvfInvInertiaOut). The PS3 asm
    // prototype supplies the declaration shape. Breaker supplies the argument semantics:
    // @0x8259C9B0 loads mTransform.wAxis and @0x8259C9B8 subtracts it from v1, proving lPoint is
    // the absolute world point rather than an already-relative arm. The inverse inertia used is
    // this body's mWorldInverseInertia. The precise VMX reciprocal refinement is lowered to C
    // division; the data flow and all three stores follow the assembly.
    // ---------------------------------------------------------------------------------------
    VecFloat ExternalPhysicsBody::CalculateCollisionImpulseWithInanimateObject(
        Vector3 lPoint, Vector3 lPointVel, Vector3 lCollisionNormal,
        VecFloat lvfRestitution, Vector3* lpImpulseOut, VecFloat* lpvfInvInertiaOut)
    {
        CGS_ASSERT(lpvfInvInertiaOut != nullptr, "lpvfInvInertiaOut != NULL");

        const f32 lfRestitution = lvfRestitution.x;   // broadcast VecFloat -> scalar (de-modelled lane)

        // @0x8259C9B0..C9B8: the source argument is a WORLD point; derive r here.
        const Vector3 lvPointArm = vpu::Subtract(lPoint, mTransform.wAxis);

        // Angular term: I^-1 ( r x n ), then ( I^-1(rxn) ) x r, projected onto n.
        const Vector3 lvRxN = vpu::Cross(lvPointArm, lCollisionNormal);
        const Vector3 lvAngular = vpu::Add(
            vpu::Add(vpu::Mult(mWorldInverseInertia.xAxis, lvRxN.x),
                     vpu::Mult(mWorldInverseInertia.yAxis, lvRxN.y)),
            vpu::Mult(mWorldInverseInertia.zAxis, lvRxN.z));
        const Vector3 lvAngularAtContact = vpu::Cross(lvAngular, lvPointArm);

        // Effective inverse mass: 1/m + n . (angular term). m is stored as VecFloat (broadcast).
        const f32 lfInvMass = 1.0f / mfMass.x;
        const f32 lfDenominator = lfInvMass + vpu::Dot(lCollisionNormal, lvAngularAtContact);
        const f32 lfInvDenominator = 1.0f / lfDenominator;

        // Impulse magnitude and the impulse vector j*n.
        const f32 lfRelativeNormalSpeed = vpu::Dot(lPointVel, lCollisionNormal);
        const f32 lfImpulse = -(1.0f + lfRestitution) * lfRelativeNormalSpeed * lfInvDenominator;
        const Vector3 lvImpulse = vpu::Mult(lCollisionNormal, std::fabs(lfImpulse));

        // @0x8259CA94 stores v124 BEFORE the reciprocal refinement: the out-value is the
        // denominator, not its reciprocal. @0x8259CAC4/CAD0 strips the return's sign for the
        // impulse-vector store, while @0x8259CAC8 preserves the signed hidden return.
        VecFloat lvfInvInertia; lvfInvInertia.x = lvfInvInertia.y = lvfInvInertia.z = lvfInvInertia.w = lfDenominator;
        *lpvfInvInertiaOut = lvfInvInertia;
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
    // Both damp mAngularVelocity in place. The poly chain is the EARenderWare VMX pow(base, exp)
    // lane
    // approximation: vlogefp (log2) + a Chebyshev-style polynomial (coefficient tables at
    // 0x82014AC0..0x82014AF0) + vexptefp (exp2), combined per axis. The net effect is a
    // frame-rate-correct exponential decay of the angular velocity. The exponent is measured
    // in 60-Hz reference frames, not seconds:
    //
    // DampenAngularVelocity scales omega isotropically. DampPitchYawRoll instead loads the body
    // axes at this+0/+0x10/+0x20, and sequentially removes each projected component:
    //
    //     omega -= axis * dot(omega, axis) * pow(dampingPerFrameAt60Hz, dt * 60)
    //
    // DampPitchYawRoll uses a separate coefficient per body axis (pitch=x, yaw=y, roll=z),
    // and each later projection observes the result of the previous removal.
    //
    // FLAG (modelled, not bit-verified): the exact per-lane select machinery and the polynomial
    // coefficients (unk_82014A*) are NOT reproduced -- they are the SDK's
    // internal pow() approximation, with no project home. The recovered DATA FLOW (load
    // mAngularVelocity, raise a damping base to the `dt * kvfSixty` power, then scale/project as
    // described above) is reproduced with std::pow. DecFIGS' static initializer @0x12DF24 writes the
    // literal 60.0 splat to BrnPhysics::kvfSixty; Breaker names that slot unk_82FB9AF0 and
    // multiplies it by the incoming dt. The polynomial's final bit pattern is not reproduced.
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::DampenAngularVelocity(VecFloat lvfDampingPerSecond, VecFloat lvfDeltaTime)
    {
        const f32 lfFactor = std::pow(lvfDampingPerSecond.x, lvfDeltaTime.x * 60.0f);
        mAngularVelocity = vpu::Mult(mAngularVelocity, lfFactor);
    }

    void ExternalPhysicsBody::DampPitchYawRoll(VecFloat lvfPitchDamping, VecFloat lvfYawDamping,
                                               VecFloat lvfRollDamping, VecFloat lvfDeltaTime)
    {
        // @0x825BE210..E264: no projection is touched for a zero-sized simulation step.
        static const f32 KF_TIME_STEP_EPSILON = 1.1920928955078125e-07f; // stru_8208F620.x
        if (std::fabs(lvfDeltaTime.x) <= KF_TIME_STEP_EPSILON)
            return;

        const f32 lfDt = lvfDeltaTime.x * 60.0f;

        // @0x825BE398..E424: pitch about the body's x axis.
        const f32 lfPitchFactor = std::pow(lvfPitchDamping.x, lfDt);
        const f32 lfPitchVelocity = vpu::Dot(mAngularVelocity, mTransform.xAxis);
        mAngularVelocity = vpu::Subtract(
            mAngularVelocity,
            vpu::Mult(mTransform.xAxis, lfPitchVelocity * lfPitchFactor));

        // @0x825BE4A0..E5A8: yaw uses the velocity left by the pitch pass.
        const f32 lfYawFactor = std::pow(lvfYawDamping.x, lfDt);
        const f32 lfYawVelocity = vpu::Dot(mAngularVelocity, mTransform.yAxis);
        mAngularVelocity = vpu::Subtract(
            mAngularVelocity,
            vpu::Mult(mTransform.yAxis, lfYawVelocity * lfYawFactor));

        // @0x825BE66C..E704: roll uses the velocity left by both earlier passes.
        const f32 lfRollFactor = std::pow(lvfRollDamping.x, lfDt);
        const f32 lfRollVelocity = vpu::Dot(mAngularVelocity, mTransform.zAxis);
        mAngularVelocity = vpu::Subtract(
            mAngularVelocity,
            vpu::Mult(mTransform.zAxis, lfRollVelocity * lfRollFactor));
    }

    // ---------------------------------------------------------------------------------------
    // ReadPropertiesFromChangeInertiaEvent  @0x825A2388   (74 instructions, read line by line)
    //
    // Reseat mass + local inertia from a queued sim InChangeRigidBodyInertia event, then
    // re-derive the world tensor. The asm, in order:
    //   * `lwz 0x40(event)` -> mu32Flags; `rlwinm ..,29,29` / `..,30,30` -- BOTH bit 2 (0x4,
    //     the mInvMass flag) AND bit 1 (0x2, the mInvTens flag) must be set or the function is
    //     a no-op. (Bit map per the ProcessChangeRigidBodyInertiaQueue drain, quoted in
    //     CgsPhysicsSimulationIO_Events.h.)
    //   * three identity rows stored into mLocalInverseInertia (+0x70/+0x80/+0x90):
    //     gIVector {1,0,0,0}, unk_82181510 {0,1,0,0}, unk_82181520 {0,0,1,0} -- the settled
    //     identity-basis constants -- then each row is vmulfp128'd by the vspltw'd x/y/z lane
    //     of the event's mInertia.mInvTens vector (event+0x10). Net effect: the local inverse
    //     inertia becomes diag(invTens.x, invTens.y, invTens.z), written below directly (the
    //     store-identity-then-scale staging is register scheduling, not observable state).
    //   * `lfs f13, <event+0x20>` == mInertia.mInvMass; `fdivs f0, 1.0f, f13`; vspltw ->
    //     `stvx128` to +0xD0 == mfMass = splat(1 / inverse mass) -- the event carries the
    //     INVERSE mass, the body stores the mass (all consumers here divide by mfMass.x).
    //     Deliberately NOT guarded against a zero inverse mass: the console does not guard,
    //     and the same modelling note as CalculateNewVelocity's divide applies.
    //   * `bl CalculateWorldIntertia`.
    // (The 32-byte stack copy of event+0x20..0x40 the asm makes is a dead by-value staging --
    // only the +0x20 float is ever read back out of it. The Hex-Rays for this function fails
    // variable allocation; every claim above is from the asm.)
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::ReadPropertiesFromChangeInertiaEvent(
        const CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia* lpEvent)
    {
        const u32 luFlags = lpEvent->mu32Flags;
        if ((luFlags & 0x4u) != 0 && (luFlags & 0x2u) != 0)   // mInvMass AND mInvTens present
        {
            const Vector3 lvInvTens = lpEvent->mInertia.GetInverseInertia();

            // diag(invTens) -- the identity rows scaled per-lane (see the banner).
            mLocalInverseInertia.xAxis = Vector3{ lvInvTens.x, 0.0f, 0.0f, 0.0f };
            mLocalInverseInertia.yAxis = Vector3{ 0.0f, lvInvTens.y, 0.0f, 0.0f };
            mLocalInverseInertia.zAxis = Vector3{ 0.0f, 0.0f, lvInvTens.z, 0.0f };

            const f32 lfMass = 1.0f / lpEvent->mInertia.GetInverseMass();
            mfMass.x = lfMass;
            mfMass.y = lfMass;
            mfMass.z = lfMass;
            mfMass.w = lfMass;   // vspltw -- all four lanes, same as SetMass

            CalculateWorldIntertia();
        }
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

    // @0x825A24B0  BrnPhysics::ExternalPhysicsBody::CheckState  (607 insns; bodied 2026-08-07,
    // orchestrator wave -- the driving spine brackets every stage with it)
    //
    // Ten NaN sweeps over the body state, in the console's exact member order (each `li rN` /
    // `addi rN` below is the asm's own offset, rel. the EPB base):
    //   +0x00..0x30  mTransform, 4 rows, xyz lanes    -> "Bad transform"            (:679)
    //   +0x70        mLocalInverseInertia, 3 rows xyz -> "Bad local inverse inertia"
    //   +0xA0        mWorldInverseInertia, 3 rows xyz -> "Bad world inverse inertia" (:0x2B3)
    //   +0xD0        mfMass          (whole register) -> "Bad mass"
    //   +0xE0        mTotalLinearForce                -> "Bad total linear force"
    //   +0xF0        mTotalTorque                     -> "Bad total torque"
    //   +0x100       mTotalLinearImpulse              -> "Bad total linear impulse"
    //   +0x110       mTotalAngularImpulse             -> "Bad total angular impulse"
    //   +0x40        mLinearVelocity                  -> "Bad linear velocity"
    //   +0x50        mAngularVelocity                 -> "Bad angular velocity"
    // The console's test is `vcmpeqfp. v, v` -- a lane self-compare that fails only on NaN --
    // per xyz lane for the matrices and whole-register for the scalars/accumulators. On a
    // failure the caller's stage string goes through gpDebugPrint (gated on
    // gxMessageFilterFlags bit 0) and the console's own assert text fires.
    namespace
    {
        inline bool IsLaneNaN(f32 lfValue) { return !(lfValue == lfValue); }   // vcmpeqfp self-test

        template <typename TRow>
        inline bool AnyNaN3(const TRow& lrRow)
        {
            return IsLaneNaN(lrRow.x) || IsLaneNaN(lrRow.y) || IsLaneNaN(lrRow.z);
        }

        template <typename TRow>
        inline bool AnyNaN4(const TRow& lrRow)
        {
            return AnyNaN3(lrRow) || IsLaneNaN(lrRow.w);
        }

        void CheckStateFail(const char* lpcContext, const char* lpcWhat)
        {
            // the console's failure path: print the stage string, then assert.
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << lpcContext;
            CGS_ASSERT(false, lpcWhat);
        }
    }

    void ExternalPhysicsBody::CheckState(const char* lpcContext) const
    {
        // mTransform -- four rows, xyz lanes each.
        if (AnyNaN3(mTransform.xAxis) || AnyNaN3(mTransform.yAxis) ||
            AnyNaN3(mTransform.zAxis) || AnyNaN3(mTransform.wAxis))
            CheckStateFail(lpcContext, "Bad transform");

        if (AnyNaN3(mLocalInverseInertia.xAxis) || AnyNaN3(mLocalInverseInertia.yAxis) ||
            AnyNaN3(mLocalInverseInertia.zAxis))
            CheckStateFail(lpcContext, "Bad local inverse inertia");

        if (AnyNaN3(mWorldInverseInertia.xAxis) || AnyNaN3(mWorldInverseInertia.yAxis) ||
            AnyNaN3(mWorldInverseInertia.zAxis))
            CheckStateFail(lpcContext, "Bad world inverse inertia");

        if (AnyNaN4(mfMass))
            CheckStateFail(lpcContext, "Bad mass");

        if (AnyNaN4(mTotalLinearForce))
            CheckStateFail(lpcContext, "Bad total linear force");

        if (AnyNaN4(mTotalTorque))
            CheckStateFail(lpcContext, "Bad total torque");

        if (AnyNaN4(mTotalLinearImpulse))
            CheckStateFail(lpcContext, "Bad total linear impulse");

        if (AnyNaN4(mTotalAngularImpulse))
            CheckStateFail(lpcContext, "Bad total angular impulse");

        if (AnyNaN4(mLinearVelocity))
            CheckStateFail(lpcContext, "Bad linear velocity");

        if (AnyNaN4(mAngularVelocity))
            CheckStateFail(lpcContext, "Bad angular velocity");
    }

    // ---------------------------------------------------------------------------------------
    // GetLocalVelocity -- ⭐ 2026-08-14 (walls leg 4). Console-inline (no export on either
    // console); the header's own gloss is the body: the velocity of a point on this body
    // (= mLinearVelocity + mAngularVelocity x r). The InputSpace selects how the point maps to
    // the moment arm r: WORLD_SPACE points subtract the body position (both mounted callers,
    // ApplyCarCarImpulse's closing-velocity pair, pass world points); BODY_SPACE points rotate
    // through the transform rows first (no translation).
    // ---------------------------------------------------------------------------------------
    Vector3 ExternalPhysicsBody::GetLocalVelocity(Vector3 lPoint, rw::physics::InputSpace leSpace) const
    {
        Vector3 lvR;
        if ( leSpace == rw::physics::WORLD_SPACE )
        {
            lvR = vpu::Subtract(lPoint, mTransform.wAxis);
        }
        else
        {
            lvR = Vector3{
                mTransform.xAxis.x * lPoint.x + mTransform.yAxis.x * lPoint.y + mTransform.zAxis.x * lPoint.z,
                mTransform.xAxis.y * lPoint.x + mTransform.yAxis.y * lPoint.y + mTransform.zAxis.y * lPoint.z,
                mTransform.xAxis.z * lPoint.x + mTransform.yAxis.z * lPoint.y + mTransform.zAxis.z * lPoint.z,
                0.0f };
        }
        return vpu::Add(mLinearVelocity, vpu::Cross(mAngularVelocity, lvR));
    }

    // ---------------------------------------------------------------------------------------
    // GetLinearMomentum -- ⭐ 2026-08-19 (wave Q6 / the jointed lean+tilt prop response).
    // Console-inline (no address of its own on either console); the header's block comment
    // above the declaration carries the store-for-store recovery out of
    // HandleContactWithLeanProp @0x8260FCA4..0x8260FCBC and its corroborating twin in
    // HandleContactWithTiltProp @0x82610A08..0x82610A18, and is not repeated here.
    //
    //     v*m + F*dt + J
    //
    // ⚠️ ALL FOUR LANES, not xyz. Every one of the console's five instructions is a full
    // 4-lane VMX op (vmulfp128 / vmaddfp / vaddfp), and the caller immediately feeds the
    // result to a `vmsum3fp128` dot that ignores .w anyway -- so the w lane is carried, not
    // cleared, exactly as the register does. `vpu::Mult(Vector3, float)` and `vpu::Add` are
    // both 4-lane in this tree, so the spelling below reproduces that without comment.
    //
    // ⚠️ mfMass is a broadcast VecFloat (`stvx128` of a splat -- see the +0xD0 note at the
    // head of this file); its .x lane IS the mass. Same for the incoming timestep, which the
    // console builds with a `vspltw` before the multiply.
    // ---------------------------------------------------------------------------------------
    Vector3 ExternalPhysicsBody::GetLinearMomentum(VecFloat lvfTimeStep) const
    {
        const Vector3 lvMomentumFromVelocity = vpu::Mult(mLinearVelocity, mfMass.x);
        const Vector3 lvMomentumFromForce    = vpu::Mult(mTotalLinearForce, lvfTimeStep.x);

        return vpu::Add(vpu::Add(lvMomentumFromVelocity, lvMomentumFromForce),
                        mTotalLinearImpulse);
    }

}
