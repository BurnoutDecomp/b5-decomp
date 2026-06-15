#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"

// CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::ActivateHullEvent, 8>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (8) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::ActivateHullEvent, 8>::Construct();
