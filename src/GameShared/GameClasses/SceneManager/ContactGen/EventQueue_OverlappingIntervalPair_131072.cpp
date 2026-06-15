#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsInterval.h"

// CgsModule::EventQueue<CgsSceneManager::OverlappingIntervalPair, 131072>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (131072) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<CgsSceneManager::OverlappingIntervalPair, 131072>::Construct();
