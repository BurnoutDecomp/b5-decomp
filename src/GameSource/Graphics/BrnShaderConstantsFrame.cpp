#include "GameSource/Graphics/BrnShaderConstantsFrame.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT

static const char KAC_ARTIST_ASSERT_FILE[] = "..\\..\\..\\GameSource\\Graphics/BrnShaderConstantsFrame.h";


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
    // FLAG: ARTIST 0x823F7478 stores flt_82001C98 (== 1.0) to mfWhiteLevel (this+0x318),
    // then 0 to mbLockedForWriting (this+0x31C). DecFIGS 0x352004 confirms *(this+792)=1.0.
    // (Was 0.0f.) White level is a multiplier; 0.0f would black out everything.
    mfWhiteLevel = 1.0f;
    mbLockedForWriting = false;
}

// ============================================================================
// Lock-checked accessors the console emitted out-of-line. Reconstructed from
// BURNOUT_X360_ARTIST.XEX + the DecFIGS DWARF. Every getter asserts the frame is finalized
// (NOT still write-locked, mbLockedForWriting @this+0x31C == 0); every setter asserts it IS
// write-locked. The X360 fires the streamed assert (dropped file/line + trailing "\n"); the
// house CGS_ASSERT keeps the stringized condition. Each body is a single member load/store
// (the SIMD lvx/stvx copy of a Matrix44/Vector4/Vector3, or an lfs/stfs of an f32).
// ============================================================================

// ---- write-lock setters ("true == mbLockedForWriting") ---------------------

// X360 0x827B0018: write the per-frame view-projection matrix. 64-byte store @this+0x00.
void BrnShaderConstantsFrame::SetViewProjectionMatrix(Matrix44 lMatrix)
{
    CGS_ASSERT(true == mbLockedForWriting, "true == mbLockedForWriting");
    mViewProjection = lMatrix;
}

// X360 0x827B00A8: write one of the six env-map face view-projection matrices. 64-byte
// store @this+0x90+face*64.
void BrnShaderConstantsFrame::SetEnvMapViewProjectionMatrix(BrnGraphics::EEnvironmentMapFace leFace, Matrix44 lMatrix)
{
    CGS_ASSERT(true == mbLockedForWriting, "true == mbLockedForWriting");
    CGS_ASSERT(leFace < BrnGraphics::E_FACE_NUM, "leEnvMapFace < BrnGraphics::E_FACE_NUM");
    maEnvMapViewProjectionMatrices[leFace] = lMatrix;
}

// X360 0x827B0158: write the camera transform. 64-byte affine store @this+0x50.
void BrnShaderConstantsFrame::SetCameraTransform(Matrix44Affine lM)
{
    CGS_ASSERT(true == mbLockedForWriting, "true == mbLockedForWriting");
    mCameraTransform = lM;
}

// X360 0x827B01E8: write the env-map render view position. 16-byte store @this+0x210.
void BrnShaderConstantsFrame::SetEnvMapViewPosition(Vector3 lV)
{
    CGS_ASSERT(true == mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "true == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 298 /* ARTIST 0x827B021C */));
    mEnvMapViewPosition = lV;
}

// ---- read-lock getters ("false == mbLockedForWriting") ---------------------

// X360 0x823F4980.
Matrix44 BrnShaderConstantsFrame::GetViewProjectionMatrix() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 238 /* ARTIST 0x823F49B0 */));
    return mViewProjection;
}

// X360 0x823F4A10: lock-checked, bounds-checked (base @this+0x90, stride 0x40).
Matrix44 BrnShaderConstantsFrame::GetEnvMapViewProjectionMatrix(BrnGraphics::EEnvironmentMapFace leFace) const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 257 /* ARTIST 0x823F4A44 */));
    CGS_ASSERT(leFace < BrnGraphics::E_FACE_NUM,
        CGS_ASSERT_MESSAGE_AT(
            "leEnvMapFace < BrnGraphics::E_FACE_NUM", KAC_ARTIST_ASSERT_FILE, 258 /* ARTIST 0x823F4A68 */));
    return maEnvMapViewProjectionMatrices[leFace];
}

// X360 0x823F4AC0.
Vector3 BrnShaderConstantsFrame::GetViewPosition() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 273 /* ARTIST 0x823F4AF0 */));
    return mViewPosition;
}

// X360 0x823F4B30.
Vector3 BrnShaderConstantsFrame::GetEnvMapViewPosition() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 305 /* ARTIST 0x823F4B60 */));
    return mEnvMapViewPosition;
}

// X360 0x823F4BA0.
Vector3 BrnShaderConstantsFrame::GetKeyLightDirection() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 320 /* ARTIST 0x823F4BD0 */));
    return mKeyLightDirection;
}

// X360 0x823F4C10.
Vector3 BrnShaderConstantsFrame::GetKeyLightColour() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 334 /* ARTIST 0x823F4C40 */));
    return mKeyLightColour;
}

// X360 0x823F4C80.
Vector4 BrnShaderConstantsFrame::GetCloudDarkColour0() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 349 /* ARTIST 0x823F4CB0 */));
    return mCloudDarkColour0;
}

// X360 0x823F4CF0.
Vector4 BrnShaderConstantsFrame::GetCloudLiteColour0() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 363 /* ARTIST 0x823F4D20 */));
    return mCloudLiteColour0;
}

// X360 0x823F4D60.
Vector4 BrnShaderConstantsFrame::GetCloudTextureScaleAndOffsets0() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 377 /* ARTIST 0x823F4D90 */));
    return mCloudTextureScaleAndOffsets0;
}

// X360 0x823F4DD0.
Vector4 BrnShaderConstantsFrame::GetCloudLayerDensity() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 391 /* ARTIST 0x823F4E00 */));
    return mCloudLayerDensity;
}

// X360 0x823F4E40.
Vector4 BrnShaderConstantsFrame::GetCloudLayerInvFeather() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 405 /* ARTIST 0x823F4E70 */));
    return mCloudLayerInvFeather;
}

// X360 0x823F4EB0.
Vector4 BrnShaderConstantsFrame::GetCloudLayerOpacity() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 419 /* ARTIST 0x823F4EE0 */));
    return mCloudLayerOpacity;
}

// X360 0x823F4F20.
Vector4 BrnShaderConstantsFrame::GetFogScattering() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 433 /* ARTIST 0x823F4F50 */));
    return mFogScattering;
}

// X360 0x823F4F90.
f32 BrnShaderConstantsFrame::GetCloudDistanceCurve() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 447 /* ARTIST 0x823F4FB8 */));
    return mfCloudDistanceCurve;
}

// X360 0x823F4FE8.
Vector4 BrnShaderConstantsFrame::GetTopColourDrk() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 482 /* ARTIST 0x823F5018 */));
    return mTopColourDrk;
}

// X360 0x823F5058.
Vector4 BrnShaderConstantsFrame::GetHorColourPow() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 489 /* ARTIST 0x823F5088 */));
    return mHorColourPow;
}

// X360 0x823F50C8.
Vector4 BrnShaderConstantsFrame::GetSunColourPow() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 496 /* ARTIST 0x823F50F8 */));
    return mSunColourPow;
}

// X360 0x823F5138.
Vector3 BrnShaderConstantsFrame::GetHorBleedSclPow() const
{
    CGS_ASSERT(!mbLockedForWriting,
        CGS_ASSERT_MESSAGE_AT(
            "false == mbLockedForWriting", KAC_ARTIST_ASSERT_FILE, 503 /* ARTIST 0x823F5168 */));
    return mHorBleedSclPow;
}
