#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerDebugComponent.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h" // manager storage and pools
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "rw/math/vpu/matrix44affine_operation.h" // TransformPoint / TransformVector
#include "rw/math/vpu/matrix44_operation.h"       // full view-projection inverse
#include "rw/math/vpu/vector3_operation.h"         // Cross / Normalize / IsValid
#include "SDKs/XboxMath/XMVectorTan.h"           // RenderCone's original 0x825CA1FC call

#include <cstddef>   // offsetof (layout asserts)
#include <cmath>

// BrnPhysics::Vehicle::PhysicalTrafficManagerDebugComponent - Construct, the debug menu page, and
// the ARTIST-attested world-render path for physical traffic.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace
    {
        float ControlsFlush(float lfValue)
        {
            return std::fpclassify(lfValue) == FP_SUBNORMAL
                ? std::copysign(0.0f, lfValue) : lfValue;
        }

        float ControlsMultiplyAdd(float lfA, float lfB, float lfC)
        {
            return ControlsFlush(std::fmaf(ControlsFlush(lfA), ControlsFlush(lfB), ControlsFlush(lfC)));
        }

        Vector3 ControlsScale(Vector3 lvValue, float lfScale)
        {
            lfScale = ControlsFlush(lfScale);
            { const Vector3 lResult = {ControlsFlush(ControlsFlush(lvValue.x) * lfScale),
                           ControlsFlush(ControlsFlush(lvValue.y) * lfScale),
                           ControlsFlush(ControlsFlush(lvValue.z) * lfScale),
                           ControlsFlush(ControlsFlush(lvValue.w) * lfScale)}; return lResult; }
        }

        Vector3 ControlsScaleAdd(Vector3 lvValue, float lfScale, Vector3 lvBase)
        {
            { const Vector3 lResult = {ControlsMultiplyAdd(lvValue.x, lfScale, lvBase.x),
                           ControlsMultiplyAdd(lvValue.y, lfScale, lvBase.y),
                           ControlsMultiplyAdd(lvValue.z, lfScale, lvBase.z),
                           ControlsMultiplyAdd(lvValue.w, lfScale, lvBase.w)}; return lResult; }
        }

        Vector3 ControlsAdd(Vector3 lvA, Vector3 lvB)
        {
            { const Vector3 lResult = {ControlsFlush(ControlsFlush(lvA.x) + ControlsFlush(lvB.x)),
                           ControlsFlush(ControlsFlush(lvA.y) + ControlsFlush(lvB.y)),
                           ControlsFlush(ControlsFlush(lvA.z) + ControlsFlush(lvB.z)),
                           ControlsFlush(ControlsFlush(lvA.w) + ControlsFlush(lvB.w))}; return lResult; }
        }

        Vector3 ControlsSubtract(Vector3 lvA, Vector3 lvB)
        {
            { const Vector3 lResult = {ControlsFlush(ControlsFlush(lvA.x) - ControlsFlush(lvB.x)),
                           ControlsFlush(ControlsFlush(lvA.y) - ControlsFlush(lvB.y)),
                           ControlsFlush(ControlsFlush(lvA.z) - ControlsFlush(lvB.z)),
                           ControlsFlush(ControlsFlush(lvA.w) - ControlsFlush(lvB.w))}; return lResult; }
        }

        float ControlsNegativeMultiplySubtract(float lfA, float lfB, float lfC)
        {
            const float lfDifference = ControlsFlush(
                std::fmaf(ControlsFlush(lfA), ControlsFlush(lfB), -ControlsFlush(lfC)));
            return lfDifference != lfDifference ? lfDifference : -lfDifference;
        }

        Vector3 ConeCross(Vector3 lvA, Vector3 lvB)
        {
            { const Vector3 lResult = {
                ControlsNegativeMultiplySubtract(lvA.z, lvB.y, ControlsFlush(ControlsFlush(lvA.y) * ControlsFlush(lvB.z))),
                ControlsNegativeMultiplySubtract(lvA.x, lvB.z, ControlsFlush(ControlsFlush(lvA.z) * ControlsFlush(lvB.x))),
                ControlsNegativeMultiplySubtract(lvA.y, lvB.x, ControlsFlush(ControlsFlush(lvA.x) * ControlsFlush(lvB.y))),
                ControlsNegativeMultiplySubtract(lvA.w, lvB.w, ControlsFlush(ControlsFlush(lvA.w) * ControlsFlush(lvB.w)))}; return lResult; }
        }

        Vector3 NormalizeConeBasis(Vector3 lvValue)
        {
            const Vector3 lvFlushed{ControlsFlush(lvValue.x), ControlsFlush(lvValue.y),
                                    ControlsFlush(lvValue.z), ControlsFlush(lvValue.w)};
            const float lfLengthSquared = rw::math::vpu::SLerpDetail::Dot3(lvFlushed, lvFlushed);
            // ARTIST 0x825CA0EC..110 / 128..14C and its alternate arm:
            // two rsqrt refinements, with NO zero-vector fallback.
            return ControlsScale(lvValue, rw::math::vpu::SLerpDetail::RefinedReciprocalSqrt(lfLengthSquared, 2));
        }
    }

    // RenderControls @0x825CA2B8. v1 is the world point, r4 the controls and r5
    // the renderer; this is unused. The clip test uses Z before any perspective
    // divide. The full inverse's first two rows form the billboard, with only
    // the first row scaled by virtual-screen height / width.
    void PhysicalTrafficManagerDebugComponent::RenderControls(
        Vector3 lvPosition, const BrnPlayerDriverControls* lpControls,
        CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        CGS_ASSERT(lpControls != 0, "lpControls"); // :695
        CGS_ASSERT(lpDisplay != 0, "lpDisplay");   // :696

        const Vector3 lvCentre = ControlsAdd(lvPosition, Vector3{0.0f, 2.5f, 0.0f, 0.0f});
        const Matrix44 lViewProjection = lpDisplay->GetViewProjectionMatrix();
        const float lfClipZ = ControlsMultiplyAdd(lViewProjection.zAxis.z, lvCentre.z,
            ControlsMultiplyAdd(lViewProjection.yAxis.z, lvCentre.y,
                ControlsMultiplyAdd(lViewProjection.xAxis.z, lvCentre.x, lViewProjection.wAxis.z)));
        if (0.001f > lfClipZ) // equality and unordered values continue, as vcmpgtfp does
            return;

        VecFloat lvfDeterminant;
        const Matrix44 lInverse = rw::math::vpu::Inverse(lViewProjection, lvfDeterminant);
        const Vector2 lvScreenSize = lpDisplay->GetVirtualScreenSize();
        // 0x825CA4C4..0x825CA4D8: vrefp plus TWO unguarded Newton steps,
        // then a separately rounded multiply by the screen height.
        const float lfAspect = ControlsFlush(
            rw::math::vpu::SLerpDetail::RefinedReciprocal(lvScreenSize.x, 2) * ControlsFlush(lvScreenSize.y));
        const Vector3 lvRight = ControlsScale(
            Vector3{lInverse.xAxis.x, lInverse.xAxis.y, lInverse.xAxis.z, lInverse.xAxis.w}, lfAspect);
        const Vector3 lvUp{lInverse.yAxis.x, lInverse.yAxis.y, lInverse.yAxis.z, lInverse.yAxis.w};

        rw::RGBA lBackground, lSteeringColour, lBrakeColour, lGasColour;
        lBackground.m_rgba = 0xFF008CB4u;     // dword_82FBA074
        lSteeringColour.m_rgba = 0xFF008CFFu; // dword_82FBA070
        lBrakeColour.m_rgba = 0xE63C3CB4u;    // dword_82FBA050
        lGasColour.m_rgba = 0xE6B43C3Cu;      // dword_82FBA034

        lpDisplay->DrawLine(ControlsSubtract(lvCentre, ControlsScale(lvRight, 2.0f)),
                            ControlsScaleAdd(lvRight, 2.0f, lvCentre), lBackground);
        const Vector3 lvSteering = ControlsScaleAdd(lvRight, lpControls->mfSteering * 2.0f, lvCentre);
        lpDisplay->DrawLine(ControlsScaleAdd(lvUp, 0.2f, lvSteering),
                            ControlsSubtract(lvSteering, ControlsScale(lvUp, 0.2f)), lSteeringColour);

        const Vector3 lvBrake = ControlsScaleAdd(lvUp, 0.4f,
            ControlsScaleAdd(lvRight, -0.8f, lvCentre)); // unk_82FBA060
        lpDisplay->DrawLine(lvBrake, ControlsScaleAdd(lvUp, 1.5f, lvBrake), lBackground);
        const Vector3 lvBrakeHalfWidth = ControlsScale(lvRight, 0.3f);
        const Vector3 lvBrakeHeight = ControlsScale(lvUp, lpControls->mfBrake * 1.5f);
        const Vector3 lvBrakeLeft = ControlsSubtract(lvBrake, lvBrakeHalfWidth);
        const Vector3 lvBrakeRight = ControlsAdd(lvBrake, lvBrakeHalfWidth);
        lpDisplay->DrawSolidQuad(lvBrakeLeft, lvBrakeRight,
            ControlsAdd(lvBrakeRight, lvBrakeHeight), ControlsAdd(lvBrakeLeft, lvBrakeHeight), lBrakeColour);

        const Vector3 lvGas = ControlsScaleAdd(lvUp, 0.4f,
            ControlsScaleAdd(lvRight, 0.8f, lvCentre)); // unk_82FBA040
        lpDisplay->DrawLine(lvGas, ControlsScaleAdd(lvUp, 1.5f, lvGas), lBackground);
        const Vector3 lvGasHalfWidth = ControlsScale(lvRight, 0.3f);
        const Vector3 lvGasHeight = ControlsScale(lvUp, lpControls->mfGas * 1.5f);
        const Vector3 lvGasLeft = ControlsSubtract(lvGas, lvGasHalfWidth);
        const Vector3 lvGasRight = ControlsAdd(lvGas, lvGasHalfWidth);
        lpDisplay->DrawSolidQuad(lvGasLeft, lvGasRight,
            ControlsAdd(lvGasRight, lvGasHeight), ControlsAdd(lvGasLeft, lvGasHeight), lGasColour);
    }

    // ========================================================================================
    // PhysicalTrafficManagerDebugComponent::Construct
    // DWARF: BrnPhysicalTrafficManagerDebugComponent.cpp:54  `void Construct(PhysicalTrafficManager *)`
    //
    // The X360 build INLINED this into PhysicalTrafficManager::Construct, so no out-of-line
    // symbol survives; the store set below is transcribed from that expansion
    // (0x82636DF8..0x82636E28, r29 == the component base). The base's own Construct survives as
    // a real call there -- `bl 0x8284CB38`, which is a single `blr`: the ICF representative for
    // every empty function in the image (IDA labels it BaseCollisionGenerator::Destruct).
    //
    // The one non-zero default is mfAirRamLengthScale = 5.0f (flt_8200426C, read out of the
    // image as 40 A0 00 00). It is a plain scalar-pool constant, not one of the static-init-
    // filled slots that read zero in the image.
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::Construct(PhysicalTrafficManager* lpTrafficManager)
    {

        mpTrafficManager           = lpTrafficManager;  // stw  r31,   0x0C(r29)
        mbDrawTrafficAirRams       = false;             // stb  0,     0x10(r29)
        mfAirRamLengthScale        = 5.0f;              // stfs 5.0f,  0x14(r29)   flt_8200426C
        mbDrawDriverControls       = false;             // stb  0,     0x18(r29)
        mbDrawPhysicalTraffic      = false;             // stb  0,     0x19(r29)
        mbDrawJoints               = false;             // stb  0,     0x1A(r29)
        mbDrawSimpleTraffic        = false;             // stb  0,     0x1B(r29)
        mbDrawTrafficUsingBoxWorld = false;             // stb  0,     0x1C(r29)
        mbDrawCatchupTargets       = false;             // stb  0,     0x1D(r29)
        mbDrawResetOnWaterHeight   = false;             // stb  0,     0x1E(r29)
    }

    // ========================================================================================
    // Layout pins. Never called -- exists only so offsetof() can see the private members.
    // X360-RELATIVE: the base's vtable pointer and mpTrafficManager both widen on x64, so the
    // absolute X360 offsets (+12 .. +30) cannot be asserted; every delta below can, and each one
    // is a gap between two of the asm's own store targets.
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::_AssertLayout()
    {
        typedef PhysicalTrafficManagerDebugComponent D;
        CGS_STATIC_ASSERT((offsetof(D, mbDrawTrafficAirRams) - offsetof(D, mpTrafficManager) == sizeof(void*)),
                      "mbDrawTrafficAirRams follows the manager pointer (X360 +12 -> +16)");
        CGS_STATIC_ASSERT((offsetof(D, mfAirRamLengthScale) - offsetof(D, mbDrawTrafficAirRams) == 4),
                      "mfAirRamLengthScale is one word past the air-ram flag (X360 +16 -> +20)");
        CGS_STATIC_ASSERT((offsetof(D, mbDrawDriverControls)       - offsetof(D, mfAirRamLengthScale)  == 4), "X360 +20 -> +24");
        CGS_STATIC_ASSERT((offsetof(D, mbDrawPhysicalTraffic)      - offsetof(D, mbDrawDriverControls) == 1), "X360 +25");
        CGS_STATIC_ASSERT((offsetof(D, mbDrawJoints)               - offsetof(D, mbDrawDriverControls) == 2), "X360 +26");
        CGS_STATIC_ASSERT((offsetof(D, mbDrawSimpleTraffic)        - offsetof(D, mbDrawDriverControls) == 3), "X360 +27");
        CGS_STATIC_ASSERT((offsetof(D, mbDrawTrafficUsingBoxWorld) - offsetof(D, mbDrawDriverControls) == 4), "X360 +28");
        CGS_STATIC_ASSERT((offsetof(D, mbDrawCatchupTargets)       - offsetof(D, mbDrawDriverControls) == 5), "X360 +29");
        CGS_STATIC_ASSERT((offsetof(D, mbDrawResetOnWaterHeight)   - offsetof(D, mbDrawDriverControls) == 6), "X360 +30");
    }

    // @0x827DBA50  BrnPhysics::Vehicle::PhysicalTrafficManagerDebugComponent::GetName
    //   lis r11,aPhysicalTraffi@ha ; addi r3,r11,aPhysicalTraffi@l "Physical traffic" ; blr
    const char* PhysicalTrafficManagerDebugComponent::GetName() const
    {
        return "Physical traffic";
    }

    // ========================================================================================
    // OnActivate @0x8261C678
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::OnActivate()
    {
        RegisterFunction(&PhysicalTrafficManagerDebugComponent::ResetTrafficAboveWater, this,
                         "Reset traffic on water");
        RegisterVariable(&mbDrawCatchupTargets, "Draw catchup targets");
        RegisterVariable(&mbDrawSimpleTraffic, "Draw simple traffic");
        RegisterVariable(&mbDrawPhysicalTraffic, "Draw physical traffic");
        RegisterVariable(&mbDrawJoints, "Draw joints");
        RegisterVariable(&mbDrawTrafficUsingBoxWorld, "Draw box-triangle tests");

        RegisterVariable(&mpTrafficManager->mbAllowArticulatedJointBreaking,
                         "Allow breaking joints");
        RegisterVariable(&mpTrafficManager->mArticulatedJointPool.mfSwingAngleDegrees,
                         "Max joint swing angle");
        RegisterVariable(&mpTrafficManager->mArticulatedJointPool.mfMaxSwingVelocityDegrees,
                         "Max joint swing velocity");
        SetRange(&mpTrafficManager->mArticulatedJointPool.mfSwingAngleDegrees, 0.0f, 360.0f);
        SetRange(&mpTrafficManager->mArticulatedJointPool.mfMaxSwingVelocityDegrees, 0.0f, 360.0f);
        SetStep(&mpTrafficManager->mArticulatedJointPool.mfSwingAngleDegrees, 1.0f);
        SetStep(&mpTrafficManager->mArticulatedJointPool.mfMaxSwingVelocityDegrees, 1.0f);

        RegisterVariable(&mpTrafficManager->mArticulatedJointPool.mfTwistAngleDegrees,
                         "Max joint twist angle");
        RegisterVariable(&mpTrafficManager->mArticulatedJointPool.mfMaxTwistVelocityDegrees,
                         "Max joint twist velocity");
        SetRange(&mpTrafficManager->mArticulatedJointPool.mfTwistAngleDegrees, 0.0f, 360.0f);
        SetRange(&mpTrafficManager->mArticulatedJointPool.mfMaxTwistVelocityDegrees, 0.0f, 360.0f);
        SetStep(&mpTrafficManager->mArticulatedJointPool.mfTwistAngleDegrees, 1.0f);
        SetStep(&mpTrafficManager->mArticulatedJointPool.mfMaxTwistVelocityDegrees, 1.0f);

        RegisterVariable(&mpTrafficManager->mfJointSwingBreakVelocity,
                         "Joint swing break velocity");
        RegisterVariable(&mpTrafficManager->mfJointTwistBreakVelocity,
                         "Joint twist break velocity");
        RegisterVariable(&mpTrafficManager->mfJointLinearBreakMph,
                         "Joint linear break speed");
        SetStep(&mpTrafficManager->mfJointSwingBreakVelocity, 0.1f);
        SetStep(&mpTrafficManager->mfJointTwistBreakVelocity, 0.1f);
        SetStep(&mpTrafficManager->mfJointLinearBreakMph, 1.0f);

        RegisterVariable(&mbDrawTrafficAirRams, "Draw traffic air rams");
        RegisterVariable(&mfAirRamLengthScale, "Air ram rendering scale");
        SetStep(&mfAirRamLengthScale, 0.5f);
        RegisterVariable(&mbDrawDriverControls, "Draw driver controls");
        RegisterVariable(&gbAllowTrafficFreezing, "Allow traffic freezing");
    }

    // ========================================================================================
    // RenderWorld @0x82636628
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::RenderWorld(
        CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        CGS_ASSERT(lpDisplay != 0, "lpDisplay != NULL");

        if (mbDrawTrafficAirRams)
        {
            for (s32 liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetFirstNonZeroBit();
                 liTraffic >= 0;
                 liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetNextNonZeroBit(liTraffic))
            {
                PhysicalTrafficVehicle* const lpTrafficVehicle =
                    mpTrafficManager->GetTrafficVehicle(liTraffic);
                if (lpTrafficVehicle->IsSimple())
                    continue;

                TrafficPhysics* const lpTrafficPhysics = lpTrafficVehicle->GetFullTrafficPhysics();
                for (s32 liAirRam = lpTrafficPhysics->mUsedAirRams.GetFirstNonZeroBit();
                     liAirRam >= 0;
                     liAirRam = lpTrafficPhysics->mUsedAirRams.GetNextNonZeroBit(liAirRam))
                {
                    RenderAirRam(&lpTrafficPhysics->mAirRamEffect[liAirRam], lpTrafficPhysics,
                                 lpDisplay);
                }
            }
        }

        if (mbDrawDriverControls)
            RenderDriverControls(lpDisplay);
        if (mbDrawPhysicalTraffic)
            DrawPhysicalTraffic(lpDisplay);
        if (mbDrawSimpleTraffic)
            DrawSimpleTraffic(lpDisplay);
        if (mbDrawTrafficUsingBoxWorld)
            DrawTrafficUsingBoxWorldTests(lpDisplay);
        if (mbDrawCatchupTargets)
            DrawCatchupTargets(lpDisplay);
        if (mbDrawResetOnWaterHeight)
            DrawResetOnWaterHeight(lpDisplay);
    }

    // ========================================================================================
    // RenderAirRam @0x825EDAB8
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::RenderAirRam(
        const VehiclePhysics::AirRamEffect* lpAirRam, const VehiclePhysics* lpVehicle,
        CgsDev::Debug3DImmediateRender* lpDisplay) const
    {
        CGS_ASSERT(lpAirRam != 0, "lpAirRam"); // :562
        CGS_ASSERT(lpVehicle != 0, "lpVehicle"); // :563
        CGS_ASSERT(lpDisplay != 0, "lpDisplay"); // :564

        const Matrix44Affine lVehicleTransform = lpVehicle->GetTransform();
        Vector3 lImpulse = lpAirRam->mImpulse;
        if (lpAirRam->meImpulseSpace == rw::physics::BODY_SPACE)
        {
            lImpulse = ControlsScaleAdd(lVehicleTransform.zAxis, lImpulse.z,
                ControlsScaleAdd(lVehicleTransform.yAxis, lImpulse.y,
                    ControlsScale(lVehicleTransform.xAxis, lImpulse.x)));
        }
        else
        {
            CGS_ASSERT(lpAirRam->meImpulseSpace == rw::physics::WORLD_SPACE,
                       "lpAirRam->meImpulseSpace == rw::physics::WORLD_SPACE"); // :577
        }

        // 0x825EDBD4..0x825EDC40: vmsum3fp128, two rsqrt refinements,
        // then |impulse| = squaredLength * inverseLength, with vsel zero.
        const Vector3 lvFlushedImpulse{ControlsFlush(lImpulse.x), ControlsFlush(lImpulse.y),
                                       ControlsFlush(lImpulse.z), ControlsFlush(lImpulse.w)};
        const f32 lfLengthSquared = rw::math::vpu::SLerpDetail::Dot3(lvFlushedImpulse, lvFlushedImpulse);
        const f32 lfInverseLength = rw::math::vpu::SLerpDetail::RefinedReciprocalSqrt(lfLengthSquared, 2);
        const f32 lfMagnitude = lfLengthSquared == 0.0f ? 0.0f
            : ControlsFlush(lfLengthSquared * lfInverseLength);
        const f32 lfScale = mfAirRamLengthScale;
        if (lfMagnitude < ControlsFlush(0.01f / lfScale))
            return;

        // 0x825EDD60/80 negates the NORMALIZED impulse via sign-bit XOR.
        // The Normalize itself has no zero guard after the visibility gate.
        const Vector3 lvNormalizedImpulse = ControlsScale(lImpulse, lfInverseLength);
        const Vector3 lDirection{-lvNormalizedImpulse.x, -lvNormalizedImpulse.y,
                                  -lvNormalizedImpulse.z, -lvNormalizedImpulse.w};
        const Vector3 lStart = ControlsScaleAdd(lVehicleTransform.zAxis, lpAirRam->mPosition.z,
            ControlsScaleAdd(lVehicleTransform.yAxis, lpAirRam->mPosition.y,
                ControlsScaleAdd(lVehicleTransform.xAxis, lpAirRam->mPosition.x, lVehicleTransform.wAxis)));
        // 0x825EDD08..6C takes ANOTHER square root of |impulse| before
        // the scale multiply, and 0x825EDCC4/0x825EDD68 use eight degrees.
        const f32 lfMagnitudeRoot = lfMagnitude == 0.0f ? 0.0f
            : ControlsFlush(lfMagnitude * rw::math::vpu::SLerpDetail::RefinedReciprocalSqrt(lfMagnitude, 2));
        const f32 lfConeLength = ControlsFlush(ControlsFlush(lfScale) * lfMagnitudeRoot);
        const f32 lfConeTheta = 8.0f * 0.01745329238474369f; // flt_82004C88 * flt_8208F5F4
        const rw::RGBA lColour(0xB4, 0xB4, 0xFF, 0x96); // 0x96FFB4B4, renderer's packed low-red order
        const VecFloat lvfLength{lfConeLength, lfConeLength, lfConeLength, lfConeLength};
        const VecFloat lvfTheta{lfConeTheta, lfConeTheta, lfConeTheta, lfConeTheta};
        RenderCone(lStart, lDirection, lvfLength, lColour.m_rgba, lvfTheta, lpDisplay);
    }

    // ========================================================================================
    // RenderCone @0x825C9E90
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::RenderCone(
        Vector3 lvConeStart, Vector3 lvConeDirection, VecFloat lvfConeLength,
        u32 luColour, VecFloat lvfTheta,
        CgsDev::Debug3DImmediateRender* lpDisplay) const
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lvConeStart), "IsValid( lConeStart )"); // :618
        CGS_ASSERT(rw::math::vpu::IsValid(lvConeDirection), "IsValid( lConeDir )"); // :619
        // 0x825C9FD4/0x825CA008 compare the WHOLE VecFloat register with itself.
        CGS_ASSERT(lvfConeLength.x == lvfConeLength.x && lvfConeLength.y == lvfConeLength.y
                   && lvfConeLength.z == lvfConeLength.z && lvfConeLength.w == lvfConeLength.w,
                   "IsValid( lfConeLength )"); // :620
        CGS_ASSERT(lvfTheta.x == lvfTheta.x && lvfTheta.y == lvfTheta.y
                   && lvfTheta.z == lvfTheta.z && lvfTheta.w == lvfTheta.w,
                   "IsValid( lfTheta )"); // :621
        CGS_ASSERT(lpDisplay != 0, "lpDisplay"); // :622

        // The original tests the absolute direction dot with global Y against 0.9. It uses Y as
        // the first basis when the direction is not near vertical and X in the near-parallel arm.
        const f32 lfDotY = rw::math::vpu::SLerpDetail::Dot3(lvConeDirection, Vector3{0.0f, 1.0f, 0.0f, 0.0f});
        const bool lbUseY = 0.9f > std::fabs(lfDotY); // ordered compare: unordered takes the X arm
        Vector3 lBasisA, lBasisB;
        if (lbUseY)
        {
            lBasisB = NormalizeConeBasis(ConeCross(lvConeDirection, Vector3{0.0f, 1.0f, 0.0f, 0.0f}));
            lBasisA = NormalizeConeBasis(ConeCross(lBasisB, lvConeDirection));
        }
        else
        {
            lBasisA = NormalizeConeBasis(ConeCross(Vector3{1.0f, 0.0f, 0.0f, 0.0f}, lvConeDirection));
            lBasisB = NormalizeConeBasis(ConeCross(lvConeDirection, lBasisA));
        }

        const f32 lfRadius = ControlsFlush(lvfConeLength.x * XboxMath::XMVectorTan(lvfTheta.x));
        const Vector3 lBaseCentre = ControlsScaleAdd(lvConeDirection, lvfConeLength.x, lvConeStart);
        const Vector3 lBasisAOffset = ControlsScale(lBasisA, lfRadius);
        const Vector3 lBasisBOffset = ControlsScale(lBasisB, lfRadius);
        const Vector3 lCornerA = ControlsAdd(lBaseCentre, lBasisAOffset);
        const Vector3 lCornerB = ControlsAdd(lBaseCentre, lBasisBOffset);
        const Vector3 lCornerC = ControlsSubtract(lBaseCentre, lBasisAOffset);
        const Vector3 lCornerD = ControlsSubtract(lBaseCentre, lBasisBOffset);
        const rw::RGBA lPackedColour(static_cast<u8>(luColour),
                                     static_cast<u8>(luColour >> 8),
                                     static_cast<u8>(luColour >> 16),
                                     static_cast<u8>(luColour >> 24));

        lpDisplay->DrawTriangle(lvConeStart, lCornerB, lCornerA, lPackedColour);
        lpDisplay->DrawTriangle(lvConeStart, lCornerC, lCornerB, lPackedColour);
        lpDisplay->DrawTriangle(lvConeStart, lCornerD, lCornerC, lPackedColour);
        lpDisplay->DrawTriangle(lvConeStart, lCornerA, lCornerD, lPackedColour);
        lpDisplay->DrawQuad(lCornerA, lCornerB, lCornerC, lCornerD, lPackedColour);
    }

    // ========================================================================================
    // RenderDriverControls @0x825EDD98
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::RenderDriverControls(
        CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        CGS_ASSERT(mpTrafficManager->mpaTrafficDrivers != 0,
                   "mpTrafficManager->mpaTrafficDrivers"); // :668
        CGS_ASSERT(lpDisplay != 0, "lpDisplay != NULL"); // :696

        for (s32 liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetFirstNonZeroBit();
             liTraffic >= 0;
             liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetNextNonZeroBit(liTraffic))
        {
            CGS_ASSERT(liTraffic < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                       "liVehicle < ku8TotalMaxNumPhysicalTraffic"); // :741
            const PhysicalTrafficVehicle* const lpTrafficVehicle =
                mpTrafficManager->GetTrafficVehicle(liTraffic);
            const VehicleDriver* const lpDriver = mpTrafficManager->GetTrafficDriver(liTraffic);
            RenderControls(lpTrafficVehicle->mpVehicleBody->GetPosition(),
                           lpDriver->GetControls(), lpDisplay);
        }
    }

    // ========================================================================================
    // ResetTrafficAboveWater @0x825EE4A0
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::ResetTrafficAboveWater(void* lpUserData)
    {
        PhysicalTrafficManagerDebugComponent* const lpComponent =
            static_cast<PhysicalTrafficManagerDebugComponent*>(lpUserData);
        PhysicalTrafficManager* const lpTrafficManager = lpComponent->mpTrafficManager;
        const s32 liTraffic = lpTrafficManager->mUsedTrafficVehicles.GetFirstNonZeroBit();
        if (liTraffic < 0)
            return;

        SimpleVehiclePhysics* const lpTrafficPhysics =
            lpTrafficManager->GetTrafficVehicle(liTraffic)->mpVehicleBody;
        Matrix44Affine lGraphicsTransform = lpTrafficPhysics->GetGraphicsVehicleTransform();
        {
            const Vector3 lAggregateValue = { 3355.0f, 6.0f, -1378.8f, 0.0f };
            lGraphicsTransform.wAxis = lAggregateValue;
        }
        lpTrafficPhysics->SetGraphicsVehicleTransform(lGraphicsTransform);
    }

    // ========================================================================================
    // DrawTrafficBox @0x8261C998
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::DrawTrafficBox(
        const PhysicalTrafficVehicle* lpTrafficVehicle,
        CgsDev::Debug3DImmediateRender* lpDisplay, bool lbDrawWheels)
    {
        CGS_ASSERT(lpTrafficVehicle != 0, "lpTrafficVehicle != NULL");
        CGS_ASSERT(lpDisplay != 0, "lpDisplay != NULL");

        const SimpleVehiclePhysics* const lpTrafficPhysics = lpTrafficVehicle->mpVehicleBody;
        CgsGeometric::Box lSimpleTrafficBox;
        lpTrafficPhysics->GetSimpleVehicleBox(lSimpleTrafficBox);
        const Vector3 lvBoxHalfExtents = lSimpleTrafficBox.GetDimensions();
        const Vector3 lvBoxInf{ -lvBoxHalfExtents.x, -lvBoxHalfExtents.y,
                                -lvBoxHalfExtents.z, 0.0f };
        const Vector3 lvBoxSup{ lvBoxHalfExtents.x, lvBoxHalfExtents.y,
                                lvBoxHalfExtents.z, 0.0f };
        lpDisplay->DrawBox(lvBoxInf, lvBoxSup, lSimpleTrafficBox.GetTransform(),
                           rw::RGBA(255, 255, 255, 255));

        const Matrix44Affine lGraphicsTransform = lpTrafficPhysics->GetGraphicsVehicleTransform();
        const f32 lfHalfLength = lpTrafficPhysics->GetHalfExtent().z;
        const Vector3 lvArrowStart = ControlsScaleAdd(lGraphicsTransform.zAxis, lfHalfLength, lGraphicsTransform.wAxis);
        const Vector3 lvArrowEnd = ControlsScaleAdd(lGraphicsTransform.zAxis, lfHalfLength + 1.5f, lGraphicsTransform.wAxis);
        lpDisplay->DrawArrow(lvArrowStart, lvArrowEnd, rw::RGBA(255, 0, 0, 255));

        if (!lbDrawWheels)
            return;

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            const Wheel* const lpWheel =
                lpTrafficPhysics->GetWheel(static_cast<EVehicleDrivenWheel>(liWheel));
            const f32 lfWheelRadius = lpWheel->mSlipVariables.w;
            const Vector3 lvWheelHalfExtents{ 0.15f, lfWheelRadius, lfWheelRadius, 0.0f };
            const Vector3 lvWheelInf{ -lvWheelHalfExtents.x, -lvWheelHalfExtents.y,
                                      -lvWheelHalfExtents.z, 0.0f };
            const Vector3 lvWheelSup{ lvWheelHalfExtents.x, lvWheelHalfExtents.y,
                                      lvWheelHalfExtents.z, 0.0f };
            const Matrix44Affine lWheelTransform = lpTrafficPhysics->GetWheelsWorldTransfrom(
                static_cast<EVehicleDrivenWheel>(liWheel), false);
            lpDisplay->DrawBox(lvWheelInf, lvWheelSup, lWheelTransform,
                               rw::RGBA(0, 0, 255, 255));
        }
    }

    // ========================================================================================
    // DrawSimpleTraffic @0x8262C3E8
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::DrawSimpleTraffic(
        CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        for (s32 liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetFirstNonZeroBit();
             liTraffic >= 0;
             liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetNextNonZeroBit(liTraffic))
        {
            const PhysicalTrafficVehicle* const lpTrafficVehicle =
                mpTrafficManager->GetTrafficVehicle(liTraffic);
            if (lpTrafficVehicle->IsSimple())
                DrawTrafficBox(lpTrafficVehicle, lpDisplay, true);
        }
    }

    // ========================================================================================
    // DrawTrafficUsingBoxWorldTests @0x8262C730
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::DrawTrafficUsingBoxWorldTests(
        CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        for (s32 liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetFirstNonZeroBit();
             liTraffic >= 0;
             liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetNextNonZeroBit(liTraffic))
        {
            const PhysicalTrafficVehicle* const lpTrafficVehicle =
                mpTrafficManager->GetTrafficVehicle(liTraffic);
            if (lpTrafficVehicle->mbUsingBoxWithWorld)
                DrawTrafficBox(lpTrafficVehicle, lpDisplay, false);
        }
    }

    // ========================================================================================
    // DrawPhysicalTraffic @0x825ED508 (named ARTIST export hole)
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::DrawPhysicalTraffic(
        CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        for (s32 liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetFirstNonZeroBit();
             liTraffic >= 0;
             liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetNextNonZeroBit(liTraffic))
        {
            const PhysicalTrafficVehicle* const lpTrafficVehicle =
                mpTrafficManager->GetTrafficVehicle(liTraffic);
            const SimpleVehiclePhysics* const lpTrafficPhysics = lpTrafficVehicle->mpVehicleBody;
            CgsGeometric::Box lTrafficBox;
            lpTrafficPhysics->GetSimpleVehicleBox(lTrafficBox);
            const Vector3 lvHalfExtents = lTrafficBox.GetDimensions();
            const Vector3 lvBoxInf{ -lvHalfExtents.x, -lvHalfExtents.y,
                                    -lvHalfExtents.z, 0.0f };
            const Vector3 lvBoxSup{ lvHalfExtents.x, lvHalfExtents.y,
                                    lvHalfExtents.z, 0.0f };
            const rw::RGBA lCarColour = lpTrafficPhysics->IsFrozen()
                ? rw::RGBA(0, 0, 255, 255) : rw::RGBA(255, 255, 255, 255);
            lpDisplay->DrawBox(lvBoxInf, lvBoxSup, lTrafficBox.GetTransform(), lCarColour);

            const Matrix44Affine lGraphicsTransform = lpTrafficPhysics->GetGraphicsVehicleTransform();
            const f32 lfHalfLength = lpTrafficPhysics->GetHalfExtent().z;
            const Vector3 lvArrowStart = ControlsScaleAdd(lGraphicsTransform.zAxis, lfHalfLength, lGraphicsTransform.wAxis);
            const Vector3 lvArrowEnd = ControlsScaleAdd(lGraphicsTransform.zAxis, lfHalfLength + 1.5f, lGraphicsTransform.wAxis);
            lpDisplay->DrawArrow(lvArrowStart, lvArrowEnd, rw::RGBA(255, 0, 0, 255));

            for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            {
                const Wheel* const lpWheel =
                    lpTrafficPhysics->GetWheel(static_cast<EVehicleDrivenWheel>(liWheel));
                const f32 lfWheelRadius = lpWheel->mSlipVariables.w;
                const Vector3 lvWheelHalfExtents{ 0.15f, lfWheelRadius, lfWheelRadius, 0.0f };
                const Vector3 lvWheelInf{ -lvWheelHalfExtents.x, -lvWheelHalfExtents.y,
                                          -lvWheelHalfExtents.z, 0.0f };
                const Vector3 lvWheelSup{ lvWheelHalfExtents.x, lvWheelHalfExtents.y,
                                          lvWheelHalfExtents.z, 0.0f };
                const Matrix44Affine lWheelTransform = lpTrafficPhysics->GetWheelsWorldTransfrom(
                    static_cast<EVehicleDrivenWheel>(liWheel), false);
                lpDisplay->DrawBox(lvWheelInf, lvWheelSup, lWheelTransform,
                                   rw::RGBA(0, 0, 255, 255));
            }

            if (mbDrawJoints
                && lpTrafficVehicle->GetArticulatedVehicleType()
                    == PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_CAB
                && lpTrafficVehicle->HasNonBrokenJoint())
            {
                const ArticulatedJoint* const lpJoint =
                    mpTrafficManager->mArticulatedJointPool.GetJoint(lpTrafficVehicle->miJointIndex);
                const Matrix44Affine lJointTransform = rw::math::vpu::Mult(
                    lpJoint->GetParentToJointTransform(), lpTrafficPhysics->GetTransform());
                lpDisplay->DrawAxis(lJointTransform);
            }
        }
    }

    // ========================================================================================
    // DrawCatchupTargets @0x825ED0C0
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::DrawCatchupTargets(
        CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        CGS_ASSERT(lpDisplay != 0, "lpDisplay != NULL");
        for (s32 liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetFirstNonZeroBit();
             liTraffic >= 0;
             liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetNextNonZeroBit(liTraffic))
        {
            const VehicleDriver* const lpVehicleDriver = mpTrafficManager->GetTrafficDriver(liTraffic);
            if (lpVehicleDriver->GetNumInterpSteps() <= 0)
                continue;

            const SimpleVehiclePhysics* const lpTrafficPhysics =
                mpTrafficManager->GetTrafficVehicle(liTraffic)->mpVehicleBody;
            Matrix44Affine lTargetTransform = lpVehicleDriver->GetCatchupTargetTransform();
            // 0x825ED2C0..2F0 subtracts target-basis COM offset before BOTH
            // the target box and arrow endpoint; its accumulation is y/x/z.
            const Vector3 lvCOMOffset = lpTrafficPhysics->GetSimpleAttribs()->mCOMOffset;
            const Vector3 lvTargetCOMOffset = ControlsScaleAdd(lTargetTransform.zAxis, lvCOMOffset.z,
                ControlsScaleAdd(lTargetTransform.xAxis, lvCOMOffset.x,
                    ControlsScale(lTargetTransform.yAxis, lvCOMOffset.y)));
            lTargetTransform.wAxis = ControlsSubtract(lTargetTransform.wAxis, lvTargetCOMOffset);
            const Vector3 lvHalfExtents = lpTrafficPhysics->GetHalfExtent();
            const Vector3 lvBoxInf{ -lvHalfExtents.x, -lvHalfExtents.y,
                                    -lvHalfExtents.z, 0.0f };
            const Vector3 lvBoxSup{ lvHalfExtents.x, lvHalfExtents.y,
                                    lvHalfExtents.z, 0.0f };
            lpDisplay->DrawBox(lvBoxInf, lvBoxSup, lTargetTransform,
                               rw::RGBA(255, 255, 255, 255));
            const Vector3 lvCurrentPosition = lpTrafficPhysics->GetGraphicsVehicleTransform().wAxis;
            lpDisplay->DrawArrow(lvCurrentPosition, lTargetTransform.wAxis,
                                 rw::RGBA(255, 255, 255, 255));
        }
    }

    // ========================================================================================
    // DrawResetOnWaterHeight @0x825EE0F8
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::DrawResetOnWaterHeight(
        CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        for (s32 liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetFirstNonZeroBit();
             liTraffic >= 0;
             liTraffic = mpTrafficManager->mUsedTrafficVehicles.GetNextNonZeroBit(liTraffic))
        {
            const PhysicalTrafficVehicle* const lpTrafficVehicle =
                mpTrafficManager->GetTrafficVehicle(liTraffic);
            const SimpleVehiclePhysics* const lpTrafficPhysics = lpTrafficVehicle->mpVehicleBody;
            const Vector3 lLineStart = lpTrafficPhysics->GetGraphicsVehicleTransform().wAxis;
            Vector3 lLineEnd = lLineStart;
            lLineEnd.y = mpTrafficManager->mavfLowestPointWorldSpace[liTraffic].y;
            lpDisplay->DrawArrow(lLineStart, lLineEnd, rw::RGBA(255, 255, 255, 255));
            lpDisplay->DrawSphere(lLineEnd, 0.1f, rw::RGBA(255, 255, 255, 255));
        }
    }
}
}
