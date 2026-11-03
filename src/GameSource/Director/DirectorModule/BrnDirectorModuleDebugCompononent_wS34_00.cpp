// BrnDirector::DebugComponent ("Camera" debug page) -- the camera HUD overlay and the panorama
// screenshot pass (partfile of BrnDirectorModuleDebugCompononent.cpp).

#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"


#include "GameSource/Director/BrnDirectorModule.h"                         // DirectorModule::GetMainDirector
#include "GameSource/Director/BrnMainDirector.h"                           // MainDirector::CameraDebugInfo
#include "GameSource/Director/Camera/Camera.h"                             // Camera::Camera
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                  // Utils::ConvertFOVDegsToLensLength
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                    // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "SDKs/XboxMath/XMVectorSinCos.h"                                  // the inlined sine / cosine pair

namespace BrnDirector
{

namespace
{
    // RenderHUD's text block: right-aligned, 16-unit text on 17.6-unit lines, anchored a tenth
    // of the screen in from the right edge and an eighth of the screen down.
    const f32          KF_TEXT_SIZE         = 16.0f;
    const f32          KF_TEXT_JUSTIFICATION = 1.0f;
    const f32          KF_LINE_SIZE         = 17.6f;
    const f32          KF_X_INSET_FRACTION  = 0.1f;
    const f32          KF_Y_OFFSET_FRACTION = 0.125f;
    const CgsDev::RGBA KU_TEXT_COLOUR       = 0xFF00FF00u;
    const s32          KI_MESSAGE_LENGTH    = 64;

    // The panorama grid: 5 pitch rows of 10 yaw steps. Row 0 looks 45 degrees up, each row tilts
    // down by 22.5 degrees; each yaw step turns by -36 degrees. Every shot is taken at 55 degrees FOV.
    const s32 KI_PANORAMA_PITCH_STEPS = 5;
    const s32 KI_PANORAMA_YAW_STEPS   = 10;
    const f32 KF_PANORAMA_PITCH_START = 0.78539819f;    // pi / 4
    const f32 KF_PANORAMA_PITCH_STEP  = 0.39269909f;    // pi / 8
    const f32 KF_PANORAMA_YAW_STEP    = -0.62831855f;   // -pi / 5
    const f32 KF_PANORAMA_ROLL        = 0.0f;
    const f32 KF_PANORAMA_FOV         = 55.0f;

