// BrnDirector::Camera::BehaviourHeliCam::Parameters::Construct -- the heli-cam parameter block's
// seed, which the camera parameter bank runs over its "HeliCam Default" block.

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"

namespace BrnDirector
{
namespace Camera
{

void BehaviourHeliCam::Parameters::Construct()
{
    Behaviour::Parameters::Construct();
    mType = eBehaviourHeliCam;

    mShakeParams.Construct();
    mShakeParams.mfZShakeMagnitudeDegs   = 0.0f;
    mfInitialDistanceX                   = 0.0f;
    mShakeParams.mfXYShakeMagnitudeDegs  = 0.006f;
    mShakeParams.mfXYWobbleMagnitudeDegs = 0.15f;
    mShakeParams.mfWobbleCenteringFactor = 0.01f;
    mfHeight                             = 0.25f;
    mfInitialDistanceZ                   = 0.25f;
    mfVelocityMPS                        = 40.0f;
    mFOV.mfFOV                           = 15.0f;

    mLookerParams.Construct();
    mLookerParams.mfDesiredPerceivedDistance       = 40.0f;
    mLookerParams.mfMinFOV                         = 5.0f;
    mLookerParams.mfToleranceForDistanceFromIdeal  = 0.5f;
    mLookerParams.mfToleranceForDistanceFromTarget = 0.1f;
}

} // namespace Camera
} // namespace BrnDirector
