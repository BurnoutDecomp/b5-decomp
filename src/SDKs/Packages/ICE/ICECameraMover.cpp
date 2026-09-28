// ============================================================================
// SDKs/Packages/ICE/ICECameraMover.cpp
//
// Runtime bodies for ICE::ICECameraMover -- the system that drives the in-game (ICE)
// camera from a recorded camera take -- and the two ICECameraAnchor queries it reads the
// car through. The type layout (member NAMES/OFFSETS) lives in the home
// GameSource/Director/Camera/ICECameraMover.h; members are accessed BY NAME here.
//
// ⭐⭐ REWRITTEN 2026-09-27 (OWNERLIST lane L5 -- the pause camera) FROM THE X360 ARTIST WORDS,
// function by function, and MOUNTED. The previous body was a paraphrase that had never been in
// the link (18 unresolved leaves) and was wrong wherever it was specific: the bloom threshold was
// clamped the wrong way round, the depth-of-field band was handed to the camera in the wrong
// argument order, the lens FOV went through a helper that did not exist, the forward follower
// normalised the car's forward instead of the console's zero vector, and the dutch roll was an
// unbodied placeholder. Its only caller on the console is the ICE movie playback that the pause
// camera (ArbStateCrashNav's looping pause playlist) and every other director ICE movie run on:
// ICEWrapper::Update @0x82540180 -> Update(1.0f) -> UpdateFrameBegin / UpdateFrameEnd.
//
// The per-element take reads are the console's inlined value-type test + mValues[] load, which
// is exactly ICETake::GetValueFloat(s32) @0x8252C0B8 (float element: the stored word; any other
// type: its s32 converted -- extsw / fcfid / frsp, one rounding); the integer reads are real
// `bl ICETake::GetValueInt` calls. Element indices are ICEElementDescriptions[] slots
// (SDKs/Packages/ICE/ICEData.cpp names each one).
//
// ROUNDING (scratch/CRASHPARITY_0922/ROUNDING_RULE.md): scalar fmuls / fadds / fsubs round once each
// (rule 4); vmaddfp / vnmsubfp lanes are FUSED (rule 3, std::fma); vmsum3fp128 is one rounding of
// the f64 sum (rule 1); the vrsqrtefp estimate is modelled as the correctly rounded 1/sqrt (rule 5).
// Every fsel clamp is spelled through rw::math::fpu::Min / Max / Clamp, which ARE the fsel forms
// (the DWARF names those same helpers at these sites).
// ============================================================================

#include "GameSource/Director/Camera/ICECameraMover.h"   // the mover home (this type)

#include "SDKs/Packages/ICE/ICEData.hpp"                    // ICE::ICETake value / interval queries
#include "SDKs/Packages/ICE/ICEDataEnums.hpp"               // ICE::ICEElementDescriptions (the lens range), eICESpace
#include "SDKs/Packages/ICE/ICEMath.hpp"                    // ConvertLensLengthToFovAngle / Angles::AngToDeg
#include "GameSource/Director/Camera/Utils/CameraUtils.h"   // BrnDirector::Camera::Utils::CreateLookAt (both forms)
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h" // ConsoleVpu::Dot3 (vmsum3fp128, rule 1)
#include "SDKs/XboxMath/XMVectorSinCos.h"                   // the XDK sine / cosine the dutch roll inlines
#include "rw/math/fpu/scalar_operation.h"                   // rw::math::fpu::Min / Max / Clamp (the fsel forms)
#include <cmath>                                            // std::fma / std::sqrt

namespace ICE
{

namespace
{
    // The ICEElementDescriptions[] slots the mover samples (the table's own mpTag names).
    enum EICEElement
    {
        E_ICE_EYE_X             = 0,
        E_ICE_EYE_Y             = 1,
        E_ICE_EYE_Z             = 2,
        E_ICE_LOOK_X            = 3,
        E_ICE_LOOK_Y            = 4,
        E_ICE_LOOK_Z            = 5,
        E_ICE_DUTCH             = 6,
        E_ICE_LENS_LENGTH       = 9,
        E_ICE_NEAR_FOCUS        = 12,
        E_ICE_FAR_FOCUS         = 13,
        E_ICE_BLUR_FALLOFF      = 14,
        E_ICE_BLUR_INTENSITY    = 15,
        E_ICE_TIME_SCALE        = 19,
        E_ICE_FADE              = 21,
        E_ICE_SPACE_EYE         = 30,
        E_ICE_SPACE_LOOK        = 31,
        E_ICE_RAWFOCUS_OVERRIDE = 39,
        E_ICE_EVENT_TAG         = 41,
        E_ICE_OVERLAY           = 42,
        E_ICE_FADE_TO_COLOR     = 43
    };