    // rw::math::vpu::Matrix44AffineFromEulerXYZ as the console inlines it: Rx * Ry * Rz for row
    // vectors, sine and cosine from the inlined XMVectorSinCos. The two "+" terms are single fused
    // multiply-adds, the two "-" terms a multiply then a subtract. Only the three rotation rows
    // are produced; the caller supplies the position row.
    Matrix44Affine Matrix44AffineFromEulerXYZ(const Vector3& lrAngles)
    {
        f32 lfSinX, lfCosX, lfSinY, lfCosY, lfSinZ, lfCosZ;
        XboxMath::XMVectorSinCos(&lfSinX, &lfCosX, lrAngles.GetX());
        XboxMath::XMVectorSinCos(&lfSinY, &lfCosY, lrAngles.GetY());
        XboxMath::XMVectorSinCos(&lfSinZ, &lfCosZ, lrAngles.GetZ());

        // ARTIST 0x8221B860..8E0 performs the matrix products in VMX.
        // Replicated lanes preserve vmulfp/vsubfp/vmaddfp rounding and denormal rules.
        const XMVECTOR sinX = XMVectorReplicate(lfSinX);
        const XMVECTOR cosX = XMVectorReplicate(lfCosX);
        const XMVECTOR sinY = XMVectorReplicate(lfSinY);
        const XMVECTOR cosY = XMVectorReplicate(lfCosY);
        const XMVECTOR sinZ = XMVectorReplicate(lfSinZ);
        const XMVECTOR cosZ = XMVectorReplicate(lfCosZ);
        const XMVECTOR cosXSinZ = __vmulfp(cosX, sinZ);
        const XMVECTOR sinXSinZ = __vmulfp(sinX, sinZ);
        const XMVECTOR sinXCosZ = __vmulfp(sinX, cosZ);
        const XMVECTOR cosXCosZ = __vmulfp(cosX, cosZ);

        Matrix44Affine lResult;
        lResult.xAxis = Vector3(__vmulfp(cosY, cosZ).x, __vmulfp(cosY, sinZ).x, -lfSinY);
        lResult.yAxis = Vector3(__vsubfp(__vmulfp(sinY, sinXCosZ), cosXSinZ).x,
                                __vmaddfp(sinY, sinXSinZ, cosXCosZ).x, __vmulfp(cosY, sinX).x);
        lResult.zAxis = Vector3(__vmaddfp(sinY, cosXCosZ, sinXSinZ).x,
                                __vsubfp(__vmulfp(sinY, cosXSinZ), sinXCosZ).x, __vmulfp(cosY, cosX).x);
        return lResult;
    }
}

// ----------------------------------------------------------------------------
// DebugComponent::RenderHUD
//
// With "Draw camera pos" ticked, prints the director's camera debug snapshot down the right of
// the screen: position, FOV, the lens length that FOV makes on the snapshot's aspect ratio, the
// look-at point and the near clip.
// ----------------------------------------------------------------------------
void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
{
    const Vector2 lScreenSize = lpRender->GetVirtualScreenSize();
    const f32 lfX = lScreenSize.mV.x - lScreenSize.mV.x * KF_X_INSET_FRACTION;
    f32       lfY = lScreenSize.mV.y * KF_Y_OFFSET_FRACTION;

    if (!mbShowCameraPos)
    {
        return;
    }

    const MainDirector::CameraDebugInfo& lrInfo =
        mpDirectorModule->GetMainDirector().GetCameraDebugInfo();
    char lacMessage[KI_MESSAGE_LENGTH];

    CgsCore::SPrintf(lacMessage, KI_MESSAGE_LENGTH, "%.2f : Camera X", static_cast<f64>(lrInfo.mfX));
    lpRender->DrawAlignedText(lacMessage, lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR, KF_TEXT_JUSTIFICATION);
    lfY += KF_LINE_SIZE;

    CgsCore::SPrintf(lacMessage, KI_MESSAGE_LENGTH, "%.2f : Camera Y", static_cast<f64>(lrInfo.mfY));
    lpRender->DrawAlignedText(lacMessage, lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR, KF_TEXT_JUSTIFICATION);
    lfY += KF_LINE_SIZE;

    CgsCore::SPrintf(lacMessage, KI_MESSAGE_LENGTH, "%.2f : Camera Z", static_cast<f64>(lrInfo.mfZ));
    lpRender->DrawAlignedText(lacMessage, lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR, KF_TEXT_JUSTIFICATION);
    lfY += KF_LINE_SIZE;

    CgsCore::SPrintf(lacMessage, KI_MESSAGE_LENGTH, "%.2f : FOV     ", static_cast<f64>(lrInfo.mfFOV));
    lpRender->DrawAlignedText(lacMessage, lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR, KF_TEXT_JUSTIFICATION);
    lfY += KF_LINE_SIZE;

    CgsCore::SPrintf(lacMessage, KI_MESSAGE_LENGTH, "%.2f : Lens    ",
                     static_cast<f64>(Camera::Utils::ConvertFOVDegsToLensLength(lrInfo.mfFOV,
                                                                                lrInfo.mfAspectRatio)));
    lpRender->DrawAlignedText(lacMessage, lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR, KF_TEXT_JUSTIFICATION);
    lfY += KF_LINE_SIZE;

    CgsCore::SPrintf(lacMessage, KI_MESSAGE_LENGTH, "%.2f %.2f %.2f : LookAt ",
                     static_cast<f64>(lrInfo.mAt.GetX()), static_cast<f64>(lrInfo.mAt.GetY()),
                     static_cast<f64>(lrInfo.mAt.GetZ()));
    lpRender->DrawAlignedText(lacMessage, lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR, KF_TEXT_JUSTIFICATION);

    CgsCore::SPrintf(lacMessage, KI_MESSAGE_LENGTH, "%.2f : N Clip  ", static_cast<f64>(lrInfo.mfNearClip));
    lpRender->DrawAlignedText(lacMessage, lfX, lfY + KF_LINE_SIZE, KF_TEXT_SIZE, KU_TEXT_COLOUR,
                              KF_TEXT_JUSTIFICATION);
}

// ----------------------------------------------------------------------------
// DebugComponent::UpdatePanoramaScreenshots
//
// While a panorama is armed (TakePanorama), aims the frame camera at the current grid cell,
// fixes the FOV and requests a screenshot, then advances the grid: yaw first, then pitch. The
// request disarms itself after the last row; the position row of the camera is left alone.
// ----------------------------------------------------------------------------
void DebugComponent::UpdatePanoramaScreenshots(Camera::Camera* lpCamera)
{
    if (!mbTakePanoramaScreenshot)
    {
        return;
    }

    const f32 lfPitch = KF_PANORAMA_PITCH_START -
                        static_cast<f32>(miPanoramaStepPitch) * KF_PANORAMA_PITCH_STEP;
    const f32 lfYaw   = static_cast<f32>(miPanoramaStepYaw) * KF_PANORAMA_YAW_STEP;

    const Vector3 lAngles(lfPitch, lfYaw, KF_PANORAMA_ROLL);

    Matrix44Affine lCameraMatrix = Matrix44AffineFromEulerXYZ(lAngles);
    lCameraMatrix.wAxis = lpCamera->GetTransform().wAxis;
    lpCamera->SetTransform(lCameraMatrix);

    lpCamera->ValidateTransformWithDebugInfo();
    lpCamera->SetFOV(KF_PANORAMA_FOV);
    lpCamera->GetEffects().mbRequestingScreenshot = true;

    ++miPanoramaStepYaw;
    if (miPanoramaStepYaw == KI_PANORAMA_YAW_STEPS)
    {
        ++miPanoramaStepPitch;
        if (miPanoramaStepPitch == KI_PANORAMA_PITCH_STEPS)
        {
            mbTakePanoramaScreenshot = false;
        }
        else
        {
            miPanoramaStepYaw = 0;
        }
    }
}

} // namespace BrnDirector
