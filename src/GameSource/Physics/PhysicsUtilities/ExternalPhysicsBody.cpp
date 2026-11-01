#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// Native ARTIST ExternalPhysicsBody. Real XDK VMX operations retain all
// lanes, ordered fused arithmetic, reciprocal refinements and store order.
// Root's PC-only state removal and the native120-byte class gates remain.
#include <cstddef>   // offsetof

CGS_STATIC_ASSERT((sizeof(BrnPhysics::ExternalPhysicsBody) == 0x120),
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

    CGS_STATIC_ASSERT((sizeof(EpbLayoutProbe) == sizeof(BrnPhysics::ExternalPhysicsBody)),
                  "the probe adds nothing, so it shares the class's frame");
    CGS_STATIC_ASSERT((offsetof(EpbLayoutProbe, mLocalInverseInertia) == 0x70),
                  "mLocalInverseInertia @ +0x70");
    CGS_STATIC_ASSERT((offsetof(EpbLayoutProbe, mWorldInverseInertia) == 0xA0),
                  "mWorldInverseInertia @ +0xA0");
    CGS_STATIC_ASSERT((offsetof(EpbLayoutProbe, mfMass)               == 0xD0),
                  "mfMass @ +0xD0");
    CGS_STATIC_ASSERT((offsetof(EpbLayoutProbe, mTotalLinearForce)    == 0xE0),
                  "mTotalLinearForce @ +0xE0");
    CGS_STATIC_ASSERT((offsetof(EpbLayoutProbe, mTotalTorque)         == 0xF0),
                  "mTotalTorque @ +0xF0");
    CGS_STATIC_ASSERT((offsetof(EpbLayoutProbe, mTotalLinearImpulse)  == 0x100),
                  "mTotalLinearImpulse @ +0x100");
    CGS_STATIC_ASSERT((offsetof(EpbLayoutProbe, mTotalAngularImpulse) == 0x110),
                  "mTotalAngularImpulse @ +0x110");
}

namespace BrnPhysics
{
    namespace vpu = rw::math::vpu;

    // DecFIGS ExternalPhysicsBody.h:40; ARTIST82C5B030 initializes
    // the full row82FB9AF0 from the60.0f word82092BC4.
    const VecFloat kvfSixty(60.0f);

    namespace
    {
        inline __vector4 One() { return __vcfsx(__vspltisw(1), 0); }
        inline __vector4 SignMask()
        {
            const __vector4 bits = __vspltisw(-1);
            return __vslw(bits, bits);
        }
        inline __vector4 FlipSigns(__vector4 value) { return __vxor(value, SignMask()); }
        inline __vector4 SplatValue(f32 value) { return __vspltw(__lvlx(&value, 0), 0); }

        inline bool IsValid3(__vector4 value)
        {
            unsigned int comparison = 0;
            __vector4 lane = __vspltw(value, 0);
            __vcmpeqfpR(lane, lane, &comparison);
            if (!(comparison & 0x80u)) return false;
            lane = __vspltw(value, 1);
            __vcmpeqfpR(lane, lane, &comparison);
            if (!(comparison & 0x80u)) return false;
            lane = __vspltw(value, 2);
            __vcmpeqfpR(lane, lane, &comparison);
            return (comparison & 0x80u) != 0;
        }

        inline bool IsValid4(__vector4 value)
        {
            unsigned int comparison = 0;
            __vcmpeqfpR(value, value, &comparison);
            return (comparison & 0x80u) != 0;
        }

        inline __vector4 ReciprocalTwice(__vector4 value)
        {
            const __vector4 one = One();
            __vector4 estimate = __vrefp(value);
            estimate = __vmaddfp(estimate, __vnmsubfp(estimate, value, one), estimate);
            return __vmaddfp(estimate, __vnmsubfp(estimate, value, one), estimate);
        }

