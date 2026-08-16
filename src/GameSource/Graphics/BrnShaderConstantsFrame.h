#pragma once

#include "BrnCommonTypes.h"                                       // Matrix44, Matrix44Affine, Vector3, Vector4
#include "GameSource/World/EnvironmentMap/BrnEnvironmentMap.h"    // BrnGraphics::EEnvironmentMapFace

// BrnShaderConstantsFrame - the renderer's per-frame shader constant buffer: camera /
// view-projection, the six env-map face view-projections, key light, and the sky / cloud /
// fog parameters the world + sky shaders read. BrnRendererModule double-buffers a pair of
// these (maShaderConstantsFrames[2]). Reconstructed from the DecFIGS DWARF
// (BrnShaderConstantsFrame.{h,cpp}); layout is console-faithful.
//
// Lock discipline: the frame is filled while write-locked (mbLockedForWriting, this+0x31C)
// and read only once finalized. The X360 guards each accessor it emitted OUT-OF-LINE with a
// lock assert -- getters assert the frame is NOT write-locked ("false == mbLockedForWriting");
// setters assert it IS ("true == mbLockedForWriting"). Those accessors are declared here and
// bodied in BrnShaderConstantsFrame.cpp. The remaining accessors were fully inlined at their
// call sites (no standalone symbol) and are kept as trivial inline members below.
struct BrnShaderConstantsFrame
{
    BrnShaderConstantsFrame() = default;
    // X360 0x823FC338: compiler-emitted copy constructor (address-taken by
    // BrnRendererModule::Render's frame swap). The console body is a memberwise byte copy of
    // the whole object 0x000..0x31C -- exactly the defaulted copy constructor.
    BrnShaderConstantsFrame(const BrnShaderConstantsFrame&) = default;

    void Construct();

    // ---- accessors the console emitted out-of-line (lock-asserted; bodies in .cpp) --------
    void           SetViewProjectionMatrix(Matrix44 lMatrix);                                              // X360 0x827B0018
    Matrix44       GetViewProjectionMatrix() const;                                                        // X360 0x823F4980
    void           SetEnvMapViewProjectionMatrix(BrnGraphics::EEnvironmentMapFace leFace, Matrix44 lMatrix); // X360 0x827B00A8
    Matrix44       GetEnvMapViewProjectionMatrix(BrnGraphics::EEnvironmentMapFace leFace) const;            // X360 0x823F4A10
    void           SetCameraTransform(Matrix44Affine lM);                                                  // X360 0x827B0158
    void           SetEnvMapViewPosition(Vector3 lV);                                                      // X360 0x827B01E8
    Vector3        GetEnvMapViewPosition() const;                                                          // X360 0x823F4B30
    Vector3        GetViewPosition() const;                                                                // X360 0x823F4AC0
    Vector3        GetKeyLightDirection() const;                                                           // X360 0x823F4BA0
    Vector3        GetKeyLightColour() const;                                                              // X360 0x823F4C10
    Vector4        GetTopColourDrk() const;                                                                // X360 0x823F4FE8
    Vector4        GetHorColourPow() const;                                                                // X360 0x823F5058
    Vector4        GetSunColourPow() const;                                                                // X360 0x823F50C8
    Vector3        GetHorBleedSclPow() const;                                                              // X360 0x823F5138
    Vector4        GetFogScattering() const;                                                               // X360 0x823F4F20
    Vector4        GetCloudDarkColour0() const;                                                            // X360 0x823F4C80
    Vector4        GetCloudLiteColour0() const;                                                            // X360 0x823F4CF0
    Vector4        GetCloudTextureScaleAndOffsets0() const;                                                // X360 0x823F4D60
    Vector4        GetCloudLayerDensity() const;                                                           // X360 0x823F4DD0
    Vector4        GetCloudLayerInvFeather() const;                                                        // X360 0x823F4E40
    Vector4        GetCloudLayerOpacity() const;                                                           // X360 0x823F4EB0
    f32            GetCloudDistanceCurve() const;                                                          // X360 0x823F4F90

