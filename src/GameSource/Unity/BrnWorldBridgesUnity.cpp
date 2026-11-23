// BrnWorldBridgesUnity: one of ARTIST's unity translation units (DWARF compile unit BrnWorldBridgesUnity.cpp),
// ARTIST 0x827A4F58-0x827AF464.
// Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x820C99C8: ...gamesource\unity\../World/Bridges/WorldBridgeCrashToEntityModules.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 25/30 transitions forward);
// first member: the one PS3's static constructor names (_GLOBAL__I_ of WorldModule::BridgeAIModuleToOutput).

#include "../World/Bridges/WorldBridgeCrashInputs.cpp"  // holds functions of WorldBridgeInputToCrash.cpp (x360 tree split)
#include "../World/Bridges/WorldBridgeInputToEntityModules.cpp"
#include "../World/Bridges/WorldBridgeAIModule.cpp"  // holds functions of WorldBridgeAIToEntityModules.cpp (x360 tree split)
#include "../World/Bridges/WorldBridgePropModule.cpp"  // holds functions of WorldBridgeAIToEntityModules.cpp (x360 tree split)
#include "../World/Bridges/WorldBridgeCrashToEntityModules.cpp"
#include "../World/Bridges/WorldBridgeCrashPostScene.cpp"  // holds functions of WorldBridgeCrashToEntityModules.cpp (x360 tree split)
#include "../World/Bridges/WorldBridgeEntityModulesToAI.cpp"
#include "../World/Bridges/WorldBridgeEntityModulesToCrash.cpp"
#include "../World/Bridges/WorldBridgeEntityModulesToEntityModules.cpp"
#include "../World/Bridges/WorldBridgeRaceCarToTrafficModule.cpp"  // holds functions of WorldBridgeEntityModulesToEntityModules.cpp (x360 tree split)
#include "../World/Bridges/WorldBridgeEntityModulesToOutput.cpp"
#include "../World/Bridges/WorldBridgeEntityModulesToPhysics.cpp"
#include "../World/Bridges/WorldBridgePhysicsToEntityModules.cpp"
#include "../World/Bridges/WorldBridgeEntityModulesToScene.cpp"
#include "../World/Bridges/WorldBridgePhysicsToScene.cpp"
#include "../World/Bridges/WorldBridgeSceneToEntityModules.cpp"
#include "../World/Bridges/WorldBridgeSceneToPhysics.cpp"
#include "../World/Bridges/WorldBridgeToEntityModules.cpp"
// Left out until the duplicate is resolved: WorldBridgeRaceCarToPropModule.cpp and
// WorldBridgeWorldModuleToPropModule.cpp (functions of WorldBridgeEntityModulesToEntityModules.cpp)
// each repeat WorldBridgePropModule.cpp's file-local _AssertLayout layout pins.