        inline __vector4 CrossNative(__vector4 a, __vector4 b)
        {
            const __vector4 product = __vmulfp(a, __vpermwi(b, 0x63));
            return __vpermwi(__vnmsubfp(__vpermwi(a, 0x63), b, product), 0x63);
        }

        inline __vector4 RowProduct(__vector4 x, __vector4 y, __vector4 z, __vector4 value)
        {
            __vector4 result = __vmulfp(x, __vspltw(value, 0));
            result = __vmaddfp(y, __vspltw(value, 1), result);
            return __vmaddfp(z, __vspltw(value, 2), result);
        }

        inline __vector4 RotateToWorld(const Matrix44Affine& transform, __vector4 value)
        {
            return RowProduct(transform.xAxis.mV, transform.yAxis.mV, transform.zAxis.mV, value);
        }

        // Tagged local-force/impulse helpers: ARTIST825A17EC/19F4/1AA4
        // start with Y's rounded product, then fuse X and Z in that order.
        inline __vector4 RotateTaggedToWorld(const Matrix44Affine& transform, __vector4 value)
        {
            __vector4 result = __vmulfp(transform.yAxis.mV, __vspltw(value, 1));
            result = __vmaddfp(transform.xAxis.mV, __vspltw(value, 0), result);
            return __vmaddfp(transform.zAxis.mV, __vspltw(value, 2), result);
        }

        // Primary image words82014AF0/AE0/AD0/AC0, in the original order.
        static const __vector4 kLogLow = {1.44268966f, -0.721165776f, 0.478684813f, -0.347305417f};
        static const __vector4 kLogHigh = {0.241873696f, -0.137531206f, 0.0520646796f, -0.00931049418f};
        static const __vector4 kExpLow = {1.0f, -0.693147182f, 0.240226462f, -0.055503644f};
        static const __vector4 kExpHigh = {0.00961597636f, -0.00132823968f, 0.000147491097f, -1.08635004e-05f};

        inline __vector4 Polynomial7(__vector4 x, __vector4 low, __vector4 high)
        {
            __vector4 lo = __vmaddfp(x, __vspltw(low, 1), __vspltw(low, 0));
            __vector4 hi = __vmaddfp(x, __vspltw(high, 1), __vspltw(high, 0));
            const __vector4 x2 = __vmulfp(x, x);
            const __vector4 x3 = __vmulfp(x, x2);
            lo = __vmaddfp(x2, __vspltw(low, 2), lo);
            hi = __vmaddfp(x2, __vspltw(high, 2), hi);
            const __vector4 x4 = __vmulfp(x2, x2);
            lo = __vmaddfp(x3, __vspltw(low, 3), lo);
            hi = __vmaddfp(x3, __vspltw(high, 3), hi);
            return __vmaddfp(x4, hi, lo);
        }

