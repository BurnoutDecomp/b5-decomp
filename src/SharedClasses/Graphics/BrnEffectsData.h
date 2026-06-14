#pragma once

#include "types.hpp"

namespace rw
{
namespace math
{
struct alignas(16) Vector2
{
    f32 mafLane[4];
};

struct alignas(16) Vector4
{
    f32 mafLane[4];
};
}
}

struct Matrix44Affine
{
    rw::math::Vector4 mRows[4];
};

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
    static const rw::math::Vector4 kv4DefScale;

    f32 mfLuminance;
    f32 mfThreshold;
    rw::math::Vector4 mv4Scale;

    void Construct()
    {
        mfLuminance = KF_DEF_BLOOM_LUMINANCE;
        mfThreshold = KF_DEF_BLOOM_THRESHOLD;
        mv4Scale = kv4DefScale;
    }
};

struct VignetteData
{
    static const f32 kfDefAngle;
    static const f32 kfDefSharpness;
    static const rw::math::Vector2 kv2DefAmount;
    static const rw::math::Vector2 kv2DefCentre;
    static const rw::math::Vector4 kv4DefInnerColour;
    static const rw::math::Vector4 kv4DefOuterColour;

    f32 mfAngle;
    f32 mfSharpness;
    rw::math::Vector2 mv2Amount;
    rw::math::Vector2 mv2Centre;
    rw::math::Vector4 mv4InnerColour;
    rw::math::Vector4 mv4OuterColour;

    void Construct()
    {
        mfAngle = KF_DEF_VIGNETTE_ANGLE;
        mfSharpness = KF_DEF_VIGNETTE_SHARPNESS;
        mv2Amount = kv2DefAmount;
        mv2Centre = kv2DefCentre;
        mv4InnerColour = kv4DefInnerColour;
        mv4OuterColour = kv4DefOuterColour;
    }
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
    static const rw::math::Vector2 kv2DefBlendAmount;
    static const rw::math::Vector2 kv2DefBlurAmount;
    static const rw::math::Vector2 kv2DefBlendCentre;
    static const rw::math::Vector2 kv2DefBlurCentre;

    f32 mfOpacity;
    f32 mfVelocity;
    f32 mfSharpness;
    f32 mfNoise;
    f32 mfAngle;
    u8  mPad14[12];
    rw::math::Vector2 mv2BlendAmount;
    rw::math::Vector2 mv2BlurAmount;
    rw::math::Vector2 mv2BlendCentre;
    rw::math::Vector2 mv2BlurCentre;

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
    static const rw::math::Vector4 kv4DefaultColour;

    rw::math::Vector4 mv4Colour;

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
    rw::math::Vector4            mLinearVelocity;
    rw::math::Vector4            mAngularVelocity;
    f32                          mfSpeedMPH;
    f32                          mfSteering;
    BrnDirector::Camera::MotionBlurData mMotionBlurData;
    bool                         mbIsGameCamera;
};

static_assert(sizeof(rw::math::Vector2) == 16, "Vector2 layout drift");
static_assert(sizeof(rw::math::Vector4) == 16, "Vector4 layout drift");
static_assert(sizeof(Matrix44Affine) == 64, "Matrix44Affine layout drift");
static_assert(sizeof(BrnEffects::BloomData) == 32, "BloomData layout drift");
static_assert(sizeof(BrnEffects::VignetteData) == 80, "VignetteData layout drift");
static_assert(sizeof(BrnEffects::DepthOfFieldData) == 32, "DepthOfFieldData layout drift");
static_assert(sizeof(BrnEffects::BlurData) == 96, "BlurData layout drift");
static_assert(sizeof(BrnEffects::TintData) == 4, "TintData layout drift");
static_assert(sizeof(BrnEffects::TintData2d) == 16, "TintData2d layout drift");
static_assert(sizeof(BrnDirector::Camera::MotionBlurData) == 12, "MotionBlurData layout drift");

void BrnEffectsFrame::Construct()
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
