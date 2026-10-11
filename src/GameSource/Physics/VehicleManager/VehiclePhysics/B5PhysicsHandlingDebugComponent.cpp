#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"   // RaceCarPhysics / Wheel::RoadContact
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "rw/math/vpu/vector3_operation.h"

#include <cstddef>   // offsetof
#include <cmath>

// BrnPhysics::Vehicle::DebugComponent -- the component's data side: the per-car two-phase
// Construct, the gear-stat reset, the two debug-menu name getters, the last-wall-triangle setter
// and the per-frame sampling tick. The render/HUD side is owned by its own dev-UI pass.
//
// Every body here reaches its state through NAMED MEMBERS. The 1024-byte opaque slot the
// component used to be addressed through -- and the SlotField<T> offset seam that went with it --
// are gone: VehicleManager embeds `DebugComponent maRaceCarDebugComponent[8]` by value, so the
// elements are constructed, carry a live vptr, and index at the host stride.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace
    {
        rw::RGBA HandlingColour(u32 luPackedColour)
        {
            rw::RGBA lColour;
            lColour.m_rgba = luPackedColour;
            return lColour;
        }

        f32 HandlingFlush(f32 lfValue)
        {
            return std::fpclassify(lfValue) == FP_SUBNORMAL
                ? std::copysign(0.0f, lfValue) : lfValue;
        }

        f32 HandlingMultiply(f32 lfA, f32 lfB)
        {
            return HandlingFlush(HandlingFlush(lfA) * HandlingFlush(lfB));
        }

        f32 HandlingAdd(f32 lfA, f32 lfB)
        {
            return HandlingFlush(HandlingFlush(lfA) + HandlingFlush(lfB));
        }

        f32 HandlingMultiplyAdd(f32 lfA, f32 lfC, f32 lfB)
        {
            return HandlingFlush(std::fmaf(HandlingFlush(lfA), HandlingFlush(lfC), HandlingFlush(lfB)));
        }

        f32 HandlingNegativeMultiplySubtract(f32 lfA, f32 lfC, f32 lfB)
        {
            const f32 lfDifference = HandlingMultiplyAdd(lfA, lfC, -HandlingFlush(lfB));
            return lfDifference != lfDifference ? lfDifference : -lfDifference;
        }

        f32 HandlingReciprocal(f32 lfValue)
        {
            lfValue = HandlingFlush(lfValue);
            // FLAG (estimate model): rounded 1/x models the vrefp seed. Both
            // original Newton refinements remain unconditional.
            f32 lfEstimate = HandlingFlush(static_cast<f32>(1.0 / static_cast<f64>(lfValue)));
            for (s32 liStep = 0; liStep < 2; ++liStep)
            {
                const f32 lfResidual = HandlingNegativeMultiplySubtract(lfEstimate, lfValue, 1.0f);
                lfEstimate = HandlingMultiplyAdd(lfEstimate, lfResidual, lfEstimate);
            }
            return lfEstimate;
        }

        Vector3 HandlingScale(Vector3 lValue, f32 lfScale)
        {
            return Vector3{HandlingMultiply(lValue.x, lfScale), HandlingMultiply(lValue.y, lfScale),
                           HandlingMultiply(lValue.z, lfScale), HandlingMultiply(lValue.w, lfScale)};
        }

        Vector3 HandlingAdd(Vector3 lA, Vector3 lB)
        {
            return Vector3{HandlingAdd(lA.x, lB.x), HandlingAdd(lA.y, lB.y),
                           HandlingAdd(lA.z, lB.z), HandlingAdd(lA.w, lB.w)};
        }

        Vector3 HandlingMultiplyAdd(Vector3 lA, f32 lfC, Vector3 lB)
        {
            return Vector3{HandlingMultiplyAdd(lA.x, lfC, lB.x), HandlingMultiplyAdd(lA.y, lfC, lB.y),
                           HandlingMultiplyAdd(lA.z, lfC, lB.z), HandlingMultiplyAdd(lA.w, lfC, lB.w)};
        }

        // Scalar fsel lowering used by the original label/current-value clamps.
        // An unordered range subtraction chooses the range, as ARTIST does.
        f32 ClampGripLabel(f32 lfValue, f32 lfRange)
        {
            const f32 lfNonnegative = -lfValue >= 0.0f ? 0.0f : lfValue;
            return lfRange - lfNonnegative >= 0.0f ? lfNonnegative : lfRange;
        }
    }

    // ARTIST unnamed 825DA4A8, attested by the handling vtable at 820CF988.
    // These mirrors are refreshed while the component is active; the inertia
    // menu values are not applied back to the car by this update.
    void DebugComponent::Update()
    {
        mafInertia[0] = mpRaceCarPhysics->mLocalInverseInertia.xAxis.x * 100.0f;
        mafInertia[1] = mpRaceCarPhysics->mLocalInverseInertia.yAxis.y * 100.0f;
        mafInertia[2] = mpRaceCarPhysics->mLocalInverseInertia.zAxis.z * 100.0f;
        // ARTIST loads the same COM-offset X lane three times at 825DA59C,
        // 825DA5A8 and 825DA5B4; retain that original aliasing.
        for (s32 liAxis = 0; liAxis < 3; ++liAxis)
            mafHackComOffset[liAxis] = mpRaceCarPhysics->mpAttribs->mBaseAttribs.mCOMOffset.x;
        const Engine& lrEngine = mpRaceCarPhysics->mEngine;
        const Vector4& lrDrive = lrEngine.mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay;
        const Vector4& lrClutch = lrEngine.mvClutchFactor_RPM_CurrentGearChangeTime;
        mfRPM = lrClutch.y;
        mfEngineDrive = lrDrive.x;
        mfReactionTorque = lrDrive.y;
        mfFlyWheelAngularVelocity = lrDrive.z;
        mfClutchDelay = lrDrive.w;
        mfClutchFactor = lrClutch.x;
        if (mbRenderFrontWheelsGripCurves)
            mFrontWheelsGripCurveWindow.Show();
        else
            mFrontWheelsGripCurveWindow.Hide();
        if (mbRenderRearWheelsGripCurves)
            mRearWheelsGripCurveWindow.Show();
        else
            mRearWheelsGripCurveWindow.Hide();
    }

    // ARTIST 825D9B38: all four suspension lines are drawn, then ground
    // contacts are marked only for wheels whose copied on-ground byte is set.
    void DebugComponent::DrawTractionLines(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            Vector3 lStart{};
            Vector3 lEnd{};
            mpRaceCarPhysics->GetTractionLine(static_cast<EVehicleDrivenWheel>(liWheel), lStart, lEnd);
            lpDisplay->DrawArrow(lStart, lEnd, HandlingColour(0xFFFFFFFFu));
            if (maRoadContacts[liWheel].mbIsOnGround)
                lpDisplay->DrawPoint(maRoadContacts[liWheel].mPosition, HandlingColour(0xFF0000FFu));
        }
    }

    // ARTIST 825B9110: the cached road normal is drawn with unit length.
    void DebugComponent::DrawTractionLineNormals(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            const Wheel::RoadContact& lrContact = maRoadContacts[liWheel];
            if (lrContact.mbIsOnGround)
                lpDisplay->DrawArrow(lrContact.mPosition, lrContact.mPosition + lrContact.mNormal,
                                      HandlingColour(0xFF0000FFu));
        }
    }

    // ARTIST 826031A0: the ten-unit test ray only changes the Y lane.
    void DebugComponent::DrawAboveGroundLineTest(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        const Vector3 lStart = mpRaceCarPhysics->GetPosition();
        Vector3 lEnd = lStart;
        lEnd.y -= 10.0f;
        lpDisplay->DrawArrow(lStart, lEnd, HandlingColour(0xFFFFFFFFu));
        const AboveGroundTestResult& lrResult = *mpRaceCarPhysics->GetAboveGroundTestResult();
        if (lrResult.mbValid)
        {
            lpDisplay->DrawPoint(lrResult.mIntersectionPosition, HandlingColour(0xFF0000FFu));
            lpDisplay->DrawArrow(lrResult.mIntersectionPosition,
                lrResult.mIntersectionPosition + lrResult.mIntersectionNormal, HandlingColour(0xFF0000FFu));
        }
    }

    // ARTIST 825B9170 reads the actual showtime deformation accumulators.
    void DebugComponent::RenderDeformationStats(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        char lacText[128];
        CgsCore::SPrintf(lacText, 128, "Average deformation: %f (limit: %f)",
                        static_cast<double>(msPlayerParams.mfDeformationScale),
                        static_cast<double>(msPlayerParams.mfDamageBudget));
        lpDisplay->DrawText(lacText, 100.0f, 100.0f, 20.0f, 0xFFFFFFFFu);
    }

    // ARTIST 825B92A8: the stored triangle is offset along its normal even
    // when it has not received a collision yet; there is no validity gate.
    void DebugComponent::DrawLastWallTriangle(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        CGS_ASSERT(lpDisplay != nullptr, "lpDisplay != NULL");
        static const f32 K_OFFSET = 0.04f; // splat at 82FB9FA0
        const Vector3 lOffset = HandlingScale(mLastWallTriangle.mNormal, K_OFFSET);
        const Vector3 lVertex0 = HandlingAdd(mLastWallTriangle.mVertex0, lOffset);
        const Vector3 lVertex1 = HandlingAdd(mLastWallTriangle.mVertex1, lOffset);
        const Vector3 lVertex2 = HandlingAdd(mLastWallTriangle.mVertex2, lOffset);
        // 825B9370 loads one byte (0x40), then rotates it into alpha.
        lpDisplay->DrawSolidTriangle(lVertex0, lVertex1, lVertex2, HandlingColour(0x400000FFu));
        lpDisplay->DrawHollowTriangle(lVertex0, lVertex1, lVertex2, HandlingColour(0xFFFFFFFFu));
        const f32 lfRadius = 0.07f; // flt_82F2A4E8
        lpDisplay->DrawHollowSphere(lVertex0, lfRadius, HandlingColour(0xFFFFFFFFu));
        lpDisplay->DrawHollowSphere(lVertex1, lfRadius, HandlingColour(0xFFFFFFFFu));
        lpDisplay->DrawHollowSphere(lVertex2, lfRadius, HandlingColour(0xFFFFFFFFu));
    }

    // ARTIST 825DA6A8: only the producer's applied flag gates each wheel;
    // retain the two reciprocal refinements and the fused endpoint offset.
    void DebugComponent::DrawSuspensionForces(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        CGS_ASSERT(lpDisplay != nullptr, "lpDisplay != NULL");
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            if (mabSuspensionForceApplied[liWheel])
            {
                const Vector3 lStart = mpRaceCarPhysics->GetWheelsWorldTransfrom(
                    static_cast<EVehicleDrivenWheel>(liWheel), false).wAxis;
                const f32 lfScale = HandlingReciprocal(2000.0f); // flt_82F2A504
                const Vector3 lEnd = HandlingMultiplyAdd(maSuspensionForces[liWheel], lfScale, lStart);
                lpDisplay->DrawArrow(lStart, lEnd, HandlingColour(0xFFFFFFFFu));
                lpDisplay->DrawHollowSphere(lStart, mpRaceCarPhysics->maWheels[liWheel].mSlipVariables.w,
                                            HandlingColour(0xFFFFFFFFu));
            }
        }
    }

    // ARTIST 82621208: a piecewise-linear curve through zero, peak and floor,
    // followed by the floor plateau. The axes and every tick remain separate.
    void GripCurveDebugGraph::Render(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        const CgsDev::RGBA lAxisColour = 0xFF000000u;
        const CgsDev::RGBA lCurveColour = 0xFFFF0000u; // dword_82FBA2A4
        const auto lOrigin = GetPointOnGraph(0.0f, 0.0f);
        lpDisplay->DrawLine(GetPointOnGraph(0.0f, mAxisRanges.y), lOrigin, lAxisColour);
        lpDisplay->DrawLine(lOrigin, GetPointOnGraph(mAxisRanges.x, 0.0f), lAxisColour);
        const Vector4& lrCurve = mpGripCurve->maGripVariables;
        const auto lPeak = GetPointOnGraph(lrCurve.x, lrCurve.z);
        const auto lFloor = GetPointOnGraph(lrCurve.y, lrCurve.w);
        lpDisplay->DrawLine(lOrigin, lPeak, lCurveColour);
        lpDisplay->DrawLine(lPeak, lFloor, lCurveColour);
        lpDisplay->DrawLine(lFloor, GetPointOnGraph(mAxisRanges.x, lrCurve.w), lCurveColour);
        PlotSlipRatioLabel(lpDisplay, mAxisRanges.x, lAxisColour);
        PlotCoefficientLabel(lpDisplay, mAxisRanges.y, lAxisColour);
        PlotSlipRatioLabel(lpDisplay, lrCurve.x, lAxisColour);
        PlotSlipRatioLabel(lpDisplay, lrCurve.y, lAxisColour);
        PlotCoefficientLabel(lpDisplay, lrCurve.z, lAxisColour);
        PlotCoefficientLabel(lpDisplay, lrCurve.w, lAxisColour);
    }

    // ARTIST 82603B28 (raw-image asm; export hole), DWARF h:185.
    void GripCurveDebugGraph::PlotCoefficientLabel(CgsDev::Debug2DImmediateRender* lpDisplay,
                                                   f32 lfCoefficient, CgsDev::RGBA lColour)
    {
        lfCoefficient = ClampGripLabel(lfCoefficient, mAxisRanges.y);
        char lacText[32];
        CgsCore::SPrintf(lacText, 16, "%4.1f", static_cast<double>(lfCoefficient));
        const auto lPoint = GetPointOnGraph(0.0f, lfCoefficient);
        auto lStart = lPoint;
        auto lEnd = lPoint;
        lStart.x -= 3.0f;
        lEnd.x += 3.0f;
        lpDisplay->DrawLine(lStart, lEnd, 0xFF000000u);
        lpDisplay->DrawText(lacText, lPoint.x - 36.0f, lPoint.y - 9.0f, 18.0f, lColour);
    }

    // ARTIST 82603CA8, DWARF h:192. Tick colour is always the axis colour;
    // only the text receives the caller's wheel/curve colour.
    void GripCurveDebugGraph::PlotSlipRatioLabel(CgsDev::Debug2DImmediateRender* lpDisplay,
                                                 f32 lfSlipRatio, CgsDev::RGBA lColour)
    {
        lfSlipRatio = ClampGripLabel(lfSlipRatio, mAxisRanges.x);
        char lacText[32];
        CgsCore::SPrintf(lacText, 16, "%4.1f", static_cast<double>(lfSlipRatio));
        const auto lPoint = GetPointOnGraph(lfSlipRatio, 0.0f);
        auto lStart = lPoint;
        auto lEnd = lPoint;
        lStart.y -= 3.0f;
        lEnd.y += 3.0f;
        lpDisplay->DrawLine(lStart, lEnd, 0xFF000000u);
        lpDisplay->DrawText(lacText, lPoint.x - 18.0f, lPoint.y + 9.0f, 18.0f, lColour);
    }

    // ARTIST 82603E20: evaluate the actual curve at the clamped absolute
    // slip, then draw its sample circle and both coordinate labels.
    void GripCurveDebugGraph::PlotCurrentValue(CgsDev::Debug2DImmediateRender* lpDisplay,
                                              f32 lfSlipRatio, CgsDev::RGBA lColour)
    {
        lfSlipRatio = ClampGripLabel(std::fabs(lfSlipRatio), mAxisRanges.x);
        const f32 lfCoefficient = mpGripCurve->GetCoefficient(rw::math::vpu::Splat(lfSlipRatio)).x;
        lpDisplay->DrawCircle(GetPointOnGraph(lfSlipRatio, lfCoefficient), 5.0f, 8, lColour);
        PlotCoefficientLabel(lpDisplay, lfCoefficient, lColour);
        PlotSlipRatioLabel(lpDisplay, lfSlipRatio, lColour);
    }

    // ---- layout gate ---------------------------------------------------------------------------
    //
    // The recovered class accounts for all 1024 bytes of the console object with nothing left over.
    // Its host image is bigger, because six pointer-shaped things widen 4 -> 8 (the base's vptr and
    // list link, this class's car pointer, each window's vptr and its two wheel pointers) and two
    // of those widenings force a 16-byte re-alignment behind them. The chain below names each step
    // where it happens and adds it to a running term, so every member's console offset is pinned by
    // a host offsetof the compiler checks. Get any member's type, size or order wrong and one of
    // these fires.
    void DebugComponent::_AssertLayout()
    {
        typedef DebugComponent D;

        // Step 1: the base. Console 12 bytes (vptr + the active flag + the list link) padded to
        // 0x10 by the road-contact array's 16-byte alignment; host 24, padded to 0x20.
        const std::ptrdiff_t KD_AFTER_BASE = 0x10;
        static_assert(offsetof(D, maRoadContacts) == 0x010 + KD_AFTER_BASE, "maRoadContacts");

        // Step 2: nothing widens across the road-contact mirror -- RoadContact is 0x30 on both
        // sides (two 16-byte vectors, a float, a collision tag and four flag bytes).
        static_assert(sizeof(Wheel::RoadContact) == 0x30, "RoadContact is width-identical");
        static_assert(offsetof(D, mFrontWheelsGripCurveWindow) == 0x0D0 + KD_AFTER_BASE,
                      "mFrontWheelsGripCurveWindow");

        // Step 3: one grip-curve window apiece. Each is 0xC0 on the console and 0xD0 on the host:
        // its two wheel pointers widen (+8) and the 16-byte alignment its graphs impose rounds the
        // tail up (+8). The window's own base is width-identical, so the graphs stay at +0x30/+0x70
        // inside it.
        const std::ptrdiff_t KD_WINDOW_STEP = 0x10;
        static_assert(sizeof(GripCurveDebugWindow) == 0x0C0 + KD_WINDOW_STEP,
                      "GripCurveDebugWindow host size -- the number KD_WINDOW_STEP carries");
        const std::ptrdiff_t KD_AFTER_WINDOWS = KD_AFTER_BASE + 2 * KD_WINDOW_STEP;
        static_assert(offsetof(D, mRearWheelsGripCurveWindow) == 0x190 + KD_AFTER_BASE + KD_WINDOW_STEP,
                      "mRearWheelsGripCurveWindow");
        static_assert(offsetof(D, mpRaceCarPhysics) == 0x250 + KD_AFTER_WINDOWS, "mpRaceCarPhysics");

        // Step 4: the car pointer itself widens 4 -> 8. Everything from the inertia triple to the
        // clutch factor is float- or int-shaped and moves as one block behind it.
        const std::ptrdiff_t KD_AFTER_CAR_POINTER = KD_AFTER_WINDOWS + 4;
        static_assert(offsetof(D, mafInertia)     == 0x254 + KD_AFTER_CAR_POINTER, "mafInertia");
        static_assert(offsetof(D, miCurrentGear)  == 0x260 + KD_AFTER_CAR_POINTER, "miCurrentGear");
        static_assert(offsetof(D, mfLastFrameRpm) == 0x264 + KD_AFTER_CAR_POINTER, "mfLastFrameRpm");

        static_assert(sizeof(GearStats) == 28, "GearStats is width-identical (six floats + a flag)");
        static_assert(offsetof(D, mGearStats) == 0x268 + KD_AFTER_CAR_POINTER, "mGearStats");
        static_assert(offsetof(D, mfSpeedMPH) == 0x310 + KD_AFTER_CAR_POINTER,
                      "mfSpeedMPH -- and therefore that the six gear rows close on it");

        static_assert(offsetof(D, mafHackComOffset)  == 0x314 + KD_AFTER_CAR_POINTER, "mafHackComOffset");
        static_assert(offsetof(D, mfHackCrashExtraPitchVelocity)  == 0x320 + KD_AFTER_CAR_POINTER, "mfHackCrashExtraPitchVelocity");
        static_assert(offsetof(D, mfHackCrashExtraYawVelocity)    == 0x324 + KD_AFTER_CAR_POINTER, "mfHackCrashExtraYawVelocity");
        static_assert(offsetof(D, mfHackCrashExtraRollVelocity)   == 0x328 + KD_AFTER_CAR_POINTER, "mfHackCrashExtraRollVelocity");
        static_assert(offsetof(D, mfHackCrashExtraLinearVelocity) == 0x32C + KD_AFTER_CAR_POINTER, "mfHackCrashExtraLinearVelocity");
        static_assert(offsetof(D, mfRPM)                      == 0x330 + KD_AFTER_CAR_POINTER, "mfRPM");
        static_assert(offsetof(D, mfEngineDrive)              == 0x334 + KD_AFTER_CAR_POINTER, "mfEngineDrive");
        static_assert(offsetof(D, mfReactionTorque)           == 0x338 + KD_AFTER_CAR_POINTER, "mfReactionTorque");
        static_assert(offsetof(D, mfFlyWheelAngularVelocity)  == 0x33C + KD_AFTER_CAR_POINTER, "mfFlyWheelAngularVelocity");
        static_assert(offsetof(D, mfClutchDelay)              == 0x340 + KD_AFTER_CAR_POINTER, "mfClutchDelay");
        static_assert(offsetof(D, mfClutchFactor)             == 0x344 + KD_AFTER_CAR_POINTER, "mfClutchFactor");

        // Step 5: the wall triangle is 16-aligned, and the block in front of it ends four bytes
        // short of a 16-byte boundary on the console. So the +4 the car pointer cost is ABSORBED by
        // padding the console already had, and the term drops back to what it was before the
        // pointer -- the one place in this class where a widening costs nothing. From here to the
        // end of the object it stays constant.
        const std::ptrdiff_t KD_AFTER_TRIANGLE_ALIGN = KD_AFTER_WINDOWS;
        static_assert(sizeof(CgsGeometric::Triangle4::AOSTriangle) == 0x50,
                      "AOSTriangle is width-identical (four vectors + three floats, 16-aligned)");
        static_assert(offsetof(D, mLastWallTriangle) == 0x350 + KD_AFTER_TRIANGLE_ALIGN, "mLastWallTriangle");

        // The fourteen toggles are one contiguous run; asserting both ends pins all of them,
        // because a bool array has no padding to hide a missing or extra member in.
        static_assert(offsetof(D, mbRenderCOMPosition) == 0x3A0 + KD_AFTER_TRIANGLE_ALIGN, "mbRenderCOMPosition");
        static_assert(offsetof(D, mbStoredPhysics)     == 0x3AD + KD_AFTER_TRIANGLE_ALIGN, "mbStoredPhysics");

        static_assert(offsetof(D, maSuspensionForces)        == 0x3B0 + KD_AFTER_TRIANGLE_ALIGN, "maSuspensionForces");
        static_assert(offsetof(D, mabSuspensionForceApplied) == 0x3F0 + KD_AFTER_TRIANGLE_ALIGN, "mabSuspensionForceApplied");
        static_assert(offsetof(D, mfDownForceY)              == 0x3F4 + KD_AFTER_TRIANGLE_ALIGN, "mfDownForceY");

        // And the closure: the console object ends at 1024 with eight bytes of tail pad behind the
        // down-force mirror, so the host object is 1024 + the accumulated term, rounded to 16.
        static_assert(sizeof(D) == 1024 + KD_AFTER_TRIANGLE_ALIGN,
                      "the recovered class closes on the console's 1024-byte object -- this is the "
                      "number VehicleManager's drift term is derived from");
    }

    // BrnPhysics::Vehicle::GearStats::Reset
    //   One gear row back to open and empty: six floats and the row's closed flag.
    void GearStats::Reset()
    {
        mfTimeInGear                 = 0.0f;
        mfSpeedOutOfGearGoingUp      = 0.0f;
        mfSpeedOutOfGearGoingDown    = 0.0f;
        mfRpmWhenChangedOutGoingUp   = 0.0f;
        mfRpmWhenChangedOutGoingDown = 0.0f;
        mfRpmWhenChangedInGoingUp    = 0.0f;
        mbDoneUpdating               = false;
    }

    // BrnPhysics::Vehicle::DebugComponent::ResetGearStats
    //   Clear all six gear rows. The debug menu registers this as an action callback with the
    //   component as its user data, which is why it is static and takes a void*: Update accumulates
    //   the timestep into the row miCurrentGear names until the gear-stat report closes it, and
    //   this puts every row back to open and empty.
    void DebugComponent::ResetGearStats(void* lpComponent)
    {
        DebugComponent* lpThis = static_cast<DebugComponent*>(lpComponent);

        for (u32 luGear = 0; luGear < 6u; ++luGear)
        {
            lpThis->mGearStats[luGear].Reset();
        }
    }

    // BrnPhysics::Vehicle::DebugComponent::Construct
    //   The per-car two-phase init VehicleManager::PrepareData runs once per car. It records the
    //   car this component samples, seats the two grip-curve windows, and clears everything the
    //   component accumulates or draws. The console schedules these independent stores across the
    //   whole object; they are grouped here by what they belong to, in console order.
    //
    //   The console issues one further call on `this` right after the assert. That call lands on a
    //   body which is a bare return, and identical bodies are folded together, so the symbol it
    //   carries belongs to an unrelated class. It stores nothing, so it has no statement -- in
    //   particular it does NOT set a vptr; the vptr comes from the constructor, as it does here.
    //
    //   NOT written, on the console or here: the gear cursor, the speed mirror, the inertia
    //   triple, the COM-offset triple and the engine-report block. All of them start zeroed (the
    //   manager lives in static storage on both sides) and their writers are elsewhere.
    void DebugComponent::Construct(RaceCarPhysics* lpRaceCarPhysics)
    {
        CGS_ASSERT(lpRaceCarPhysics != nullptr, "lpRaceCarPhysics != NULL");

        mpRaceCarPhysics = lpRaceCarPhysics;
        mfLastFrameRpm   = 0.0f;

        // The last-wall-triangle record: four zero vectors, then the three edge cosines. The
        // console issues the vectors out of address order (the normal first); they are independent.
        mLastWallTriangle.mVertex0      = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mLastWallTriangle.mVertex1      = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mLastWallTriangle.mVertex2      = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mLastWallTriangle.mNormal       = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mLastWallTriangle.mfEdgeCosine0 = 0.0f;
        mLastWallTriangle.mfEdgeCosine1 = 0.0f;
        mLastWallTriangle.mfEdgeCosine2 = 0.0f;

        // Every draw / report toggle the component's render side reads, off. (The two the console
        // seeds later, interleaved with the crash-velocity scratch, are below where it puts them.)
        mbRenderCOMPosition           = false;
        mbRenderHandlingBody          = false;
        mbOutputGearStats             = false;
        mbOutputDeformationStats      = false;
        mbShowTireFrictions           = false;
        mbRenderFrontWheelsGripCurves = false;
        mbRenderRearWheelsGripCurves  = false;
        mbDrawTractionLines           = false;
        mbDrawTractionLineNormals     = false;
        mbDrawAboveGroundLineTest     = false;
        mbDrawLastWallTriangle        = false;
        mbStoredPhysics               = false;

        mFrontWheelsGripCurveWindow.Construct();
        mRearWheelsGripCurveWindow.Construct();

        ResetGearStats(this);   // inlined by the console at this point in the body

        mfHackCrashExtraPitchVelocity  = 0.0f;
        mbDrawAngularVelocity          = false;
        mfHackCrashExtraYawVelocity    = 0.0f;
        mfHackCrashExtraRollVelocity   = 0.0f;
        mbDrawSuspensionForces         = false;
        mfHackCrashExtraLinearVelocity = 0.0f;

        // One console loop, two cursors: the per-wheel suspension force and its applied flag.
        for (u32 luWheel = 0; luWheel < eNumDrivenWheels; ++luWheel)
        {
            maSuspensionForces[luWheel]        = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
            mabSuspensionForceApplied[luWheel] = false;
        }

        mfDownForceY = 0.0f;
    }

    // BrnPhysics::Vehicle::DebugComponent::GetName
    //   lis r11,aHandling@ha ; addi r3,r11,aHandling@l "Handling" ; blr
    const char* DebugComponent::GetName() const
    {
        return "Handling";
    }

    // BrnPhysics::Vehicle::DebugComponent::GetPath
    //   lis r11,aPhysics@ha ; addi r3,r11,aPhysics@l "Physics" ; blr
    const char* DebugComponent::GetPath() const
    {
        return "Physics";
    }

    // BrnPhysics::Vehicle::DebugComponent::OnActivate @0x82620730.
    // The member addresses and menu order follow the ARTIST body. The raw gap operands in that
    // export resolve to the named VehiclePhysics lanes and component members below.
    void DebugComponent::OnActivate()
    {
        RegisterVariable(&mbDrawSuspensionForces, "Suspension..", "Draw suspension forces");
        RegisterVariable(&mpRaceCarPhysics->mbHasAir, "Suspension..", "Has air");
        RegisterVariable(&mfDownForceY, "Suspension..", "DownForce");
        SetReadOnly(&mpRaceCarPhysics->mbHasAir, true);
        SetReadOnly(&mfDownForceY, true);

        RegisterVariable(&mbRenderCOMPosition, "Rendering..", "Render COM");
        RegisterVariable(&mbRenderHandlingBody, "Rendering..", "Render Handling Body");
        RegisterVariable(&mbDrawTractionLines, "Rendering..", "Render traction lines");
        RegisterVariable(&mbDrawTractionLineNormals, "Rendering..", "Render traction line normals");
        RegisterVariable(&mbDrawAboveGroundLineTest, "Rendering..", "Render above ground test");
        RegisterVariable(&mbDrawLastWallTriangle, "Rendering..", "Render last wall triangle");
        RegisterVariable(&mbDrawAngularVelocity, "Rendering..", "Draw angular velocity");

        RegisterVariable(&mbShowTireFrictions, "", "Show & Hide Tire Frictions");
        RegisterVariable(&mbOutputGearStats, "", "Output gear stats");
        RegisterFunction(&ResetGearStats, this, "Reset gear stats");
        RegisterVariable(&mbOutputDeformationStats, "", "Output deformation stats");

        RegisterVariable(&mafInertia[0], "", "Inertia X");
        RegisterVariable(&mafInertia[1], "", "Inertia Y");
        RegisterVariable(&mafInertia[2], "", "Inertia Z");
        RegisterVariable(&mfSpeedMPH, "", "Speed MPH");

        RegisterVariable(&mpRaceCarPhysics->mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w,
                         "", "Drift Scale");
        RegisterVariable(
            &mpRaceCarPhysics->mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.w,
            "", "Drift Angle");
        RegisterVariable(
            &mpRaceCarPhysics->mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.x,
            "", "Side Force Mag");
        RegisterVariable(&mpRaceCarPhysics->mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.x,
                         "", "Steering Angle");
        RegisterVariable(
            &mpRaceCarPhysics->mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.w,
            "", "Brake Scale");
        RegisterVariable(&mpRaceCarPhysics->mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.y,
                         "", "Maintained Speed");
        RegisterVariable(
            &mpRaceCarPhysics->mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.x,
            "", "Maintain Z");
        RegisterVariable(
            &mpRaceCarPhysics->mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.y,
            "", "Maintain Vel");
        SetStep(
            &mpRaceCarPhysics->mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.x,
            0.0099999998f);
        SetStep(
            &mpRaceCarPhysics->mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.y,
            0.0099999998f);

        InitWindows();
    }

    // BrnPhysics::Vehicle::DebugComponent::InitWindows @0x82603128.
    // The default overload files the two toggles under this component's "Physics" path. The
    // windows bind to the same four maWheels slots the original uses at +0x130/+0x210/+0x2F0/+0x3D0.
    void DebugComponent::InitWindows()
    {
        RegisterVariable(&mbRenderFrontWheelsGripCurves, "Front wheel grip curves");
        RegisterVariable(&mbRenderRearWheelsGripCurves, "Rear wheel grip curves");

        mFrontWheelsGripCurveWindow.Prepare(&mpRaceCarPhysics->maWheels[0],
                                            &mpRaceCarPhysics->maWheels[1],
                                            "Front wheel grip curves");
        mRearWheelsGripCurveWindow.Prepare(&mpRaceCarPhysics->maWheels[2],
                                           &mpRaceCarPhysics->maWheels[3],
                                           "Rear wheel grip curves");
    }

    // BrnPhysics::Vehicle::DebugComponent::SetLastWallTriangle
    //   Record the collision triangle of the last wall the car scraped (for the
    //   DrawLastWallTriangle debug overlay). The console asserts lpTriangle != NULL then copies the
    //   whole 80-byte AOSTriangle, which is exactly the member assign.
    void DebugComponent::SetLastWallTriangle(const CgsGeometric::Triangle4::AOSTriangle* lpTriangle)
    {
        CGS_ASSERT(lpTriangle != nullptr, "lpTriangle != NULL");

        mLastWallTriangle = *lpTriangle;
    }

    // BrnPhysics::Vehicle::DebugComponent::Update
    //   The per-car debug tick VehicleManager::UpdateVehiclePhysics calls once per live car with
    //   the sim timestep. It samples, it never drives anything: it accumulates time into the
    //   current gear's stats row, mirrors the four wheels' road-contact results and the car's
    //   speed, and returns. Nothing here is conditional on a debug-menu toggle -- the toggles are
    //   read by the render/HUD side, which is the dev-UI pass's work.
    void DebugComponent::Update(f32 lfTimeStep)
    {
        // (1) Gear stats. The row for the gear currently being sampled keeps adding the timestep
        //     until the gear-stat report closes it; a closed row is left alone.
        GearStats& lrGearRow = mGearStats[miCurrentGear];
        if (!lrGearRow.mbDoneUpdating)
        {
            lrGearRow.mfTimeInGear = lfTimeStep + lrGearRow.mfTimeInGear;
        }

        // Construct stored this when VehicleManager::PrepareData built the per-car components.
        RaceCarPhysics* lpCar = mpRaceCarPhysics;

        // (2) Mirror each wheel's road-contact result. The console copies the leading 0x2C bytes of
        //     the wheel -- exactly Wheel::RoadContact -- and leaves the four pad bytes of each
        //     0x30-byte destination untouched.
        for (u32 luWheel = 0; luWheel < eNumDrivenWheels; ++luWheel)
        {
            const Wheel::RoadContact& lrSource =
                lpCar->GetWheel(static_cast<EVehicleDrivenWheel>(luWheel)).GetRoadContact();
            Wheel::RoadContact& lrMirror = maRoadContacts[luWheel];

            lrMirror.mPosition               = lrSource.mPosition;
            lrMirror.mNormal                 = lrSource.mNormal;
            lrMirror.mfLineDistanceToRoad    = lrSource.mfLineDistanceToRoad;
            lrMirror.mCollisionTag           = lrSource.mCollisionTag;
            lrMirror.mbIsOnGround            = lrSource.mbIsOnGround;
            lrMirror.mbWasOnGroundLastUpdate = lrSource.mbWasOnGroundLastUpdate;
            lrMirror.mbIsCloseToGround       = lrSource.mbIsCloseToGround;
            lrMirror.mbLineTestIsValid       = lrSource.mbLineTestIsValid;
        }

        // (3) Mirror the car's speed. The console loads the whole mfSpeedMPH vector and keeps
        //     the x lane.
        mfSpeedMPH = lpCar->GetSpeedMPH().x;
    }

    // BrnPhysics::Vehicle::DebugComponent::RecordDownForce
    //   The tail of VehiclePhysics::UpdateDownForce: mirror the magnitude of the down-force it just
    //   applied, for the suspension page of the debug menu. The console inlines this, storing the
    //   force vector's y lane (the only lane the force has).
    void DebugComponent::RecordDownForce(Vector3 lvDownForce)
    {
        mfDownForceY = lvDownForce.y;
    }
}
}
