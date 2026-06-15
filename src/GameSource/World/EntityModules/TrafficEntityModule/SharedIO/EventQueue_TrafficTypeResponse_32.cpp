#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h"

// CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse, 32>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (32) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse, 32>::Construct();
