#include "GameSource/Graphics/BrnShaderConstantsFrame.h"

// @ PS3 0x352004 - reset the frame to a neutral state: identity transforms, everything
// else zeroed, and unlocked for writing. Reconstructed from the DecFIGS DWARF
// (BrnShaderConstantsFrame.cpp:36).
void BrnShaderConstantsFrame::Construct()
{
    mViewProjection.SetIdentity();
    mViewPosition.SetZero();
    mCameraTransform.SetIdentity();

    for (s32 liCount = 0; liCount < BrnGraphics::E_FACE_NUM; ++liCount)
    {
        maEnvMapViewProjectionMatrices[liCount].SetIdentity();
    }

    mEnvMapViewPosition.SetZero();
    mKeyLightDirection.SetZero();
    mKeyLightColour.SetZero();
    mCloudLayerRadii.SetZero();
    mCloudDarkColour0.SetZero();
    mCloudLiteColour0.SetZero();
    mCloudTextureScaleAndOffsets0.SetZero();
    mCloudLayerDensity.SetZero();
    mCloudLayerInvFeather.SetZero();
    mCloudLayerOpacity.SetZero();
    mTopColourDrk.SetZero();
    mHorColourPow.SetZero();
    mSunColourPow.SetZero();
    mHorBleedSclPow.SetZero();
    mFogScattering.SetZero();
    mUnbiasedKeyLightDirection.SetZero();

    mfCloudDistanceCurve = 0.0f;
    mGameTime = 0.0f;
    mfWhiteLevel = 0.0f;
    mbLockedForWriting = false;
}