    // The hard-cut test runs on channel 0 (MAIN), edge 0 (UpdateHardCuts: `lhz 0xD8(take)` ==
    // mChannels[0] + 4, `li r5, 0`).
    const s32 KI_HARD_CUT_CHANNEL = 0;
    const s32 KI_HARD_CUT_ELEMENT = 0;

    // The constants, read from the image (x360rd):
    const f32 KF_PERCENT_TO_UNIT      = 0.01f;         // flt_82002138 == 0x3C23D70A (also SimTime's floor)
    const f32 KF_BLOOM_THRESHOLD_BASE = 0.2f;          // flt_82004744 == 0x3E4CCCCD
    const f32 KF_BLOOM_THRESHOLD_GAIN = -2.0f;         // flt_82006D70 == 0xC0000000
    const f32 KF_BLOOM_LUMINANCE_GAIN = 0.02f;         // flt_82005574 == 0x3CA3D70A
    const f32 KF_TWO_PI               = 6.2831855f;    // flt_82001C94 == 0x40C90FDB (the dutch is authored in turns)
    const f32 KF_DOT_FLIP             = -2.0f;         // flt_82006D70 == 0xC0000000 (UpdateForwardVector)

    // ICECameraMover.cpp:18 / :19 (DWARF names): flt_8207AC98 == 0x42480000, flt_8207AC9C == 0x3A83126F.
    const f32 KI_DEPTH_OF_FIELD_MAX_FALLOFF_DISTANCE = 50.0f;
    const f32 KI_DEPTH_OF_FIELD_MIN_FALLOFF_DISTANCE = 0.001f;

    // The take's value for an element, as a float (the console's inlined type test).
    inline f32 GetElementFloat(const ICETake* lpTake, EICEElement leElement)
    {
        return lpTake->GetValueFloat(static_cast<s32>(leElement));
    }

