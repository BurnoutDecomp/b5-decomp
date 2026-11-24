// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.cpp
//
// BrnDirector::DebugComponent -- the director module's in-game debug menu component
// (registered under the name "Camera" -- see GetName). Of the 7 recovered functions of
// this class, 3 are reconstructed here:
//   - Construct      (runs on every boot)
//   - GetName
//   - TakePanorama
//   - (Destruct is declared in the original header but has no recovered body)
//
// RenderHUD and UpdatePanoramaScreenshots are bodied in BrnDirectorModuleDebugCompononent_wS34_00.cpp.
// SavePlaylists, LoadPlaylists and StartEditor are bodied in BrnDirectorModuleDebugCompononent_wS34_01.cpp.
// OnActivate is bodied in BrnDirectorModuleDebugCompononent_wS34_02.cpp.
// ============================================================================
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Director/DirectorModule/BrnDirectorModuleDebugCompononent.cpp"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// DebugComponent::Construct  (called by BrnDirector::DirectorModule::Construct)
//
// The first call in the recovered prologue resolves to a name from an unrelated class
// (CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct), but a Construct
// calling an unrelated class's Destruct makes no sense -- this is the well-known linker
// ICF folding pattern documented for this class family (see CgsDebugComponent.cpp
// DebugComponent::Construct: two zero-stores, mbActive=false / mpDebugLinkedListNext=
// nullptr). It is the base CgsDev::DebugComponent::Construct() call, folded at link time
// with a byte-identical trivial function from an unrelated TU.
// ----------------------------------------------------------------------------
void DebugComponent::Construct(DirectorModule* lpDirectorModule)
{
    CgsDev::DebugComponent::Construct();   // ARTIST 0x821F6FF4 (folded at 0x8284CB38)

    CGS_ASSERT(lpDirectorModule != 0, CGS_ASSERT_MESSAGE_AT("lpCameraModule != NULL", CGS_ARTIST_FILE, 57));

    mpDirectorModule          = lpDirectorModule;
    mbShowCameraPos           = false;
    mbShowCrashShotInfo       = false;
    mbTakePanoramaScreenshot  = false;
}

// ----------------------------------------------------------------------------
// DebugComponent::GetName
// ----------------------------------------------------------------------------
const char* DebugComponent::GetName() const
{
    return "Camera";
}

// ----------------------------------------------------------------------------
// DebugComponent::TakePanorama
//
// Debug-menu action callback (OnActivate registers it with userData=this -- see the
// header FLAG comment). Arms the panorama screenshot request on the rising edge
// (resets the pitch/yaw step counters); a no-op while a request is already pending.
// ----------------------------------------------------------------------------
void DebugComponent::TakePanorama(void* lpUserData)
{
    DebugComponent* lpThis = reinterpret_cast<DebugComponent*>(lpUserData);

    if (!lpThis->mbTakePanoramaScreenshot)
    {
        lpThis->miPanoramaStepPitch      = 0;
        lpThis->miPanoramaStepYaw        = 0;
        lpThis->mbTakePanoramaScreenshot = true;
    }
}

}

// FOLDED FROM BrnDirectorModuleDebugCompononent_wS34_00.cpp
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

// FOLDED FROM BrnDirectorModuleDebugCompononent_wS34_01.cpp
// BrnDirector::DebugComponent ("Camera" debug page) -- the playlist save / load actions and the ICE
// editor entry (partfile of BrnDirectorModuleDebugCompononent.cpp).

#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"

