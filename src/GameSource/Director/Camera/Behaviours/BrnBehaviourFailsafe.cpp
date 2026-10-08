// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.cpp
//
// BrnDirector::Camera::BehaviourFailsafe -- all six of the failsafe camera's Behaviour overrides
// (Construct, Prepare, Update, GetCollisionPolicy, SetupTweaker, GetName).
//
// What the camera does: it plays the first take of the director resource manager's "Failsafe"
// ICE shot group around the player's car, then re-anchors that take to an upright, orientation-
// lagged copy of the car's frame and spins it slowly about the car's vertical axis (10 degrees a
// second), keeping the take's own dutch roll. The arbitrator testbed is what allocates it.
//
// Every vector operation below is rounded the way the console rounds it (the campaign rounding
// rule): the rotations' sine and cosine are the XDK's XMVectorSinCos, the arc-cosine is
// XMVectorACos, the dots are vmsum3fp128 (Utils::ConsoleVpu::Dot3) and the matrix products and
// point/vector transforms are the fused vmaddfp cascades of Utils::ConsoleVpu.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"

#include "GameSource/Director/Camera/Camera.h"                     // Camera (Update publishes into it)
#include "GameSource/Director/Camera/BrnCameraState.h"             // CameraState::E_FLAG_VALID
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"     // Utils::Tweaker::Construct
#include "GameSource/Director/Camera/Utils/CameraUtils.h"          // Utils::CreateLookAt
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"        // Utils::ConsoleVpu (the fused VMX forms)
#include "GameSource/Director/BrnDirectorResourceManager.h"        // GetFailsafe / GetKeyAnimFromGuid
#include "GameSource/Director/Shots/BrnShotController.h"           // ShotContext
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"      // Attrib::Gen::shotgroup::ShotList
#include "GameSource/AttribSys/Generated/classes/iceanim.h"        // Attrib::Gen::iceanim (the shot)
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"             // ICE::CameraSpaceHandler
#include "SDKs/XboxMath/XMVectorSinCos.h"                          // XboxMath::XMVectorSinCos
#include "SDKs/XboxMath/XMVectorACos.h"                            // XboxMath::XMVectorACos
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // [FLAG PC witness] BRN_CAMBODY_DIAG

#include <cmath>     // std::signbit
#include <cstdlib>   // getenv ([FLAG PC witness] BRN_CAMBODY_DIAG)

namespace
{
    using rw::math::vpu::Matrix44Affine;
    using rw::math::vpu::Vector3;

    // Degrees to radians (0x3C8EFA35), the spin's conversion.
    const f32 KF_DEGS_TO_RADS = 0.017453292f;

    // vmaxfp / vminfp on one broadcast lane: a NaN operand gives a NaN (the first operand's when
    // both are), and -0 orders below +0. Not the fsel rw::math::fpu forms.
    f32 VecFloatMax(f32 lfA, f32 lfB)
    {
        if (lfA != lfA) return lfA;
        if (lfB != lfB) return lfB;
        if (lfA == lfB) return std::signbit(lfA) ? lfB : lfA;
        return (lfA > lfB) ? lfA : lfB;
    }

    f32 VecFloatMin(f32 lfA, f32 lfB)
    {
        if (lfA != lfA) return lfA;
        if (lfB != lfB) return lfB;
        if (lfA == lfB) return std::signbit(lfA) ? lfA : lfB;
        return (lfA < lfB) ? lfA : lfB;
    }

    // rw::math::vpu::Matrix44AffineFromYRotationAngle with the XDK sine and cosine: rows
    // (c,0,-s) / (0,1,0) / (s,0,c) / 0, packed with vperm / vrlimi128 (no arithmetic). The
    // console's w lanes carry copies; the transforms that read this matrix use x, y and z only.
    Matrix44Affine RotationY(f32 lfAngleRads)
    {
        f32 lfSin;
        f32 lfCos;
        XboxMath::XMVectorSinCos(&lfSin, &lfCos, lfAngleRads);
        Matrix44Affine lResult;
        lResult.xAxis = Vector3{ lfCos, 0.0f, -lfSin, 0.0f };
        lResult.yAxis = Vector3{  0.0f, 1.0f,   0.0f, 0.0f };
        lResult.zAxis = Vector3{ lfSin, 0.0f,  lfCos, 0.0f };
        lResult.wAxis = Vector3{  0.0f, 0.0f,   0.0f, 0.0f };
        return lResult;
    }

    // rw::math::vpu::Matrix44AffineFromZRotationAngle, the same way: rows (c,s,0) / (-s,c,0) /
    // (0,0,1) / 0.
    Matrix44Affine RotationZ(f32 lfAngleRads)
    {
        f32 lfSin;
        f32 lfCos;
        XboxMath::XMVectorSinCos(&lfSin, &lfCos, lfAngleRads);
        Matrix44Affine lResult;
        lResult.xAxis = Vector3{  lfCos, lfSin, 0.0f, 0.0f };
        lResult.yAxis = Vector3{ -lfSin, lfCos, 0.0f, 0.0f };
        lResult.zAxis = Vector3{   0.0f,  0.0f, 1.0f, 0.0f };
        lResult.wAxis = Vector3{   0.0f,  0.0f, 0.0f, 0.0f };
        return lResult;
    }