        // ARTIST825B2CD8..2E74 and the three inlined825BE210 pipelines.
        // Preserve estimate, polynomial rounding boundaries and bit fixups.
        inline __vector4 PowerNative(__vector4 base, __vector4 exponent)
        {
            const __vector4 zero = __vspltisw(0);
            const __vector4 bits = __vspltisw(-1);
            const __vector4 sign = __vslw(bits, bits);
            const __vector4 exponentMask = __vslw(bits, __vspltisw(-9));
            const __vector4 one = One();
            const __vector4 absolute = __vandc(base, sign);
            const __vector4 mantissa = __vsel(absolute, one, exponentMask);
            const __vector4 x = __vsubfp(mantissa, one);
            const __vector4 integralLog = __vrfim(__vlogefp(absolute));
            const __vector4 xTimesExponent = __vmulfp(x, exponent);
            const __vector4 integerTimesExponent = __vmulfp(integralLog, exponent);
            const __vector4 logPolynomial = Polynomial7(x, kLogLow, kLogHigh);
            const __vector4 argument = __vmaddfp(xTimesExponent, logPolynomial, integerTimesExponent);
            const __vector4 integerPart = __vrfim(argument);
            const __vector4 fraction = __vsubfp(argument, integerPart);
            const __vector4 integralPower = __vexptefp(integerPart);
            const __vector4 expPolynomial = Polynomial7(fraction, kExpLow, kExpHigh);
            const __vector4 initial = __vrefp(expPolynomial);
            const __vector4 first = __vmaddfp(initial, __vnmsubfp(expPolynomial, initial, one), initial);
            const __vector4 second = __vmaddfp(first, __vnmsubfp(expPolynomial, first, one), first);
            const __vector4 reciprocal = __vsel(initial, second, __vcmpeqfp(first, first));

            const __vector4 integerBits = __vctsxs(exponent, 0);
            const __vector4 odd = __vand(integerBits, __vspltisw(1));
            const __vector4 oddSign = __vand(__vand(base, sign), __vslw(odd, bits));
            const __vector4 zeroExponent = __vcmpeqfp(exponent, zero);
            const __vector4 negativeExponent = __vcmpgtfp(zero, exponent);
            const __vector4 zeroBase = __vcmpeqfp(base, zero);
            const __vector4 negativeBase = __vcmpgtfp(zero, base);
            const __vector4 integerExponent = __vcmpeqfp(exponent, __vrfiz(exponent));
            const __vector4 invalidNegative = __vandc(negativeBase, integerExponent);
            const __vector4 zeroOrZeroExponent = __vor(zeroBase, zeroExponent);
            const __vector4 fixMask = __vor(invalidNegative, zeroOrZeroExponent);
            const __vector4 nanBits = __vsrw(exponentMask, __vspltisw(1));
            __vector4 special = __vsel(nanBits, __vor(zero, oddSign), __vandc(zeroBase, negativeExponent));
            special = __vsel(special, one, zeroExponent);
            const __vector4 approximate = __vor(__vmulfp(integralPower, reciprocal), oddSign);
            return __vsel(approximate, special, fixMask);
        }
    }

    void ExternalPhysicsBody::AddWorldSpaceForce(Vector3 value)
    {
        CGS_ASSERT(IsValid3(value.mV), "Bad force added ");
        mTotalLinearForce.mV = __vaddfp(mTotalLinearForce.mV, value.mV);
    }

    void ExternalPhysicsBody::AddWorldSpaceTorque(Vector3 value)
    {
        CGS_ASSERT(IsValid3(value.mV), "Bad torque added ");
        mTotalTorque.mV = __vaddfp(mTotalTorque.mV, value.mV);
    }

    void ExternalPhysicsBody::AddWorldSpaceImpulse(Vector3 value)
    {
        CGS_ASSERT(IsValid3(value.mV), "Bad impulse added ");
        mTotalLinearImpulse.mV = __vaddfp(mTotalLinearImpulse.mV, value.mV);
    }

    void ExternalPhysicsBody::AddWorldSpaceAngularImpulse(Vector3 value)
    {
        CGS_ASSERT(IsValid3(value.mV), "Bad angular impulse added ");
        mTotalAngularImpulse.mV = __vaddfp(mTotalAngularImpulse.mV, value.mV);
    }

    void ExternalPhysicsBody::AddLocalSpaceForce(Vector3 value)
    {
        CGS_ASSERT(IsValid3(value.mV), "Bad force added ");
        const __vector4 world = RotateToWorld(mTransform, value.mV);
        mTotalLinearForce.mV = __vaddfp(mTotalLinearForce.mV, world);
    }

