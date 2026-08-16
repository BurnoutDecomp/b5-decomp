#pragma once

#include "BrnCommonTypes.h"   // Vector2, Vector3, Vector4, Matrix44Affine (shared rw::math::vpu types)

// Declaration shape for the six data structs and BrnEffectsFrame: the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/SharedClasses/Graphics/BrnEffectsData.h:84..321) and, for
// BrnEffectsFrame's accessor surface, the Feb-2007 header
// references/Feb-2007/BrnEntityModuleUnity/GameSource/Effects/BrnEffectsFrame.h (bodies
// copied verbatim from it). Member ORDER/offsets stay pinned by the X360 asm, which is
// unchanged by this file's 2026-08-15 edit -- see the static_asserts at the bottom.

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

// ====================================================================================
// ⭐ SetToBlend IS A NON-STATIC MEMBER TAKING (A, wA, B, wB [, C, wC, D, wD]).
// Corrected 2026-08-15 (postfx step-4 bloom wave, arbitrator group). The previous
// declaration -- `static T* SetToBlend(T* lpResult, const T* lpA, f32 wa, f32 wb, ...,
// const T* lpB, ...)` -- had the weights BUNCHED and the sources after them. Three
// independent witnesses say the real shape interleaves them:
//
//  1. DWARF (rung 2, and it is the DECLARATION authority):
//       BrnEffectsData.h:113  void BloomData::SetToBlend(const BloomData&, float32_t,
//                                                        const BloomData&, float32_t);
//       BrnEffectsData.h:120  ...(const BloomData&, f32, const BloomData&, f32,
//                                 const BloomData&, f32, const BloomData&, f32);
//     -- non-static (no explicit result parameter), `void`, and alternating.
//  2. X360 asm register usage. A float argument consumes (skips) its GPR slot, so an
//     interleaved list lands the pointers in r4/r6/r8/r10 and the weights in f1..f4,
//     whereas the bunched form would use r4 then r9/r10/r11. Every body uses the former:
//       BloomData::SetToBlend @0x823F34B0    lfs 0(r4)/0(r6)/0(r8)/0(r10), f1..f4
//       VignetteData::SetToBlend @0x823F35B8 lvx r4/r6 at 0x10,0x20,0x30,0x40, f1,f2
//       BlurData::SetToBlend @0x823F3A50     lvx r4/r6 at 0x20,0x30,0x40,0x50, f1,f2
//       sub_823F3758 (VignetteData 4-way)    r4/r6/r8/r10, f1..f4
//       sub_823F3C30 (BlurData 4-way)        r4/r6/r8/r10, f1..f4
//     and the call sites match: EvalEffectData<BloomData> @0x823F9BE0-0x823F9C04 sets
//     r3=out, r4=slot0, r6=slot1, r8=slot2, r10=slot3 with f1..f4 = the four weights;
//     EvalEffectData<VignetteData> @0x823FA020 does `mr r3,r17 / mr r4,r17 / fsubs f1 /
//     addi r6,&lAlt / fmr f2` -- `lRes.SetToBlend(lRes, 1-w, lAlt, w)` with r3 == r4,
//     i.e. the result IS `this`.
//  3. The tree already spells the sibling family this way: BrnEnvCloudsData.h:23/:25
//     `void SetToBlend(const CloudsData& lValueA, float lfWeightA, ...)`.
// The 2-way/4-way pairs for DepthOfFieldData and TintData2d (DWARF :210/:217 and
// :314/:321) exist in the source but were INLINED by the X360 compiler into
// EvalEffectData<DepthOfFieldData> @0x823FA060 and <TintData2d> @0x823FA630; their bodies
// below are the de-optimised form of those inline expansions, member for member.
// ====================================================================================

struct BloomData
{
    static const f32 kfDefLuminance;      // flt_820A3A14 == 0.98f
    static const f32 kfDefThreshold;      // flt_820A3A98 == 0.77f
    static const Vector4 kv4DefScale;     // unk_82FFAED0  (defined in BrnEffectsData.cpp from the CRT static-init dump)

    f32 mfLuminance;
    f32 mfThreshold;
    Vector4 mv4Scale;

    void Construct()
    {
        mfLuminance = KF_DEF_BLOOM_LUMINANCE;
        mfThreshold = KF_DEF_BLOOM_THRESHOLD;
        mv4Scale = kv4DefScale;
    }