    // The reference space an integer SPACE element names: `clrlwi r5, r3, 24` -- the low byte.
    inline eICESpace ToSpace(s32 liSpace)
    {
        return static_cast<eICESpace>(static_cast<u8>(liSpace));
    }
}

// ============================================================================
// ICECameraAnchor::GetGeometryPosition / GetForwardVector (DWARF ICECameraMover.cpp:86 / :102)
//
// No ARTIST symbol: both expand to one row load off the anchor, whose first member is the
// CameraSpaceHandler and whose first matrix is mCarToWorld. The position is the w row (`lvx128
// v13, r11, 0x30` in Construct @0x8252B934 and UpdateHardCuts @0x82531B84), the forward the z row
// (`lvx128 v0, r11, 0x20` @0x8252B930, `lvx128 v10, r9, 0x20` in UpdateForwardVector @0x82533C4C).
// ============================================================================
Vector3 ICECameraAnchor::GetGeometryPosition()
{
    return mSpace.GetCarToWorld().wAxis;
}

Vector3 ICECameraAnchor::GetForwardVector()
{
    return mSpace.GetCarToWorld().zAxis;
}

// ============================================================================
// ICECameraMover::Construct @0x8252B700 (DWARF ICECameraMover.cpp:190)
//
// Store order is the asm's: mpCar (r5), mfSimTime = 1.0 (flt_82001C98), mpTake (r7),
// miHardCutInterval = -1, muOldTag = 0, mpICECamera (r6). The view index, shake group and
// resource manager (r4, r8, r9) are not read.
// The two Cubic3D followers are each a 132-byte stack image copied in by memcpy (0x8252B7E8,
// 0x8252B888): every Cubic1D lane zero except duration = 1.0, state 0, and flags 1 in the
// accel-offset block but 0 in the forward block. The world->camera matrix is the rows
// (1,0,0,0) (0,1,0,0) (0,0,1,0) (0,0,0,0) -- the fourth stack row is four zero words
// (0x8252B88C..0x8252B920; the DWARF's Matrix44Affine::SetIdentity, whose w row is zero). Then the
// camera's overlay byte (+0x04) is set to 1 (@0x8252B928) and the bungee pair snaps to the car:
// position = the w row (-> +0x160), forward = the z row (-> +0x170).
// ============================================================================
void ICECameraMover::Construct(s32 /*liViewIndex*/, ICECameraAnchor* lpCar, ICECamera* lpICECamera,
                               ICETake* lpTake, ICEGroup* /*lpShakeGroup*/,
                               const IResourceManager* /*lpResourceMgr*/)
{
    mpCar             = lpCar;
    mfSimTime         = 1.0f;
    mpTake            = lpTake;
    miHardCutInterval = -1;
    muOldTag          = 0;
    mpICECamera       = lpICECamera;

    for (s32 liComponent = 0; liComponent < 3; ++liComponent)
    {
        const Cubic1D lRest;                       // Val..time 0, duration 1.0, state 0, flags 1
        mAccelOffset.maComponents[liComponent] = lRest;
        mForward.maComponents[liComponent]     = lRest;
        mForward.maComponents[liComponent].flags = 0;
    }

    for (s32 liRow = 0; liRow < 4; ++liRow)
    {
        mWorldToCamera.maRows[liRow].x = (liRow == 0) ? 1.0f : 0.0f;
        mWorldToCamera.maRows[liRow].y = (liRow == 1) ? 1.0f : 0.0f;
        mWorldToCamera.maRows[liRow].z = (liRow == 2) ? 1.0f : 0.0f;
        mWorldToCamera.maRows[liRow].w = 0.0f;
    }

    // stb 1, 4(camera). FLAG (the setter's polarity): see ICECamera::SetHideOverlay.
    mpICECamera->SetHideOverlay(false);

    const Vector3 lICEGeometryPos = mpCar->GetGeometryPosition();
    const Vector3 lICEForwardVec  = mpCar->GetForwardVector();
    mBungeeCarPos = lICEGeometryPos;
    mBungeeCarFwd = lICEForwardVec;
}

// ============================================================================
// ICECameraMover::Destruct (DWARF ICECameraMover.cpp:225). No owned resources: the followers and
// the matrix are plain members, the anchor / camera / take are non-owning pointers.
// ============================================================================
void ICECameraMover::Destruct()
{
}

// ============================================================================
// ICECameraMover::Update / UpdateFrameBegin / UpdateEventTag (DWARF ICECameraMover.cpp:472 / :252
// / :398). No ARTIST symbol for any of the three: ICEWrapper::Update @0x82540180 expands
// Update(1.0f) in place --
//   0x82540208 if (mpTake != 0) {                              (lwz 0x110 ; beq)
//   0x82540220     UpdateSimTime(1.0f);                         (bl, f1 = flt_82001C98 == 1.0)
//   0x82540224     if (!(mpTake->GetParameter() > 0.0f))       (lfs 8(take) ; bgt skips; NaN stores)
//   0x82540240         muOldTag = 0;
//   0x82540248     muOldTag = mpTake->GetValueInt(41);          (EVENT_TAG)
//                }
//   0x82540254 UpdateFrameEnd(1.0f);
// ============================================================================
void ICECameraMover::Update(f32 lfTimeStep)
{
    UpdateFrameBegin(lfTimeStep);
    UpdateFrameEnd(lfTimeStep);
}

void ICECameraMover::UpdateFrameBegin(f32 lfTimeStep)
{
    if (mpTake != 0)
    {
        UpdateSimTime(lfTimeStep);
        UpdateEventTag(lfTimeStep);
    }
}

void ICECameraMover::UpdateEventTag(f32 /*lfTimeStep*/)
{
    if (!(mpTake->GetParameter() > 0.0f))
    {
        muOldTag = 0;
    }
    const u32 eventTag = static_cast<u32>(mpTake->GetValueInt(E_ICE_EVENT_TAG));
    muOldTag = eventTag;
}

// ============================================================================
// ICECameraMover::UpdateSimTime @0x8252E418 (DWARF ICECameraMover.cpp:381)
//   0x8252E470 mfSimTime = TIME_SCALE                               (the raw value, overwritten below)
//   0x8252E480 v = TIME_SCALE * 0.01                                (fmuls)
//   0x8252E488 v = fsel(0.01 - v, 0.01, v)                          Max(0.01, v)
//   0x8252E494 v = fsel(1.0 - v, v, 1.0)                            Min(1.0, v)   (a NaN becomes 1.0)
//   0x8252E498 mfSimTime = v ; 0x8252E49C the camera's sim-time multiplier = v
// The step argument is not read.
// ============================================================================
void ICECameraMover::UpdateSimTime(f32 /*lfTimeStep*/)
{
    const f32 lfTimeScale = GetElementFloat(mpTake, E_ICE_TIME_SCALE);
    mfSimTime = lfTimeScale;

    const f32 lfSimTime = rw::math::fpu::Clamp(lfTimeScale * KF_PERCENT_TO_UNIT, KF_PERCENT_TO_UNIT, 1.0f);
    mfSimTime = lfSimTime;
    mpICECamera->SetSimTimeMultiplier(lfSimTime);
}

// ============================================================================
// ICECameraMover::UpdateFrameEnd @0x8253D988 (DWARF ICECameraMover.cpp:467)
//   if (mpTake == 0) return;                                        (lwz 0x110 ; beq)
//   the eight updates, in this order, each handed the step (f31)
//   0x8253DA10 SetCameraMatrix(&mWorldToCamera, mfSimTime * step)  (fmuls f1, f0, f31)
// ============================================================================
void ICECameraMover::UpdateFrameEnd(f32 lfTimeStep)
{
    if (mpTake == 0)
    {
        return;
    }

    UpdateTransformationMatrix(lfTimeStep);
    UpdateForwardVector(lfTimeStep);
    UpdateLens(lfTimeStep);
    UpdateFocus(lfTimeStep);
    UpdateHardCuts(lfTimeStep);
    UpdateFade(lfTimeStep);
    UpdateOverlay(lfTimeStep);
    UpdateBloom(lfTimeStep);

    mpICECamera->SetCameraMatrix(&mWorldToCamera, mfSimTime * lfTimeStep);
}

// ============================================================================
// ICECameraMover::UpdateTransformationMatrix @0x8253AC28 (DWARF ICECameraMover.cpp:371)
//   eye  = (EYE_X, EYE_Y, EYE_Z, 0)     look = (LOOK_X, LOOK_Y, LOOK_Z, 0)     (0x8253AC44..0x8253AE24)
//   0x8253AE44 worldEye  = anchor.TransformToWorld(eye,  (u8)GetValueInt(SPACE_EYE))
//   0x8253AE6C worldLook = anchor.TransformToWorld(look, (u8)GetValueInt(SPACE_LOOK))
//   0x8253AE70 eye space != 0 -> CreateLookAt(worldEye, worldLook)              @0x8220C4F8
//              eye space == 0 -> CreateLookAt(worldEye, worldLook, car up row)  @0x8220C960
//                                (the up is the anchor's mCarToWorld y row, `lvx128 v3, r11, 0x10`)
//   0x8253AF2C V = DUTCH * 2pi (fmuls), then the XDK sine / cosine inlined whole (0x8253AF3C..
//              0x8253B0C0; SDKs/XboxMath/XMVectorSinCos.h, the same instruction sequence)
//   0x8253B0B8.. x' = y * sin + (x * cos)   (vmulfp128, then a FUSED vmaddfp)
//                y' = (y * cos) - (x * sin) (two vmulfp128, then vsubfp)
//                z and w rows pass through (stored first, 0x8253B080 / 0x8253B084).
// ============================================================================
void ICECameraMover::UpdateTransformationMatrix(f32 /*lfTimeStep*/)
{
    Vector3 lEye;
    lEye.x = GetElementFloat(mpTake, E_ICE_EYE_X);
    lEye.y = GetElementFloat(mpTake, E_ICE_EYE_Y);
    lEye.z = GetElementFloat(mpTake, E_ICE_EYE_Z);
    lEye.w = 0.0f;

    Vector3 lLook;
    lLook.x = GetElementFloat(mpTake, E_ICE_LOOK_X);
    lLook.y = GetElementFloat(mpTake, E_ICE_LOOK_Y);
    lLook.z = GetElementFloat(mpTake, E_ICE_LOOK_Z);
    lLook.w = 0.0f;

    const CameraSpaceHandler& lrSpace = mpCar->GetSpace();

    const s32     liEyeSpace = mpTake->GetValueInt(E_ICE_SPACE_EYE);
    const Vector3 lWorldEye  = lrSpace.TransformToWorld(lEye, ToSpace(liEyeSpace));
    const Vector3 lWorldLook = lrSpace.TransformToWorld(lLook, ToSpace(mpTake->GetValueInt(E_ICE_SPACE_LOOK)));

    rw::math::vpu::Matrix44Affine lLookAt;
    if (liEyeSpace != 0)
    {
        lLookAt = BrnDirector::Camera::Utils::CreateLookAt(lWorldEye, lWorldLook);
    }
    else
    {
        lLookAt = BrnDirector::Camera::Utils::CreateLookAt(lWorldEye, lWorldLook,
                                                           lrSpace.GetCarToWorld().yAxis);
    }

    const f32 lfDutch = GetElementFloat(mpTake, E_ICE_DUTCH) * KF_TWO_PI;
    f32 lfSin = 0.0f;
    f32 lfCos = 0.0f;
    XboxMath::XMVectorSinCos(&lfSin, &lfCos, lfDutch);

    const Vector3& lrX = lLookAt.xAxis;
    const Vector3& lrY = lLookAt.yAxis;

    Vector4& lrRow0 = mWorldToCamera.maRows[0];
    lrRow0.x = std::fma(lrY.x, lfSin, lrX.x * lfCos);
    lrRow0.y = std::fma(lrY.y, lfSin, lrX.y * lfCos);
    lrRow0.z = std::fma(lrY.z, lfSin, lrX.z * lfCos);
    lrRow0.w = std::fma(lrY.w, lfSin, lrX.w * lfCos);

    Vector4& lrRow1 = mWorldToCamera.maRows[1];
    lrRow1.x = (lrY.x * lfCos) - (lrX.x * lfSin);
    lrRow1.y = (lrY.y * lfCos) - (lrX.y * lfSin);
    lrRow1.z = (lrY.z * lfCos) - (lrX.z * lfSin);
    lrRow1.w = (lrY.w * lfCos) - (lrX.w * lfSin);

    Vector4& lrRow2 = mWorldToCamera.maRows[2];
    lrRow2.x = lLookAt.zAxis.x;
    lrRow2.y = lLookAt.zAxis.y;
    lrRow2.z = lLookAt.zAxis.z;
    lrRow2.w = lLookAt.zAxis.w;

    Vector4& lrRow3 = mWorldToCamera.maRows[3];
    lrRow3.x = lLookAt.wAxis.x;
    lrRow3.y = lLookAt.wAxis.y;
    lrRow3.z = lLookAt.wAxis.z;
    lrRow3.w = lLookAt.wAxis.w;
}

// ============================================================================
// ICECameraMover::UpdateForwardVector @0x82533C18 (DWARF ICECameraMover.cpp:683)
//
// In ARTIST the DWARF's `v` and `fDrift` are compile-time constants -- v is stored as four zero
// words (0x82533C40..0x82533C50) and the drift is 0.0 (the 0.0 / 1.0 - 0.0 splats at 0x82533CC4 /
// 0x82533CF4) -- so what runs is:
//   0x82533C60 fDot = Dot(v, lFwd)                                   vmsum3fp128 (rule 1)
//   0x82533C78 if (!(fDot >= 0)) v = (v + lFwd) * (fDot * -2.0)      bge skips; vaddfp, fmuls, vmulfp128
//   0x82533CB8 Normalize(v): |v|^2 by vmsum3fp128, the vrsqrtefp estimate refined by TWO steps
//              e' = (0.5 e) * -(|v|^2 * e*e - 1) + e   (vmulfp128 twice, vnmsubfp, vmaddfp),
//              then v * e'                                              (vmulfp128)
//   0x82533D50 v = v * 0.0 + lFwd   (vmaddfp, fused) ; 0x82533D54 v = v * 1.0
//   0x82533D5C.. each forward follower: ValDesired = v.c, and state = 2 when it differs from the
//              follower's live Val (the inlined Cubic1D::SetValDesired -- DWARF Cubic3D::SetValDesired,
//              ICEPoint.cpp:222); a NaN always differs
//   0x82533DAC.. the three followers advance by mfSimTime * step, limits 0 (f2 = f3 = 0.0)
// A zero v normalises to NaN lanes (the estimate of 1/sqrt(0) is +inf and 0 * inf is NaN), so the
// console's followers are driven toward NaN -- reproduced, not repaired. Nothing on the camera
// path reads mForward (UpdateTransformationMatrix builds the frame from the take alone).
// ============================================================================
void ICECameraMover::UpdateForwardVector(f32 lfTimeStep)
{
    const f32 KF_HALF  = 0.5f;   // vcfsx(vspltisw 1, 1)
    const f32 KF_ONE   = 1.0f;   // vcfsx(vspltisw 1, 0)
    const f32 fDrift   = 0.0f;   // the 0.0 splat (flt_82001CC0)

    Vector3 v;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    v.w = 0.0f;

    const Vector3 lFwd = mpCar->GetForwardVector();

    const f32 fDot = BrnDirector::Camera::Utils::ConsoleVpu::Dot3(v, lFwd);
    if (!(fDot >= 0.0f))
    {
        const f32 lfScale = fDot * KF_DOT_FLIP;
        v.x = (v.x + lFwd.x) * lfScale;
        v.y = (v.y + lFwd.y) * lfScale;
        v.z = (v.z + lFwd.z) * lfScale;
        v.w = (v.w + lFwd.w) * lfScale;
    }

    // ICEMath::Normalize as the console inlines it.
    const f32 lfLengthSquared = BrnDirector::Camera::Utils::ConsoleVpu::Dot3(v, v);
    f32 lfReciprocal = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfLengthSquared)));   // vrsqrtefp
    for (s32 liStep = 0; liStep < 2; ++liStep)
    {
        const f32 lfSquare   = lfReciprocal * lfReciprocal;
        const f32 lfHalf     = lfReciprocal * KF_HALF;
        const f32 lfResidual = BrnDirector::Camera::Utils::ConsoleVpu::NegativeMultiplySubtract(
            lfLengthSquared, lfSquare, KF_ONE);
        lfReciprocal = std::fma(lfHalf, lfResidual, lfReciprocal);
    }
    Vector3 lNormal;
    lNormal.x = v.x * lfReciprocal;
    lNormal.y = v.y * lfReciprocal;
    lNormal.z = v.z * lfReciprocal;
    lNormal.w = v.w * lfReciprocal;

    const f32 lfKeep = KF_ONE - fDrift;
    v.x = std::fma(lNormal.x, fDrift, lFwd.x) * lfKeep;
    v.y = std::fma(lNormal.y, fDrift, lFwd.y) * lfKeep;
    v.z = std::fma(lNormal.z, fDrift, lFwd.z) * lfKeep;
    v.w = std::fma(lNormal.w, fDrift, lFwd.w) * lfKeep;

    const f32 lafDesired[3] = { v.x, v.y, v.z };
    for (s32 liComponent = 0; liComponent < 3; ++liComponent)
    {
        Cubic1D& lrCubic = mForward.maComponents[liComponent];
        const f32 lfDesired = lafDesired[liComponent];
        lrCubic.ValDesired = lfDesired;
        if (lfDesired != lrCubic.Val)
        {
            lrCubic.state = 2;
        }
    }

    const f32 lfSimStep = mfSimTime * lfTimeStep;
    for (s32 liComponent = 0; liComponent < 3; ++liComponent)
    {
        mForward.maComponents[liComponent].Update(lfSimStep, 0.0f, 0.0f);
    }
}

