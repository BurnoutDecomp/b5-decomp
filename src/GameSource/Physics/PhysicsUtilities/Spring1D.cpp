#include "GameSource/Physics/PhysicsUtilities/Spring1D.h"

// BrnPhysics::Spring1D -- the three ledger funcs (Construct/Prepare/Update). The X360 bodies
// are plain PPC FPU scalar arithmetic (no VMX); the reconstructions below are the de-asm'd
// equivalents written against the members BY NAME. Update's two terminal clamps use the PPC
// `fsel` instruction; fsel(a, b, c) == (a >= 0.0f ? b : c), so each clamp is reproduced with
// the exact same comparison sense the asm uses (NOT a generic std::clamp, which would differ at
// the -0.0/NaN boundary the fsel select produces).

namespace BrnPhysics
{
    // @0x8259CE48 -- zero all parameters except mfMass, which starts at 1.0 (asm: f0=0.0 stored
    // to offsets 0/4/0xC/0x10/0x18/0x20/0x1C/0x14; f13=1.0 stored to offset 8 == mfMass).
    void Spring1D::Construct()
    {
        mfDesiredLength     = 0.0f;
        mfCurrentLength     = 0.0f;
        mfMass              = 1.0f;
        mfVelocity          = 0.0f;
        mfStiffness         = 0.0f;
        mfTotalForce        = 0.0f;
        mfDampeningConstant = 0.0f;
        mfMaxStretch        = 0.0f;
        mfMinStretch        = 0.0f;
    }

    // @0x8259CE80 -- seed the spring; zero the running velocity (offset 0xC) and accumulated
    // force (offset 0x14). The asm threads the float args f1..f7 to offsets
    // 0/4/8/0x10/0x18/0x20/0x1C respectively, returning r3 = 1.
    bool Spring1D::Prepare(f32 lfDesiredLength, f32 lfCurrentLength, f32 lfMass, f32 lfStiffness,
                           f32 lfDampening, f32 lfMinStretch, f32 lfMaxStretch)
    {
        mfDesiredLength     = lfDesiredLength;   // f1 -> +0x00
        mfCurrentLength     = lfCurrentLength;   // f2 -> +0x04
        mfMass              = lfMass;            // f3 -> +0x08
        mfVelocity          = 0.0f;              //       +0x0C
        mfTotalForce        = 0.0f;              //       +0x14
        mfStiffness         = lfStiffness;       // f4 -> +0x10
        mfDampeningConstant = lfDampening;       // f5 -> +0x18
        mfMinStretch        = lfMinStretch;      // f6 -> +0x20
        mfMaxStretch        = lfMaxStretch;      // f7 -> +0x1C
        return true;
    }

    // @0x8259CEB8 -- one integration step.
    //
    //   spring force  = (mfCurrentLength - mfDesiredLength) * mfStiffness
    //   net force     = mfTotalForce - spring force
    //   acceleration  = net force / mfMass
    //   velocity'     = acceleration * dt + mfVelocity
    //   velocity''    = velocity' * (1 - mfDampeningConstant)      (viscous dampening)
    //   length'       = mfCurrentLength + velocity'' * dt
    //   length''      = clamp(length', mfMinStretch, mfMaxStretch) (via fsel, see below)
    //
    // The accumulated force (mfTotalForce) is consumed and reset to 0 at the top of the step
    // (asm stores 0.0 to offset 0x14 before the math runs).
    void Spring1D::Update(f32 lfTimeStep)
    {
        const f32 lfTotalForce = mfTotalForce;
        mfTotalForce = 0.0f;

        const f32 lfSpringForce  = (mfCurrentLength - mfDesiredLength) * mfStiffness;
        const f32 lfAcceleration = (lfTotalForce - lfSpringForce) / mfMass;
        const f32 lfRawVelocity  = lfAcceleration * lfTimeStep + mfVelocity;

        // fnmsubs f13, mfDampeningConstant, lfRawVelocity, lfRawVelocity
        //   == lfRawVelocity - mfDampeningConstant * lfRawVelocity
        const f32 lfVelocity = lfRawVelocity - mfDampeningConstant * lfRawVelocity;
        mfVelocity = lfVelocity;

        f32 lfLength = mfVelocity * lfTimeStep + mfCurrentLength;

        // Lower clamp: fsel f0, (mfMinStretch - lfLength), mfMinStretch, lfLength
        //   == (mfMinStretch - lfLength >= 0) ? mfMinStretch : lfLength   (== max(len, min))
        lfLength = ((mfMinStretch - lfLength) >= 0.0f) ? mfMinStretch : lfLength;

        // Upper clamp: fsel f0, (mfMaxStretch - lfLength), lfLength, mfMaxStretch
        //   == (mfMaxStretch - lfLength >= 0) ? lfLength : mfMaxStretch
        lfLength = ((mfMaxStretch - lfLength) >= 0.0f) ? lfLength : mfMaxStretch;

        mfCurrentLength = lfLength;
    }
}