    // 2-way per-member weighted blend (DWARF BrnEffectsData.h:113). Inlined by the X360
    // compiler; recovered from EvalEffectData<BloomData> @0x823F9C0C-0x823F9CB4.
    void SetToBlend(const BloomData& lA, f32 lfWa,
                    const BloomData& lB, f32 lfWb);

    // 4-way per-member weighted blend (DWARF :120, X360 @0x823F34B0). Each scalar and each
    // lane of mv4Scale is the linear combination of the four sources.
    void SetToBlend(const BloomData& lA, f32 lfWa,
                    const BloomData& lB, f32 lfWb,
                    const BloomData& lC, f32 lfWc,
                    const BloomData& lD, f32 lfWd);
};

struct VignetteData
{
    static const f32 kfDefAngle;              // flt_820A3A9C
    static const f32 kfDefSharpness;          // flt_820A3AA0
    static const Vector2 kv2DefAmount;        // unk_82FFAE20  (defined in BrnEffectsData.cpp from the CRT static-init dump)
    static const Vector2 kv2DefCentre;        // unk_82FFAEC0  (defined in BrnEffectsData.cpp from the CRT static-init dump)
    static const Vector4 kv4DefInnerColour;   // unk_82FFB220  (defined in BrnEffectsData.cpp from the CRT static-init dump)
    static const Vector4 kv4DefOuterColour;   // unk_82FFB1A0  (defined in BrnEffectsData.cpp from the CRT static-init dump)

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

    // DWARF :161, X360 @0x823F35B8.
    void SetToBlend(const VignetteData& lA, f32 lfWa,
                    const VignetteData& lB, f32 lfWb);
    // DWARF :168, X360 sub_823F3758 (called by EvalEffectData<VignetteData> @0x823F9F6C).
    void SetToBlend(const VignetteData& lA, f32 lfWa,
                    const VignetteData& lB, f32 lfWb,
                    const VignetteData& lC, f32 lfWc,
                    const VignetteData& lD, f32 lfWd);
};

struct DepthOfFieldData
{
    static const f32 kfDefNearPlane;     // flt_820A3AA4
    static const f32 kfDefFocalPlane;    // flt_820A3AA8
    static const f32 kfDefFocalPlane2;   // flt_820A3AAC
    static const f32 kfDefFarPlane;      // flt_820A3AB0

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

    // DWARF :210 / :217. Both were inlined on X360 -- recovered from
    // EvalEffectData<DepthOfFieldData> @0x823FA230 (2-way) and @0x823FA150 (4-way).
    // ⭐ They blend the FOUR PLANES ONLY: neither expansion touches mfDofAmount (the 2-way
    // reads/writes 0x90,0x94,0x98,0x9C of the frame and nothing at 0xA0), and the fold-in
    // at 0x823FA31C likewise writes only lRes+0,+4,+8,+0xC.
    void SetToBlend(const DepthOfFieldData& lA, f32 lfWa,
                    const DepthOfFieldData& lB, f32 lfWb);
    void SetToBlend(const DepthOfFieldData& lA, f32 lfWa,
                    const DepthOfFieldData& lB, f32 lfWb,
                    const DepthOfFieldData& lC, f32 lfWc,
                    const DepthOfFieldData& lD, f32 lfWd);
};

struct BlurData
{
    static const f32 kfDefOpacity;             // flt_820A3AB4
    static const f32 kfDefVelocity;            // flt_820A3AB8
    static const f32 kfDefSharpness;           // flt_820A3ABC
    static const f32 kfDefNoise;               // flt_820A3AC0
    static const f32 kfDefAngle;               // flt_820A3AC4
    static const Vector2 kv2DefBlendAmount;    // unk_82FFAFC0  (defined in BrnEffectsData.cpp from the CRT static-init dump)
    static const Vector2 kv2DefBlurAmount;     // unk_82FFAE90  (defined in BrnEffectsData.cpp from the CRT static-init dump)
    static const Vector2 kv2DefBlendCentre;    // unk_82FFB030  (defined in BrnEffectsData.cpp from the CRT static-init dump)
    static const Vector2 kv2DefBlurCentre;     // unk_82FFB180  (defined in BrnEffectsData.cpp from the CRT static-init dump)

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