#include "GameSource/Director/BrnDirectorModule.h"                          // DirectorModule::GetMainDirector
#include "GameSource/Director/BrnMainDirector.h"                            // MainDirector::GetArbitrator / GetICEWrapper
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                    // SharedPlaylists::Serialise<S>
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"   // Camera::DebugMenuSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"    // Camera::TextFileReadSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"   // Camera::TextFileWriteSerialiser
#include "SDKs/Packages/ICE/ICEAuthor.hpp"                                  // ICE::ICEAuthor::CreateNewTake
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnDirector
{

// Write the arbitrator's shared playlists to "d:\\playlists.txt".
void DebugComponent::SavePlaylists(void* lpUserData)
{
    DebugComponent* lpThis = static_cast<DebugComponent*>(lpUserData);

    Camera::TextFileWriteSerialiser lSerialiser;
    lSerialiser.Construct("d:\\playlists.txt");
    lpThis->mpDirectorModule->GetMainDirector().GetArbitrator().GetStateContainer().GetSharedPlaylists().Serialise(lSerialiser);
    lSerialiser.Destruct();
}

// Read the arbitrator's shared playlists back from "d:\\playlists.txt", taking their menu entries
// down first and rebuilding them for the loaded lists.
void DebugComponent::LoadPlaylists(void* lpUserData)
{
    DebugComponent*  lpThis      = static_cast<DebugComponent*>(lpUserData);
    SharedPlaylists& lrPlaylists = lpThis->mpDirectorModule->GetMainDirector().GetArbitrator().GetStateContainer().GetSharedPlaylists();

    {
        Camera::DebugMenuSerialiser lSerialiser;
        lSerialiser.Construct(lpThis, Camera::DebugMenuSerialiser::E_MODE_REMOVE_FROM_MENU);
        lrPlaylists.Serialise(lSerialiser);
    }

    {
        Camera::TextFileReadSerialiser lSerialiser;
        lSerialiser.Construct("d:\\playlists.txt");
        lrPlaylists.Serialise(lSerialiser);
        lSerialiser.Destruct();
    }

    {
        Camera::DebugMenuSerialiser lSerialiser;
        lSerialiser.Construct(lpThis, Camera::DebugMenuSerialiser::E_MODE_ADD_TO_MENU);
        lrPlaylists.Serialise(lSerialiser);
    }
}

// Create a new ICE take named "New Take" and open the in-game ICE editor on it.
void DebugComponent::StartEditor(void* lpUserData)
{
    DebugComponent* lpThis      = static_cast<DebugComponent*>(lpUserData);
    ICEWrapper&     lrICEWrapper = lpThis->mpDirectorModule->GetMainDirector().GetICEWrapper();

    ICE::ICETakeData* lpTakeData = lrICEWrapper.GetAuthor().CreateNewTake("New Take", -1);
    CGS_ASSERT(lpTakeData, CGS_ASSERT_MESSAGE_AT("lpTakeData", CGS_ARTIST_FILE, 503));
    lrICEWrapper.EditorOn(lpTakeData);
}

} // namespace BrnDirector

// FOLDED FROM BrnDirectorModuleDebugCompononent_wS34_02.cpp
// BrnDirector::DebugComponent ("Camera" debug page) -- OnActivate, the page's variable and action
// registration (partfile of BrnDirectorModuleDebugCompononent.cpp).

#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"

#include <cstddef>   // offsetof

#include "GameSource/Director/BrnDirectorModule.h"                          // DirectorModule
#include "GameSource/Director/BrnMainDirector.h"                            // MainDirector
#include "GameSource/Director/BrnDirectorResourceManager.h"                 // DirectorResourceManager::GetTestbed / GetTestbed2
#include "GameSource/Director/Arbitrator/BrnDirectorArbitrator.h"           // Arbitrator
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateTestbed.h"       // ArbStateTestbed
#include "GameSource/Director/Arbitrator/States/BrnArbStateRoaming.h"       // ArbStateRoaming
#include "GameSource/Director/Arbitrator/States/BrnArbStateTakedown.h"      // ArbStateTakedown
#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashMode.h"     // ArbStateCrashMode
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"            // SharedCameraContainer
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                 // Camera::BehaviourManager / BehaviourHandle
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"           // Camera::BehaviourParameterBank
#include "GameSource/Director/Camera/Behaviours/Serialisation.h"            // Camera::TestbedSetupSerialiser
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"
#include "GameSource/Director/Camera/Camera.h"                              // Camera::Camera clip distances
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h" // DebugPrinter
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"            // VehicleTracker crash thresholds
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT

