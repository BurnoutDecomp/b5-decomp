// BrnSoundUnity: one of ARTIST's unity translation units (DWARF compile unit BrnSoundUnity.cpp),
// ARTIST 0x82682898-0x827048C0. Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (...gamesource\unity\../Sound/Module/BrnRootSoundModule.cpp;
// the GameShared members are included by their own paths). Order: ARTIST's emission passes over the
// range (MSVC emits bottom-up over the call graph, each level in definition order: 421/440 transitions
// forward; scratch unity map, Appendix A). The x360 tree's split files follow the member whose
// functions they hold (the ledger's home file).

#include "../Sound/Module/BrnRootSoundModule.cpp"
#include "../Sound/Module/BrnRootSoundModuleIO.cpp"
#include "../Sound/Module/LogicModule/BrnState.cpp"
#include "../Sound/Module/LogicModule/BrnStateManager.cpp"
#include "../Sound/Module/LogicModule/BrnEffectObject.cpp"
#include "../Sound/Module/LogicModule/BrnEffectControl.cpp"
#include "../Sound/Module/LogicModule/Brn3DEffectControl.cpp"  // holds functions of BrnEffectControl.cpp (x360 tree split)
#include "../Sound/Module/LogicModule/Brn3DUserSpaceEffectControl.cpp"  // holds functions of BrnEffectControl.cpp (x360 tree split)
#include "../Sound/Module/LogicModule/BrnSoundLogicModule.cpp"
#include "../Sound/Module/LogicModule/BrnSoundLogicModuleIo.cpp"
#include "../Sound/Module/SharedIO/BrnPreUpdateSharedIo.cpp"
#include "../Sound/BrnResourceRegistrar.cpp"
#include "../Sound/Streaming/BrnStreamingState.cpp"
#include "../Sound/Streaming/BrnStreamingStateManager.cpp"
#include "../Sound/Streaming/BrnStreamingEffect.cpp"
#include "../Sound/Vehicles/BrnVehicleState.cpp"
#include "../Sound/Vehicles/BrnVehicleStateManager.cpp"
#include "../Sound/Vehicles/BrnPlayerVehicleState.cpp"
#include "../Sound/Vehicles/BrnPlayerVehicleStateManager.cpp"
#include "../Sound/Vehicles/BrnAIVehicleStateManager.cpp"
#include "../Sound/Vehicles/BrnAIVehicleState.cpp"
#include "../Sound/Vehicles/Traffic/BrnTrafficEngine.cpp"
#include "../Sound/Vehicles/Traffic/BrnTrafficHorn.cpp"
#include "../Sound/Vehicles/Traffic/BrnTrafficSkid.cpp"
#include "../Sound/Traffic/BrnTrafficState.cpp"
#include "../Sound/Vehicles/Traffic/BrnTrafficStateManager.cpp"
#include "../Sound/Vehicles/Traffic/BrnTrafficControl.cpp"
#include "../Sound/Vehicles/Engines/BrnPhysicsControl.cpp"
#include "../Sound/Vehicles/Engines/BrnEngines.cpp"  // holds functions of the engine members (x360 tree split)
#include "../Sound/Vehicles/Engines/BrnEngineControl.cpp"
#include "../Sound/Vehicles/Engines/BrnShiftControl.cpp"
#include "../Sound/Vehicles/Engines/BrnHybridEngineControl.cpp"
#include "../Sound/Vehicles/Engines/BrnHybridExhaustControl.cpp"  // holds functions of BrnHybridEngineControl.cpp (x360 tree split)
#include "../Sound/Vehicles/Engines/BrnClutchControl.cpp"
#include "../Sound/Vehicles/Engines/BrnDualGinsuEffect.cpp"
#include "../Sound/Vehicles/Engines/BrnDualGinsuExhaustEffect.cpp"  // holds functions of BrnDualGinsuEffect.cpp (x360 tree split)
#include "../Sound/Vehicles/Engines/BrnSingleGinsuEffect.cpp"
#include "../Sound/Vehicles/Engines/BrnLoopModelEffect.cpp"
#include "../Sound/Vehicles/Engines/BrnBoostEffect.cpp"
#include "../Sound/Vehicles/Engines/BrnAIPhysicsControl.cpp"
#include "../Sound/Vehicles/Engines/BrnTurboEffect.cpp"
#include "../Sound/Vehicles/Engines/BrnWhineEffect.cpp"
#include "../Sound/Vehicles/Engines/BrnSweetenersEffect.cpp"
#include "../Sound/Vehicles/Deformation/BrnDeformationEffect.cpp"
#include "../Sound/Vehicles/Brn3dCarPosition.cpp"
#include "../Sound/Vehicles/Wheels/BrnSkidEffect.cpp"
#include "../Sound/Vehicles/Wheels/BrnInAirEffect.cpp"
#include "../Sound/Vehicles/Wheels/BrnJunkyardInAirEffect.cpp"  // holds functions of BrnInAirEffect.cpp (x360 tree split)
#include "../Sound/Vehicles/BrnWheels.cpp"  // holds functions of BrnInAirEffect.cpp (x360 tree split)
#include "../Sound/Vehicles/Wheels/BrnWheelControl.cpp"
#include "../Sound/Vehicles/Wheels/BrnRoadNoise.cpp"
#include "../Sound/Vehicles/Wheels/BrnAISkidEffect.cpp"
#include "../Sound/Vehicles/Environment/BrnEnclosureControl.cpp"
#include "../Sound/Vehicles/Environment/BrnAmbienceEffect.cpp"
#include "../Sound/Vehicles/Environment/BrnAmbienceControl.cpp"  // holds functions of BrnAmbienceEffect.cpp (x360 tree split)
#include "../Sound/Vehicles/Environment/BrnStaticPassbyControl.cpp"
#include "../Sound/Vehicles/Environment/BrnCrashStreamEffect.cpp"
#include "../Sound/Vehicles/Environment/BrnSpeedStreamEffect.cpp"
#include "../Sound/Vehicles/Environment/BrnSpeedStreamControl.cpp"  // holds functions of BrnSpeedStreamEffect.cpp (x360 tree split)
#include "../Sound/Vehicles/Environment/BrnReverbEffect.cpp"
#include "../Sound/Vehicles/Environment/BrnCarStereoEffect.cpp"
#include "../../SharedClasses/Sound/World/BrnSoundWorldScene.cpp"  // ARTIST ../Sound/World/BrnSoundWorldScene.cpp
#include "../Sound/World/BrnEmitterEffect.cpp"
#include "../Sound/Module/LogicModule/BrnEmitter3dControl.cpp"  // holds functions of BrnEmitterEffect.cpp (x360 tree split)
#include "../Sound/Module/LogicModule/BrnEmitterState.cpp"
#include "../Sound/Module/LogicModule/BrnEmitterStateManager.cpp"
#include "../Sound/Global/BrnGlobalStateManager.cpp"
#include "../Sound/Global/BrnGlobalState.cpp"
#include "../Sound/Global/BrnMixerControl.cpp"
#include "../Sound/Global/BrnHUDEffect.cpp"
#include "../Sound/Global/BrnCameraControl.cpp"
#include "../Sound/Global/BrnMusicEffect.cpp"
#include "../Sound/Global/BrnMusicStream.cpp"  // holds functions of BrnMusicEffect.cpp (x360 tree split)
#include "../Sound/Global/BrnFxEffect.cpp"
#include "../Sound/Global/BrnPresentationEffect.cpp"
#include "../Sound/Module/LogicModule/BrnSubmixesEffect.cpp"
#include "../Sound/Global/BrnSpeechEffect.cpp"
#include "../Sound/Collision/BrnCollisionState.cpp"
#include "../Sound/Collision/BrnCollisionEffect.cpp"
#include "../Sound/Collision/BrnCollisionControl.cpp"
#include "../Sound/Collision/BrnScrapeEffect.cpp"
#include "../Sound/Collision/BrnCollisionStateManager.cpp"
#include "../Sound/Collision/BrnRaceCarCache.cpp"  // holds functions of BrnCollisionStateManager.cpp (x360 tree split)
#include "../Sound/Collision/BrnHingeStateCache.cpp"  // holds functions of BrnCollisionStateManager.cpp (x360 tree split)
#include "../Sound/Collision/BrnCollisionFrameInformation.cpp"  // holds functions of BrnCollisionStateManager.cpp (x360 tree split)
#include "../Sound/Passby/BrnPassbyState.cpp"
#include "../Sound/Passby/BrnPassbyStateManager.cpp"
#include "../Sound/Passby/BrnPassbyEffect.cpp"
#include "../Sound/Explosions/BrnExplosionEffect.cpp"
#include "../Sound/Explosions/BrnExplosionState.cpp"
#include "../Sound/BrnDebugComponent.cpp"
#include "../Sound/Debug/BrnSoundDebugStatistics.cpp"  // ARTIST ../Sound/BrnSoundStatistics.cpp
#include "../../GameShared/GameClasses/Sound/CgsSoundUtils.cpp"
#include "../../GameShared/GameClasses/Sound/IO/CgsMessage.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/CgsCommon.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/CgsEnvironment.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/CgsMasterVoice.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/CgsRegistry.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/CgsObject.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/CgsContentOnDetach.cpp"  // holds functions of CgsObject.cpp (x360 tree split)
#include "../../GameShared/GameClasses/Sound/Playback/CgsContentDtor.cpp"  // holds functions of CgsObject.cpp (x360 tree split)
#include "../../GameShared/GameClasses/Sound/Playback/CgsVoice.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacPlayerVoice.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacReverbIRContent.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoice.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacWaveContent.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/RWAC/CgsSnrResourceType.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/RWAC/CgsSnrResourceTypeDescriptor.cpp"  // holds functions of CgsSnrResourceType.cpp (x360 tree split)
#include "../../GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoice.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoiceNew.cpp"  // holds functions of CgsAemsPlayerVoice.cpp (x360 tree split)
#include "../../GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoiceDtor.cpp"  // holds functions of CgsAemsPlayerVoice.cpp (x360 tree split)
#include "../../GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsContent.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsInterfaceImplementation.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerFactory.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerFactoryDtor.cpp"  // holds functions of CgsSplicerFactory.cpp (x360 tree split)
#include "../../GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerContent.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.cpp"  // ARTIST .../Splicer/internal/SpliceManager.cpp
#include "../../GameShared/GameClasses/Sound/Playback/Splicer/internal/SpliceObjects.cpp"
#include "../../GameShared/GameClasses/Sound/Playback/Plugins/Ginsu/GinsuPlayer.cpp"  // also holds CgsGinsuSlot.cpp's function (x360 tree)
#include "../../GameShared/GameClasses/Sound/Playback/Plugins/Streaming/internal/sndplayer1shared.cpp"  // also holds sndplayer1.cpp's functions (x360 tree)
#include "../../GameShared/GameClasses/Sound/Playback/Plugins/GainArray/CgsGainArrayPlugin.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsMicrophone.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsEffectBase.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/Cgs3dEffectControl.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsEnvironment.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsDynamicMixer.cpp"  // holds functions of CgsEnvironment.cpp (x360 tree split)
#include "../../GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsState.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsStateManager.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsVoiceHierarchy.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsVoiceHierarchyResourceType.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsContent.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsVoice.cpp"
#include "../../GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.cpp"  // holds functions of CgsVoice.cpp (x360 tree split)
#include "../../GameShared/GameClasses/Sound/Logic/CgsVoicePool.cpp"  // holds functions of CgsVoice.cpp (x360 tree split)
#include "../../GameShared/GameClasses/Sound/CgsMemBase.cpp"
