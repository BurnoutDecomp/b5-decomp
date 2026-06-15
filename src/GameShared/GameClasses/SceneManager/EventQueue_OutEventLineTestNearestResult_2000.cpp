#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"

// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult, 2000>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (2000) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult, 2000>::Construct();