    // ARTIST825A1B10: all lanes of dt/mass/impulses participate.
    void ExternalPhysicsBody::CalculateNewVelocity(VecFloat deltaTime)
    {
        const __vector4 inverseMass = ReciprocalTwice(mfMass.mV);
        CGS_ASSERT(IsValid3(mAngularVelocity.mV), "rw::math::IsValid(mAngularVelocity)");
        CGS_ASSERT(IsValid3(mLinearVelocity.mV), "rw::math::IsValid(mLinearVelocity)");
        const __vector4 impulse = __vmaddfp(mTotalLinearForce.mV, deltaTime.mV, mTotalLinearImpulse.mV);
        mLinearVelocity.mV = __vmaddfp(impulse, inverseMass, mLinearVelocity.mV);
        const __vector4 angularImpulse = __vmaddfp(mTotalTorque.mV, deltaTime.mV, mTotalAngularImpulse.mV);
        const __vector4 deltaAngular = RowProduct(mWorldInverseInertia.xAxis.mV,
            mWorldInverseInertia.yAxis.mV, mWorldInverseInertia.zAxis.mV, angularImpulse);
        mAngularVelocity.mV = __vaddfp(mAngularVelocity.mV, deltaAngular);
        CGS_ASSERT(IsValid3(mAngularVelocity.mV), "rw::math::IsValid(mAngularVelocity)");
        CGS_ASSERT(IsValid3(mLinearVelocity.mV), "rw::math::IsValid(mLinearVelocity)");
        const __vector4 zero = __vspltisw(0);
        mTotalLinearImpulse.mV = zero;
        mTotalAngularImpulse.mV = zero;
        mTotalLinearForce.mV = zero;
        mTotalTorque.mV = zero;
    }

    void ExternalPhysicsBody::IntegrateTransform(VecFloat deltaTime)
    {
        mTransform.wAxis.mV = __vmaddfp(mLinearVelocity.mV, deltaTime.mV, mTransform.wAxis.mV);
        const __vector4 spin = __vmulfp(mAngularVelocity.mV, deltaTime.mV);
        mTransform.zAxis.mV = __vsubfp(mTransform.zAxis.mV, CrossNative(mTransform.zAxis.mV, spin));
        mTransform.yAxis.mV = __vsubfp(mTransform.yAxis.mV, CrossNative(mTransform.yAxis.mV, spin));
        mTransform.xAxis.mV = __vsubfp(mTransform.xAxis.mV, CrossNative(mTransform.xAxis.mV, spin));
        mTransform = vpu::OrthoNormalize3x3(mTransform);
        CalculateWorldIntertia();
    }

    void ExternalPhysicsBody::CalculateWorldIntertia()
    {
        CGS_ASSERT(IsValid3(mTransform.xAxis.mV) && IsValid3(mTransform.yAxis.mV)
            && IsValid3(mTransform.zAxis.mV) && IsValid3(mTransform.wAxis.mV),
            "rw::math::IsValid( mTransform )");
        const __vector4 r0 = mTransform.xAxis.mV;
        const __vector4 r1 = mTransform.yAxis.mV;
        const __vector4 r2 = mTransform.zAxis.mV;
        const __vector4 zero = __vspltisw(0);
        const __vector4 xzHigh = __vmrghw(r0, r2);
        const __vector4 yHigh = __vmrghw(r1, zero);
        const __vector4 xzLow = __vmrglw(r0, r2);
        const __vector4 yLow = __vmrglw(r1, zero);
        const __vector4 rt0 = __vmrghw(xzHigh, yHigh);
        const __vector4 rt1 = __vmrglw(xzHigh, yHigh);
        const __vector4 rt2 = __vmrghw(xzLow, yLow);
        const __vector4 l0 = mLocalInverseInertia.xAxis.mV;
        const __vector4 l1 = mLocalInverseInertia.yAxis.mV;
        const __vector4 l2 = mLocalInverseInertia.zAxis.mV;
        mWorldInverseInertia.xAxis.mV = RowProduct(l0, l1, l2, rt0);
        mWorldInverseInertia.yAxis.mV = RowProduct(l0, l1, l2, rt1);
        mWorldInverseInertia.zAxis.mV = RowProduct(l0, l1, l2, rt2);
        const __vector4 t0 = mWorldInverseInertia.xAxis.mV;
        const __vector4 t1 = mWorldInverseInertia.yAxis.mV;
        const __vector4 t2 = mWorldInverseInertia.zAxis.mV;
        mWorldInverseInertia.zAxis.mV = RowProduct(r0, r1, r2, t2);
        mWorldInverseInertia.yAxis.mV = RowProduct(r0, r1, r2, t1);
        mWorldInverseInertia.xAxis.mV = RowProduct(r0, r1, r2, t0);
        CGS_ASSERT(IsValid3(mWorldInverseInertia.xAxis.mV) && IsValid3(mWorldInverseInertia.yAxis.mV)
            && IsValid3(mWorldInverseInertia.zAxis.mV), "rw::math::IsValid( mWorldInverseInertia )");
    }

