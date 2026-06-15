#pragma once

#include "BrnCommonTypes.h"                                       // Matrix44, Matrix44Affine, Vector3, Vector4
#include "GameSource/World/EnvironmentMap/BrnEnvironmentMap.h"    // BrnGraphics::EEnvironmentMapFace

// BrnShaderConstantsFrame - the renderer's per-frame shader constant buffer: camera /
// view-projection, the six env-map face view-projections, key light, and the sky / cloud /
// fog parameters the world + sky shaders read. BrnRendererModule double-buffers a pair of
// these (maShaderConstantsFrames[2]). Reconstructed from the DecFIGS DWARF
// (BrnShaderConstantsFrame.{h,cpp}); layout is console-faithful.
struct BrnShaderConstantsFrame
{
    void Construct();

    void           SetViewProjectionMatrix(Matrix44 lMatrix)            { mViewProjection = lMatrix; }
    Matrix44       GetViewProjectionMatrix() const                      { return mViewProjection; }
    void           SetEnvMapViewProjectionMatrix(BrnGraphics::EEnvironmentMapFace leFace, Matrix44 lMatrix) { maEnvMapViewProjectionMatrices[leFace] = lMatrix; }
    Matrix44       GetEnvMapViewProjectionMatrix(BrnGraphics::EEnvironmentMapFace leFace) const             { return maEnvMapViewProjectionMatrices[leFace]; }
    void           SetViewPosition(Vector3 lV)                          { mViewPosition = lV; }
    Vector3        GetViewPosition() const                             { return mViewPosition; }
    void           SetCameraTransform(Matrix44Affine lM)               { mCameraTransform = lM; }
    Matrix44Affine GetCameraTransform() const                         { return mCameraTransform; }
    void           SetEnvMapViewPosition(Vector3 lV)                   { mEnvMapViewPosition = lV; }
    Vector3        GetEnvMapViewPosition() const                       { return mEnvMapViewPosition; }
    void           SetKeyLightDirection(Vector3 lV)                    { mKeyLightDirection = lV; }
    Vector3        GetKeyLightDirection() const                        { return mKeyLightDirection; }
    void           SetKeyLightColour(Vector3 lV)                       { mKeyLightColour = lV; }
    Vector3        GetKeyLightColour() const                           { return mKeyLightColour; }
    Vector4        GetTopColourDrk() const                             { return mTopColourDrk; }
    Vector4        GetHorColourPow() const                             { return mHorColourPow; }
    Vector4        GetSunColourPow() const                             { return mSunColourPow; }
    Vector3        GetHorBleedSclPow() const                           { return mHorBleedSclPow; }
    void           SetTopColourDrk(Vector4 lV)                         { mTopColourDrk = lV; }
    void           SetHorColourPow(Vector4 lV)                         { mHorColourPow = lV; }
    void           SetSunColourPow(Vector4 lV)                         { mSunColourPow = lV; }
    void           SetHorBleedSclPow(Vector3 lV)                       { mHorBleedSclPow = lV; }
    void           SetFogScattering(Vector4 lV)                        { mFogScattering = lV; }
    Vector4        GetFogScattering() const                           { return mFogScattering; }
    void           SetCloudDarkColour0(Vector4 lV)                    { mCloudDarkColour0 = lV; }
    Vector4        GetCloudDarkColour0() const                        { return mCloudDarkColour0; }
    void           SetCloudLiteColour0(Vector4 lV)                    { mCloudLiteColour0 = lV; }
    Vector4        GetCloudLiteColour0() const                        { return mCloudLiteColour0; }
    void           SetCloudTextureScaleAndOffsets0(Vector4 lV)        { mCloudTextureScaleAndOffsets0 = lV; }
    Vector4        GetCloudTextureScaleAndOffsets0() const            { return mCloudTextureScaleAndOffsets0; }
    void           SetCloudLayerDensity(Vector4 lV)                   { mCloudLayerDensity = lV; }
    Vector4        GetCloudLayerDensity() const                       { return mCloudLayerDensity; }
    void           SetCloudLayerInvFeather(Vector4 lV)               { mCloudLayerInvFeather = lV; }
    Vector4        GetCloudLayerInvFeather() const                    { return mCloudLayerInvFeather; }
    void           SetCloudLayerOpacity(Vector4 lV)                   { mCloudLayerOpacity = lV; }
    Vector4        GetCloudLayerOpacity() const                       { return mCloudLayerOpacity; }
    void           SetCloudDistanceCurve(f32 lf)                      { mfCloudDistanceCurve = lf; }
    f32            GetCloudDistanceCurve() const                      { return mfCloudDistanceCurve; }
    void           SetGameTime(f32 lf)                                { mGameTime = lf; }
    f32            GetGameTime() const                                { return mGameTime; }
    void           SetWhiteLevel(f32 lf)                              { mfWhiteLevel = lf; }
    f32            GetWhiteLevel() const                              { return mfWhiteLevel; }
    Vector3        GetUnbiasedKeyLightDirection() const               { return mUnbiasedKeyLightDirection; }
    void           SetUnbiasedKeyLightDirection(Vector3 lV)           { mUnbiasedKeyLightDirection = lV; }

private:
    Matrix44       mViewProjection;
    Vector3        mViewPosition;
    Matrix44Affine mCameraTransform;
    Matrix44       maEnvMapViewProjectionMatrices[6];
    Vector3        mEnvMapViewPosition;
    Vector3        mKeyLightDirection;
    Vector3        mKeyLightColour;
    Vector4        mCloudLayerRadii;
    Vector4        mCloudDarkColour0;
    Vector4        mCloudLiteColour0;
    Vector4        mCloudTextureScaleAndOffsets0;
    Vector4        mCloudLayerDensity;
    Vector4        mCloudLayerInvFeather;
    Vector4        mCloudLayerOpacity;
    Vector4        mTopColourDrk;
    Vector4        mHorColourPow;
    Vector4        mSunColourPow;
    Vector3        mHorBleedSclPow;
    Vector4        mFogScattering;
    Vector3        mUnbiasedKeyLightDirection;
    f32            mfCloudDistanceCurve;
    f32            mGameTime;
    f32            mfWhiteLevel;
    bool volatile  mbLockedForWriting;
};