    // DWARF :260, X360 @0x823F3A50.
    void SetToBlend(const BlurData& lA, f32 lfWa,
                    const BlurData& lB, f32 lfWb);
    // DWARF :267, X360 sub_823F3C30 (called by EvalEffectData<BlurData> @0x823FA540).
    void SetToBlend(const BlurData& lA, f32 lfWa,
                    const BlurData& lB, f32 lfWb,
                    const BlurData& lC, f32 lfWc,
                    const BlurData& lD, f32 lfWd);
};

struct TintData
{
    // ⚠ SUSPECT (raised 2026-08-15, NOT changed this wave): DWARF BrnEffectsData.h:275 types
    // this member `rw::graphics::postfx::ColourCube* mpColourCube` -- a real pointer, 4 bytes
    // on the console and 8 on the host. The tree models it as the raw guest word, which forces
    // EvalTint to do exactly the guest-int -> host-pointer cast PREAMBLE rule 1 forbids.
    // Widening it is layout-neutral here (TintData sits at frame +0x110 with 12 bytes of pad
    // before TintData2d at +0x120, so an 8-byte pointer + 8 pad still lands on +0x120), but it
    // changes what the world group's GenerateEffects writes this wave, so it is left alone and
    // listed as a follow-up.
    u32 muColourCube;

    void Construct()
    {
        muColourCube = 0;
    }
};

struct TintData2d
{
    static const Vector4 kv4DefaultColour;   // unk_82FFB230  (defined in BrnEffectsData.cpp from the CRT static-init dump)

    Vector4 mv4Colour;

    void Construct()
    {
        mv4Colour = kv4DefaultColour;
    }

    // DWARF :314 / :321. Both inlined on X360 -- recovered from
    // EvalEffectData<TintData2d> @0x823FA7F8 (2-way) and @0x823FA700 (4-way).
    void SetToBlend(const TintData2d& lA, f32 lfWa,
                    const TintData2d& lB, f32 lfWb);
    void SetToBlend(const TintData2d& lA, f32 lfWa,
                    const TintData2d& lB, f32 lfWb,
                    const TintData2d& lC, f32 lfWc,
                    const TintData2d& lD, f32 lfWd);
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

    // BrnDirector::Camera::MotionBlurData::Set @0x8220AED8 -- DWARF BrnCameraEffects.h:56,
    // `void Set(bool, bool, float32_t, float32_t)`: the two flags FIRST, then the two amounts
    // (a float argument SKIPS its GPR slot on PPC, so the asm's r4/r5 + f1/f2 IS that list).
    // It has exactly ONE caller in the whole image -- the effects module's base-frame producer,
    // BrnEffects::EffectsModule::GenerateRenderRequests @0x8227FF10 -- which is why it lands now:
    //   $ grep -rl '"name": "BrnDirector::Camera::MotionBlurData::Set"' \
    //         .ida-exports/BURNOUT_X360_ARTIST.XEX/
    //   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8227FF10.json   (the call site)
    //   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8220AED8.json   (the body)
    // Body: GameSource/Director/Camera/BrnCameraEffects.cpp (this type's compilation home).
    void Set(bool lbIsActive, bool lbIsExpensiveMotionBlur,
             f32 lfCarsBlurAmount, f32 lfWorldBlurAmount);

    // The four DWARF read accessors (BrnCameraEffects.h:59/:62/:65/:68). NONE of them exists as a
    // standalone X360 function -- the compiler inlined every use:
    //   $ grep -rlo "MotionBlurData::GetCarsBlendAmount\|MotionBlurData::GetWorldBlendAmount\|\
    //         MotionBlurData::IsActive\|MotionBlurData::IsExpensiveMotionBlur" \
    //         .ida-exports/BURNOUT_X360_ARTIST.XEX/
    //   (no hits)
    // -- so they are header inlines over the members Construct/Set already pin store-for-store,
    // exactly like the CameraEffects::GetShakeAmplitude family. They exist so the base-frame
    // producer reads the camera record BY NAME instead of by displacement.
    f32  GetCarsBlendAmount() const    { return mfCarsBlurAmount; }
    f32  GetWorldBlendAmount() const   { return mfWorldBlurAmount; }
    bool IsActive() const              { return mbIsActive; }
    bool IsExpensiveMotionBlur() const { return mbIsExpensiveMotionBlur; }
};
}
}

