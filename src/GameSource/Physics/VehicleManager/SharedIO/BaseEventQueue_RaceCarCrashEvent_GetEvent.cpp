#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::RaceCarCrashEvent (64-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent>::GetEvent(s32)  @ 0x822ACAE0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by (9 callers) the
// race-car crash consumers: RaceCarEntityModule::ProcessRaceCarCrashEvents_PostPhysics,
// ModeManager::ProcessPlayerCrashes, ScoringSystem::UpdateCrashes, ChallengeManager::PostWorldUpdate
// and HUDMessageLogic::DetectCrashes / ::DetectOnlineCrashes -- each walks the race-car crash queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275), then returns &mpEvents[liIndex] (result = (a2<<6) + mpEvents, slwi
// r11,r29,6). The 64-byte stride is sizeof(RaceCarCrashEvent) == 64. DWARF confirms
// CgsModule::BaseEventQueue<RaceCarCrashEvent>::GetEvent(int) (CgsBaseEventQueue.h dwarf line
// 7245/7283). The Hex-Rays `int` return is the ABI-returned RaceCarCrashEvent& pointer.
template BrnPhysics::Vehicle::RaceCarCrashEvent&
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent>::GetEvent(s32);