    void ExternalPhysicsBody::AddLocalForce(Vector3 force, rw::physics::InputSpace forceSpace,
        Vector3 position, rw::physics::InputSpace positionSpace)
    {
        CGS_ASSERT(IsValid3(force.mV), "Bad positioned force added");
        CGS_ASSERT(IsValid3(position.mV), "Force applied at a bad position");
        const __vector4 worldForce = forceSpace == rw::physics::BODY_SPACE ? RotateTaggedToWorld(mTransform, force.mV) : force.mV;
        const __vector4 arm = positionSpace == rw::physics::WORLD_SPACE ? __vsubfp(position.mV, mTransform.wAxis.mV) : RotateTaggedToWorld(mTransform, position.mV);
        mTotalLinearForce.mV = __vaddfp(mTotalLinearForce.mV, worldForce);
        mTotalTorque.mV = __vaddfp(mTotalTorque.mV, CrossNative(arm, worldForce));
    }

    void ExternalPhysicsBody::AddLocalImpulse(Vector3 impulse, rw::physics::InputSpace impulseSpace,
        Vector3 position, rw::physics::InputSpace positionSpace)
    {
        CGS_ASSERT(IsValid3(impulse.mV), "Bad positioned impulse added");
        CGS_ASSERT(IsValid3(position.mV), "Impulse applied at a bad position");
        const __vector4 worldImpulse = impulseSpace == rw::physics::BODY_SPACE ? RotateTaggedToWorld(mTransform, impulse.mV) : impulse.mV;
        const __vector4 arm = positionSpace == rw::physics::WORLD_SPACE ? __vsubfp(position.mV, mTransform.wAxis.mV) : RotateTaggedToWorld(mTransform, position.mV);
        mTotalLinearImpulse.mV = __vaddfp(mTotalLinearImpulse.mV, worldImpulse);
        mTotalAngularImpulse.mV = __vaddfp(mTotalAngularImpulse.mV, CrossNative(arm, worldImpulse));
    }

    void ExternalPhysicsBody::GetImpulsesFromLocalImpulse(Vector3 impulse, rw::physics::InputSpace impulseSpace,
        Vector3 position, rw::physics::InputSpace positionSpace, Vector3* linearOut, Vector3* angularOut) const
    {
        const __vector4 worldImpulse = impulseSpace == rw::physics::BODY_SPACE ? RotateTaggedToWorld(mTransform, impulse.mV) : impulse.mV;
        const __vector4 arm = positionSpace == rw::physics::WORLD_SPACE ? __vsubfp(position.mV, mTransform.wAxis.mV) : RotateTaggedToWorld(mTransform, position.mV);
        linearOut->mV = worldImpulse;
        angularOut->mV = CrossNative(arm, worldImpulse);
    }

    void ExternalPhysicsBody::Construct()
    {
        ExternallySimulatedBody::Construct();
        const __vector4 zero = __vspltisw(0);
        mTotalLinearImpulse.mV = zero; mTotalAngularImpulse.mV = zero;
        mTotalLinearForce.mV = zero; mTotalTorque.mV = zero;
    }

    void ExternalPhysicsBody::SetMass(VecFloat mass) { mfMass.mV = mass.mV; }

