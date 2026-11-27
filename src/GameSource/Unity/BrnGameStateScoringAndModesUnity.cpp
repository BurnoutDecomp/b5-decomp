// BrnGameStateScoringAndModesUnity: one of ARTIST's unity translation units (DWARF compile unit BrnGameStateScoringAndModesUnity.cpp),
// ARTIST 0x82311800-0x82354010. Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x82022AD8: ...gamesource\unity\../GameState/ModeManager/BrnModeManager.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 116/132 transitions forward);

#include "../GameState/ModeManager/BrnModeManager.cpp"
#include "../GameState/ModeManager/BrnModeManager_Release.cpp"  // holds functions of BrnModeManager.cpp (x360 tree split)
#include "../GameState/ModeManager/BrnModeManager_Destruct.cpp"  // holds functions of BrnModeManager.cpp (x360 tree split)
#include "../GameState/ModeManager/Debug/BrnCrashScoreDebugComponent.cpp"
#include "../GameState/ModeManager/Debug/BrnModeManagerDebugComponent.cpp"
#include "../GameState/ModeManager/Debug/BrnScoringSystemDebugComponent.cpp"  // X360-only member: ARTIST __FILE__ 0x82022E88 between ModeManagerDebugComponent and ScoringSystem
#include "../GameState/ModeManager/Scoring/BrnScoringSystem_UpdateB.cpp"  // holds functions of brnscoringsystem.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnScoringSystem_Standings.cpp"  // holds functions of brnscoringsystem.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnScoringSystem_UpdateA.cpp"  // holds functions of brnscoringsystem.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnScoringSystem_Lifecycle.cpp"  // holds functions of brnscoringsystem.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnScoringSystem_Finish.cpp"  // holds functions of brnscoringsystem.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnScoringSystem_Timer.cpp"  // holds functions of brnscoringsystem.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnScoringSystem_Lookup.cpp"  // holds functions of brnscoringsystem.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnCarData.cpp"  // holds functions of BrnScoringSystem.cpp (ARTIST 0x8231E0D0.. between its Lookup and Lifecycle pieces)
#include "../GameState/ModeManager/Scoring/BrnScoringSystem_Queries.cpp"  // holds functions of brnscoringsystem.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnScoringSystem_RoadRage.cpp"  // holds functions of brnscoringsystem.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp"
#include "../GameState/ModeManager/Scoring/BrnStuntModeScoring_Queries.cpp"  // holds functions of brnstuntmodescoring.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnStuntModeScoring_Register.cpp"  // holds functions of brnstuntmodescoring.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnStuntModeScoring_StuntTypes.cpp"  // holds functions of brnstuntmodescoring.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnStuntModeScoring_Lifecycle.cpp"  // holds functions of brnstuntmodescoring.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnStuntModeScoring_Combo.cpp"  // holds functions of brnstuntmodescoring.cpp (x360 tree split)
#include "../GameState/ModeManager/Scoring/BrnStuntModeScoringOnline.cpp"  // X360-only member: ARTIST __FILE__ 0x82023308 between StuntModeScoring and BaseOnlineModeScoring
#include "../GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.cpp"
#include "../GameState/ModeManager/Scoring/BrnRoadRageModeScoring.cpp"
#include "../GameState/ModeManager/Scoring/BrnOnlineRoadRageModeScoring.cpp"
#include "../GameState/ModeManager/Scoring/BrnOnlineRaceModeScoring.cpp"
#include "../GameState/ModeManager/Scoring/BrnOnlineBurningHomeRunModeScoring.cpp"
#include "../GameState/ModeManager/Scoring/BrnOnlineStuntRunModeScoring.cpp"  // X360-only member: ARTIST 0x823159C8 after OnlineBurningHomeRunModeScoring, before GameMode
#include "../GameState/ModeManager/GameModes/BrnGameMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnGameModeParams.cpp"  // X360-only member (position not pinned by ARTIST's layout)
#include "../GameState/ModeManager/GameModes/BrnOfflineGameMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnPursuitMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnCrashMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnOnlineRaceMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnOnlineShowtimeMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnOnlineFreeBurnLobbyMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnOnlineGameMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnRaceMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnBurningRoute.cpp"
#include "../GameState/ModeManager/GameModes/BrnOnlineRoadRageMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnFaceOffMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnRoadRageMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnOnlineFreeBurnMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnOnlineBurningHomeRunMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnOnlineStuntRunMode.cpp"  // X360-only member: ARTIST 0x823162A8 after OnlineFreeBurnMode, before StuntAttackMode
#include "../GameState/ModeManager/GameModes/BrnStuntAttackMode.cpp"
#include "../GameState/ModeManager/GameModes/BrnSurvivor.cpp"
#include "../GameState/ModeManager/GameModes/BrnBurnoutSkillzManager.cpp"
#include "../GameState/ModeManager/GameModeStates/BrnIntroState.cpp"
#include "../GameState/ModeManager/GameModeStates/BrnCountdownState.cpp"
#include "../GameState/ModeManager/GameModeStates/BrnInProgressState.cpp"
#include "../GameState/ModeManager/GameModeStates/BrnOutroState.cpp"
#include "../GameState/ModeManager/GameModeStates/BrnResultsState.cpp"
#include "../GameState/ModeManager/GameModeStates/BrnQuitState.cpp"
#include "../GameState/ModeManager/GameModeStates/BrnOnlineLoadingState.cpp"
#include "../GameState/ModeManager/ChallengeManager/BrnChallengeManager.cpp"
#include "../GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.cpp"
#include "../GameState/RoadRules/BrnRoadRulesManager.cpp"
#include "../GameState/RoadRules/BrnRoadRulesDebugComponent.cpp"
#include "../GameState/StreetData/BrnGameStateStreetManager.cpp"
#include "../GameState/StreetData/BrnStreetManagerDebugComponent.cpp"
#include "../GameState/StreetData/BrnChallengeHighScoreEntry.cpp"
// Left out until StuntModeScoring's DWARF tunables are one block (BrnStuntModeScoring.cpp:26..71):
// ModeManager/Scoring/BrnStuntModeScoring_UpdatePass.cpp (functions of BrnStuntModeScoring.cpp, x360 tree split)
// repeats _StuntTypes.cpp's file-local copies (KF_STUNT_ATTACK_REVERSE_TAKEOFF_SCORE_MULTIPLIER, ...).