// ============================================================================
// ICECameraMover::UpdateLens @0x8252E548 (DWARF ICECameraMover.cpp:447)
//   0x8252E594 lensLength = LENS_LENGTH
//   0x8252E5C8 lensLength = Clamp(lensLength, desc.mMin, desc.mMax)  -- the element description's own
//              range words (ICEElementDescriptions[9] +0x18 / +0x1C, read 5.0 / 500.0), two fsels:
//              a NaN length becomes the max
//   0x8252E5D8 fieldOfView = ICEMath::ConvertLensLengthToFovAngle(lensLength)  (bl ICEMath::ATan)
//   0x8252E5DC..0x8252E65C the camera FOV = ICEMath::Angles::AngToDeg(fieldOfView), through the
//              inlined Camera::SetFOV ("lfFOV > 0.0f", Camera.h:424)
// ============================================================================
void ICECameraMover::UpdateLens(f32 /*lfTimeStep*/)
{
    const ICEElementDescription& lrLensDescription = ICEElementDescriptions[E_ICE_LENS_LENGTH];
    const f32 lensLength = rw::math::fpu::Clamp(GetElementFloat(mpTake, E_ICE_LENS_LENGTH),
                                                lrLensDescription.mMin.GetFloat(),
                                                lrLensDescription.mMax.GetFloat());

    const Angle fieldOfView = ICEMath::ConvertLensLengthToFovAngle(lensLength);
    mpICECamera->SetFieldOfView(ICEMath::Angles::AngToDeg(fieldOfView));
}

