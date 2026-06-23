#pragma once

#include "BrnCommonTypes.h"   // Vector2, Vector4, Matrix44Affine (shared rw::math::vpu types)

namespace BrnEffects
{
namespace
{
const f32 KF_DEF_BLOOM_LUMINANCE = 0.98f;
const f32 KF_DEF_BLOOM_THRESHOLD = 0.77f;
const f32 KF_DEF_VIGNETTE_ANGLE = 0.0f;
const f32 KF_DEF_VIGNETTE_SHARPNESS = 0.33f;
const f32 KF_DEF_DEPTH_OF_FIELD_NEAR_PLANE = 0.0f;
const f32 KF_DEF_DEPTH_OF_FIELD_FOCAL_PLANE = 0.0f;
const f32 KF_DEF_DEPTH_OF_FIELD_FOCAL_PLANE2 = 0.0f;
const f32 KF_DEF_DEPTH_OF_FIELD_FAR_PLANE = 0.0f;
const f32 KF_DEF_DEPTH_OF_FIELD_AMOUNT = 1.0f;
const f32 KF_DEF_BLUR_OPACITY = 0.0f;
const f32 KF_DEF_BLUR_VELOCITY = 0.0f;
const f32 KF_DEF_BLUR_SHARPNESS = 0.0f;
const f32 KF_DEF_BLUR_NOISE = 0.0f;
const f32 KF_DEF_BLUR_ANGLE = 0.0f;
}

struct BloomData
{
    static const f32 kfDefLuminance;
    static const f32 kfDefThreshold;
    static const Vector4 kv4DefScale;

    f32 mfLuminance;
    f32 mfThreshold;
    Vector4 mv4Scale;

    void Construct()
    {
        mfLuminance = KF_DEF_BLOOM_LUMINANCE;
        mfThreshold = KF_DEF_BLOOM_THRESHOLD;
        mv4Scale = kv4DefScale;
    }

    // 4-way per-member weighted blend (X360 ARTIST @ 0x823F34B0). Each scalar and
    // each lane of mv4Scale is the linear combination of the four source instances
    // weighted by (lfWa..lfWd). Returns lpResult (callers chain on the return).
    static BloomData* SetToBlend(BloomData* lpResult,
                                 const BloomData* lpA, f32 lfWa, f32 lfWb, f32 lfWc, f32 lfWd,
                                 const BloomData* lpB, const BloomData* lpC, const BloomData* lpD);
};

struct VignetteData
{
    static const f32 kfDefAngle;
    static const f32 kfDefSharpness;
    static const Vector2 kv2DefAmount;
    static const Vector2 kv2DefCentre;
    static const Vector4 kv4DefInnerColour;
    static const Vector4 kv4DefOuterColour;

    f32 mfAngle;
    f32 mfSharpness;
    Vector2 mv2Amount;
    Vector2 mv2Centre;
    Vector4 mv4InnerColour;
    Vector4 mv4OuterColour;

    void Construct()
    {
        mfAngle = KF_DEF_VIGNETTE_ANGLE;
        mfSharpness = KF_DEF_VIGNETTE_SHARPNESS;
        mv2Amount = kv2DefAmount;
        mv2Centre = kv2DefCentre;
        mv4InnerColour = kv4DefInnerColour;
        mv4OuterColour = kv4DefOuterColour;
    }

    // 2-way per-member weighted blend (X360 ARTIST @ 0x823F35B8): every scalar and
    // every lane of the Vector2/Vector4 members is lpA*lfWa + lpB*lfWb. Returns lpResult.
    static VignetteData* SetToBlend(VignetteData* lpResult,
                                    const VignetteData* lpA, f32 lfWa, f32 lfWb,
                                    const VignetteData* lpB);
};

struct DepthOfFieldData
{
    static const f32 kfDefNearPlane;
    static const f32 kfDefFocalPlane;
    static const f32 kfDefFocalPlane2;
    static const f32 kfDefFarPlane;

    f32 mfNearPlane;
    f32 mfFocalPlane;
    f32 mfFocalPlane2;
    f32 mfFarPlane;
    f32 mfDofAmount;
    u8  mPad14[12];

    void Construct()
    {
        mfNearPlane = KF_DEF_DEPTH_OF_FIELD_NEAR_PLANE;
        mfFocalPlane = KF_DEF_DEPTH_OF_FIELD_FOCAL_PLANE;
        mfFocalPlane2 = KF_DEF_DEPTH_OF_FIELD_FOCAL_PLANE2;
        mfFarPlane = KF_DEF_DEPTH_OF_FIELD_FAR_PLANE;
        mfDofAmount = KF_DEF_DEPTH_OF_FIELD_AMOUNT;
    }
};

struct BlurData
{
    static const f32 kfDefOpacity;
    static const f32 kfDefVelocity;
    static const f32 kfDefSharpness;
    static const f32 kfDefNoise;
    static const f32 kfDefAngle;
    static const Vector2 kv2DefBlendAmount;
    static const Vector2 kv2DefBlurAmount;
    static const Vector2 kv2DefBlendCentre;
    static const Vector2 kv2DefBlurCentre;

    f32 mfOpacity;
    f32 mfVelocity;
    f32 mfSharpness;
    f32 mfNoise;
    f32 mfAngle;
    u8  mPad14[12];
    Vector2 mv2BlendAmount;
    Vector2 mv2BlurAmount;
    Vector2 mv2BlendCentre;
    Vector2 mv2BlurCentre;