    void ExternalPhysicsBody::Destruct()
    {
        ExternallySimulatedBody::Destruct();
        const __vector4 zero = __vspltisw(0);
        mTotalLinearImpulse.mV = zero; mTotalAngularImpulse.mV = zero;
        mTotalLinearForce.mV = zero; mTotalTorque.mV = zero;
    }

    void ExternalPhysicsBody::Release()
    {
        ExternallySimulatedBody::Release();
        const __vector4 zero = __vspltisw(0);
        mTotalLinearImpulse.mV = zero; mTotalAngularImpulse.mV = zero;
        mTotalLinearForce.mV = zero; mTotalTorque.mV = zero;
    }

    bool ExternalPhysicsBody::Prepare()
    {
        ExternallySimulatedBody::Prepare();
        const __vector4 zero = __vspltisw(0);
        mTotalLinearImpulse.mV = zero; mTotalAngularImpulse.mV = zero;
        mTotalLinearForce.mV = zero; mTotalTorque.mV = zero;
        CalculateWorldIntertia();
        return true;
    }

    VecFloat ExternalPhysicsBody::CalculateCollisionImpulseWithInanimateObject(Vector3 point,
        Vector3 pointVelocity, Vector3 normal, VecFloat restitution, Vector3* impulseOut, VecFloat* inverseOut)
    {
        const __vector4 one = One();
        const __vector4 arm = __vsubfp(point.mV, mTransform.wAxis.mV);
        const __vector4 cross = CrossNative(arm, normal.mV);
        const __vector4 angular = RowProduct(mWorldInverseInertia.xAxis.mV,
            mWorldInverseInertia.yAxis.mV, mWorldInverseInertia.zAxis.mV, cross);
        const __vector4 atContact = CrossNative(angular, arm);
        const __vector4 inverseMass = ReciprocalTwice(mfMass.mV);
        const __vector4 denominator = __vmaddfp(inverseMass, one, __vmsum3fp(atContact, normal.mV));
        const __vector4 numerator = __vmulfp(FlipSigns(__vaddfp(one, restitution.mV)), __vmsum3fp(normal.mV, pointVelocity.mV));
        CGS_ASSERT(inverseOut != 0, "lpvfInvInertiaOut != NULL");
        __vector4 reciprocal = __vrefp(denominator);
        inverseOut->mV = denominator;
        reciprocal = __vmaddfp(reciprocal, __vnmsubfp(reciprocal, denominator, one), reciprocal);
        reciprocal = __vmaddfp(reciprocal, __vnmsubfp(reciprocal, denominator, one), reciprocal);
        const __vector4 magnitude = __vmulfp(reciprocal, numerator);
        VecFloat result(magnitude);
        impulseOut->mV = __vmulfp(normal.mV, __vandc(magnitude, SignMask()));
        return result;
    }