// ============================================================================
// ICECameraMover::UpdateFocus @0x8252E678 (DWARF ICECameraMover.cpp:474)
//   RAWFOCUS_OVERRIDE == 0, or BLUR_INTENSITY <= 0 (ble; a NaN too) -> the all-zero band
//   lfBlurriness = Min(1, Max(0, BLUR_INTENSITY))                             0x8252E700 / 0x8252E718
//   lfFocusFalloff = BLUR_FALLOFF * 50                                         0x8252E6FC
//   lfFocusStartDistanceMeters        = Max(0, NEAR - falloff)                 0x8252E70C / 0x8252E720
//   lfPerfectFocusStartDistanceMeters = Max(start + 0.001, NEAR)               0x8252E728 / 0x8252E730
//   lfPerfectFocusEndDistanceMeters   = Max(perfectStart, FAR)                 0x8252E734 / 0x8252E738
//   lfFocusEndDistanceMeters          = Max(perfectEnd + 0.001, FAR + falloff) 0x8252E73C..0x8252E744
//   camera DOF SetParams(start, perfectStart, perfectEnd, end, blurriness)    0x8252E748
// Each Max / Min is rw::math::fpu's fsel form (the DWARF lists those five Max and one Min).
// ============================================================================
void ICECameraMover::UpdateFocus(f32 /*lfTimeStep*/)
{
    if (mpTake->GetValueInt(E_ICE_RAWFOCUS_OVERRIDE) == 0)
    {
        mpICECamera->SetDepthOfField(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    f32 lfBlurriness = GetElementFloat(mpTake, E_ICE_BLUR_INTENSITY);
    if (!(lfBlurriness > 0.0f))
    {
        mpICECamera->SetDepthOfField(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    const f32 lfFocusFalloff                    = GetElementFloat(mpTake, E_ICE_BLUR_FALLOFF);
    const f32 lfPerfectFocusStartDistanceMeters = GetElementFloat(mpTake, E_ICE_NEAR_FOCUS);
    const f32 lfPerfectFocusEndDistanceMeters   = GetElementFloat(mpTake, E_ICE_FAR_FOCUS);

    lfBlurriness = rw::math::fpu::Min(1.0f, rw::math::fpu::Max(0.0f, lfBlurriness));

    const f32 lfFalloffMeters = lfFocusFalloff * KI_DEPTH_OF_FIELD_MAX_FALLOFF_DISTANCE;

    const f32 lfFocusStartDistanceMeters =
        rw::math::fpu::Max(0.0f, lfPerfectFocusStartDistanceMeters - lfFalloffMeters);
    const f32 lfPerfectStart =
        rw::math::fpu::Max(lfFocusStartDistanceMeters + KI_DEPTH_OF_FIELD_MIN_FALLOFF_DISTANCE,
                           lfPerfectFocusStartDistanceMeters);
    const f32 lfPerfectEnd = rw::math::fpu::Max(lfPerfectStart, lfPerfectFocusEndDistanceMeters);
    const f32 lfFocusEndDistanceMeters =
        rw::math::fpu::Max(lfPerfectEnd + KI_DEPTH_OF_FIELD_MIN_FALLOFF_DISTANCE,
                           lfFalloffMeters + lfPerfectFocusEndDistanceMeters);

    mpICECamera->SetDepthOfField(lfFocusStartDistanceMeters, lfPerfectStart, lfPerfectEnd,
                                 lfFocusEndDistanceMeters, lfBlurriness);
}

// ============================================================================
// ICECameraMover::UpdateHardCuts @0x82531B30 (DWARF ICECameraMover.cpp:339)
//   current_interval = channel 0's current interval (`lhz 0xD8(take)`); unchanged -> nothing
//   miHardCutInterval = current_interval
//   ICETake::IsHardCut(channel 0, element 0) -> the car position goes into BOTH bungee slots
//   (`lvx128 v0, r11, 0x30` stored to +0x160 and +0x170 -- the DWARF's lForwardVec is never used)
// ============================================================================
void ICECameraMover::UpdateHardCuts(f32 /*lfTimeStep*/)
{
    const s32 current_interval = static_cast<s32>(mpTake->GetCurrentInterval(KI_HARD_CUT_CHANNEL));
    if (current_interval == miHardCutInterval)
    {
        return;
    }
    miHardCutInterval = current_interval;

    if (mpTake->IsHardCut(KI_HARD_CUT_CHANNEL, KI_HARD_CUT_ELEMENT))
    {
        const Vector3 lGeometryPos = mpCar->GetGeometryPosition();
        mBungeeCarPos = lGeometryPos;
        mBungeeCarFwd = lGeometryPos;
    }
}

// ============================================================================
// ICECameraMover::UpdateFade @0x8252E788 (DWARF ICECameraMover.cpp:521)
//   fade  = Clamp(FADE * 0.01, 0, 1)                   (fmuls, then fsel Max(0, .) / Min(1, .))
//   color = GetValueInt(FADE_TO_COLOR); cases 0..4 (unsigned `cmplwi 4 ; bgt` -> anything else is
//   a no-op) are black / white / red / green / blue:
//     0 (0,0,0)  1 (1,1,1)  2 (1,0,0)  3 (0,1,0)  4 (0,0,1)      (the jump table at 0x8252E840)
//   camera SetFadeColor(r, g, b, fade)                              0x8252E894
// ============================================================================
void ICECameraMover::UpdateFade(f32 /*lfTimeStep*/)
{
    const f32 fade = rw::math::fpu::Clamp(GetElementFloat(mpTake, E_ICE_FADE) * KF_PERCENT_TO_UNIT, 0.0f, 1.0f);

    f32 lfRed   = 0.0f;
    f32 lfGreen = 0.0f;
    f32 lfBlue  = 0.0f;

    const s32 color = mpTake->GetValueInt(E_ICE_FADE_TO_COLOR);
    switch (color)
    {
    case 0:
        break;
    case 1:
        lfRed = 1.0f; lfGreen = 1.0f; lfBlue = 1.0f;
        break;
    case 2:
        lfRed = 1.0f;
        break;
    case 3:
        lfGreen = 1.0f;
        break;
    case 4:
        lfBlue = 1.0f;
        break;
    default:
        return;
    }

    mpICECamera->SetFadeColor(lfRed, lfGreen, lfBlue, fade);
}

// ============================================================================
// ICECameraMover::UpdateOverlay @0x8252E4A8 (DWARF ICECameraMover.cpp:420)
//   0x8252E4CC if (!(take parameter > 0)) miOldOverlay = 0                 (bgt skips)
//   overlay = GetValueInt(OVERLAY)
//   overlay > 0 and != miOldOverlay: ClearOverlay (stw 0), and when overlay <= 2 (cmplwi) SetOverlay
//     (stw overlay) and miOldOverlay = overlay, return
//   overlay == 0: the camera's overlay byte (+0x04) = 1                     0x8252E530
//   every other path ends with miOldOverlay = overlay                       0x8252E534
// ============================================================================
void ICECameraMover::UpdateOverlay(f32 /*lfTimeStep*/)
{
    if (!(mpTake->GetParameter() > 0.0f))
    {
        miOldOverlay = 0;
    }

    const s32 overlay = mpTake->GetValueInt(E_ICE_OVERLAY);
    if (overlay > 0)
    {
        if (overlay != miOldOverlay)
        {
            mpICECamera->ClearOverlay();
            if (static_cast<u32>(overlay) <= 2u)
            {
                mpICECamera->SetOverlay(overlay);
                miOldOverlay = overlay;
                return;
            }
        }
    }
    else if (overlay == 0)
    {
        // stb 1, 4(camera). FLAG (the setter's polarity): see ICECamera::SetHideOverlay.
        mpICECamera->SetHideOverlay(false);
    }

    miOldOverlay = overlay;
}

// ============================================================================
// ICECameraMover::UpdateBloom @0x8252E328 (DWARF ICECameraMover.cpp:301)
// Both reads are of the FADE element (take +0x68, the type word at desc +0x0C of slot 21) -- the
// console's own choice, reproduced:
//   0x8252E38C v = FADE * 0.01 ; v = Clamp(v, 0.2, 1.0)          (fsel Max(0.2, .) / Min(1, .))
//   0x8252E3B0 lfBloomthreshold = (v - 0.2) * -2.0               (fsubs, fmuls)
//   0x8252E408 lfBloomLuminance = FADE * 0.02                     (fmuls)
//   0x8252E40C / 0x8252E410 camera bloom threshold / luminance modifiers
// ============================================================================
void ICECameraMover::UpdateBloom(f32 /*lfTimeStep*/)
{
    const f32 lfLevel = rw::math::fpu::Clamp(GetElementFloat(mpTake, E_ICE_FADE) * KF_PERCENT_TO_UNIT,
                                             KF_BLOOM_THRESHOLD_BASE, 1.0f);
    const f32 lfBloomthreshold = (lfLevel - KF_BLOOM_THRESHOLD_BASE) * KF_BLOOM_THRESHOLD_GAIN;
    const f32 lfBloomLuminance = GetElementFloat(mpTake, E_ICE_FADE) * KF_BLOOM_LUMINANCE_GAIN;

    mpICECamera->SetBloom(lfBloomthreshold, lfBloomLuminance);
}

} // namespace ICE