    // [FLAG PC witness] BRN_CAMBODY_DIAG -- NOT IN THE CONSOLE BUILD. One line per Construct /
    // Prepare and the first frames of every Update run, capped per session. Prints only.
    bool CamBodyDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_CAMBODY_DIAG") != 0);
        return sbEnabled && CgsDev::Log::gpDebugPrint != 0;
    }

    s32 siCamBodyDiagLinesLeft = 64;
}

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// Construct -- seed a freshly pooled instance.
//   the base head (the seven stores that are Behaviour::Construct), mpParameters = 0,
//   CollisionPolicyAttachedToVehicle::Construct(policy, false),
//   the key-anim controller's playback reset (timer 0, the four flags clear),
//   OrientationLag::Construct (no parameters, first frame), OrientationLag::Parameters::Construct
//   (0.05 / true) with the slerp spring then overwritten to 0.01, and the spin angle 0.
// ----------------------------------------------------------------------------
void BehaviourFailsafe::Construct()
{
    Behaviour::Construct();
    mpParameters = 0;

    mCollisionPolicy.Construct(false);

    mKeyAnimController.ResetPlayback();

    mOrientationLag.Construct();
    mOrientationLagParams.Construct();
    mOrientationLagParams.mfSlerpSpring = 0.01f;

    mfRotationOffset = 0.0f;

    if (CamBodyDiagEnabled() && siCamBodyDiagLinesLeft > 0)
    {
        --siCamBodyDiagLinesLeft;
        *CgsDev::Log::gpDebugPrint << "[cambody] Failsafe Construct [FLAG PC witness]\n";
    }
}

// ----------------------------------------------------------------------------
// Prepare -- bind the controller to the first take of the "Failsafe" shot group, name the
// behaviour after that take, point the orientation lag at its own tunables. Always true.
// ----------------------------------------------------------------------------
bool BehaviourFailsafe::Prepare(const BehaviourSharedPrepareReleaseInfo& lSharedInfo)
{
    CGS_ASSERT(lSharedInfo.mpDirectorResourceManager != 0, "lSharedInfo.mpDirectorResourceManager");   // :73
    const DirectorResourceManager& lrResources = *lSharedInfo.GetDirectorResourceManager();

    CGS_ASSERT(lrResources.GetFailsafe().Num_ShotList() > 0,
               "lSharedInfo.mpDirectorResourceManager->GetFailsafe().Num_ShotList() > 0");              // :74
    const Attrib::Gen::iceanim lShot(*lrResources.GetFailsafe().ShotList(0), 0);

    const bool lbControllerPrepared = mKeyAnimController.Prepare(lrResources, lShot.GetAnimGuid());
    CGS_ASSERT(lbControllerPrepared,
               "mKeyAnimController.Prepare(*lSharedInfo.mpDirectorResourceManager, lShot.Guid())");     // :76
    (void)lbControllerPrepared;

    const ICE::ICETakeData* lpTakeData = lrResources.GetKeyAnimFromGuid(lShot.GetAnimGuid());
    CGS_ASSERT(lpTakeData != 0, "lpTakeData != NULL");                                                 // :80
    SetDebugParametersName(lpTakeData->GetName());

    mOrientationLag.SetParameters(&mOrientationLagParams);

    SetPrepared();

    if (CamBodyDiagEnabled() && siCamBodyDiagLinesLeft > 0)
    {
        --siCamBodyDiagLinesLeft;
        *CgsDev::Log::gpDebugPrint << "[cambody] Failsafe Prepare take \"" << lpTakeData->GetName()
                                   << "\" guid " << lShot.GetAnimGuid()
                                   << " length " << lpTakeData->GetLength() << " [FLAG PC witness]\n";
    }
    return true;
}

