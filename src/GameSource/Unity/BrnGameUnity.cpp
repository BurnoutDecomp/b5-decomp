// BrnGameUnity: one of ARTIST's unity translation units (DWARF compile unit BrnGameUnity.cpp),
// ARTIST 0x823A8B40-0x823F3190. Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x82036320: ...gamesource\unity\../Game/BrnGameModule.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 71/80 transitions forward);

#include "../Game/BrnGameModule.cpp"
#include "../Game/GameBridgeDirectorToX.cpp"
#include "../Game/BrnDispatchThreadInputBuffer.cpp"
#include "../Game/BrnGlobalCpuMonitors.cpp"
#include "../Game/BrnAutoTestManager.cpp"
#include "../Game/BrnAutoTestManagerLUACallbacks.cpp"  // holds functions of BrnAutoTestManager.cpp (x360 tree split)
#include "../Game/GameBridgeGUIToX.cpp"
#include "../Game/GameBridgeNetworkToX.cpp"
#include "../Game/BrnLUACall.cpp"
#include "../Game/GameBridgeGameStateToX.cpp"
#include "../Game/GameBridgeGameStateToX_ShowtimeGuiEvents.cpp"  // holds functions of GameBridgeGameStateToX.cpp (x360 tree split)
#include "../Game/GameBridgeWorldToX.cpp"
#include "../Game/GameBridgeWorldToGui.cpp"
#include "../Game/GameBridgeRendererToX.cpp"  // holds functions of GameBridgeWorldToGui.cpp (x360 tree split)
#include "../Game/GameBridgeControllerToX.cpp"
#include "../Game/GameBridgeSoundToX.cpp"
#include "../GameFlowController/TopLevel/BrnGameMainFlowController.cpp"
#include "../Game/GameBridgeReplayToX.cpp"
#include "../GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"  // holds functions of brnloadingscriptedstate.cpp (x360 tree split)
#include "../GameFlowController/TopLevel/BrnGameMainFlowCheckDiskSpace.cpp"
#include "../GameFlowController/TopLevel/BrnGameMainFlowInGameState.cpp"
#include "../Game/BrnLoadingScreenRenderer.cpp"
// X360-only members (no PS3 counterpart): every ARTIST pass after BrnLoadingScreenRenderer runs
// SystemHWX360, SystemHWX360Massive, Massive, MassiveSubscriber, MassiveDebugComponent
// (0x823AAE10..0x823ABC28, 0x823C1290..0x823C1688, 0x823C6C70..0x823C6D48).
#include "../Game/X360/BrnSystemHWX360.cpp"
#include "../Game/X360/BrnSystemHWX360Massive.cpp"
#include "../Massive/BrnMassive.cpp"
#include "../Massive/BrnMassiveSubscriber.cpp"
#include "../Massive/BrnMassiveDebugComponent.cpp"