namespace BrnDirector
{

// Registers the page: the parameter-bank and playlist file actions, the testbed controls and its
// per-block / ICE-take activators, then the camera, moment, debug-output, crash, misc, clipping and
// timestep tweakables. The four "Current Camera" readouts are made read-only last.
void DebugComponent::OnActivate()
{
    MainDirector&                   lrMainDirector = mpDirectorModule->GetMainDirector();
    const DirectorResourceManager&  lrResources    = mpDirectorModule->mDirectorResourceManager;
    Arbitrator&                     lrArbitrator   = lrMainDirector.GetArbitrator();
    ArbitratorStateContainer&       lrStates       = lrArbitrator.GetStateContainer();
    ArbStateTestbed&                lrTestbed      = lrArbitrator.mArbStateTestbed;
    Camera::BehaviourManager&       lrBehaviours   = lrMainDirector.GetBehaviourManager();
    Camera::BehaviourParameterBank& lrBank         = lrBehaviours.GetBehaviourParameterBank();
    MainDirector::CameraDebugInfo&  lrCameraInfo   = lrMainDirector.GetCameraDebugInfo();

    // ---- file actions -------------------------------------------------------------------------
    RegisterFunction(&Camera::BehaviourParameterBank::SaveParameters, &lrBank, "", "Save");
    RegisterFunction(&SavePlaylists, this, "Save playlists");
    RegisterFunction(&Camera::BehaviourParameterBank::LoadParameters, &lrBank, "", "Load");
    RegisterFunction(&LoadPlaylists, this, "Load playlists");
    RegisterFunction(&StartEditor, this, "Start Editor");

    // ---- testbed ------------------------------------------------------------------------------
    lrTestbed.SetDebugComponent(this);
    RegisterFunction(&ArbStateTestbed::Deactivate, NULL, "Testbed", "Deactivate");
    RegisterVariable(&lrTestbed.mbLoopIceMovies, "Testbed", "Loop ICE testbed movies");
    lrTestbed.RegisterIceAnimsWithDebugComponent(&lrResources.GetTestbed(), lrResources, "Testbed/Ice");
    lrTestbed.RegisterIceAnimsWithDebugComponent(&lrResources.GetTestbed2(), lrResources, "Testbed/Ice2");

    {
        Camera::TestbedSetupSerialiser lSerialiser;
        lSerialiser.Construct(this);
        lrBank.Serialise(lSerialiser);
    }

    Camera::Behaviour::Parameters& lrExternalParams = lrBank.GetGameplayExternalCameraParamsForCar();
    RegisterFunction(&ArbStateTestbed::GenericActivateCam, &lrExternalParams, "Testbed", lrExternalParams.GetDebugName());

    // ---- the shared gameplay-external camera --------------------------------------------------
    Camera::BehaviourHandle<Camera::BehaviourGameplayExternal>& lrExternalCam =
        lrArbitrator.GetSharedCameras().mGameplayExternal;

    CGS_ASSERT(lrExternalCam.IsAllocated(), "IsAllocated()");
    RegisterVariable(&lrExternalCam.GetBehaviour()->mbEnableDebugRender, "", "Draw follow-cam pivot");

    // ---- current camera -----------------------------------------------------------------------
    RegisterVariable(&lrMainDirector.miForcedCameraCarIndex, "Current Camera", "Override Player");
    SetRange(&lrMainDirector.miForcedCameraCarIndex, -1, 7);
    RegisterVariable(&lrCameraInfo.mfFOV, "Current Camera", "FOV");
    RegisterVariable(&lrCameraInfo.mfX, "Current Camera", "X");
    RegisterVariable(&lrCameraInfo.mfY, "Current Camera", "Y");
    RegisterVariable(&lrCameraInfo.mfZ, "Current Camera", "Z");

    // ---- moments ------------------------------------------------------------------------------
    RegisterVariable(&lrMainDirector.mbAllowJumpMoment, "Moment Options", "Allow Jump Moment");
    RegisterVariable(&lrMainDirector.mbAllowStuntMoment, "Moment Options", "Allow Stunt Moment");
    RegisterVariable(&lrMainDirector.mbAllowHardStopMoment, "Moment Options", "Allow Hard Stop Moment");

    // ---- debug output -------------------------------------------------------------------------
    // The three printers are the director's named DebugPrinter storage (default, behaviour, moment).
    RegisterVariable(&reinterpret_cast<DebugPrinter*>(lrMainDirector.maDebugPrinterMain)->mbEnabled,
                     "Debug Ouput", "Enable Default Debug Output");
    RegisterVariable(&reinterpret_cast<DebugPrinter*>(lrMainDirector.maDebugPrinterA)->mbEnabled,
                     "Debug Ouput", "Enable Behaviour Debug Output");
    RegisterVariable(&reinterpret_cast<DebugPrinter*>(lrMainDirector.maDebugPrinterB)->mbEnabled,
                     "Debug Ouput", "Enable Moment Debug Output");
    RegisterVariable(&mbShowCameraPos, "Debug Ouput", "Draw camera pos");
    RegisterVariable(&lrMainDirector.mbShowAllCameraNames, "Debug Ouput", "Show all camera names");
    RegisterVariable(&lrMainDirector.mbShowDebugCameraNames, "Debug Ouput", "Show debug camera names");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_SHOW_WORLD_MAP_DEBUG],
                     "Debug Ouput", "Show World Map debug");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_SHOW_CAMERA_STATE_FLAGS],
                     "Debug Ouput", "Show Camera state flags");
    RegisterVariable(&mbShowCrashShotInfo, "Debug Ouput", "Show Shot Selection Info");
    RegisterVariable(&lrStates.mArbStateCrashing.mbDebugDisplayActive, "Debug Ouput", "Show Crash Info");
    RegisterVariable(&lrBehaviours.mbDebugDisplayAllCameras,
                     "Debug Ouput", "Debug Display All Camera Transforms");

    // ---- crash thresholds ---------------------------------------------------------------------
    RegisterVariable(&VehicleTracker::sfMinSpeedBelowMaxMPHForNormalCrash,
                     "Crash Thresholds", "Min Speed below Max MPH for Normal Crashes");
    RegisterVariable(&VehicleTracker::sfMinSpeedBelowMaxMPHForHighSpeedCrash,
                     "Crash Thresholds", "Min Speed below Max MPH for Fast Crashes");

    // ---- misc flags ---------------------------------------------------------------------------
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_ALWAYS_SUPER_WIDE],
                     "Misc Flags", "Always anamorphic");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_DISABLE_AFTERTOUCH_CAMERA],
                     "Misc Flags", "Disable Aftertouch camera");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_FORCE_SLOMO_NOT_ALLOWED],
                     "Misc Flags", "Force slomo not allowed");
    RegisterVariable(&lrMainDirector.mbDebugAssertNoIllegalSlomo, "Misc Flags", "Assert no illegal slomo usage");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_FORCE_CUTSCENE_BARS_ON],
                     "Misc Flags", "Force cutscene bars on");
    RegisterVariable(&lrArbitrator.mbDoAttractMode, "Misc Flags", "Do attract mode");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_FORCE_SUPER_SLOMO_IN_CRASHES],
                     "Misc Flags", "Force super slomo in crashes");
    RegisterVariable(&lrStates.mArbStateTakedown.mbUseTakedownDebugCam, "Misc Flags", "Use debug takedown camera");
    RegisterVariable(&lrStates.mArbStateTakedown.mbAlwaysUseShutdownCam, "Misc Flags", "Always do shutdown TD camera");
    RegisterVariable(&lrStates.mArbStateCrashMode.mbAllowCloseup, "Misc flags", "Allow Showtime closeups");
    RegisterVariable(&lrStates.mArbStateRoaming.mbPlayRaceEndEffect, "Misc flags", "Play race end effect");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_EVENT_END_REQUEST],
                     "Misc Flags", "Enable finish line testing");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_EVENT_END_FORCED],
                     "Misc Flags", "Force event state to active");

    // ---- clipping -----------------------------------------------------------------------------
    RegisterVariable(&Camera::Camera::KF_SMALL_NEAR_CLIP_DISTANCE, "Clipping", "Small Near Clip");
    RegisterVariable(&Camera::Camera::KF_DEFAULT_NEAR_CLIP_DISTANCE, "Clipping", "Default Near Clip");
    RegisterVariable(&Camera::Camera::KF_DEFAULT_FAR_CLIP_DISTANCE, "Clipping", "Default Far Clip");
    SetStep(&Camera::Camera::KF_SMALL_NEAR_CLIP_DISTANCE, 0.01f);
    SetStep(&Camera::Camera::KF_DEFAULT_NEAR_CLIP_DISTANCE, 0.01f);
    SetStep(&Camera::Camera::KF_DEFAULT_FAR_CLIP_DISTANCE, 100.0f);

    // ---- page-root toggles --------------------------------------------------------------------
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_DEBUG_ZERO_TIMESTEP], "Zero Timestep");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_DEBUG_SINGLE_TIMESTEP],
                     "Single Non-Zero Timestep");
    RegisterVariable(&lrStates.mArbStateRoaming.mbDisablePictureParadise, "Disable Idle Pic Paradise");
    RegisterVariable(&ArbStateRoaming::KF_IDLE_TIME_BEFORE_PICTURE_PARADISE, "Pic Paradise Delay Secs");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_FORCE_NEXT_WORLD_CRASH_FAST_TOP_DOWN],
                     "Force next crash to fast topdown");
    RegisterVariable(&lrMainDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_DISABLE_DOF], "Disable DOF");
    RegisterFunction(&TakePanorama, this, "", "Take Panorama");
    RegisterVariable(&lrArbitrator.mbDoRenderMetrics, "RenderMetricsMode");

    CGS_ASSERT(lrExternalCam.IsAllocated(), "IsAllocated()");
    RegisterVariable(&lrExternalCam.GetBehaviour()->mbEnableBoostEffects, "", "Enable Boost Effects");

    SetReadOnly(&lrCameraInfo.mfFOV, true);
    SetReadOnly(&lrCameraInfo.mfX, true);
    SetReadOnly(&lrCameraInfo.mfY, true);
    SetReadOnly(&lrCameraInfo.mfZ, true);
}

