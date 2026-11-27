// BrnGameStateUnity: one of ARTIST's unity translation units (DWARF compile unit BrnGameStateUnity.cpp),
// ARTIST 0x82356A60-0x823A5F38. Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x8202C1F8: ...gamesource\unity\../GameState/BrnGameStateModule.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 141/151 transitions forward); first member from GCC's _GLOBAL__I_BrnGameState::GameStateModule::Construct (DecFIGS).

#include "../GameState/BrnGameStateModule.cpp"
#include "../GameState/GameStateModule_GetActiveRaceCarIndexFromNetworkPlayer.cpp"  // holds functions of BrnGameStateModule.cpp (x360 tree split)
#include "../GameState/GameStateModule_RoadRules.cpp"  // holds functions of BrnGameStateModule.cpp (x360 tree split)
#include "../GameState/GameStateModule_SendSetUpAllDriveThrus.cpp"  // holds functions of BrnGameStateModule.cpp (x360 tree split)
#include "../GameState/GameStateModule_ProcessGameEvents.cpp"  // holds functions of BrnGameStateModule.cpp (x360 tree split)
#include "../GameState/BrnGameStateModuleIO.cpp"
#include "../GameState/BrnGameStateSharedIO.cpp"
#include "../GameState/BrnGameStateDebugComponent.cpp"
#include "../GameState/BrnResetPlayerDebugComponent.cpp"
#include "../GameState/MugshotManager/BrnMugshotManager.cpp"
#include "../GameState/PaybackManager/BrnPaybackManager.cpp"
#include "../GameState/PaybackManager/BrnPaybackDebugComponent.cpp"  // X360-only member: ARTIST 0x82357B88 after MugshotManager, before ImageManagerBase
#include "../GameState/ImageManager/BrnGameStateImageManagerBase.cpp"
#include "../GameState/InviteManager/BrnGameStateInviteManager.cpp"
#include "../GameState/FlybyManager/BrnGameStateFlybyManager.cpp"
#include "../GameState/FlybyManager/BrnGameStateOnlineFlybyManager.cpp"
#include "../GameState/Offences/BrnDriveThruManager.cpp"
#include "../GameState/RumbleManager/BrnRumbleManager.cpp"
#include "../GameState/TriggerQueryManager/BrnTriggerQueryManager.cpp"
#include "../GameState/TriggerQueryManager/BrnTriggerQueryManager_Prepare.cpp"  // holds functions of BrnTriggerQueryManager.cpp (x360 tree split)
#include "../GameState/TriggerQueryManager/BrnTriggerQueryManagerDebugComponent.cpp"
#include "../GameState/TriggerQueryManager/BrnTriggerQueryManagerDebugComponent_RenderWorld.cpp"  // holds functions of BrnTriggerQueryManagerDebugComponent.cpp (x360 tree split)
#include "../GameState/SharedIO/BrnGameStateToGuiIOInterfaces.cpp"
#include "../GameState/CarSelect/BrnCarSelectManager.cpp"
#include "../GameState/NetworkRoundManager/BrnNetworkRoundManager.cpp"
#include "../GameState/CarSelect/BrnOnlineCarSelectManager.cpp"
#include "../GameState/Offences/BrnStuntManager.cpp"
#include "../GameState/Offences/BrnStuntManagerDebugComponent.cpp"
#include "../GameState/Offences/BrnStuntManagerDebugComponent_GetTriggerWorldRegion.cpp"  // holds functions of BrnStuntManagerDebugComponent.cpp (x360 tree split)
#include "../GameState/TrainingManager/BrnTrainingManager.cpp"
#include "../GameState/TakedownManager/BrnTakedownManager.cpp"
#include "../GameState/TakedownManager/BrnTakedownManagerDebugComponent.cpp"
#include "../GameState/ModeManager/Hud/BrnHUDMessageLogic.cpp"
#include "../GameState/ModeManager/Hud/BrnHUDMessageLogic_Destruct.cpp"  // holds functions of BrnHUDMessageLogic.cpp (x360 tree split)
#include "../GameState/Progression/BrnProgressionManager.cpp"
#include "../GameState/Progression/BrnProfile.cpp"
#include "../GameState/Progression/BrnProgressionDebugComponent.cpp"
#include "../GameState/RichPresenceManager/BrnGameStateRichPresenceManagerBase.cpp"
#include "../GameState/AchievementManager/BrnGameStateAchievementManagerBase.cpp"
#include "../GameState/RichPresenceManager/X360/BrnGameStateRichPresenceManagerX360.cpp"  // X360-only member: every pass places it after AchievementManagerBase (0x8235B210, 0x82372C18)
// The X360 AchievementManagerBase hooks: ARTIST emits them where AchievementManagerX360.cpp's first pass
// sits (0x8235B2D8..0x8235B6A8, after RichPresenceManagerX360); the x360 tree keeps their bodies in these two files.
#include "../GameState/AchievementManager/PS3/BrnGameStateAchievementManagerPS3.cpp"
#include "../GameState/AchievementManager/BrnGameStateAchievementManagerBase_Freeburn.cpp"  // holds functions of the hooks' file (x360 tree split)
#include "../GameState/AchievementManager/X360/BrnGameStateAchievementManagerX360.cpp"  // X360-only member: ARTIST __FILE__ ../gamestate/achievementmanager/x360/brngamestateachievementmanagerx360.cpp
#include "../GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.cpp"  // X360-only member: 0x82367428 after AchievementManagerX360 (0x82367000)
