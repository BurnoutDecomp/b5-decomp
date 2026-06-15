#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h"

// CgsModule::EventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent, 24>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (24) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent, 24>::Construct();