// NEVER CALLED. Pins the byte adjacency the page's registrations rely on.
void DebugComponent::_AssertLayout()
{
    CGS_STATIC_ASSERT((offsetof(DebugComponent, mbShowCrashShotInfo) == offsetof(DebugComponent, mbShowCameraPos) + 1),
                  "DebugComponent: +0x10 / +0x11");
    CGS_STATIC_ASSERT((offsetof(MainDirector::CameraDebugInfo, mfX) == 0x00 &&
                  offsetof(MainDirector::CameraDebugInfo, mfY) == 0x04 &&
                  offsetof(MainDirector::CameraDebugInfo, mfZ) == 0x08 &&
                  offsetof(MainDirector::CameraDebugInfo, mfFOV) == 0x0C),
                  "CameraDebugInfo: X / Y / Z / FOV at +0x00 / +0x04 / +0x08 / +0x0C");
    CGS_STATIC_ASSERT((offsetof(MainDirector, mbShowDebugCameraNames) == offsetof(MainDirector, miForcedCameraCarIndex) + 4 &&
                  offsetof(MainDirector, mbDebugAssertNoIllegalSlomo) == offsetof(MainDirector, mbShowDebugCameraNames) + 1),
                  "MainDirector: +0x33100 override index, +0x33104 / +0x33105 camera-name and slomo-assert flags");
    CGS_STATIC_ASSERT((offsetof(MainDirector, mbAllowStuntMoment) == offsetof(MainDirector, mbAllowJumpMoment) + 1 &&
                  offsetof(MainDirector, mbAllowHardStopMoment) == offsetof(MainDirector, mbAllowJumpMoment) + 2 &&
                  offsetof(MainDirector, mbShowAllCameraNames) == offsetof(MainDirector, mbAllowJumpMoment) + 3 &&
                  offsetof(MainDirector, maStateFlagTail) == offsetof(MainDirector, mbAllowJumpMoment) + 4),
                  "MainDirector: +0x3542C..+0x3542F then the flag tail at +0x35430");
    CGS_STATIC_ASSERT((sizeof(bool) == 1 && sizeof(((MainDirector*)0)->maStateFlagTail) == 0x20),
                  "MainDirector: the flag tail is 0x20 one-byte flags");
    CGS_STATIC_ASSERT((sizeof(DebugPrinter) == 0x24 && offsetof(DebugPrinter, mbEnabled) == 0x20 &&
                  sizeof(((MainDirector*)0)->maDebugPrinterA) == sizeof(DebugPrinter) &&
                  sizeof(((MainDirector*)0)->maDebugPrinterB) == sizeof(DebugPrinter) &&
                  sizeof(((MainDirector*)0)->maDebugPrinterMain) >= sizeof(DebugPrinter)),
                  "DebugPrinter: 0x24 bytes, mbEnabled at +0x20; each printer span holds one");
    CGS_STATIC_ASSERT((offsetof(Camera::BehaviourGameplayExternal, mbEnableBoostEffects) ==
                  offsetof(Camera::BehaviourGameplayExternal, mbEnableDebugRender) + 1),
                  "BehaviourGameplayExternal: +0xB5E / +0xB5F");
    CGS_STATIC_ASSERT((offsetof(Arbitrator, mbDoRenderMetrics) == offsetof(Arbitrator, mbDoAttractMode) + 1),
                  "Arbitrator: +0x44FD / +0x44FE");
    CGS_STATIC_ASSERT((offsetof(ArbStateRoaming, mbPlayRaceEndEffect) == offsetof(ArbStateRoaming, mbDisablePictureParadise) + 1),
                  "ArbStateRoaming: +0x39F / +0x3A0");
    CGS_STATIC_ASSERT((offsetof(ArbStateTakedown, mbUseTakedownDebugCam) == offsetof(ArbStateTakedown, mbAlwaysUseShutdownCam) + 1),
                  "ArbStateTakedown: +0x62C / +0x62D");
}

} // namespace BrnDirector

#undef CGS_ARTIST_FILE