// ----------------------------------------------------------------------------
// Update.
//   1. Mark the camera valid, then run the take: the controller resolves its reference spaces
//      through a copy of the shared handler whose CAR and CAR2 spaces are both the player's car.
//   2. The take's dutch: the angle between its up axis and the up axis of an upright look-at
//      along its view (Dot clamped to [0, 1], XMVectorACos, negated when the Dot is negative).
//   3. The take's eye and view direction in the car's frame (the car frame's orthonormal
//      inverse), the car's frame stood upright (a look-at along its nose) and lagged.
//   4. The spin: 10 degrees a second of the un-slowed world step, about the vertical.
//   5. The eye and view, spun and carried back out through the lagged upright frame, give an
//      upright look-at; the take's dutch rolls it, and the result is the camera.
// ----------------------------------------------------------------------------
bool BehaviourFailsafe::Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo)
{
    static f32 sfRotationRate = 10.0f;   // degrees per second

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");                                             // :102

    lrCamera.GetState().SetFlag(CameraState::E_FLAG_VALID, true);

    const Matrix44Affine& lrCarToWorld = lrSharedInfo.mPlayerInfo.mRaceCarState.mTransform;

    ICE::CameraSpaceHandler lSpaceHandler(*lrSharedInfo.GetCameraSpaceHandler());
    lSpaceHandler.SetCarToWorld(lrCarToWorld);
    lSpaceHandler.SetCar2ToWorld(lrCarToWorld);

    ShotContext lShotContext;
    lShotContext.mpAllVehicleData     = lrSharedInfo.GetWorld();
    lShotContext.mpCameraSpaceHandler = &lSpaceHandler;
    lShotContext.mpTimestep           = &lrSharedInfo.GetTimestep();
    mKeyAnimController.Update(lShotContext, &lrCamera);

    const Matrix44Affine lTransform  = lrCamera.GetTransform();
    const Matrix44Affine lWorldToCar = Utils::ConsoleVpu::InverseOfMatrixWithOrthonormal3x3(lrCarToWorld);

    // The take's dutch.
    const Matrix44Affine lUprightCamera = Utils::CreateLookAt(lTransform.wAxis, lTransform.wAxis + lTransform.zAxis);
    const f32 lfUpDot = Utils::ConsoleVpu::Dot3(lUprightCamera.yAxis, lTransform.yAxis);
    f32 lfDutch = XboxMath::XMVectorACos(VecFloatMin(1.0f, VecFloatMax(0.0f, lfUpDot)));
    if (0.0f > lfUpDot)
    {
        lfDutch = -lfDutch;
    }

    // The car's frame stood upright, then lagged.
    const Matrix44Affine lUprightCarToWorld = Utils::CreateLookAt(lrCarToWorld.wAxis, lrCarToWorld.wAxis + lrCarToWorld.zAxis);
    const f32 lfTimestep = lrSharedInfo.GetTimestep(BrnDirector::Timestep::E_WORLD_NO_SLOMO);
    mOrientationLag.Update(lfTimestep, lUprightCarToWorld);
    const Matrix44Affine lLaggedCarToWorld = mOrientationLag.GetTransform();

    // The take's eye and view direction in the car's frame.
    const Vector3 lLocalEye       = Utils::ConsoleVpu::TransformPoint(lWorldToCar, lTransform.wAxis);
    const Vector3 lLocalDirection = Utils::ConsoleVpu::TransformVector(lWorldToCar, lTransform.zAxis);

    // The spin.
    mfRotationOffset = Utils::ConsoleVpu::MultiplyAdd(lfTimestep, sfRotationRate, mfRotationOffset);
    const Matrix44Affine lSpin = RotationY(mfRotationOffset * KF_DEGS_TO_RADS);

    const Vector3 lEye = Utils::ConsoleVpu::TransformPoint(
        lLaggedCarToWorld, Utils::ConsoleVpu::TransformPoint(lSpin, lLocalEye));
    const Vector3 lDirection = Utils::ConsoleVpu::TransformVector(
        lLaggedCarToWorld, Utils::ConsoleVpu::TransformVector(lSpin, lLocalDirection));
    const Matrix44Affine lUprightTransform = Utils::CreateLookAt(lEye, lEye + lDirection);

    lrCamera.SetTransform(Utils::ConsoleVpu::Mult(RotationZ(lfDutch), lUprightTransform));
    lrCamera.ValidateTransformWithDebugInfo();

    if (CamBodyDiagEnabled() && siCamBodyDiagLinesLeft > 0)
    {
        static s32 siFrame = 0;
        if (siFrame < 8 || siFrame % 60 == 0)
        {
            --siCamBodyDiagLinesLeft;
            const Vector3& lrPublished = lrCamera.GetTransform().wAxis;
            *CgsDev::Log::gpDebugPrint
                << "[cambody] Failsafe Update frame " << siFrame
                << " car " << lrCarToWorld.wAxis.x << " " << lrCarToWorld.wAxis.y << " " << lrCarToWorld.wAxis.z
                << " take " << lTransform.wAxis.x << " " << lTransform.wAxis.y << " " << lTransform.wAxis.z
                << " cam " << lrPublished.x << " " << lrPublished.y << " " << lrPublished.z
                << " spin " << mfRotationOffset << " dutch " << lfDutch
                << " fov " << lrCamera.GetFOV() << " [FLAG PC witness]\n";
        }
        ++siFrame;
    }
    return true;
}

// The vehicle-attached policy Construct seeded.
CollisionPolicy* BehaviourFailsafe::GetCollisionPolicy()
{
    return &mCollisionPolicy;
}

void BehaviourFailsafe::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();
}

const char* BehaviourFailsafe::GetName() const
{
    return "BehaviourFailsafe";
}

} // namespace Camera
} // namespace BrnDirector
