// BrnWorldAIUnity: one of ARTIST's unity translation units (DWARF compile unit BrnWorldAIUnity.cpp),
// ARTIST 0x82765C50-0x8279B974. Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x820C50F0: ...gamesource\unity\../World/AI/BrnAICar.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 94/103 transitions forward);

#include "../World/AI/BrnAICar.cpp"
#include "../World/AI/BrnAIDriver.cpp"
#include "../World/AI/Route/BrnRacingLine.cpp"  // RacingLine::ClearSectionCache: header-inline in the original (DWARF BrnRacingLine.h:452), emitted by BrnAIDriver.cpp's callers
#include "../World/AI/BrnAIAggression.cpp"
#include "../World/AI/BrnAIAggressiveness.cpp"
#include "../World/AI/BrnAIUtils.cpp"
#include "../World/AI/BrnAIBuzzBy.cpp"
#include "../World/AI/BrnAIDebugComponent.cpp"
#include "../World/AI/BrnAIDebugUtils.cpp"
#include "../World/AI/RaceBalancing/BrnRaceBalancingDebugComponent.cpp"
#include "../World/AI/Route/BrnAStar.cpp"
#include "../World/AI/Route/BrnRoute.cpp"
#include "../World/AI/Route/BrnRouteMapModule.cpp"
#include "../World/AI/Route/BrnRouteMapDebugComponent.cpp"
#include "../World/AI/PID/BrnPIDController.cpp"
#include "../World/AI/RacingLine/BrnRacingLineGenerator.cpp"
#include "../World/AI/RacingLine/BrnHardNoGoMap.cpp"
#include "../World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp"
#include "../World/AI/ResetOnTrack/BrnResetOnTrackManager_AvoidObstacles.cpp"  // holds functions of BrnResetOnTrackManager.cpp (x360 tree split)
#include "../World/AI/ResetOnTrack/BrnResetOnTrackDebugComponent.cpp"
#include "../World/AI/RacingLine/BrnAISteeringFan.cpp"
#include "../World/AI/BrnRouteRequestManager.cpp"
#include "../World/AI/RaceBalancing/BrnRaceBalancingManager.cpp"
#include "../World/AI/BrnHNGTest.cpp"
#include "../World/AI/RaceBalancing/BrnRaceBalancingRoute.cpp"
#include "../World/AI/BrnAIModule.cpp"
#include "../World/AI/BrnAIModule_Drive.cpp"  // holds functions of BrnAIModule.cpp (x360 tree split)
#include "../World/AI/BrnAIModule_ResetPump.cpp"  // holds functions of BrnAIModule.cpp (x360 tree split)
#include "../World/AI/SharedIO/BrnAIModuleIO_InputBuffer_PostPhysics.cpp"  // holds functions of BrnAIModuleIO.cpp (x360 tree split)
#include "../World/AI/SharedIO/BrnAIModuleIO_InputBuffer_Accessors.cpp"  // holds functions of BrnAIModuleIO.cpp (x360 tree split)
#include "../World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.cpp"  // holds functions of BrnAIModuleIO.cpp (x360 tree split)
#include "../World/AI/SharedIO/BrnAIModuleIO_OutputBuffer_Accessors.cpp"  // holds functions of BrnAIModuleIO.cpp (x360 tree split)
// Left out until its duplicates are resolved: ResetOnTrack/BrnResetOnTrackManager_Strategies.cpp
// (functions of BrnResetOnTrackManager.cpp) repeats TU-local helpers other members define
// (Dot2D, IsSimilar2D, KF_ZERO) instead of the rwmath vpu calls ARTIST makes.