    VecFloat ExternalPhysicsBody::CalculateCollisionImpulseWithBody(const ExternalPhysicsBody& body,
        Vector3 pointA, Vector3 pointB, Vector3 impactVelocity, Vector3 normal, VecFloat restitution,
        Vector3* impulseOut, VecFloat* inverseAOut, VecFloat* inverseBOut) const
    {
        const __vector4 one = One();
        const __vector4 armA = __vsubfp(pointA.mV, mTransform.wAxis.mV);
        const __vector4 armB = __vsubfp(pointB.mV, body.mTransform.wAxis.mV);
        const __vector4 a = RowProduct(mWorldInverseInertia.xAxis.mV, mWorldInverseInertia.yAxis.mV,
            mWorldInverseInertia.zAxis.mV, CrossNative(armA, normal.mV));
        const __vector4 b = RowProduct(body.mWorldInverseInertia.xAxis.mV, body.mWorldInverseInertia.yAxis.mV,
            body.mWorldInverseInertia.zAxis.mV, CrossNative(armB, normal.mV));
        const __vector4 apart = CrossNative(a, armA);
        const __vector4 bpart = CrossNative(b, armB);
        const __vector4 inverseA = __vmulfp(ReciprocalTwice(mfMass.mV), one);
        const __vector4 inverseB = __vmulfp(ReciprocalTwice(body.mfMass.mV), one);
        const __vector4 denominator = __vaddfp(__vaddfp(inverseA, inverseB), __vmsum3fp(__vaddfp(apart, bpart), normal.mV));
        const __vector4 numerator = __vmulfp(FlipSigns(__vaddfp(one, restitution.mV)), __vmsum3fp(normal.mV, impactVelocity.mV));
        CGS_ASSERT(inverseAOut != 0, "lpfInvInertiaAOut != NULL");
        inverseAOut->mV = __vaddfp(inverseA, __vmsum3fp(apart, normal.mV));
        CGS_ASSERT(inverseBOut != 0, "lpfInvInertiaBOut != NULL");
        __vector4 reciprocal = __vrefp(denominator);
        reciprocal = __vmaddfp(reciprocal, __vnmsubfp(reciprocal, denominator, one), reciprocal);
        inverseBOut->mV = __vaddfp(inverseB, __vmsum3fp(bpart, normal.mV));
        reciprocal = __vmaddfp(reciprocal, __vnmsubfp(reciprocal, denominator, one), reciprocal);
        const __vector4 magnitude = __vmulfp(reciprocal, numerator);
        VecFloat result(magnitude);
        const __vector4 negativeMagnitude = FlipSigns(__vandc(magnitude, SignMask()));
        impulseOut->mV = __vmulfp(normal.mV, negativeMagnitude);
        return result;
    }

    void ExternalPhysicsBody::DampenAngularVelocity(VecFloat damping, VecFloat deltaTime)
    {
        const __vector4 exponent = __vmulfp(kvfSixty.mV, deltaTime.mV);
        mAngularVelocity.mV = __vmulfp(mAngularVelocity.mV, PowerNative(damping.mV, exponent));
    }

    void ExternalPhysicsBody::DampPitchYawRoll(VecFloat pitch, VecFloat yaw, VecFloat roll, VecFloat deltaTime)
    {
        unsigned int comparison = 0;
        __vcmpgtfpR(__vandc(deltaTime.mV, SignMask()), SplatValue(1.1920928955078125e-07f), &comparison);
        if (comparison & 0x20u) return;
        const __vector4 exponent = __vmulfp(kvfSixty.mV, deltaTime.mV);
        const __vector4 pitchFactor = PowerNative(pitch.mV, exponent);
        const __vector4 pitchVelocity = __vmsum3fp(mTransform.xAxis.mV, mAngularVelocity.mV);
        const __vector4 pitchScale = __vmulfp(pitchVelocity, pitchFactor);
        mAngularVelocity.mV = __vsubfp(mAngularVelocity.mV, __vmulfp(mTransform.xAxis.mV, pitchScale));
        const __vector4 yawFactor = PowerNative(yaw.mV, exponent);
        const __vector4 yawVelocity = __vmsum3fp(mTransform.yAxis.mV, mAngularVelocity.mV);
        const __vector4 yawScale = __vmulfp(yawVelocity, yawFactor);
        mAngularVelocity.mV = __vsubfp(mAngularVelocity.mV, __vmulfp(mTransform.yAxis.mV, yawScale));
        const __vector4 rollFactor = PowerNative(roll.mV, exponent);
        const __vector4 rollVelocity = __vmsum3fp(mTransform.zAxis.mV, mAngularVelocity.mV);
        const __vector4 rollScale = __vmulfp(rollVelocity, rollFactor);
        mAngularVelocity.mV = __vsubfp(mAngularVelocity.mV, __vmulfp(mTransform.zAxis.mV, rollScale));
    }

