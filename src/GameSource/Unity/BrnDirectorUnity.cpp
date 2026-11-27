// BrnDirectorUnity: one of ARTIST's unity translation units (DWARF compile unit BrnDirectorUnity.cpp),
// ARTIST 0x821F5550-0x82276820. Each member also compiles on its own; this file holds only their includes.
// Include form: the other Brn* unity TUs' (relative to this directory). ARTIST holds no unity-prefixed
// __FILE__ string for a Director member: their asserts quote ..\..\..\GameSource\Director/<path>, found
// through an include directory as CgsDevelopmentUnity's members are (DECISIONS D17), so the exact text is unknown.
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 290/308 transitions forward); first member from GCC's _GLOBAL__I_BrnDirector::Arbitrator::Construct (DecFIGS).

#include "../Director/Arbitrator/BrnDirectorArbitrator.cpp"
#include "../Director/Arbitrator/BrnDirectorArbitratorSharedCameraContainer.cpp"
#include "../Director/Arbitrator/BrnDirectorArbitratorStateContainer.cpp"
#include "../Director/Arbitrator/BrnDirectorArbitratorState.cpp"  // DWARF/PS3 member with no ARTIST body of its own (position not pinned)
#include "../Director/Arbitrator/States/BrnArbStateTestbed.cpp"
#include "../Director/Arbitrator/States/BrnArbStateRoaming.cpp"
#include "../Director/Arbitrator/States/BrnArbStateCrashing.cpp"
#include "../Director/Arbitrator/States/BrnArbStateTakedown.cpp"
#include "../Director/Arbitrator/States/BrnArbStateCrashMode.cpp"
#include "../Director/BrnCrashAnalyser.cpp"
#include "../Director/Arbitrator/States/BrnArbStateCrashNav.cpp"
#include "../Director/Arbitrator/States/BrnArbStatePostEvent.cpp"
#include "../Director/Arbitrator/States/BrnArbStateRaceIntro.cpp"
#include "../Director/Arbitrator/States/BrnArbStateDriveThru.cpp"
#include "../Director/Arbitrator/States/BrnArbStateOnlineRaceIntro.cpp"
#include "../Director/Arbitrator/States/BrnArbStateCarSelect.cpp"
#include "../Director/Arbitrator/States/BrnArbStateAttractMode.cpp"
#include "../Director/Arbitrator/States/BrnArbStateRankUp.cpp"
#include "../Director/Arbitrator/States/BrnArbStateRenderMetrics.cpp"
#include "../Director/Arbitrator/States/BrnArbStateOnlineCarSelect.cpp"
#include "../Director/BrnMainDirector.cpp"  // X360-only member: ARTIST places MainDirector between ArbStateOnlineCarSelect and DirectorModule in every pass (0x82236EB0.., 0x8224FB38..)
#include "../Director/DirectorModule/BrnDirectorModule.cpp"
#include "../Director/DirectorModule/BrnDirectorModuleIO.cpp"
#include "../Director/DirectorModule/BrnDirectorModuleIOSceneQuery.cpp"  // holds functions of BrnDirectorModuleIO.cpp (x360 tree split)
#include "../Director/DirectorModule/BrnDirectorModuleIOInputBuffer.cpp"  // holds functions of BrnDirectorModuleIO.cpp (x360 tree split)
#include "../Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.cpp"  // holds functions of BrnDirectorModuleIO.cpp (x360 tree split)
#include "../Director/BrnDirectorResourceManager.cpp"
#include "../Director/BrnDirectorResourceManagerICE.cpp"  // holds functions of BrnDirectorResourceManager.cpp (x360 tree split)
#include "../Director/Utils/BrnShotSelector.cpp"  // member GameSource/Director/BrnShotSelector.cpp (the x360 tree keeps it under Utils/)
#include "../Director/BrnDirectorDevTools.cpp"
#include "../Director/DirectorModule/BrnDirectorModuleDebugCompononent.cpp"
#include "../Director/DirectorModule/BrnDirectorModuleDebugPrinter.cpp"
#include "../Director/MomentController/BrnMomentController.cpp"
#include "../Director/MomentController/BrnMomentParameterBank.cpp"
#include "../Director/MomentController/BrnMomentSelector.cpp"
#include "../Director/MomentController/BrnMoment.cpp"  // DWARF/PS3 member (Moment base), no ARTIST body of its own (position not pinned)
#include "../Director/Utils/BrnSceneQueryInterface.cpp"
#include "../Director/Utils/BrnPostBox.cpp"  // DWARF/PS3 member, no ARTIST body of its own (position not pinned)
#include "../Director/MomentController/Moments/BrnMomentHardStop.cpp"
#include "../Director/MomentController/Moments/BrnMomentHitTraffic.cpp"
#include "../Director/MomentController/Moments/BrnMomentTumbling.cpp"
#include "../Director/MomentController/Moments/BrnMomentTakedownLookback.cpp"
#include "../Director/MomentController/Moments/BrnMomentPassengerSeesAction.cpp"
#include "../Director/MomentController/Moments/BrnMomentBystanderSeesAction.cpp"
#include "../Director/MomentController/Moments/BrnMomentFailsafe.cpp"
#include "../Director/MomentController/Moments/BrnMomentPlayerJumping.cpp"
#include "../Director/MomentController/Moments/BrnMomentPlayerStunt.cpp"
#include "../Director/MomentController/Moments/BrnMomentStaticCamImpact.cpp"
#include "../Director/MomentController/Moments/BrnMomentNewCarJoined.cpp"
#include "../Director/MomentController/Moments/BrnMomentStationaryCrash.cpp"
#include "../Director/Utils/BrnAbstractPool.cpp"
#include "../Director/Utils/BrnVehicleRef.cpp"
#include "../Director/Utils/BrnDirectorVehicleTracker.cpp"
#include "../Director/Utils/BrnDirectorWorldMap.cpp"
#include "../Director/Utils/BrnICEMoviePlayer.cpp"
#include "../Director/Utils/BrnICEMoviePlayerSerialise.cpp"  // holds functions of BrnICEMoviePlayer.cpp (x360 tree split)
#include "../Director/Utils/BrnDirectorAllVehicleData.cpp"
#include "../Director/Utils/BrnDirectorEffectTrigger.cpp"
#include "../Director/Shots/ShotControllers/BrnCameraInterpolationController.cpp"
#include "../Director/Shots/ShotControllers/BrnInertiaController.cpp"
#include "../Director/Camera/Camera.cpp"
#include "../Director/Shots/ShotControllers/BrnKeyAnimController.cpp"
#include "../Director/Camera/BrnBehaviourManager.cpp"
#include "../Director/Camera/BrnBehaviourParameterBank.cpp"
#include "../Director/Camera/BrnCameraState.cpp"
#include "../Director/Camera/BrnCameraEffects.cpp"
#include "../Director/Camera/BrnDepthOfField.cpp"  // DWARF/PS3 member, no ARTIST body of its own (position not pinned)
#include "../Director/Camera/Utils/BrnCameraRig.cpp"
#include "../Director/Camera/Utils/BrnCameraRigConstruct.cpp"  // holds functions of BrnCameraRig.cpp (x360 tree split)
#include "../Director/Camera/BrnCameraReference.cpp"
#include "../Director/Camera/BrnCameraValidityAccount.cpp"
#include "../Director/Camera/Utils/BrnCameraShake.cpp"
#include "../Director/Camera/Utils/BrnCameraShakeICEController.cpp"  // holds functions of BrnCameraShake.cpp (x360 tree split)
#include "../Director/Camera/Utils/BrnCameraShakeUpdate.cpp"  // holds functions of BrnCameraShake.cpp (x360 tree split)
#include "../Director/Shots/ShotControllers/BrnKeyAnimShakeController.cpp"
#include "../Director/Camera/Utils/BrnCameraSphericalRotationController.cpp"
#include "../Director/Camera/Utils/BrnCameraTweaker.cpp"
#include "../Director/Camera/Utils/BrnDebugController.cpp"
#include "../Director/Camera/Utils/BrnCamera2DRotationController.cpp"
#include "../Director/Camera/Utils/CameraUtils.cpp"
#include "../Director/Camera/Utils/BrnLooker.cpp"
#include "../Director/Camera/Utils/BrnOrientationLag.cpp"
#include "../Director/Camera/Utils/BrnVehicleCollisionPredictor.cpp"
#include "../Director/Camera/Utils/BrnPositionFinder.cpp"
#include "../Director/Camera/Utils/BrnPositionLag.cpp"
#include "../Director/Camera/Utils/BrnCameraSmoothMover.cpp"
#include "../Director/Camera/Utils/BrnInterpolater.cpp"  // DWARF/PS3 member, no ARTIST body of its own (position not pinned)
#include "../Director/Camera/Behaviours/Behaviour.cpp"  // DWARF/PS3 member (Behaviour base): ARTIST 0x82206388 / 0x822063E8 sit before the collision policies
#include "../Director/Camera/BrnCollisionPolicyAttachedToVehicle.cpp"  // holds functions of brncollisionpolicy.cpp (x360 tree split)
#include "../Director/Camera/BrnVisibilityCollisionPolicy.cpp"  // holds functions of brncollisionpolicy.cpp (x360 tree split)
#include "../Director/Camera/BrnVisibilityTest.cpp"  // holds functions of brncollisionpolicy.cpp (x360 tree split)
#include "../Director/Camera/BrnGeometryCollisionPredictor.cpp"  // holds functions of brncollisionpolicy.cpp (x360 tree split)
#include "../Director/Camera/Behaviours/BrnBehaviourGameplayExternal.cpp"  // holds functions of behaviourgameplayexternal.cpp (x360 tree split)
#include "../Director/Camera/Behaviours/BrnBehaviourGameplayBumper.cpp"
#include "../Director/Camera/Behaviours/BehaviourRig.cpp"
#include "../Director/Camera/Behaviours/BehaviourBystanderCam.cpp"
#include "../Director/Camera/Behaviours/BehaviourBystanderCamImpactControllers.cpp"  // holds functions of BehaviourBystanderCam.cpp (x360 tree split)
#include "../Director/Camera/Behaviours/BrnBehaviourHeliCam.cpp"  // holds functions of behaviourhelicam.cpp (x360 tree split)
#include "../Director/Camera/Behaviours/BehaviourPassengerCam.cpp"
#include "../Director/Camera/Behaviours/BrnBehaviourInterpolate.cpp"
#include "../Director/Camera/Behaviours/BrnBehaviourGyroCam.cpp"
#include "../Director/Camera/Behaviours/BrnBehaviourAftertouchCam.cpp"
#include "../Director/Camera/Behaviours/BrnBehaviourLooseAttachment.cpp"
#include "../Director/Camera/Behaviours/BrnBehaviourIceAnim.cpp"
#include "../Director/Camera/Behaviours/BrnBehaviourFixedCam.cpp"
#include "../Director/Camera/Behaviours/BrnBehaviourRoadRunner.cpp"
#include "../Director/Camera/Behaviours/BrnBehaviourRenderMetrics.cpp"
#include "../Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.cpp"
#include "../Director/Camera/Behaviours/BehaviourSpirallingDeathcam.cpp"  // holds functions of brnbehaviourspirallingdeathcam.cpp (x360 tree split)
#include "../Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.cpp"
#include "../Director/Shots/ShotControllers/BrnPerlinShakeController.cpp"
// Left out until their duplicates are resolved: Camera/Behaviours/BrnBehaviourAftertouchCrash.cpp (member 84)
// and BrnBehaviourFailsafe.cpp (86) repeat the TU-local helpers BrnBehaviourAftertouchCam.cpp defines
// (MakeVector3WithLanes, HasNegativeSign, IsZeroVmx, VecFloatMax/Min/Clamp, RotationX/Z, RefinedReciprocalSqrt,
// GuardedRoot) instead of the rwmath vpu calls ARTIST makes, and Failsafe's file-scope anonymous
// `using rw::math::vpu::Vector3` clashes with BrnCommonTypes.h's global Vector3 in later members.
// Also left out: Camera/Behaviours/BrnBehaviourDebugFlyWorld.cpp (95) repeats BrnBehaviourGameplayExternal.cpp's
// TU-local MakeVector3WithLanes lane builder (an imitation of the rwmath splats ARTIST uses).
// Also left out: Camera/BrnCameraFinaliser.cpp (functions of BrnDirectorModule.cpp, PS3 home; ARTIST's
// CameraFinaliser::Update 0x82250440 leads that member) repeats BrnKeyAnimShakeController.cpp's TU-local
// ReadFloat/ReadInt attribute-offset readers (raw-offset stand-ins for the attribute layout).