// BrnEffectsFrame -- one producer's post-FX contribution for one layer/slot for one frame.
// Accessor surface and bodies: Feb-2007 GameSource/Effects/BrnEffectsFrame.h, verbatim
// (that header declares every Set*/Get* below `inline` in the class). The layout is the
// X360 one and is untouched; the offsets each accessor compiles to are pinned by
// BrnRendererModule::Render @0x8240BFA8 and the five EvalEffectData bodies -- see the
// per-specialisation asm citations in GameSource/Graphics/BrnEffectsArbitrator.cpp.
class BrnEffectsFrame
{
public:
    void Construct();

    inline void SetUseBloom(bool lbUseBloom)                 { mbUseBloom = lbUseBloom; }
    inline void SetUseVignette(bool lbUseVignette)           { mbUseVignette = lbUseVignette; }
    inline void SetUseDepthOfField(bool lbUseDepthOfField)   { mbUseDepthOfField = lbUseDepthOfField; }
    inline void SetUseBlur(bool lbUseBlur)                   { mbUseBlur = lbUseBlur; }
    inline void SetUseTint(bool lbUseTint)                   { mbUseTint = lbUseTint; }
    inline void SetUseTint2d(bool lbUseTint2d)               { mbUseTint2d = lbUseTint2d; }

    inline void SetBloomData(const BrnEffects::BloomData& lData, f32 lfWeight = 1.0f)
    { mBloomData = lData; mfBloomWeight = lfWeight; }
    inline void SetVignetteData(const BrnEffects::VignetteData& lData, f32 lfWeight = 1.0f)
    { mVignetteData = lData; mfVignetteWeight = lfWeight; }
    inline void SetDepthOfFieldData(const BrnEffects::DepthOfFieldData& lData, f32 lfWeight = 1.0f)
    { mDepthOfFieldData = lData; mfDepthOfFieldWeight = lfWeight; }
    inline void SetBlurData(const BrnEffects::BlurData& lData, f32 lfWeight = 1.0f)
    { mBlurData = lData; mfBlurWeight = lfWeight; }
    inline void SetTintData(const BrnEffects::TintData& lData, f32 lfWeight = 1.0f)
    { mTintData = lData; mfTintWeight = lfWeight; }
    inline void SetTintData2d(const BrnEffects::TintData2d& lData, f32 lfWeight = 1.0f)
    { mTintData2d = lData; mf2dTintWeight = lfWeight; }

    inline void SetLinearVelocity(Vector3::InParam lvLinear)     { mLinearVelocity = lvLinear; }
    inline void SetAngularVelocity(Vector3::InParam lvAngular)   { mAngularVelocity = lvAngular; }
    inline void SetSpeedMPH(f32 lSpeed)                          { mfSpeedMPH = lSpeed; }
    inline void SetSteering(f32 lSteering)                       { mfSteering = lSteering; }
    inline void SetCarTransform(Matrix44Affine lTransform)       { mCarTransform = lTransform; }
    inline void SetIsRacingGameplayCamera(bool lGameCamera)      { mbIsGameCamera = lGameCamera; }
    inline void SetCameraTransform(Matrix44Affine lTransform)    { mCameraTransform = lTransform; }

    inline bool GetUseBloom() const          { return mbUseBloom; }
    inline bool GetUseVignette() const       { return mbUseVignette; }
    inline bool GetUseDepthOfField() const   { return mbUseDepthOfField; }
    inline bool GetUseBlur() const           { return mbUseBlur; }
    inline bool GetUseTint() const           { return mbUseTint; }
    inline bool GetUseTint2d() const         { return mbUseTint2d; }

    inline f32 GetBloomWeight() const        { return mfBloomWeight; }
    inline f32 GetVignetteWeight() const     { return mfVignetteWeight; }
    inline f32 GetDepthOfFieldWeight() const { return mfDepthOfFieldWeight; }
    inline f32 GetBlurWeight() const         { return mfBlurWeight; }
    inline f32 GetTintWeight() const         { return mfTintWeight; }
    inline f32 Get2dTintWeight() const       { return mf2dTintWeight; }

