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
    static_assert(offsetof(DebugComponent, mbShowCrashShotInfo) == offsetof(DebugComponent, mbShowCameraPos) + 1,
                  "DebugComponent: +0x10 / +0x11");
    static_assert(offsetof(MainDirector::CameraDebugInfo, mfX) == 0x00 &&
                  offsetof(MainDirector::CameraDebugInfo, mfY) == 0x04 &&
                  offsetof(MainDirector::CameraDebugInfo, mfZ) == 0x08 &&
                  offsetof(MainDirector::CameraDebugInfo, mfFOV) == 0x0C,
                  "CameraDebugInfo: X / Y / Z / FOV at +0x00 / +0x04 / +0x08 / +0x0C");
    static_assert(offsetof(MainDirector, mbShowDebugCameraNames) == offsetof(MainDirector, miForcedCameraCarIndex) + 4 &&
                  offsetof(MainDirector, mbDebugAssertNoIllegalSlomo) == offsetof(MainDirector, mbShowDebugCameraNames) + 1,
                  "MainDirector: +0x33100 override index, +0x33104 / +0x33105 camera-name and slomo-assert flags");
    static_assert(offsetof(MainDirector, mbAllowStuntMoment) == offsetof(MainDirector, mbAllowJumpMoment) + 1 &&
                  offsetof(MainDirector, mbAllowHardStopMoment) == offsetof(MainDirector, mbAllowJumpMoment) + 2 &&
                  offsetof(MainDirector, mbShowAllCameraNames) == offsetof(MainDirector, mbAllowJumpMoment) + 3 &&
                  offsetof(MainDirector, maStateFlagTail) == offsetof(MainDirector, mbAllowJumpMoment) + 4,
                  "MainDirector: +0x3542C..+0x3542F then the flag tail at +0x35430");
    static_assert(sizeof(bool) == 1 && sizeof(MainDirector::maStateFlagTail) == 0x20,
                  "MainDirector: the flag tail is 0x20 one-byte flags");
    static_assert(sizeof(DebugPrinter) == 0x24 && offsetof(DebugPrinter, mbEnabled) == 0x20 &&
                  sizeof(MainDirector::maDebugPrinterA) == sizeof(DebugPrinter) &&
                  sizeof(MainDirector::maDebugPrinterB) == sizeof(DebugPrinter) &&
                  sizeof(MainDirector::maDebugPrinterMain) >= sizeof(DebugPrinter),
                  "DebugPrinter: 0x24 bytes, mbEnabled at +0x20; each printer span holds one");
    static_assert(offsetof(Camera::BehaviourGameplayExternal, mbEnableBoostEffects) ==
                  offsetof(Camera::BehaviourGameplayExternal, mbEnableDebugRender) + 1,
                  "BehaviourGameplayExternal: +0xB5E / +0xB5F");
    static_assert(offsetof(Arbitrator, mbDoRenderMetrics) == offsetof(Arbitrator, mbDoAttractMode) + 1,
                  "Arbitrator: +0x44FD / +0x44FE");
    static_assert(offsetof(ArbStateRoaming, mbPlayRaceEndEffect) == offsetof(ArbStateRoaming, mbDisablePictureParadise) + 1,
                  "ArbStateRoaming: +0x39F / +0x3A0");
    static_assert(offsetof(ArbStateTakedown, mbUseTakedownDebugCam) == offsetof(ArbStateTakedown, mbAlwaysUseShutdownCam) + 1,
                  "ArbStateTakedown: +0x62C / +0x62D");
}

} // namespace BrnDirector