    void Construct()
    {
        mfOpacity = KF_DEF_BLUR_OPACITY;
        mfVelocity = KF_DEF_BLUR_VELOCITY;
        mfSharpness = KF_DEF_BLUR_SHARPNESS;
        mfNoise = KF_DEF_BLUR_NOISE;
        mfAngle = KF_DEF_BLUR_ANGLE;
        mv2BlendAmount = kv2DefBlendAmount;
        mv2BlurAmount = kv2DefBlurAmount;
        mv2BlendCentre = kv2DefBlendCentre;
        mv2BlurCentre = kv2DefBlurCentre;
    }

    // 2-way per-member weighted blend (X360 ARTIST @ 0x823F3A50): every scalar and
    // every lane of the Vector2 members is lpA*lfWa + lpB*lfWb. Returns lpResult.
    static BlurData* SetToBlend(BlurData* lpResult,
                                const BlurData* lpA, f32 lfWa, f32 lfWb,
                                const BlurData* lpB);
};

struct TintData
{
    u32 muColourCube;

    void Construct()
    {
        muColourCube = 0;
    }
};

struct TintData2d
{
    static const Vector4 kv4DefaultColour;

    Vector4 mv4Colour;

    void Construct()
    {
        mv4Colour = kv4DefaultColour;
    }
};
}

namespace BrnDirector
{
namespace Camera
{
struct MotionBlurData
{
    f32  mfCarsBlurAmount;
    f32  mfWorldBlurAmount;
    bool mbIsActive;
    bool mbIsExpensiveMotionBlur;
    u8   mPadA[2];

    void Construct();
    static MotionBlurData Interpolate(const MotionBlurData& lLhs, const MotionBlurData& lRhs, f32 lfT);
};
}
}

class BrnEffectsFrame
{
public:
    void Construct();

    // Read accessors used by BrnGraphics::EffectsArbitrator's per-frame eval passes
    // (additive, inline -- do not change the layout). The X360 EvalTint @0x823FD570
    // reads mbUseTint (frame byte +4), mfTintWeight (f32 +0x18) and
    // mTintData.muColourCube (u32 +0x110).
    bool                       GetUseTint() const        { return mbUseTint; }
    f32                        GetTintWeight() const      { return mfTintWeight; }
    const BrnEffects::TintData& GetTintData() const       { return mTintData; }

private:
    bool mbUseBloom;
    bool mbUseVignette;
    bool mbUseDepthOfField;
    bool mbUseBlur;
    bool mbUseTint;
    bool mbUseTint2d;
    u8   mPad6[2];

    f32 mfBloomWeight;
    f32 mfVignetteWeight;
    f32 mfDepthOfFieldWeight;
    f32 mfBlurWeight;
    f32 mfTintWeight;
    f32 mf2dTintWeight;

    BrnEffects::BloomData        mBloomData;
    BrnEffects::VignetteData     mVignetteData;
    BrnEffects::DepthOfFieldData mDepthOfFieldData;
    BrnEffects::BlurData         mBlurData;
    BrnEffects::TintData         mTintData;
    u8                           mPad114[12];
    BrnEffects::TintData2d       mTintData2d;
    Matrix44Affine               mCarTransform;
    Matrix44Affine               mCameraTransform;
    Vector4            mLinearVelocity;
    Vector4            mAngularVelocity;
    f32                          mfSpeedMPH;
    f32                          mfSteering;
    BrnDirector::Camera::MotionBlurData mMotionBlurData;
    bool                         mbIsGameCamera;
};

static_assert(sizeof(Vector2) == 16, "Vector2 layout drift");
static_assert(sizeof(Vector4) == 16, "Vector4 layout drift");
static_assert(sizeof(Matrix44Affine) == 64, "Matrix44Affine layout drift");
static_assert(sizeof(BrnEffects::BloomData) == 32, "BloomData layout drift");
static_assert(sizeof(BrnEffects::VignetteData) == 80, "VignetteData layout drift");
static_assert(sizeof(BrnEffects::DepthOfFieldData) == 32, "DepthOfFieldData layout drift");
static_assert(sizeof(BrnEffects::BlurData) == 96, "BlurData layout drift");
static_assert(sizeof(BrnEffects::TintData) == 4, "TintData layout drift");
static_assert(sizeof(BrnEffects::TintData2d) == 16, "TintData2d layout drift");
static_assert(sizeof(BrnDirector::Camera::MotionBlurData) == 12, "MotionBlurData layout drift");

inline void BrnEffectsFrame::Construct()
{
    mbUseBloom = false;
    mbUseVignette = false;
    mbUseDepthOfField = false;
    mbUseBlur = false;
    mbUseTint = false;
    mbUseTint2d = false;

    mfBloomWeight = 0.0f;
    mfVignetteWeight = 0.0f;
    mfDepthOfFieldWeight = 0.0f;
    mfBlurWeight = 0.0f;
    mfTintWeight = 0.0f;
    mf2dTintWeight = 0.0f;

    mBloomData.Construct();
    mVignetteData.Construct();
    mDepthOfFieldData.Construct();
    mBlurData.Construct();
    mTintData.Construct();
    mTintData2d.Construct();
    mMotionBlurData.Construct();
}