    inline const BrnEffects::BloomData&        GetBloomData() const        { return mBloomData; }
    inline const BrnEffects::VignetteData&     GetVignetteData() const     { return mVignetteData; }
    inline const BrnEffects::DepthOfFieldData& GetDepthOfFieldData() const { return mDepthOfFieldData; }
    inline const BrnEffects::BlurData&         GetBlurData() const         { return mBlurData; }
    inline const BrnEffects::TintData&         GetTintData() const         { return mTintData; }
    inline const BrnEffects::TintData2d&       GetTintData2d() const       { return mTintData2d; }

    inline Vector3        GetLinearVelocity() const        { return mLinearVelocity; }
    inline Vector3        GetAngularVelocity() const       { return mAngularVelocity; }
    inline f32            GetSpeedMPH() const              { return mfSpeedMPH; }
    inline f32            GetSteering() const              { return mfSteering; }
    inline Matrix44Affine GetCarTransform() const          { return mCarTransform; }
    inline bool           GetIsRacingGameplayCamera() const{ return mbIsGameCamera; }
    inline Matrix44Affine GetCameraTransform() const       { return mCameraTransform; }

    // Post-Feb-2007 pair (the frame gained a MotionBlurData; EffectsArbitrator gained
    // GetMotionBlurData, DWARF BrnEffectsArbitrator.h:125). The three fields are read at
    // frame +0x1D8 / +0x1DC / +0x1E0 by BrnRendererModule::Render @0x8240BFA8 lines 398-401:
    //   v293 = *(496*internal + mapaEffectsFrames[0] + 472) * 255.0   -> mfCarsBlurAmount
    //   v294 = *(496*internal + mapaEffectsFrames[0] + 476) * 255.0   -> mfWorldBlurAmount
    //   v292 = *(496*internal + mapaEffectsFrames[0] + 480)           -> mbIsActive
    // The getter is attested by that read; the setter mirrors the Feb-2007 Set* family and is
    // INFERRED (no console producer for it is live on PC).
    inline const BrnDirector::Camera::MotionBlurData& GetMotionBlurData() const
    { return mMotionBlurData; }
    inline void SetMotionBlurData(const BrnDirector::Camera::MotionBlurData& lData)
    { mMotionBlurData = lData; }

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
    // Feb-2007 + DWARF type these two `rw::math::Vector3`; the tree had them as Vector4.
    // Same 16-byte alignas(16) storage, so the layout is unchanged -- the change is so the
    // arbitrator's `Vector3 GetLinearVelocity() const` (DWARF BrnEffectsArbitrator.h:110)
    // has the type it is declared with.
    Vector3            mLinearVelocity;
    Vector3            mAngularVelocity;
    f32                          mfSpeedMPH;
    f32                          mfSteering;
    BrnDirector::Camera::MotionBlurData mMotionBlurData;
    bool                         mbIsGameCamera;
};

static_assert(sizeof(Vector2) == 16, "Vector2 layout drift");
static_assert(sizeof(Vector3) == 16, "Vector3 layout drift");
static_assert(sizeof(Vector4) == 16, "Vector4 layout drift");
static_assert(sizeof(Matrix44Affine) == 64, "Matrix44Affine layout drift");
static_assert(sizeof(BrnEffects::BloomData) == 32, "BloomData layout drift");
static_assert(sizeof(BrnEffects::VignetteData) == 80, "VignetteData layout drift");
static_assert(sizeof(BrnEffects::DepthOfFieldData) == 32, "DepthOfFieldData layout drift");
static_assert(sizeof(BrnEffects::BlurData) == 96, "BlurData layout drift");
static_assert(sizeof(BrnEffects::TintData) == 4, "TintData layout drift");
static_assert(sizeof(BrnEffects::TintData2d) == 16, "TintData2d layout drift");
static_assert(sizeof(BrnDirector::Camera::MotionBlurData) == 12, "MotionBlurData layout drift");
// The frame stride the whole subsystem is addressed with: every X360 frame address is
// `496 * (2*slot + internal) + mapaEffectsFrames[layer]` (e.g. EvalTint @0x823FD614
// `mulli r10, r10, 0x1F0`). Pin it so a member edit that changes the stride fails here
// instead of silently mis-indexing.
static_assert(sizeof(BrnEffectsFrame) == 496, "BrnEffectsFrame stride drift (X360 0x1F0)");

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