    void ExternalPhysicsBody::ReadPropertiesFromChangeInertiaEvent(
        const CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia* event)
    {
        if ((event->mu32Flags & 4u) && (event->mu32Flags & 2u))
        {
            const Vector3 inverse = event->mInertia.GetInverseInertia();
            const __vector4 x = {1.0f, 0.0f, 0.0f, 0.0f};
            const __vector4 y = {0.0f, 1.0f, 0.0f, 0.0f};
            const __vector4 z = {0.0f, 0.0f, 1.0f, 0.0f};
            mLocalInverseInertia.xAxis.mV = x;
            mLocalInverseInertia.yAxis.mV = y;
            mLocalInverseInertia.zAxis.mV = z;
            mLocalInverseInertia.xAxis.mV = __vmulfp(mLocalInverseInertia.xAxis.mV, __vspltw(inverse.mV, 0));
            // This one scalar division is the actual ARTIST825A245C fdivs.
            const f32 mass = 1.0f / event->mInertia.GetInverseMass();
            mLocalInverseInertia.yAxis.mV = __vmulfp(mLocalInverseInertia.yAxis.mV, __vspltw(inverse.mV, 1));
            mLocalInverseInertia.zAxis.mV = __vmulfp(mLocalInverseInertia.zAxis.mV, __vspltw(inverse.mV, 2));
            mfMass.mV = SplatValue(mass);
            CalculateWorldIntertia();
        }
    }

    namespace
    {
        void CheckStateFail(const char* context, const char* message)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << context;
            CGS_ASSERT(false, message);
        }
    }

    void ExternalPhysicsBody::CheckState(const char* context) const
    {
        if (!IsValid3(mTransform.xAxis.mV) || !IsValid3(mTransform.yAxis.mV)
            || !IsValid3(mTransform.zAxis.mV) || !IsValid3(mTransform.wAxis.mV)) CheckStateFail(context, "Bad transform");
        if (!IsValid3(mLocalInverseInertia.xAxis.mV) || !IsValid3(mLocalInverseInertia.yAxis.mV)
            || !IsValid3(mLocalInverseInertia.zAxis.mV)) CheckStateFail(context, "Bad local inverse inertia");
        if (!IsValid3(mWorldInverseInertia.xAxis.mV) || !IsValid3(mWorldInverseInertia.yAxis.mV)
            || !IsValid3(mWorldInverseInertia.zAxis.mV)) CheckStateFail(context, "Bad world inverse inertia");
        if (!IsValid4(mfMass.mV)) CheckStateFail(context, "Bad mass");
        if (!IsValid3(mTotalLinearForce.mV)) CheckStateFail(context, "Bad total linear force");
        if (!IsValid3(mTotalTorque.mV)) CheckStateFail(context, "Bad total torque");
        if (!IsValid3(mTotalLinearImpulse.mV)) CheckStateFail(context, "Bad total linear impulse");
        if (!IsValid3(mTotalAngularImpulse.mV)) CheckStateFail(context, "Bad total angular impulse");
        if (!IsValid3(mLinearVelocity.mV)) CheckStateFail(context, "Bad linear velocity");
        if (!IsValid3(mAngularVelocity.mV)) CheckStateFail(context, "Bad angular velocity");
    }

    Vector3 ExternalPhysicsBody::GetLocalVelocity(Vector3 point, rw::physics::InputSpace space) const
    {
        const __vector4 arm = space == rw::physics::WORLD_SPACE ? __vsubfp(point.mV, mTransform.wAxis.mV) : RotateToWorld(mTransform, point.mV);
        return Vector3(__vaddfp(mLinearVelocity.mV, CrossNative(mAngularVelocity.mV, arm)));
    }

    Vector3 ExternalPhysicsBody::GetLinearMomentum(VecFloat deltaTime) const
    {
        const __vector4 force = __vmulfp(mTotalLinearForce.mV, deltaTime.mV);
        const __vector4 momentum = __vmaddfp(mLinearVelocity.mV, mfMass.mV, force);
        return Vector3(__vaddfp(momentum, mTotalLinearImpulse.mV));
    }
}