    // ---- accessors fully inlined on the console (trivial; no lock symbol survives) ---------
    void           SetViewPosition(Vector3 lV)                          { mViewPosition = lV; }
    Matrix44Affine GetCameraTransform() const                         { return mCameraTransform; }
    void           SetKeyLightDirection(Vector3 lV)                    { mKeyLightDirection = lV; }
    void           SetKeyLightColour(Vector3 lV)                       { mKeyLightColour = lV; }
    void           SetTopColourDrk(Vector4 lV)                         { mTopColourDrk = lV; }
    void           SetHorColourPow(Vector4 lV)                         { mHorColourPow = lV; }
    void           SetSunColourPow(Vector4 lV)                         { mSunColourPow = lV; }
    void           SetHorBleedSclPow(Vector3 lV)                       { mHorBleedSclPow = lV; }
    void           SetFogScattering(Vector4 lV)                        { mFogScattering = lV; }
    void           SetCloudDarkColour0(Vector4 lV)                    { mCloudDarkColour0 = lV; }
    void           SetCloudLiteColour0(Vector4 lV)                    { mCloudLiteColour0 = lV; }
    void           SetCloudTextureScaleAndOffsets0(Vector4 lV)        { mCloudTextureScaleAndOffsets0 = lV; }
    void           SetCloudLayerDensity(Vector4 lV)                   { mCloudLayerDensity = lV; }
    void           SetCloudLayerInvFeather(Vector4 lV)               { mCloudLayerInvFeather = lV; }
    void           SetCloudLayerOpacity(Vector4 lV)                   { mCloudLayerOpacity = lV; }
    void           SetCloudDistanceCurve(f32 lf)                      { mfCloudDistanceCurve = lf; }
    void           SetGameTime(f32 lf)                                { mGameTime = lf; }
    f32            GetGameTime() const                                { return mGameTime; }
    void           SetWhiteLevel(f32 lf)                              { mfWhiteLevel = lf; }
    f32            GetWhiteLevel() const                              { return mfWhiteLevel; }
    Vector3        GetUnbiasedKeyLightDirection() const               { return mUnbiasedKeyLightDirection; }
    void           SetUnbiasedKeyLightDirection(Vector3 lV)           { mUnbiasedKeyLightDirection = lV; }

private:
    Matrix44       mViewProjection;                        // @0x000
    Vector3        mViewPosition;                          // @0x040
    Matrix44Affine mCameraTransform;                       // @0x050 (4x16 = 64 bytes)
    Matrix44       maEnvMapViewProjectionMatrices[6];      // @0x090 (stride 64)
    Vector3        mEnvMapViewPosition;                    // @0x210
    Vector3        mKeyLightDirection;                     // @0x220
    Vector3        mKeyLightColour;                        // @0x230
    Vector4        mCloudLayerRadii;                       // @0x240
    Vector4        mCloudDarkColour0;                      // @0x250
    Vector4        mCloudLiteColour0;                      // @0x260
    Vector4        mCloudTextureScaleAndOffsets0;          // @0x270
    Vector4        mCloudLayerDensity;                     // @0x280
    Vector4        mCloudLayerInvFeather;                  // @0x290
    Vector4        mCloudLayerOpacity;                     // @0x2A0
    Vector4        mTopColourDrk;                          // @0x2B0
    Vector4        mHorColourPow;                          // @0x2C0
    Vector4        mSunColourPow;                          // @0x2D0
    Vector3        mHorBleedSclPow;                        // @0x2E0
    Vector4        mFogScattering;                         // @0x2F0
    Vector3        mUnbiasedKeyLightDirection;             // @0x300
    f32            mfCloudDistanceCurve;                   // @0x310
    f32            mGameTime;                              // @0x314
    f32            mfWhiteLevel;                           // @0x318
    bool volatile  mbLockedForWriting;                     // @0x31C

public:
    // ADDITIVE (not X360 symbols). The frame's only producer is
    // WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410 -- RECONSTRUCTED as of
    // the env-manager go-live wave (2026-08-16) -- and the console opens/closes the write
    // lock AROUND it, in BrnRendererModule::Update, which is not reconstructed. So the two
    // PC callers of that producer (WorldModule::GenerateDispatchListsBringUp) and the sky
    // bring-up publisher drive the lock by name rather than poking the flag.
    // Delete when BrnRendererModule::Update lands.
    void LockForWriting()   { mbLockedForWriting = true; }
    void UnlockForWriting() { mbLockedForWriting = false; }
};

// [FLAG PC bring-up] The shader-constants frame the WORLD producer filled this dispatch
// frame. Defined in BrnWorldModule.cpp, written by WorldModule::GenerateDispatchListsBringUp
// through the REAL WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410, which on
// the console writes it via lpDispatchInputBuffer->GetShaderConstantsFrame(). The renderer's
// own maShaderConstantsFrames[] is private to BrnRendererModule and the dispatch IO buffer
// set does not exist on PC, so this is the seam -- the same one gBrnSkyCameraBringUp
// already crosses for the camera.
//
// It carries the LIVE sky gradient / cloud / key-light / fog set from the environment
// manager, i.e. exactly the members BrnRendererModule::PublishSkyConstantsBringUp still
// hard-codes from the noon keyframe. DELETE-WHEN that publisher becomes a copy of this
// frame and the dispatch IO buffer set is real.
extern BrnShaderConstantsFrame gBrnWorldShaderConstantsFrameBringUp;
extern bool                    gbBrnWorldShaderConstantsFrameBringUpValid;

// [FLAG PC bring-up] The stand-in camera the world producer framed this frame.
//
// The console's WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410 fills the
// renderer's BrnShaderConstantsFrame from the world module's own camera + the environment
// manager's blended keyframe. Neither producer is live: the camera is
// GenerateDispatchListsBringUp's stand-in and the environment settings are not converted
// or streamed. So the one thing the renderer cannot derive for itself -- the view
// projection the world was actually drawn with -- is handed across here, and the renderer
// supplies the rest (BrnRendererModule::PublishSkyConstantsBringUp).
//
// DELETE together with GenerateDispatchListsBringUp / PublishSkyConstantsBringUp when the
// real camera and EnvironmentManager::GenerateShaderConstants land.
struct BrnSkyCameraBringUp
{
    Matrix44 mViewProjection;   // ROW-VECTOR convention, as everywhere in the dispatch path
    Vector3  mViewPosition;     // the eye
    bool     mbValid;
};
extern BrnSkyCameraBringUp gBrnSkyCameraBringUp;
