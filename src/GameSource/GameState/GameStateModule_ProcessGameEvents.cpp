// BrnGameState::GameStateModule::ProcessGameEvents -- the module's game-event dispatcher.
//
// PreWorldUpdate hands it the frame's merged game-event queue, the output action queue, the
// pre-world input buffer and the output buffer. It walks the queue once, front to back, and answers
// every event by its id through one switch; after the walk it posts the prop-progression report the
// prop world asked for (case 112 arms it) and clears the request. The whole walk is bracketed by the
// module's "Process events" PerfMonCpu monitor.
//
// The jump table has 176 slots (ids 0..175). Ids 3, 19, 30, 56, 86, 87, 90, 91, 92, 134 and anything
// above 175 reach the default arm, which asserts with the offending id. Ids 11, 13, 63, 116 and 148
// jump straight to the next event. Every other id has its own arm; the arms are grouped by the
// managers they drive into five private helpers, ProcessGameEvents_Group1..5, one partfile each
// (GameStateModule_ProcessGameEvents_wBT_01..05.cpp). The case lists below are the groups.
//
// Right after starting the monitor the console copies the input buffer's 48-byte timer block into
// mTimerStatusInterface; the mode and scoring updates that follow the walk read the module's copy.

#include "GameSource/GameState/BrnGameStateModule.h"

#include <stdlib.h>                                                     // getenv (the action-199 witness)

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // PerfMonCpu::StartMonitor / StopMonitor
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint (the action-199 witness)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // GetFirstEvent / GetNextEvent / AddEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // GameEventQueue, PreWorldInputBuffer, OutputBuffer
#include "GameSource/GameState/BrnGameEvents.h"                          // EGameEventType
#include "GameSource/GameState/BrnGameActions.h"                         // PropSmashReportAction (action 199)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // ProgressionManager::GetProfile
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile::GetHitProps
